/**********************************************************\
Original Author: Andrew Ofisher (zaltar)

License:    GNU General Public License, version 3.0
            http://www.gnu.org/licenses/gpl-3.0.txt

Copyright 2012 Andrew Ofisher
\**********************************************************/

#include "BlabbleAudioManager.h"
#include "Blabble.h"
#include "BlabbleLogging.h"

#include "boost/filesystem.hpp"
#include "boost/filesystem/operations.hpp"
#include <boost/optional.hpp>


// REITEK: compare the currently set capture and playback device with those provided: if they are the same, there is no need to set them
static bool CompareCurrentAudioDevices(int capture, int playback)
{
	int captureId, playbackId;

	pj_status_t status = pjsua_get_snd_dev(&captureId, &playbackId);
	if (status == PJ_SUCCESS)
	{
		if ((captureId == capture) && (playbackId == playback))
			return true;
	}

	return false;
}

// REITEK: Get/parse parameters passed to the plugin upon manager creation

BlabbleAudioManager::BlabbleAudioManager(Blabble& pluginCore) :
	pluginCore_(pluginCore),
//	wav_path_(wavPath),
	ring_audio_device_(-1),
	ring_volume_(NULL),
	using_inring_tone_(false),
	old_capture_dev_(-1),
	old_playback_dev_(-1),
	old_playback_volume_(NULL),
	pool_(NULL),
	ring_port_(NULL),
	in_ring_port_(NULL),
	call_wait_ring_port_(NULL),
	in_ring_player_(-1),
	wav_player_(-1),
	ring_slot_(-1),
	in_ring_slot_(-1),
	call_wait_slot_(-1),
	wav_slot_(-1)
{
	// Fail early

	pool_ = pjsua_pool_create("PluginSIP", 4096, 4096);
	if (pool_ == NULL)
		throw std::runtime_error("Ran out of memory creating pool!");

	// Set defaults

	// REITEK: Use our own directory for ringtones etc
#if 0
	std::string path = executionPath;
	unsigned int tmp = path.find("plugins");
	if (tmp != std::string::npos)
	{
		path = path.substr(0, tmp + 7);
	}
	tmp = path.find(FBSTRING_PluginFileName".");
	if (tmp != std::string::npos)
	{
		path = path.substr(0, tmp - 1);
	}
#endif

	std::string path;

#if defined(XP_WIN)
	std::string appdata = getenv("ALLUSERSPROFILE");
	path = appdata + "\\Mozilla\\Plugins";
#elif defined(XP_UNIX)
	//std::string appdata = getenv("HOME");
	//path = appdata + "/Reitek/Contact/BrowserPlugin";
	path = "/usr/share/sounds/reitek-pluginsip";
#endif

	wav_path_ = path;

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("Audio files path: ") + wav_path_;
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

#if defined(XP_WIN)
	default_ring_file_ = wav_path_ + "\\ringtone.wav";
#elif defined(XP_UNIX)
	default_ring_file_ = wav_path_ + "/ringtone.wav";
#endif

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("Default ring file: ") + default_ring_file_;
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	// Read optional configuration parameters

	boost::optional<std::string> paramStr;

	// !!! TODO: Check what happens with an invalid value (e.g. a string)
	// ringAudioDevice must be >= 0
	if ((paramStr = pluginCore.getParam("ringAudioDevice")) && (std::stoi(*paramStr) >= 0))
	{
		ring_audio_device_ = std::stoi(*paramStr);

		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " INFO:                 " + std::string("Configured custom ring audio device: ") + boost::lexical_cast<std::string>(ring_audio_device_);
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}
	}

	// !!! TODO: Check what happens with an invalid double value
	if ((paramStr = pluginCore.getParam("ringVolume")) && (std::stod(*paramStr) >= 0.0))
	{
		ring_volume_.reset(new double(std::stod(*paramStr)));

		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " INFO:                 " + std::string("Configured custom ring volume: ") + boost::lexical_cast<std::string>(*ring_volume_);
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}
	}

	if (paramStr = pluginCore.getParam("ringSound"))
	{
		ring_file_ = *paramStr;

		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " INFO:                 " + std::string("Configured custom ring file: ") + ring_file_;
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}
	}

	try {
		// Generate tones (they are allocated once and only here)

		// Generate "inring" tone

		pj_str_t name = pj_str(const_cast<char*>("inring"));
		pjmedia_tone_desc tone[3];
		pj_status_t status;

		tone[0].freq1 = 440;
		tone[0].freq2 = 480;
		tone[0].on_msec = 2000;
		tone[0].off_msec = 1000;
		tone[1].freq1 = 440;
		tone[1].freq2 = 480;
		tone[1].on_msec = 2000;
		tone[1].off_msec = 4000;
		tone[2].freq1 = 440;
		tone[2].freq2 = 480;
		tone[2].on_msec = 2000;
		tone[2].off_msec = 3000;

		status = pjmedia_tonegen_create2(pool_, &name, 8000, 1, 160, 16, PJMEDIA_TONEGEN_LOOP, &in_ring_port_);
		if (status != PJ_SUCCESS)
			throw std::runtime_error("Failed inring pjmedia_tonegen_create2");

		status = pjmedia_tonegen_play(in_ring_port_, 1, tone, PJMEDIA_TONEGEN_LOOP);
		if (status != PJ_SUCCESS)
			throw std::runtime_error("Failed inring pjmedia_tonegen_play");

		// Generate "ring" tone

		tone[0].off_msec = 4000;
		name = pj_str(const_cast<char*>("ring"));

		status = pjmedia_tonegen_create2(pool_, &name, 8000, 1, 160, 16, PJMEDIA_TONEGEN_LOOP, &ring_port_);
		if (status != PJ_SUCCESS)
			throw std::runtime_error("Failed ring pjmedia_tonegen_create2");

		status = pjmedia_tonegen_play(ring_port_, 3, tone, PJMEDIA_TONEGEN_LOOP);
		if (status != PJ_SUCCESS)
			throw std::runtime_error("Failed ring pjmedia_tonegen_play");

		status = pjsua_conf_add_port(pool_, ring_port_, &ring_slot_);
		if (status != PJ_SUCCESS)
			throw std::runtime_error("Failed ring pjsua_conf_add_port");

		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " INFO:                 " + std::string("ring tone slot: ") + boost::lexical_cast<std::string>(ring_slot_);
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}

		// Generate "call_wait" tone

		tone[0].freq1 = 440;
		tone[0].freq2 = 0;
		tone[0].on_msec = 500;
		tone[0].off_msec = 2000;
		tone[1].freq1 = 440;
		tone[1].freq2 = 0;
		tone[1].on_msec = 500;
		tone[1].off_msec = 4000;
		name = pj_str(const_cast<char*>("call_wait"));

		status = pjmedia_tonegen_create2(pool_, &name, 8000, 1, 160, 16,
			PJMEDIA_TONEGEN_LOOP, &call_wait_ring_port_);
		if (status != PJ_SUCCESS)
			throw std::runtime_error("Failed call_wait pjmedia_tonegen_create2");

		status = pjmedia_tonegen_play(call_wait_ring_port_, 2, tone, PJMEDIA_TONEGEN_LOOP);
		if (status != PJ_SUCCESS)
			throw std::runtime_error("Failed call_wait pjmedia_tonegen_play");

		status = pjsua_conf_add_port(pool_, call_wait_ring_port_, &call_wait_slot_);
		if (status != PJ_SUCCESS)
			throw std::runtime_error("Failed call_wait pjsua_conf_add_port");

		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " INFO:                 " + std::string("call_wait tone slot: ") + boost::lexical_cast<std::string>(call_wait_slot_);
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}

		// Apply ring configuration
		ApplyRingSound();
	}
	catch (std::runtime_error& e) 
	{
		pj_pool_release(pool_);
		pool_ = NULL;
		throw e;
	}
}

