/**********************************************************\
Original Author: Andrew Ofisher (zaltar)

License:    GNU General Public License, version 3.0
            http://www.gnu.org/licenses/gpl-3.0.txt

Copyright 2012 Andrew Ofisher
\**********************************************************/

#include "PjsuaManager.h"
#include "Blabble.h"
#include "BlabbleAccount.h"
#include "BlabbleCall.h"
#include "BlabbleAudioManager.h"
#include "BlabbleLogging.h"

#include "global/config.h"

#include <pjsua-lib/pjsua_internal.h>
#include <string>
#ifdef WIN32
#include <io.h>
#endif
#include <stdio.h>
#include <boost/optional.hpp>

// REITEK: Internal log function for inhibiting built in pjsip logging
static void log_func(int level, const char *data, int len) {}


PjsuaManagerWeakPtr PjsuaManager::instance_;

PjsuaManagerPtr PjsuaManager::GetManager(Blabble& pluginCore)
{
	PjsuaManagerPtr tmp = instance_.lock();
	if(!tmp) 
	{
		tmp = PjsuaManagerPtr(new PjsuaManager(pluginCore));
		instance_ = std::weak_ptr<PjsuaManager>(tmp);
	}
	
	return tmp;
}

PjsuaManager::PjsuaManager(Blabble& pluginCore)
{
	// REITEK: Get/parse parameters passed to the plugin upon manager creation

	boost::optional<std::string> logging, ice, ecalgo, loglevelparam;
	bool enableIce = false;

	if ((logging = pluginCore.getParam("logging")) && *logging == "true")
	{
		BlabbleLogging::initLogging();
	}

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 " + std::string(FBSTRING_PluginName) + std::string(" version ") + std::string(FBSTRING_PLUGIN_VERSION);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	// REITEK: Output the parameters passed to the plugin

	const FB::VariantMap& params = pluginCore.getParams();
	if (!params.empty())
	{
		for (FB::VariantMap::const_iterator it = params.begin(); it != params.end(); ++it)
		{
			const FB::variant& val = it->second;

			// Don't attempt to convert this kind of variant to a string, it will trow an exception
			if (!val.is_of_type<FB::JSObjectPtr>())
			{
				const std::string& strval = val.convert_cast<std::string>();

				{
					// !!! UGLY (should automatically conform to pjsip formatting)
					const std::string str = " INFO:                 " + it->first + std::string(": ") + strval;
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
				}
			}
		}
	}
	else
	{
		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " INFO:                 " + std::string("No parameters passed to the plugin");
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		}
	}

	if ((ice = pluginCore.getParam("enableice")) && *ice == "true")
	{
		enableIce = true;
	}

	const std::string& stunServer = pluginCore.getParam("stunserver").get_value_or("");

	// REITEK: read insecure and secure SIP ports from configuration and construct the manager using each one of them (or the default value if not configured)
	const int sipPort = std::stoi(pluginCore.getParam("sipport").get_value_or("5060"));
	const int sipTlsPort = std::stoi(pluginCore.getParam("siptlsport").get_value_or("5061"));

	// REITEK: read EC tail len and EC algo (0 = default, 1=speex, 2=suppressor) from configuration

	// !!! NOTE: Default value is 0, 64 should be used instead for connectivity over Internet
	const int ecTailLen = std::stoi(pluginCore.getParam("ectaillen").get_value_or("0"));

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 ectaillen set to " + boost::lexical_cast<std::string>(ecTailLen);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	// !!! NOTE: Default value is 0, 2 should be used instead for connectivity over Internet
	int ecAlgo = 0;

	if (ecalgo = pluginCore.getParam("ecalgo"))
	{
		if ((*ecalgo == "0") || (*ecalgo == "default")) { ecAlgo = 0; }
		else if ((*ecalgo == "1") || (*ecalgo == "speex")) { ecAlgo = 1; }
		else if ((*ecalgo == "2") || (*ecalgo == "suppressor")) { ecAlgo = 2; }
	}

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 ecalgo set to " + boost::lexical_cast<std::string>(ecAlgo);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	pj_status_t status;
	pjsua_config cfg;
	pjsua_logging_config log_cfg;
	pjsua_media_config media_cfg;
	pjsua_transport_config tran_cfg, tls_tran_cfg, tran6_cfg, tls_tran6_cfg;

	pjsua_transport_config_default(&tran_cfg);
	pjsua_transport_config_default(&tls_tran_cfg);
	pjsua_transport_config_default(&tran6_cfg);
	pjsua_transport_config_default(&tls_tran6_cfg);

	pjsua_media_config_default(&media_cfg);
	pjsua_config_default(&cfg);
	pjsua_logging_config_default(&log_cfg);

	// REITEK: Tweak maximum number of calls in order to also reduce memory usage (at most 1 active plus 1 for consultation are needed)
	//cfg.max_calls = 511;
	cfg.max_calls = 2;

	cfg.cb.on_incoming_call = &PjsuaManager::OnIncomingCall;
	cfg.cb.on_call_media_state = &PjsuaManager::OnCallMediaState;
	cfg.cb.on_call_state = &PjsuaManager::OnCallState;
	cfg.cb.on_reg_state = &PjsuaManager::OnRegState;
	cfg.cb.on_transport_state = &PjsuaManager::OnTransportState;
