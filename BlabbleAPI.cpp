/**********************************************************\
Original Author: Andrew Ofisher (zaltar)

License:    GNU General Public License, version 3.0
            http://www.gnu.org/licenses/gpl-3.0.txt

Copyright 2012 Andrew Ofisher
\**********************************************************/

#include "PjsuaManager.h"
#include "variant_list.h"
#include "DOM/Document.h"

#include "BlabbleAPI.h"
#include "BlabbleAccount.h"
#include "PjsuaManager.h"
#include "BlabbleAudioManager.h"
#include "BlabbleLogging.h"
#include <string>


BlabbleAPIInvalid::BlabbleAPIInvalid(const char* err)
{ 
	error_ = std::string(err);
	registerProperty("error", make_property(this, &BlabbleAPIInvalid::error)); 
}

BlabbleAPI::BlabbleAPI(const FB::BrowserHostPtr& host, const PjsuaManagerPtr& manager) :
	browser_host_(host), manager_(manager)
{
	registerMethod("createAccount", make_method(this, &BlabbleAPI::CreateAccount));
	registerMethod("playWav", make_method(this, &BlabbleAPI::PlayWav));
	registerMethod("stopWav", make_method(this, &BlabbleAPI::StopWav));
	registerMethod("log", make_method(this, &BlabbleAPI::Log));
	// REITEK: Disable TLS flag (TLS is handled differently)
#if 0
	registerProperty("tlsEnabled", make_property(this, &BlabbleAPI::has_tls));
#endif	

	registerMethod("getAudioDevices", make_method(this, &BlabbleAPI::GetAudioDevices));
	registerMethod("setAudioDevice", make_method(this, &BlabbleAPI::SetAudioDevice));
	registerMethod("getCurrentAudioDevice", make_method(this, &BlabbleAPI::GetCurrentAudioDevice));
	registerMethod("getVolume", make_method(this, &BlabbleAPI::GetVolume));
	registerMethod("setVolume", make_method(this, &BlabbleAPI::SetVolume));
	registerMethod("getSignalLevel", make_method(this, &BlabbleAPI::GetSignalLevel));
	
	//Nuove Features
	registerProperty("version", make_property(this, &BlabbleAPI::getVersion));
	registerMethod("getLogAD", make_method(this, &BlabbleAPI::getLogAD));
	registerMethod("writeLogAD", make_method(this, &BlabbleAPI::writeLogAD));
	registerMethod("setLogDimension", make_method(this, &BlabbleAPI::setLogDimension));
	registerMethod("setLogNumber", make_method(this, &BlabbleAPI::setLogNumber));
	registerMethod("getLogDimension", make_method(this, &BlabbleAPI::getLogDimension));
	registerMethod("getLogNumber", make_method(this, &BlabbleAPI::getLogNumber));
	registerMethod("logSender", make_method(this, &BlabbleAPI::ZipSender));
	registerMethod("setCodecPriority", make_method(this, &BlabbleAPI::SetCodecPriority));
	registerMethod("setLogPath", make_method(this, &BlabbleAPI::setLogPath));
	registerMethod("setRingAudioDevice", make_method(this, &BlabbleAPI::setRingAudioDevice));
	registerMethod("getRingAudioDevice", make_method(this, &BlabbleAPI::getRingAudioDevice));
	registerMethod("setRingVolume", make_method(this, &BlabbleAPI::setRingVolume));
	registerMethod("getRingVolume", make_method(this, &BlabbleAPI::getRingVolume));
	registerMethod("setRingSound", make_method(this, &BlabbleAPI::setRingSound));
	registerMethod("getRingSound", make_method(this, &BlabbleAPI::getRingSound));

	registerProperty("accounts", make_property(this, &BlabbleAPI::accounts));
}

BlabbleAPI::~BlabbleAPI()
{
	std::vector<BlabbleAccountWeakPtr>::iterator it;
	for (it = accounts_.begin(); it < accounts_.end(); it++) {
		BlabbleAccountPtr acct = it->lock();
		if (acct) {
			acct->Destroy();
		}
	}
	accounts_.clear();
}

void BlabbleAPI::Log(int level, const std::string msg)
{
	BlabbleLogging::blabbleLog(0, msg.c_str(), 0);
	//BLABBLE_JS_LOG(level, msg.c_str());
}