BlabbleAudioManager::~BlabbleAudioManager()
{
}

void BlabbleAudioManager::StopRings()
{
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("StopRings");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	pjsua_conf_disconnect(ring_slot_, 0);
	pjmedia_tonegen_rewind(ring_port_);

	if (in_ring_slot_ > -1)
	{
		pjsua_conf_disconnect(in_ring_slot_, 0);
	}
	if (in_ring_player_ > -1)
	{
		pjsua_player_set_pos(in_ring_player_, 0);
	}

	pjsua_conf_disconnect(call_wait_slot_, 0);
	pjmedia_tonegen_rewind(call_wait_ring_port_);

	if (old_playback_dev_ > -1)
	{
		RestoreAudioDevice();
	}

	if (old_playback_volume_.get())
	{
		RestoreAudioVolume();
	}
}

void BlabbleAudioManager::StartOutRing()
{
	pjsua_conf_connect(ring_slot_, 0);
}

void BlabbleAudioManager::StartInRing()
{
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("StartInRing");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	// Stop playing the wav file not related to a call
	StopWav();

	if (pjsua_call_get_count() > 1)
	{
		pjsua_conf_connect(call_wait_slot_, 0);
	}
	else
	{
		// The ring file could have been changed: apply ring configuration
		ApplyRingSound();

		if (ring_audio_device_ > -1)
		{
			if (SaveAudioDevice())
			{
				// Change the audio devices only if necessary
				if (!CompareCurrentAudioDevices(old_capture_dev_, ring_audio_device_))
				{
					{
						// !!! UGLY (should automatically conform to pjsip formatting)
						const std::string str = "DEBUG:                 " + std::string("pjsua_set_snd_dev");
						BlabbleLogging::blabbleLog(0, str.c_str(), 0);
					}

					// !!! CHECK: Do not change the capture device
					const pj_status_t status = pjsua_set_snd_dev(old_capture_dev_, ring_audio_device_);
					if (status != PJ_SUCCESS)
					{
						// !!! UGLY (should automatically conform to pjsip formatting)
						std::string str = " ERROR:                Could not change audio device before ring playback";
						BlabbleLogging::blabbleLog(0, str.c_str(), 0);
					}
					else
					{
						// !!! UGLY (should automatically conform to pjsip formatting)
						const std::string str = " INFO:                 " +
							std::string("Set audio device to ") +
							boost::lexical_cast<std::string>(ring_audio_device_);
						BlabbleLogging::blabbleLog(0, str.c_str(), 0);
					}
				}
			}
			else
			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				std::string str = " ERROR:                Could not save current audio device information: unable to change the audio device for ring playback";
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}
		}

		if (ring_volume_.get())
		{
			if (SaveAudioVolume())
			{
				const double playbackVolume = *ring_volume_;
				const pj_status_t status = pjsua_conf_adjust_tx_level(0, (float)playbackVolume);
				if (status != PJ_SUCCESS)
				{
					// !!! UGLY (should automatically conform to pjsip formatting)
					std::string str = " ERROR:                Could not change audio volume before ring playback";
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
				}
			}
			else
			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				std::string str = " ERROR:                Could not get current audio volume before ring playback";
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}
		}

		pjsua_conf_connect(in_ring_slot_, 0);
	}
}