#if 0	// REITEK: Disabled
	cfg.cb.on_call_transfer_status = &PjsuaManager::OnCallTransferStatus;
#endif
	cfg.cb.on_call_tsx_state = &PjsuaManager::OnCallTsxState;

	// REITEK: Default log level is 4

	//int loglevel = 4;
	int loglevel = 5;	// FOR DEBUGGING REASONS; !!! NOTE: log level 6 is UNUSABLE !!!

	// REITEK: Read desired log level
	if (loglevelparam = pluginCore.getParam("loglevel"))
	{
		// Simple "trick" to raise the log level to higher values by not allowing the direct int value

		if (*loglevelparam == "debug")
			loglevel = 5;
		else
		{
			const int loglevelparamint = std::stoi(*loglevelparam);

			if (loglevelparamint < 5)
				loglevel = loglevelparamint;
		}
	}

	log_cfg.level = loglevel;
	log_cfg.console_level = loglevel;

	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		const std::string str = " INFO:                 log level set to " + boost::lexical_cast<std::string>(loglevel);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	// REITEK: Log messages!
	log_cfg.msg_logging = PJ_TRUE;
	log_cfg.decor = PJ_LOG_HAS_SENDER | PJ_LOG_HAS_SPACE | PJ_LOG_HAS_LEVEL_TEXT | PJ_LOG_HAS_THREAD_ID | PJ_LOG_HAS_THREAD_SWC;
	log_cfg.cb = BlabbleLogging::blabbleLog;

	// UDP transport settings

	tran_cfg.port = sipPort;

	// REITEK: !!! CHECK: Make port range configurable ?
	tran_cfg.port_range = 200;

	// TLS transport settings

	tls_tran_cfg.port = sipTlsPort;

	// REITEK: !!! CHECK: Make port range configurable ?
	tls_tran_cfg.port_range = 200;

	// REITEK: Do not attempt to verify the server certificate until we understand how to do it
	//tls_tran_cfg.tls_setting.verify_server = PJ_TRUE;
	tls_tran_cfg.tls_setting.timeout.sec = 5;
	tls_tran_cfg.tls_setting.method = PJSIP_TLSV1_METHOD;

	// UDP (IPv6) transport settings

	tran6_cfg.port = sipPort;

	// REITEK: !!! CHECK: Make port range configurable ?
	tran6_cfg.port_range = 200;

	// TLS (IPv6) transport settings

	tls_tran6_cfg.port = sipTlsPort;

	// REITEK: !!! CHECK: Make port range configurable ?
	tls_tran6_cfg.port_range = 200;

	// REITEK: Do not attempt to verify the server certificate until we understand how to do it
	//tls_tran6_cfg.tls_setting.verify_server = PJ_TRUE;
	tls_tran6_cfg.tls_setting.timeout.sec = 5;
	tls_tran6_cfg.tls_setting.method = PJSIP_TLSV1_METHOD;

	media_cfg.no_vad = 1;
	media_cfg.enable_ice = enableIce ? PJ_TRUE : PJ_FALSE;

	// REITEK: Set EC tail len and EC algo
	media_cfg.ec_tail_len = ecTailLen;
	media_cfg.ec_options = ecAlgo;	// !!! NOTE: Additional options are not known (yet)
	//media_cfg.snd_auto_close_time = -1;

	if (!stunServer.empty()) 
	{
		cfg.stun_srv_cnt = 1;
		cfg.stun_srv[0] = pj_str(const_cast<char*>(stunServer.c_str()));
	}

	// REITEK: User-Agent header handling
	//cfg.user_agent = pj_str("Reitek PluginSIP");

	const std::string& userAgent = std::string("Reitek ") + FBSTRING_ProductName + "/" + FBSTRING_PLUGIN_VERSION;
	cfg.user_agent = pj_str(const_cast<char*>(userAgent.c_str()));

	pj_log_set_log_func(&log_func);

	//pjsip_cfg()->endpt.resolve_hostname_to_get_interface = PJ_TRUE;

	status = pjsua_create();
	if (status != PJ_SUCCESS)
		throw std::runtime_error("pjsua_create failed");

	status = pjsua_init(&cfg, &log_cfg, &media_cfg);
	if (status != PJ_SUCCESS) 
		throw std::runtime_error("pjsua_init failed");

	try
	{
		status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tran_cfg, &this->udp_transport);
		if (status != PJ_SUCCESS)
			throw std::runtime_error("pjsua_transport_create failed for UDP transport");

		// !!! CHECK: What about TCP without TLS?

		status = pjsua_transport_create(PJSIP_TRANSPORT_TLS, &tls_tran_cfg, &this->tls_transport);
		// REITEK: Disable TLS flag (TLS is handled differently)
