/**********************************************************\
Original Author: Andrew Ofisher (zaltar)

License:    GNU General Public License, version 3.0
            http://www.gnu.org/licenses/gpl-3.0.txt

Copyright 2012 Andrew Ofisher
\**********************************************************/

#include "JSObject.h"
#include "variant_list.h"
#include "Blabble.h"
#include "BlabbleAccount.h"
#include "BlabbleCall.h"
#include "PjsuaManager.h"
#include "BlabbleAudioManager.h"
#include "FBWriteOnlyProperty.h"
#include "BlabbleLogging.h"
#include "boost/lexical_cast.hpp"
#include <string>

BlabbleAccount::BlabbleAccount(PjsuaManagerPtr manager) :  
	ringing_call_(0), pjsua_manager_(manager), id_(-1), timeout_(60), retry_(15)
	// REITEK: Disable TLS flag (TLS is handled differently)
#if 0	
	, use_tls_(false)
#endif
{
	// REITEK: Don't allow making a call on its own
	//registerMethod("makeCall", make_method(this, &BlabbleAccount::MakeCall));
	registerMethod("unregister", make_method(this, &BlabbleAccount::Unregister));
	registerMethod("register", make_method(this, &BlabbleAccount::Register));
	registerMethod("destroy", make_method(this, &BlabbleAccount::Destroy));

	registerProperty("activeCall", make_property(this, &BlabbleAccount::active_call));
	registerProperty("calls", make_property(this, &BlabbleAccount::calls));
	registerProperty("isRegistered", make_property(this, &BlabbleAccount::registered));
    registerProperty("host", make_property(this, &BlabbleAccount::server));
	registerProperty("username", make_property(this, &BlabbleAccount::username));

	registerProperty("onIncomingCall", make_write_only_property(this, &BlabbleAccount::set_on_incoming_call));
	registerProperty("onRegState", make_write_only_property(this, &BlabbleAccount::set_on_reg_state));

	//new features
	registerMethod("setProxy", make_method(this, &BlabbleAccount::set_proxyURL));
}

void BlabbleAccount::Register()
{
	if (id_ != INVALID_ACCOUNT)
	{
		pjsua_acc_set_registration(id_, PJ_TRUE);
	} 
	else 
	{
		if (server_.empty())
			throw std::runtime_error("Attempt to register account with no server host set.");

		bool useTlsForRegistrar = false;

		// REITEK: Handle pseudo-URI scheme for using TLS on the connection to the registrar
		std::string server = server_;
		if (!server.empty() && (server.find("tls:") == 0)) {
			server = server.substr(4);
			useTlsForRegistrar = true;
		}

		if (server.empty())
			throw std::runtime_error("Attempt to register account with no server host set.");

		std::string accId = "sip:" + username_ + "@" + server;

		// REITEK: Check validity of id URL
		if (pjsua_verify_sip_url(accId.c_str()) != 0) {
			throw std::runtime_error(std::string("Attempt to register account using invalid username ") + username_ + " or server " + server);
		}

		std::string regUri = "sip:" + server;

		// REITEK: Disable TLS flag (TLS is handled differently)
#if 0
		if (use_tls_) {
			regUri += ";transport=TLS";
		}
#endif

		if (useTlsForRegistrar) {
			regUri += ";transport=TLS";
		}

		// REITEK: Check validity of reg URL
		if (pjsua_verify_sip_url(regUri.c_str()) != 0) {
			throw std::runtime_error(std::string("Attempt to register account using invalid server ") + server);
		}

		// REITEK: Handle pseudo-URI scheme for using TLS on the connection to the outbound proxy

		std::string proxyURL = proxyURL_;
		if (!proxyURL.empty() && (proxyURL.find("tls:") == 0)) {
			proxyURL = "sip:" + proxyURL.substr(4) + ";transport=TLS";
		}

		// REITEK: Check validity of proxy URL
		if (!proxyURL.empty() && (pjsua_verify_sip_url(proxyURL.c_str()) != 0)) {
			throw std::runtime_error(std::string("Attempt to register account using invalid proxyURL") + proxyURL);
		}

		PjsuaManagerPtr manager = GetManager();
		pj_status_t status;
		pjsua_acc_config acc_cfg;

		pjsua_acc_config_default(&acc_cfg);

		acc_cfg.id = pj_str(const_cast<char*>(accId.c_str()));
		acc_cfg.reg_uri = pj_str(const_cast<char*>(regUri.c_str()));
		acc_cfg.reg_retry_interval = retry_;
		acc_cfg.reg_timeout = timeout_;

		if (!username_.empty()) {
			acc_cfg.cred_count = 1;
			acc_cfg.cred_info[0].realm = pj_str(const_cast<char*>("*"));
			acc_cfg.cred_info[0].scheme = pj_str(const_cast<char*>("digest"));
			acc_cfg.cred_info[0].username = pj_str(const_cast<char*>(username_.c_str()));
			acc_cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
			acc_cfg.cred_info[0].data = pj_str(const_cast<char*>(password_.c_str()));
		}

		// REITEK: Disable re-INVITE/UPDATE when more than one codec is received
		acc_cfg.lock_codec = PJ_FALSE;

		// REITEK: Set proxy URL
		if (!proxyURL.empty()) {
			acc_cfg.proxy[acc_cfg.proxy_cnt++] = pj_str(const_cast<char *>(proxyURL.c_str()));
		}

		// REITEK: Use signalling interface for media
		acc_cfg.rtp_cfg.media_from_signalling_interface = PJ_TRUE;

#if 0	// REITEK: Attempt to bind the account to a specific IPv6 transport (only for testing purposes)
		if (!useTlsForRegistrar)
		{
			/* Bind the account to IPv6 transport */
			//acc_cfg.transport_id = manager->GetUDP6TransportID();
		}
		else
		{
			/* Bind the account to IPv6 transport */
			//acc_cfg.transport_id = manager->GetTLS6TransportID();
		}
#endif

		status = pjsua_acc_add(&acc_cfg, PJ_TRUE, &id_);

		if (status != PJ_SUCCESS)
			throw std::runtime_error("pjsua_acc_add returned " + status);

		manager->AddAccount(this->get_shared());
	}
}