/**
	Callback called by pjsip upon end of file played using StartWav

	!!! NOTE: StopWav MUST BE called within another thread else race conditions may occur when switching audio devices
*/
static pj_status_t on_playwav_done(pjmedia_port *port, void *usr_data)
{
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = "DEBUG:                 " + std::string("on_playwav_done callback function");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	if (usr_data != NULL)
	{
		//((BlabbleAudioManager *)usr_data)->StopWav();
		((BlabbleAudioManager *)usr_data)->OnWavStopped();
	}
	else
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " ERROR:                NULL usr_data passed to on_playwav_done callback function";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	/*
		// !!! NOTE: Keep in mind this (from pjsip documentation):

		If the callback returns non-PJ_SUCCESS, the playback will stop. Note that if application destroys the file port in the callback, it must return non-PJ_SUCCESS here.
	*/

	return PJ_EEOF;
}

bool BlabbleAudioManager::PlayWav(FB::VariantMap playWavParams)
{
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("PlayWav");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

#if 0	// !!! NOTE: Allow it also during a call
	if (pjsua_call_get_count() > 0)
	{
		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " WARNING:              " + std::string("At least one call is already active, playWav cannot be started");
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}

		return false;
	}
#endif

	std::string fileName;
	int audioDevice = -1;
	std::auto_ptr<double> volume;

	FB::VariantMap::const_iterator iter = playWavParams.find("fileName");
	if (iter == playWavParams.end() || !iter->second.can_be_type<std::string>())
	{
		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " ERROR:                " +
				std::string("fileName not specified or not a string");

			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}

		return false;
	}
	else
	{
		fileName = iter->second.convert_cast<std::string>();

		if (fileName.empty())
		{
			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				const std::string str = " ERROR:                " +
					std::string("Empty fileName specified");

				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}

			return false;
		}
	}

	iter = playWavParams.find("audioDevice");
	if (iter != playWavParams.end())
	{
		if (iter->second.can_be_type<int>())
		{
			audioDevice = iter->second.convert_cast<int>();
		}
		else
		{
			const std::type_info& type = iter->second.get_type();

			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				const std::string str = " WARNING:              " +
					std::string("Specified audioDevice value cannot be read from a ") +
					std::string(type.name());

				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}
		}
	}

	iter = playWavParams.find("volume");
	if (iter != playWavParams.end())
	{
		if (iter->second.can_be_type<double>())
		{
			volume.reset(new double(iter->second.convert_cast<double>()));
		}
		else
		{
			const std::type_info& type = iter->second.get_type();

			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				const std::string str = " WARNING:              " +
					std::string("Specified volume value cannot be read from a ") +
					std::string(type.name());

				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}
		}
	}

	bool loop = false;