#if 0
		has_tls_ = status == PJ_SUCCESS;
		if (!has_tls_) {
			// !!! UGLY (should automatically conform to pjsip formatting)
			BLABBLE_LOG_DEBUG(" WARN:                 pjsua_transport_create failed for TLS transport: TLS not enabled");
			this->tls_transport = -1;
		}
#endif

		// REITEK: TLS transport opening is now mandatory
		if (status != PJ_SUCCESS)
			throw std::runtime_error("pjsua_transport_create failed for TLS transport");

		// REITEK: Attempt to open IPv6 transports (but do not fail if they cannot be opened)

		status = pjsua_transport_create(PJSIP_TRANSPORT_UDP6, &tran6_cfg, &this->udp6_transport);
		if (status != PJ_SUCCESS)
		{
			BLABBLE_LOG_DEBUG(" WARN:                 pjsua_transport_create failed for UDP IPv6 transport: UDP IPv6 not enabled");
			this->udp6_transport = -1;
		}

		status = pjsua_transport_create(PJSIP_TRANSPORT_TLS6, &tls_tran6_cfg, &this->tls6_transport);
		if (status != PJ_SUCCESS)
		{
			BLABBLE_LOG_DEBUG(" WARN:                 pjsua_transport_create failed for TLS IPv6 transport: TLS IPv6 not enabled");
			this->tls6_transport = -1;
		}

		status = pjsua_start();
		if (status != PJ_SUCCESS)
			throw std::runtime_error("pjsua_start failed");

		// REITEK: Codecs priority handling
		pj_str_t tmpstr;

		// !!! FIXME: Hardwired now in order to test G. 729
		pjsua_codec_set_priority(pj_cstr(&tmpstr, "*"), 0);
		pjsua_codec_set_priority(pj_cstr(&tmpstr, "g729"), 255);
		pjsua_codec_set_priority(pj_cstr(&tmpstr, "pcmu"), 240);
		pjsua_codec_set_priority(pj_cstr(&tmpstr, "pcma"), 230);
		pjsua_codec_set_priority(pj_cstr(&tmpstr, "speex/8000"), 190);
		pjsua_codec_set_priority(pj_cstr(&tmpstr, "ilbc"), 189);
		pjsua_codec_set_priority(pj_cstr(&tmpstr, "speex/16000"), 180);
		pjsua_codec_set_priority(pj_cstr(&tmpstr, "speex/32000"), 0);
		pjsua_codec_set_priority(pj_cstr(&tmpstr, "gsm"), 100);