BlabbleAccount::~BlabbleAccount()
{
	Destroy();
}

void BlabbleAccount::Destroy()
{
	std::string str = "DEBUG:                 +++BlabbleAccount::Destroy()+++";
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

	//Verify the manager is still around by grabbing it
	PjsuaManagerPtr manager = pjsua_manager_.lock();
	if (manager)
	{
		{
			boost::recursive_mutex::scoped_lock lock(this->calls_mutex_);
			BlabbleCallList::iterator it;

			const size_t numCalls = calls_.size();

			str = "DEBUG:                 Iterating at most on " + boost::lexical_cast<std::string>(numCalls) + " calls";
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);

			size_t iterNum = 1;

			while ((it = calls_.begin()) != calls_.end())
			{
				(*it)->LocalEnd();

				iterNum++;
				if (iterNum > numCalls) {
					break;
				}
			}
			calls_.clear();
		}
	
		if (pjsua_acc_is_valid(id_) == PJ_TRUE)
		{
			pjsua_acc_del(id_);
		}

		manager->RemoveAccount(id_);
	}

	str = "DEBUG:                 ---BlabbleAccount::Destroy()---";
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);
}

void BlabbleAccount::Unregister()
{
	pjsua_acc_set_registration(id_, PJ_FALSE);
}

bool BlabbleAccount::OnIncomingCall(pjsua_call_id call_id, pjsip_rx_data *rdata)
{
	if (ringing_call_ > 0) 
	{
		//We are busy ringing. Sorry.
		pjsua_call_hangup(call_id, 486, NULL, NULL);
		return false;
	}

	pjsua_call_info info;
	pjsua_call_get_info(call_id, &info);
	std::string cid = std::string(info.remote_contact.ptr, info.remote_contact.slen);
	unsigned int s, e;
	for (s = 0; s < cid.length() && cid[s] != ':'; s++);
	s++;
	for (e = s; e < cid.length() && cid[e] != '@'; e++);
	if (s < cid.length() && e < cid.length())
		cid = cid.substr(s, e-s);

	BlabbleCallPtr call = boost::make_shared<BlabbleCall>(get_shared());

	if (!call->RegisterIncomingCall(call_id))
		return false;

	{
		boost::recursive_mutex::scoped_lock lock(this->calls_mutex_);
		calls_.push_back(call);
	}

	// REITEK: !!! The call id is saved even though the call is immediately answered
	ringing_call_ = call->id();

	if (on_incoming_call_)
		on_incoming_call_->InvokeAsync("", FB::variant_list_of(BlabbleCallWeakPtr(call))(BlabbleAccountWeakPtr(get_shared())));

	if (!call->HandleIncomingCall(rdata))
	{
		//Error occurred
		{
			boost::recursive_mutex::scoped_lock lock(this->calls_mutex_);
			calls_.remove(call);
		}

		return false;
	}

	return true;
}