BlabbleAccountWeakPtr BlabbleAPI::CreateAccount(const FB::VariantMap &params)
{
	BlabbleAccountPtr account = std::make_shared<BlabbleAccount>(manager_);
	try 
	{
		FB::VariantMap::const_iterator iter = params.find("host");
		if (iter != params.end())
			account->set_server(iter->second.cast<std::string>());

		if ((iter = params.find("username")) != params.end())
			account->set_username(iter->second.cast<std::string>());

		if ((iter = params.find("password")) != params.end())
			account->set_password(iter->second.cast<std::string>());

		// REITEK: Disable TLS flag (TLS is handled differently)
#if 0
		if ((iter = params.find("useTls")) != params.end() &&
			iter->second.is_of_type<bool>())
		{
			account->set_use_tls(iter->second.cast<bool>());

			const std::string str = "DEBUG:                 " + std::string("useTls: ") + boost::lexical_cast<std::string>(account->use_tls());
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}

		if ((iter = params.find("identity")) != params.end() &&
			iter->second.is_of_type<std::string>())
		{
			account->set_default_identity(iter->second.cast<std::string>());
		}
#endif

		if ((iter = params.find("onIncomingCall")) != params.end() &&
			iter->second.is_of_type<FB::JSObjectPtr>())
		{
			account->set_on_incoming_call(iter->second.cast<FB::JSObjectPtr>());
		}

		if ((iter = params.find("onRegState")) != params.end() &&
			iter->second.is_of_type<FB::JSObjectPtr>())
		{
			account->set_on_reg_state(iter->second.cast<FB::JSObjectPtr>());
		}

		// REITEK: If specified, set proxy URL for the new account (its validity is checked within the Register() method)
		if ((iter = params.find("proxyURL")) != params.end() &&
			iter->second.is_of_type<std::string>())
		{
			account->set_proxyURL(iter->second.cast<std::string>());
		}

		account->Register();
	}
	catch (const std::exception &e)
	{
		throw FB::script_error(std::string("Unable to create account: ") + e.what());
	}

	accounts_.push_back(account);
	return BlabbleAccountWeakPtr(account);
}

BlabbleAccountPtr BlabbleAPI::FindAcc(int acc_id)
{
	return manager_->FindAcc(acc_id);
}

bool BlabbleAPI::PlayWav(FB::VariantMap playWavParams)
{
	return manager_->audio_manager()->PlayWav(playWavParams);
}

void BlabbleAPI::StopWav()
{
	manager_->audio_manager()->StopWav();
}

#ifdef WIN32
std::string ANSI_to_UTF8(char * ansi)
{
	const int inlen = ::MultiByteToWideChar(CP_ACP, NULL, ansi, strlen(ansi), NULL, 0);
	wchar_t* wszString = new wchar_t[inlen + 1];
	::MultiByteToWideChar(CP_ACP, NULL, ansi, strlen(ansi), wszString, inlen);
	wszString[inlen] = '\0';

	const int outlen = ::WideCharToMultiByte(CP_UTF8, NULL, wszString, wcslen(wszString), NULL, 0, NULL, NULL);
	char* utf8 = new char[outlen + 1];
	::WideCharToMultiByte(CP_UTF8, NULL, wszString, wcslen(wszString), utf8, outlen, NULL, NULL);
	utf8[outlen] = '\0';

	std::string ret(utf8);

	delete wszString;
	delete utf8;

	return ret;
}
#endif

FB::VariantList BlabbleAPI::GetAudioDevices()
{
	unsigned int count = pjmedia_aud_dev_count();
	pjmedia_aud_dev_info* audio_info = new pjmedia_aud_dev_info[count];
	pj_status_t status = pjsua_enum_aud_devs(audio_info, &count);
	if (status == PJ_SUCCESS) {
		FB::VariantList devices;
		for (unsigned int i = 0; i < count; i++)
		{
			FB::VariantMap map;
#ifdef WIN32
			map["name"] = ANSI_to_UTF8(audio_info[i].name);
#else
			map["name"] = std::string(audio_info[i].name);
#endif
			map["driver"] = std::string(audio_info[i].driver);
			map["inputs"] = audio_info[i].input_count;
			map["outputs"] = audio_info[i].output_count;
			map["id"] = i;
			devices.push_back(map);
		}
		delete[] audio_info;
		return devices;
	}
	delete[] audio_info;
	return FB::VariantList();
}

FB::VariantMap BlabbleAPI::GetCurrentAudioDevice()
{
	int captureId, playbackId;
	FB::VariantMap map, capInfo, playInfo;
	pjmedia_aud_dev_info audio_info;

	pj_status_t status = pjsua_get_snd_dev(&captureId, &playbackId);
	if (status == PJ_SUCCESS)
	{
		status = pjmedia_aud_dev_get_info(captureId, &audio_info);
		if (status == PJ_SUCCESS) {
#ifdef WIN32
			capInfo["name"] = ANSI_to_UTF8(audio_info.name);
#else
			capInfo["name"] = std::string(audio_info.name);
#endif
			capInfo["driver"] = std::string(audio_info.driver);
			capInfo["inputs"] = audio_info.input_count;
			capInfo["outputs"] = audio_info.output_count;
			capInfo["id"] = captureId;
			map["capture"] = capInfo;
		}
		status = pjmedia_aud_dev_get_info(playbackId, &audio_info);
		if (status == PJ_SUCCESS) {
#ifdef WIN32
			playInfo["name"] = ANSI_to_UTF8(audio_info.name);
#else
			playInfo["name"] = std::string(audio_info.name);
#endif
			playInfo["driver"] = std::string(audio_info.driver);
			playInfo["inputs"] = audio_info.input_count;
			playInfo["outputs"] = audio_info.output_count;
			playInfo["id"] = playbackId;
			map["playback"] = playInfo;
		}
	} else {
		map["error"] = status;
	}
	return map;
}