#if 0
		// REITEK: G.729 codec handling
		pjmedia_codec_param g729_param;

		status = pjsua_codec_get_param(pj_cstr(&tmpstr, "g729"), &g729_param);
		if (status != PJ_SUCCESS)
			throw std::runtime_error("Cannot get G.729 default parameters");

		g729_param.setting.frm_per_pkt = m_config.getG729FramesPerPacket();
		pjsua_codec_set_param(pj_cstr(&tmp, "g729"), &g729_param);
#endif

		audio_manager_ = std::make_shared<BlabbleAudioManager>(pluginCore);

		// !!! UGLY (should automatically conform to pjsip formatting)
		BLABBLE_LOG_DEBUG(" INFO:                 PjsuaManager startup complete");
	}
	catch (std::runtime_error& e)
	{
		std::string str = "Error during PjsuaManager startup: " + boost::lexical_cast<std::string>(e.what());
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_ERROR("Error in PjsuaManager. " << e.what());
		pjsua_destroy();
		throw e;
	}
}

PjsuaManager::~PjsuaManager()
{
	pjsua_call_hangup_all();

	accounts_.clear();

	if (audio_manager_)
		audio_manager_.reset();

	pjsua_destroy();
}

void PjsuaManager::AddAccount(const BlabbleAccountPtr &account)
{
	if (account->id() == INVALID_ACCOUNT)
		throw std::runtime_error("Attempt to add uninitialized account.");

	accounts_[account->id()] = account;
}

void PjsuaManager::RemoveAccount(pjsua_acc_id acc_id)
{
	accounts_.erase(acc_id);
}

BlabbleAccountPtr PjsuaManager::FindAcc(int acc_id)
{
	if (pjsua_acc_is_valid(acc_id) == PJ_TRUE) 
	{
		BlabbleAccountMap::iterator it = accounts_.find(acc_id);
		if (it != accounts_.end()) 
		{
			return it->second;
		}
	}

	return BlabbleAccountPtr();
}

void PjsuaManager::SetCodecPriority(const char* codec, int value)
{
	pj_str_t tmpstr;
	pjsua_codec_set_priority(pj_cstr(&tmpstr, codec), value);
}

//Event handlers

//Static
void PjsuaManager::OnTransportState(pjsip_transport *tp, pjsip_transport_state state, 
	const pjsip_transport_state_info *info)
{
	if (state == PJSIP_TP_STATE_DISCONNECTED) 
	{
		pjsip_tls_state_info *tmp = ((pjsip_tls_state_info*)info->ext_info);
		if (tmp->ssl_sock_info->verify_status != PJ_SSL_CERT_ESUCCESS) 
		{
			//bad cert
		}
	}
}

//Static
void PjsuaManager::OnIncomingCall(pjsua_acc_id acc_id, pjsua_call_id call_id, pjsip_rx_data *rdata)
{
	std::string str = "OnIncomingCall called for PJSIP account id: " + boost::lexical_cast<std::string>(acc_id)+", PJSIP call id: " + boost::lexical_cast<std::string>(call_id);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	//BLABBLE_LOG_TRACE("OnIncomingCall called for PJSIP account id: " << acc_id << ", PJSIP call id: " << call_id);
	PjsuaManagerPtr manager = PjsuaManager::instance_.lock();

	if (!manager)
	{
		//How is this even possible?
		pjsua_call_hangup(call_id, 0, NULL, NULL);
		return;
	}

	// REITEK: Prefer local codec ordering (!!! CHECK: Make it configurable ?)

	pjsua_call *call;
	pjsip_dialog *dlg;
	pj_status_t status;

	status = acquire_call("PjsuaManager::OnIncomingCall", call_id, &call, &dlg);
	if (status == PJ_SUCCESS)
	{
		if (call->inv->neg != NULL)
		{
			status = pjmedia_sdp_neg_set_prefer_remote_codec_order(call->inv->neg, PJ_FALSE);
			if (status != PJ_SUCCESS)
			{
				std::string str = "Could not set codec negotiation preference on local side for PJSIP account id: " + boost::lexical_cast<std::string>(acc_id)+", PJSIP call id: " + boost::lexical_cast<std::string>(call_id);
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);
				//BLABBLE_LOG_ERROR("Could not set codec negotiation preference on local side for PJSIP account id: " << acc_id << ", PJSIP call id: " << call_id);
			}
		}
		else
		{
			std::string str = "WARNING: NULL SDP negotiator: cannot set codec negotiation preference on local side for PJSIP account id: " + boost::lexical_cast<std::string>(acc_id)+", PJSIP call id: " + boost::lexical_cast<std::string>(call_id);
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);
			//BLABBLE_LOG_DEBUG("WARNING: NULL SDP negotiator: cannot set codec negotiation preference on local side for PJSIP account id: " << acc_id << ", PJSIP call id: " << call_id);
		}

		pjsip_dlg_dec_lock(dlg);
	}
	else {
		std::string str = "Could not acquire lock to set codec negotiation preference on local side for PJSIP account id: " + boost::lexical_cast<std::string>(acc_id)+", PJSIP call id: " + boost::lexical_cast<std::string>(call_id);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_ERROR("Could not acquire lock to set codec negotiation preference on local side for PJSIP account id: " << acc_id << ", PJSIP call id: " << call_id);
	}

	BlabbleAccountPtr acc = manager->FindAcc(acc_id);
	if (acc && acc->OnIncomingCall(call_id, rdata))
	{
		return;
	}
	
	//Otherwise we respond busy if no one wants the call
	pjsua_call_hangup(call_id, 486, NULL, NULL);
}