//#if 0	// !!! NOTE: loop is always disabled (it makes possible to restore audio device and audio volume in a predictable way)
	iter = playWavParams.find("loop");
	if (iter != playWavParams.end())
	{
		if (iter->second.can_be_type<bool>())
		{
			loop = iter->second.convert_cast<bool>();
		}
		else
		{
			const std::type_info& type = iter->second.get_type();

			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				const std::string str = " WARNING:              " +
					std::string("Specified loop value cannot be read from a ") +
					std::string(type.name());

				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}

		}
	}
//#endif

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("loop: ") + boost::lexical_cast<std::string>(loop);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

#if 0	// REITEK: Allow relative/absolute paths
	std::string path = 
#if WIN32
		wav_path_ + "\\" + fileName;
#else
		wav_path_ + "/" + fileName;
#endif
#endif

	std::string wav_file_to_use = fileName;

	const boost::filesystem::path wavFilePath(wav_file_to_use);
	if (wavFilePath.is_relative())
	{
		// If the configured ring file does not an absolute path, make it one
		wav_file_to_use = boost::filesystem::absolute(wavFilePath, wav_path_).generic_string();
	}

	bool create_player = true;

	if (wav_player_ > -1)
	{
		// If one of these change, the current wav player must be destroyed
		if ((wav_file_to_use != used_play_file_) || (loop != used_play_loop_))
			StopWav();
		else
		{
			create_player = false;

			if (old_playback_dev_ > -1)
			{
				RestoreAudioDevice();
			}

			if (old_playback_volume_.get())
			{
				RestoreAudioVolume();
			}

			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				const std::string str = "DEBUG:                 " + std::string("pjsua_player_set_pos");
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}

			pjsua_player_set_pos(wav_player_, 0);
		}
	}

	if (create_player)
	{
		if (wav_player_ > -1)
		{
			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				const std::string str = "DEBUG:                 " + std::string("pjsua_player_destroy");
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}

			pjsua_player_destroy(wav_player_);

			wav_player_ = -1;
			wav_slot_ = -1;
		}

		pj_str_t wav_file = pj_str(const_cast<char*>(wav_file_to_use.c_str()));

		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = "DEBUG:                 " + std::string("pjsua_player_create");
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}

		if (pjsua_player_create(&wav_file, loop ? 0 : PJMEDIA_FILE_NO_LOOP, &wav_player_) != PJ_SUCCESS)
		{
			wav_player_ = -1;

			return false;
		}

		// !!! TODO: Error checking !!!
		wav_slot_ = pjsua_player_get_conf_port(wav_player_);

		pjmedia_port *port;

		// !!! TODO: Error checking !!!
		pjsua_player_get_port(wav_player_, &port);

		// Set the EOF callback only if playing with no loop (else it would be automatically stopped)
		if (!loop)
		{
			// !!! TODO: Error checking !!!
			pjmedia_wav_player_set_eof_cb(port, (void *)this, &on_playwav_done);
		}
	}

	if (audioDevice > -1)
	{
		if (SaveAudioDevice())
		{
			// Change the audio devices only if necessary
			if (!CompareCurrentAudioDevices(old_capture_dev_, audioDevice))
			{
				{
					// !!! UGLY (should automatically conform to pjsip formatting)
					const std::string str = "DEBUG:                 " + std::string("pjsua_set_snd_dev");
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
				}

				// !!! CHECK: Do not change the capture device
				const pj_status_t status = pjsua_set_snd_dev(old_capture_dev_, audioDevice);
				if (status != PJ_SUCCESS)
				{
					// !!! UGLY (should automatically conform to pjsip formatting)
					std::string str = " ERROR:                Could not change audio device before wav playback";
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
				}
				else
				{
					// !!! UGLY (should automatically conform to pjsip formatting)
					const std::string str = " INFO:                 " +
						std::string("Set audio device to ") +
						boost::lexical_cast<std::string>(audioDevice);
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
				}
			}
		}
		else
		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			std::string str = " ERROR:                Could not save current audio device information: unable to change the audio device for wav playback";
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}
	}

	if (volume.get())
	{
		if (SaveAudioVolume())
		{
			const double playbackVolume = *volume;

			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				const std::string str = "DEBUG:                 " + std::string("pjsua_conf_adjust_tx_level");
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}

			const pj_status_t status = pjsua_conf_adjust_tx_level(0, (float)playbackVolume);
			if (status != PJ_SUCCESS)
			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				std::string str = " ERROR:                Could not change audio volume before wav playback";
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}
			else
			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				const std::string str = " INFO:                 " +
					std::string("Set audio volume to ") +
					boost::lexical_cast<std::string>(playbackVolume);
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}
		}
		else
		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			std::string str = " ERROR:                Could not get current audio volume before wav playback";
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}
	}

	// Always reconnect the wav player to the conference (it is disconnected when stopped)

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = "DEBUG:                 " + std::string("pjsua_conf_connect");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	// !!! TODO: Error checking !!!
	pjsua_conf_connect(wav_slot_, 0);

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("PlayWav done");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	used_play_file_ = wav_file_to_use;
	used_play_loop_ = loop;

	return true;
}