void BlabbleAccount::OnCallState(pjsua_call_id call_id, pjsip_event *e)
{
	BlabbleCallPtr call = FindCall(call_id);
	if (call)
	{
		call->OnCallState(call_id, e);
	} 
	else
	{
		const std::string str = "Received call state change event for unknown PJSIP call id " +
								boost::lexical_cast<std::string>(call_id) +
								", on PJSIP account id " + boost::lexical_cast<std::string>(id_);
		BlabbleLogging::blabbleLog(0,str.c_str(),0);
	}
}

#if 0	// REITEK: Disabled
bool BlabbleAccount::OnCallTransferStatus(pjsua_call_id call_id, int status)
{
	BlabbleCallPtr call = FindCall(call_id);
	if (call)
	{
		return call->OnCallTransferStatus(status);
	}
	const std::string str = "Received call state change event for unknown PJSIP call id " + boost::lexical_cast<std::string>(call_id)+", on PJSIP account id " + boost::lexical_cast<std::string>(id_);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

	//Stop getting notifications since we don't even have this call
	return true;
}
#endif

void BlabbleAccount::OnCallMediaState(pjsua_call_id call_id)
{
	BlabbleCallPtr call = FindCall(call_id);
	if (call)
	{
		call->OnCallMediaState();
	}
	else
	{
		const std::string str = "Received call state change event for unknown PJSIP call id " +
								boost::lexical_cast<std::string>(call_id) +
								", on PJSIP account id " + boost::lexical_cast<std::string>(id_);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}
}

// REITEK: Method to handle transaction state changes
void BlabbleAccount::OnCallTsxState(pjsua_call_id call_id, pjsip_transaction *tsx, pjsip_event *e)
{
	BlabbleCallPtr call = FindCall(call_id);
	if (call)
	{
		call->OnCallTsxState(call_id, tsx, e);
	}
	else
	{
		const std::string str = "Received call state change event for unknown PJSIP call id " +
								boost::lexical_cast<std::string>(call_id) +
								", on PJSIP account id " + boost::lexical_cast<std::string>(id_);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}
}

BlabbleCallPtr BlabbleAccount::FindCall(pjsua_call_id call_id)
{
	unsigned int *internalId = (unsigned int*)pjsua_call_get_user_data(call_id);
	if (internalId) {
		boost::recursive_mutex::scoped_lock lock(calls_mutex_);
		for (BlabbleCallList::iterator it = calls_.begin(); it != calls_.end(); it++) 
		{
			if ((*it)->id() == *internalId)
				return *it;
		}
	}

	return BlabbleCallPtr();
}

void BlabbleAccount::OnCallEnd(pjsua_call_id call_id, const BlabbleCallPtr& call)
{
	const std::string str = "OnCallEnd for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + ", global id " + boost::lexical_cast<std::string>(call->id());
	BlabbleLogging::blabbleLog(0,str.c_str(),0);

	if (call->id() == ringing_call_)
	{
		ringing_call_ = 0;
	}

	boost::recursive_mutex::scoped_lock lock(calls_mutex_);
	calls_.remove(call);
}

void BlabbleAccount::OnCallRingChange(const BlabbleCallPtr& call, const pjsua_call_info& info)
{
	if (info.state == PJSIP_INV_STATE_CALLING)
	{
		ringing_call_ = call->id();
	} else if (call->id() == ringing_call_)
	{
		ringing_call_ = 0;
	}
}

void BlabbleAccount::OnRegState()
{
	pjsua_acc_info info;
	pjsua_acc_get_info(id_, &info);

	if (on_reg_state_)
		on_reg_state_->InvokeAsync("", FB::variant_list_of(BlabbleAccountWeakPtr(get_shared()))((long)info.status));
}