//Static
void PjsuaManager::OnCallMediaState(pjsua_call_id call_id)
{
	PjsuaManagerPtr manager = PjsuaManager::instance_.lock();

	if (!manager)
		return;
	
	pjsua_call_info info;
	pj_status_t status;
	if ((status = pjsua_call_get_info(call_id, &info)) == PJ_SUCCESS) 
	{
		std::string str = "PjsuaManager::OnCallMediaState called with PJSIP call id: " + boost::lexical_cast<std::string>(call_id)+", state: " + boost::lexical_cast<std::string>(info.state);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_TRACE("PjsuaManager::OnCallMediaState called with PJSIP call id: " << call_id << ", state: " << info.state);
		BlabbleAccountPtr acc = manager->FindAcc(info.acc_id);
		if (acc)
		{
			acc->OnCallMediaState(call_id);
		}
	}
	else
	{
		std::string str = "PjsuaManager::OnCallMediaState failed to call pjsua_call_get_info for PJSIP call id: " + boost::lexical_cast<std::string>(call_id)+", got status: " + boost::lexical_cast<std::string>(status);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_ERROR("PjsuaManager::OnCallMediaState failed to call pjsua_call_get_info for PJSIP call id: " << call_id << ", got status: " << status);
	}

}

//Static
void PjsuaManager::OnCallState(pjsua_call_id call_id, pjsip_event *e)
{
	PjsuaManagerPtr manager = PjsuaManager::instance_.lock();

	if (!manager)
		return;

	pjsua_call_info info;
	pj_status_t status;
	if ((status = pjsua_call_get_info(call_id, &info)) == PJ_SUCCESS) 
	{
		std::string str = "PjsuaManager::OnCallState called with PJSIP call id: " + boost::lexical_cast<std::string>(call_id)+", state: " + boost::lexical_cast<std::string>(info.state);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_TRACE("PjsuaManager::OnCallState called with PJSIP call id: " << call_id << ", state: " << info.state);
		BlabbleAccountPtr acc = manager->FindAcc(info.acc_id);
		if (acc)
		{
			acc->OnCallState(call_id, e);
		}

		if (info.state == PJSIP_INV_STATE_DISCONNECTED)
		{
			//Just make sure we get rid of the call
			pjsua_call_hangup(call_id, 0, NULL, NULL);
		}
	}
	else
	{
		std::string str = "PjsuaManager::OnCallState failed to call pjsua_call_get_info for PJSIP call id: " + boost::lexical_cast<std::string>(call_id)+", got status: " + boost::lexical_cast<std::string>(status);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_ERROR("PjsuaManager::OnCallState failed to call pjsua_call_get_info for PJSIP call id: " << call_id << ", got status: " << status);
	}
}