void BlabbleAudioManager::StopWav()
{
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("StopWav");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	if (wav_player_ > -1) 
	{
		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = "DEBUG:                 " + std::string("pjsua_conf_disconnect");
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}

		pjsua_conf_disconnect(wav_slot_, 0);

		// Don't set the position now, do it only before playing the file

		// Don't destroy the player now!

		if (old_playback_dev_ > -1)
		{
			RestoreAudioDevice();
		}

		if (old_playback_volume_.get())
		{
			RestoreAudioVolume();
		}
	}

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("StopWav done");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}
}

void BlabbleAudioManager::OnWavStopped()
{
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = "DEBUG:                 " + std::string("OnWavStopped");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	std::shared_ptr<BlabbleAudioManager> audiomanagerptr = shared_from_this();
	pluginCore_.getHost()->ScheduleOnMainThread(audiomanagerptr, std::bind(&BlabbleAudioManager::StopWav, audiomanagerptr));
}

void BlabbleAudioManager::ApplyRingSound()
{
	pj_str_t ring_file;
	pjsua_player_id player_id = -1;
	pjsua_conf_port_id slot_id = -1;

	if (!ring_file_.empty())
	{
		std::string ring_file_to_use = ring_file_;

		const boost::filesystem::path ringFilePath(ring_file_);
		if (ringFilePath.is_relative())
		{
			// If the configured ring file does not an absolute path, make it one
			ring_file_to_use = boost::filesystem::absolute(ringFilePath, wav_path_).generic_string();
		}

		// If the custom ring file is the same as the one being used, keep using it
		if (ring_file_to_use == used_ring_file_)
			return;

		// Try to create a player using the configured ring file

		ring_file = pj_str(const_cast<char*>(ring_file_to_use.c_str()));

		if (pjsua_player_create(&ring_file, 0, &player_id) == PJ_SUCCESS)
		{
			slot_id = pjsua_player_get_conf_port(player_id);

			if (slot_id > -1)
			{
				// Destroy the old player
				if (in_ring_player_ > -1)
					pjsua_player_destroy(in_ring_player_);

				in_ring_player_ = player_id;

				if (using_inring_tone_)
				{
					pjsua_conf_remove_port(in_ring_slot_);
					using_inring_tone_ = false;
				}

				in_ring_slot_ = slot_id;
				used_ring_file_ = ring_file_to_use;

				{
					// !!! UGLY (should automatically conform to pjsip formatting)
					const std::string str = " INFO:                 " + std::string("Using custom ring file ") + used_ring_file_ + std::string(" on slot ") + boost::lexical_cast<std::string>(in_ring_slot_);
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
				}

				return;
			}
		}

		// Creation of the player using the configured ring file failed

		if (player_id > -1)
			pjsua_player_destroy(player_id);

		player_id = -1;
		slot_id = -1;
	}

	// If the default ring file is already being used, keep using it
	if (used_ring_file_ == default_ring_file_)
		return;

	// Try to create player using the default ring file

	ring_file = pj_str(const_cast<char*>(default_ring_file_.c_str()));

	if (pjsua_player_create(&ring_file, 0, &player_id) == PJ_SUCCESS)
	{
		slot_id = pjsua_player_get_conf_port(player_id);

		if (slot_id > -1)
		{
			// Destroy the old player
			if (in_ring_player_ > -1)
				pjsua_player_destroy(in_ring_player_);

			in_ring_player_ = player_id;

			if (using_inring_tone_)
			{
				pjsua_conf_remove_port(in_ring_slot_);
				using_inring_tone_ = false;
			}

			in_ring_slot_ = slot_id;
			used_ring_file_ = default_ring_file_;

			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				const std::string str = " INFO:                 " + std::string("Using default ring file ") + used_ring_file_ + std::string(" on slot ") + boost::lexical_cast<std::string>(in_ring_slot_);
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}

			return;
		}
	}

	// Creation of the player using the default ring file failed

	if (player_id > -1)
		pjsua_player_destroy(player_id);

	// Destroy the old player
	if (in_ring_player_ > -1)
		pjsua_player_destroy(in_ring_player_);

	in_ring_player_ = -1;
	// Don't reset in_ring_slot because it could be used by the "inring" tone
	used_ring_file_.clear();

	// No ring file could be opened, use the "inring" tone

	// Already using the "inring" tone
	if (using_inring_tone_)
		return;

	pj_status_t status = pjsua_conf_add_port(pool_, in_ring_port_, &in_ring_slot_);
	if (status != PJ_SUCCESS)
	{
		using_inring_tone_ = false;
		in_ring_slot_ = -1;

		throw std::runtime_error("Failed inring pjsua_conf_add_port");
	}

	using_inring_tone_ = true;

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " WARNING:          " + std::string("Using internal inring tone on slot ") + boost::lexical_cast<std::string>(in_ring_slot_);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}
}