// REITEK: Don't allow making a call on its own
#if 0
FB::variant BlabbleAccount::MakeCall(const FB::VariantMap &params)
{
	std::string destination, identity, displayName;
	FB::VariantMap::const_iterator iter = params.find("destination");
	if (iter == params.end())
	{
		throw FB::script_error("No destination given!");
	}
	else
	{
		destination = iter->second.cast<std::string>();
		if (destination.size() <= 4 || destination.substr(0, 4) != "sip:")
		{
			//Assume this is an extension on the current server and make this a valid SIP URI
			destination = "sip:" + destination + "@" + server_;
		}

		if (use_tls_ && destination.find("transport=TLS") == std::string::npos)
			destination += ";transport=TLS";
	}

	if ((iter = params.find("identity")) != params.end())
		identity = iter->second.convert_cast<std::string>();

	if ((iter = params.find("displayName")) != params.end())
		displayName = iter->second.convert_cast<std::string>();

	if (!identity.empty()) 
	{
		if (identity.size() <= 4 || identity.substr(0, 4) != "sip:") 
		{
			//Assume this is an extension on the current server and make this a valid SIP URI
			identity = "<sip:" + identity + "@" + server_ + ">";
		}

		if (!displayName.empty())
		{
			identity = "\"" + displayName + "\" " + identity;
		}
	} 
	else 
	{
		identity = default_identity;
	}

	BlabbleCallPtr call = boost::make_shared<BlabbleCall>(get_shared());

	if ((iter = params.find("onCallConnected")) != params.end() &&
		iter->second.is_of_type<FB::JSObjectPtr>())
	{
		call->set_on_call_connected(iter->second.cast<FB::JSObjectPtr>());
	}

	if ((iter = params.find("onCallRinging")) != params.end() &&
		iter->second.is_of_type<FB::JSObjectPtr>())
	{
		call->set_on_call_ringing(iter->second.cast<FB::JSObjectPtr>());
	}

	if ((iter = params.find("onCallEnd")) != params.end() &&
		iter->second.is_of_type<FB::JSObjectPtr>())
	{
		call->set_on_call_end(iter->second.cast<FB::JSObjectPtr>());
	}

	
	{
		boost::recursive_mutex::scoped_lock lock(this->calls_mutex_);
		calls_.push_back(call);
	}

	pj_status_t status = call->MakeCall(destination, identity);

	if (status == PJ_SUCCESS)
	{
		ringing_call_ = call->id();
		return BlabbleCallWeakPtr(call);
	}

	//Error occurred
	{
		boost::recursive_mutex::scoped_lock lock(this->calls_mutex_);
		calls_.remove(call);
	}

	char error[256];
	pj_str_t errorText = pj_strerror(status, error, 256);

	std::map<std::string, FB::variant> errorCall;
	errorCall["error"] = std::string(errorText.ptr, errorText.slen);
	errorCall["valid"] = false;

	return errorCall;
}
#endif

//JS Properties
BlabbleCallWeakPtr BlabbleAccount::active_call()
{
	pjsua_call_id ids[32];
	pjsua_call_info info;
	unsigned int count = 32;
	if (pjsua_enum_calls(ids, &count) == PJ_SUCCESS)
	{
		for (unsigned int i = 0; i < count && 
			pjsua_call_get_info(ids[i], &info) == PJ_SUCCESS; i++) 
		{
			if (info.acc_id == id_ && info.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
				BlabbleCallPtr call = FindCall(ids[i]);
				if (call)
					return call;
			}
		}
	}

	return BlabbleCallWeakPtr();
}

FB::VariantList BlabbleAccount::calls()
{
	boost::recursive_mutex::scoped_lock lock(calls_mutex_);
	FB::VariantList calls = FB::make_variant_list(calls_);
	return calls;
}

bool BlabbleAccount::registered()
{
	pjsua_acc_info info;
	if (pjsua_acc_get_info(id_, &info) == PJ_SUCCESS)
	{
		return info.status == 200;
	}

	return false;
}

PjsuaManagerPtr BlabbleAccount::GetManager()
{
	PjsuaManagerPtr ret = pjsua_manager_.lock();

	//Shouldn't ever happen
	if (!ret)
		throw std::runtime_error("BlabbleAPI Lost Manager!");

	return ret;
}
