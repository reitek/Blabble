/**********************************************************\
Original Author: Andrew Ofisher (zaltar)

License:    GNU General Public License, version 3.0
            http://www.gnu.org/licenses/gpl-3.0.txt

Copyright 2012 Andrew Ofisher
\**********************************************************/

#ifndef H_BlabbleAudioManagerPLUGIN
#define H_BlabbleAudioManagerPLUGIN

#include "variant.h"

#include <string>
#include <map>
//#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/enable_shared_from_this.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <pjlib.h>
#include <pjlib-util.h>
#include <pjnath.h>
#include <pjsip.h>
#include <pjsip_ua.h>
#include <pjsip_simple.h>
#include <pjsua-lib/pjsua.h>
#include <pjmedia.h>
#include <pjmedia-codec.h> 

class Blabble;

/*! @class A simple class to manage ringing.
 *  This class controls ringing (either via generated tone or
 *  a wave file used as a ringtone. It also handles playing the
 *  call waiting beep when another call comes in while on a call.
 */
class BlabbleAudioManager : public boost::enable_shared_from_this<BlabbleAudioManager>
{
public:
	// REITEK: Get/parse parameters passed to the plugin upon manager creation

	BlabbleAudioManager(Blabble& pluginCore);
	virtual ~BlabbleAudioManager();
	
	/*! @Brief Stop all media playing that was started by us.
	 */
	void StopRings();
	
	/*! @Brief Start playing the tone for an outgoing call.
	 */
	void StartOutRing();
	
	/*! @Brief Start playing the tone or ringtone for an outgoing call.
	 */
	void StartInRing();
	
	/*! @Brief Start playing the wave file fileName located in wavPath that was passed to the constructor.
	 */
	bool PlayWav(FB::VariantMap playWavParams);

	/*! @Brief Stop playing a wave played with StartWav.
	 *  @sa StartWav
	 */
	void StopWav();

	/*! @Brief Handle stop event of a wave played with StartWav.
	*  @sa StartWav
	*/
	void OnWavStopped();

	bool SetRingAudioDevice(FB::variant deviceId);
	int GetRingAudioDevice();

	bool SetRingVolume(FB::variant volume);
	FB::variant GetRingVolume();

	bool SetRingSound(FB::variant filePath);
	const std::string GetRingSound();

private:
	/*! @Brief Create a player for the custom ring file, with fallback on the default ring file
	*/
	void ApplyRingSound();

	/*! @Brief Save the current audio device in order to be able restore it later
	*/
	bool SaveAudioDevice();

	/*! @Brief Save the current audio volume in order to be able restore it later
	*/
	bool SaveAudioVolume();

	/*! @Brief Restore the saved audio device
	*/
	bool RestoreAudioDevice();

	/*! @Brief Restore the saved audio volume
	*/
	bool RestoreAudioVolume();


	Blabble& pluginCore_;

	// Defaults

	std::string wav_path_;							// Base path for audio files 
	std::string default_ring_file_;					// Default ring file

	// Configurable parameters

	int ring_audio_device_;							// Audio device to use for ring (-1 if the same as the default one)
	std::auto_ptr<double> ring_volume_;				// Audio volume to use for ring (NULL if the same as the default one)
	std::string ring_file_;							// Full path/filename only for custom ring file (empty if the same as the default one)

	// Modified at runtime

	std::string used_ring_file_;					// Ring file currently being used
	bool using_inring_tone_;						// Flag for removing, when appropriate, in_ring_port from the conference 

	std::string used_play_file_;					// Play file currently being used
	bool used_play_loop_;							// Play loop currently being used

	int old_capture_dev_;							// ID of the capture device used before changing it to the one to use separately for ring
	int old_playback_dev_;							// ID of the playback device used before changing it to the one to use separately for ring
	std::auto_ptr<double> old_playback_volume_;		// The  playback volume used before changing it to the one to use separately for ring (or NULL if the playback volume was not changed)

	pj_pool_t* pool_;
	pjmedia_port *ring_port_, *in_ring_port_, *call_wait_ring_port_;
	pjsua_player_id in_ring_player_, wav_player_;
	pjsua_conf_port_id ring_slot_, in_ring_slot_, call_wait_slot_, wav_slot_;
};

#endif