bool BlabbleAPI::SetAudioDevice(int capture, int playback)
{
	// REITEK: Check the currently set capture and playback device: if they are the same, there is no need to set them
	int captureId, playbackId;

	pj_status_t status = pjsua_get_snd_dev(&captureId, &playbackId);
	if (status == PJ_SUCCESS)
	{
		if ((captureId == capture) && (playbackId == playback))
			return true;
	}

	return PJ_SUCCESS == pjsua_set_snd_dev(capture, playback);
}

FB::VariantMap BlabbleAPI::GetVolume()
{
	FB::VariantMap map;

	// REITEK: Use PJSUA API to get the current volume
	pjsua_conf_port_info info;
	pj_status_t status = pjsua_conf_get_port_info(0, &info);
	if (status == PJ_SUCCESS)
	{
		map["outgoingVolume"] = info.rx_level_adj;
		map["incomingVolume"] = info.tx_level_adj;
	}
	else
	{
		map["error"] = status;
	}

	return map;
}

void BlabbleAPI::SetVolume(FB::variant outgoingVolume, FB::variant incomingVolume)
{
	if (outgoingVolume.is_of_type<double>())
	{
		pjsua_conf_adjust_rx_level(0, (float)outgoingVolume.cast<double>());
	}

	if (incomingVolume.is_of_type<double>())
	{
		pjsua_conf_adjust_tx_level(0, (float)incomingVolume.cast<double>());
	}
}

FB::VariantMap BlabbleAPI::GetSignalLevel()
{
	unsigned int txLevel, rxLevel;
	FB::VariantMap map;
	pj_status_t status = pjsua_conf_get_signal_level(0, &txLevel, &rxLevel);
	if (status == PJ_SUCCESS)
	{
		map["outgoingLevel"] = rxLevel;
		map["incomingLevel"] = txLevel;
	} 
	else 
	{
		map["error"] = status;
	}

	return map;
}

void BlabbleAPI::getLogAD()
{
	BlabbleLogging::getLogAD();
}

void BlabbleAPI::writeLogAD(std::string data) 
{
	BlabbleLogging::writeLogAD(data);
}

void BlabbleAPI::setLogDimension(int dimension)
{
	BlabbleLogging::setLogDimension(dimension);
}

void BlabbleAPI::setLogNumber(int number)
{
	BlabbleLogging::setLogNumber(number);
}

int BlabbleAPI::getLogDimension()
{
	return BlabbleLogging::getLogDimension();
}

int BlabbleAPI::getLogNumber()
{
	return BlabbleLogging::getLogNumber();
}

void BlabbleAPI::setLogPath(std::string logpath)
{
	BlabbleLogging::setLogPath(logpath);
}

void BlabbleAPI::ZipSender(std::string host) 
{
	BlabbleLogging::ZipSender(host);
}

void BlabbleAPI::SetCodecPriority(std::string codec, int value) 
{
	manager_->SetCodecPriority(codec.c_str(), value);
}
/*
void BlabbleAPI::SetCodecPriorityAll(std::map<std::string,int> codecMap)
{
	PjsuaManager::SetCodecPriorityAll(codecMap);
}
*/

bool BlabbleAPI::setRingAudioDevice(FB::variant deviceId)
{
	return manager_->audio_manager()->SetRingAudioDevice(deviceId);
}

int BlabbleAPI::getRingAudioDevice()
{
	return manager_->audio_manager()->GetRingAudioDevice();
}

bool BlabbleAPI::setRingVolume(FB::variant volume)
{
	return manager_->audio_manager()->SetRingVolume(volume);
}

FB::variant BlabbleAPI::getRingVolume()
{
	return manager_->audio_manager()->GetRingVolume();
}

bool BlabbleAPI::setRingSound(FB::variant filePath)
{
	return manager_->audio_manager()->SetRingSound(filePath);
}

const std::string BlabbleAPI::getRingSound()
{
	return manager_->audio_manager()->GetRingSound();
}

FB::VariantList BlabbleAPI::accounts()
{
	FB::VariantList accounts = FB::make_variant_list(accounts_);
	return accounts;
}