/*! @Brief Save the current audio device in order to be able restore it later
*/
bool BlabbleAudioManager::SaveAudioDevice()
{
	if (old_playback_dev_ > -1)
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " ERROR:            Current audio device already saved";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		return false;
	}

	int captureId, playbackId;

	const pj_status_t status = pjsua_get_snd_dev(&captureId, &playbackId);
	if (status != PJ_SUCCESS)
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " ERROR:            Could not get current audio device";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		return false;
	}

	old_capture_dev_ = captureId;
	old_playback_dev_ = playbackId;

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " INFO:                 Saved current audio device (" + boost::lexical_cast<std::string>(old_playback_dev_) + ")";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	return true;
}

/*! @Brief Save the current audio volume in order to be able restore it later
*/
bool BlabbleAudioManager::SaveAudioVolume()
{
	if (old_playback_volume_.get())
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " ERROR:            Current audio volume already saved";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		return false;
	}

	pjsua_conf_port_info info;
	const pj_status_t status = pjsua_conf_get_port_info(0, &info);
	if (status != PJ_SUCCESS)
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " ERROR:            Could not get current audio volume";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		return false;
	}

	old_playback_volume_.reset(new double(info.tx_level_adj));

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " INFO:                 Saved current audio volume (" + boost::lexical_cast<std::string>(info.tx_level_adj) + ")";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	return true;
}

/*! @Brief Restore the saved audio device
*/
bool BlabbleAudioManager::RestoreAudioDevice()
{
	if (old_playback_dev_ == -1)
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " ERROR:            Current audio device not saved";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		return false;
	}

	// Change the audio devices only if necessary
	if (!CompareCurrentAudioDevices(old_capture_dev_, old_playback_dev_))
	{
		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = "DEBUG:                 " + std::string("pjsua_set_snd_dev");
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}

		// !!! CHECK: Do not change the capture device
		const pj_status_t status = pjsua_set_snd_dev(old_capture_dev_, old_playback_dev_);
		if (status != PJ_SUCCESS)
		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			std::string str = " ERROR:            Could not restore the saved audio device";
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);

			return false;
		}
	}

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " INFO:                 Restored the saved audio device (" + boost::lexical_cast<std::string>(old_playback_dev_)+")";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	old_playback_dev_ = -1;

	return true;
}