// REITEK: Callback to handle transaction state changes
//Static
void PjsuaManager::OnCallTsxState(pjsua_call_id call_id, pjsip_transaction *tsx, pjsip_event *e)
{
	PjsuaManagerPtr manager = PjsuaManager::instance_.lock();

	if (!manager)
		return;

	pjsua_call_info info;
	pj_status_t status;
	if ((status = pjsua_call_get_info(call_id, &info)) == PJ_SUCCESS)
	{
		std::string str = "PjsuaManager::OnCallTsxState called with PJSIP call id: " + boost::lexical_cast<std::string>(call_id)+", state: " + boost::lexical_cast<std::string>(info.state);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_TRACE("PjsuaManager::OnCallTsxState called with PJSIP call id: "<< call_id << ", state: " << info.state);
		BlabbleAccountPtr acc = manager->FindAcc(info.acc_id);
		if (acc)
		{
			acc->OnCallTsxState(call_id, tsx, e);
		}

		// REITEK: !!! CHECK: If this necessary/wanted?
		if (info.state == PJSIP_INV_STATE_DISCONNECTED)
		{
			//Just make sure we get rid of the call
			pjsua_call_hangup(call_id, 0, NULL, NULL);
		}
	}
	else
	{
		std::string str = "PjsuaManager::OnCallTsxState failed to call pjsua_call_get_info for PJSIP call id: " + boost::lexical_cast<std::string>(call_id)+", got status: " + boost::lexical_cast<std::string>(status);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_ERROR("PjsuaManager::OnCallTsxState failed to call pjsua_call_get_info for PJSIP call id: "<< call_id << ", got status: " << status);
	}
}

//Static
void PjsuaManager::OnRegState(pjsua_acc_id acc_id)
{
	PjsuaManagerPtr manager = PjsuaManager::instance_.lock();

	if (!manager)
		return;

	BlabbleAccountPtr acc = manager->FindAcc(acc_id);
	if (acc)
	{
		acc->OnRegState();
	}
	else
	{
		std::string str = "PjsuaManager::OnRegState failed to find account PJSIP account id: " + boost::lexical_cast<std::string>(acc_id);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_ERROR("PjsuaManager::OnRegState failed to find account PJSIP account id: " << acc_id);
	}
}

#if 0	// REITEK: Disabled
//Static
void PjsuaManager::OnCallTransferStatus(pjsua_call_id call_id, int st_code, const pj_str_t *st_text, pj_bool_t final, pj_bool_t *p_cont)
{
	std::string str = "PjsuaManager::OnCallTransferState called with PJSIP call id: " + boost::lexical_cast<std::string>(call_id) + ", state: " + boost::lexical_cast<std::string>(st_code);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	//BLABBLE_LOG_TRACE("PjsuaManager::OnCallTransferState called with PJSIP call id: " << call_id << ", state: " << st_code);
	PjsuaManagerPtr manager = PjsuaManager::instance_.lock();

	if (!manager)
		return;

	pjsua_call_info info;
	pj_status_t status;
	if ((status = pjsua_call_get_info(call_id, &info)) == PJ_SUCCESS) 
	{
		BlabbleAccountPtr acc = manager->FindAcc(info.acc_id);
		if (acc)
		{
			if (acc->OnCallTransferStatus(call_id, st_code))
				(*p_cont) = PJ_TRUE;
		}
	}
	else
	{
		std::string str = "PjsuaManager::OnCallTransferStatus failed to call pjsua_call_get_info for PJSIP call id: " + boost::lexical_cast<std::string>(call_id)+", got status: " + boost::lexical_cast<std::string>(status);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_ERROR("PjsuaManager::OnCallTransferStatus failed to call pjsua_call_get_info for PJSIP call id: " << call_id << ", got status: " << status);
	}
}
#endif

/* non esposto, utile per sviluppi futuri */
/*
void PjsuaManager::setCodecPriorityAll(std::vector<std::pair<std::string, int>> codecMap)
{
	pj_str_t tmpstr;
	pjsua_codec_set_priority(pj_cstr(&tmpstr, "*"), 0);

	for (int i = 0; i < codecMap.size(); ++i)
	{
		PjsuaManager::setCodecPriority(codecMap[i].first, codecMap[i].second);
	}
}
*/