/*! @Brief Restore the saved audio volume
*/
bool BlabbleAudioManager::RestoreAudioVolume()
{
	if (!old_playback_volume_.get())
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " ERROR:            Current audio volume not saved";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		return false;
	}

	const double oldPlaybackVolume = *old_playback_volume_;

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = "DEBUG:                 " + std::string("pjsua_conf_adjust_tx_level");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	const pj_status_t status = pjsua_conf_adjust_tx_level(0, (float)oldPlaybackVolume);
	if (status != PJ_SUCCESS)
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " ERROR:            Could not restore the saved audio volume";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		return false;
	}

	old_playback_volume_.reset(NULL);

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		std::string str = " INFO:                 Restored the saved audio volume (" + boost::lexical_cast<std::string>(oldPlaybackVolume)+")";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	return true;
}

bool BlabbleAudioManager::SetRingAudioDevice(FB::variant deviceId)
{
	int deviceIdToSet = -1;

	if (!deviceId.is_null())
	{
		if (!deviceId.can_be_type<int>())
		{
			const std::type_info& type = deviceId.get_type();

			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				const std::string str = " WARNING:              " +
					std::string("Specified deviceId value cannot be read from a ") +
					std::string(type.name());

				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}
		}
		else
		{
			deviceIdToSet = deviceId.convert_cast<int>();
		}
	}

	if (deviceIdToSet > -1)
	{
		ring_audio_device_ = deviceIdToSet;

		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " INFO:                 " + std::string("Set custom ring audio device: ") + boost::lexical_cast<std::string>(ring_audio_device_);
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}

		return true;
	}

	// Invalid type or value passed: restore the default ring audio device

	ring_audio_device_ = -1;

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("Set default ring audio device");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	return true;
}

int BlabbleAudioManager::GetRingAudioDevice()
{
	return ring_audio_device_;
}

bool BlabbleAudioManager::SetRingVolume(FB::variant volume)
{
	if (volume.is_of_type<double>())
	{
		const double volumeDouble = volume.cast<double>();

		ring_volume_.reset(new double(volumeDouble));

		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " INFO:                 " + std::string("Set custom ring volume: ") + boost::lexical_cast<std::string>(*ring_volume_);
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}

		return true;
	}
	else if (!volume.is_null())
	{
		const std::type_info& type = volume.get_type();

		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " WARNING:              " +
				std::string("Unhandled type ") +
				std::string(type.name()) +
				std::string(" for Set custom ring volume");

			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}
	}

	// Invalid type passed: restore the default ring volume

	ring_volume_.reset();

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("Set default ring volume");
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	return true;
}

FB::variant BlabbleAudioManager::GetRingVolume()
{
	const double * ringVolume = ring_volume_.get();

	if (ringVolume != NULL)
	{
		return *ringVolume;
	}
	else
	{
		return FB::FBNull();
	}
}

bool BlabbleAudioManager::SetRingSound(FB::variant filePath)
{
	if (filePath.is_of_type<std::string>())
	{
		const std::string& filePathStr = filePath.cast<std::string>();

		if (!filePathStr.empty())
		{
			ring_file_ = filePathStr;

			{
				// !!! UGLY (should automatically conform to pjsip formatting)
				const std::string str = " INFO:                 " + std::string("Set custom ring sound: ") + ring_file_;
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			}

			return true;
		}
	}
	else if (!filePath.is_null())
	{
		const std::type_info& type = filePath.get_type();

		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " WARNING:              " +
				std::string("Unhandled type ") +
				std::string(type.name()) +
				std::string(" for Set custom ring sound");

			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}
	}

	// Invalid type or empty value passed: restore the default ring file

	ring_file_.clear();

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string("Set default ring sound: ") + default_ring_file_;
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	return true;
}

const std::string BlabbleAudioManager::GetRingSound()
{
	return ring_file_;
}
