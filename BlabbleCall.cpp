/**********************************************************\
Original Author: Andrew Ofisher (zaltar)

License:    GNU General Public License, version 3.0
            http://www.gnu.org/licenses/gpl-3.0.txt

Copyright 2012 Andrew Ofisher
\**********************************************************/

#include "BlabbleAudioManager.h"
#include "BlabbleCall.h"
#include "BlabbleAccount.h"
#include "Blabble.h"
#include "JSObject.h"
#include "variant_list.h"
#include "BlabbleLogging.h"
#include "FBWriteOnlyProperty.h"


#if defined(PJMEDIA_HAS_RTCP_XR) && (PJMEDIA_HAS_RTCP_XR != 0)
#   define STATS_BUF_SIZE	(1024 * 10)
#else
#   define STATS_BUF_SIZE	(1024 * 3)
#endif


/*! @Brief Static call counter to keep track of calls.
 */
unsigned int BlabbleCall::id_counter_ = 0;
unsigned int BlabbleCall::GetNextId()
{
	return ATOMIC_INCREMENT(&BlabbleCall::id_counter_);
}

BlabbleCall::BlabbleCall(const BlabbleAccountPtr& parent_account)
	: call_id_(INVALID_CALL), ringing_(false)
{
	if (parent_account) 
	{
		acct_id_ = parent_account->id();
		audio_manager_ = parent_account->GetManager()->audio_manager();
		parent_ = BlabbleAccountWeakPtr(parent_account);
	}
	else 
	{
		acct_id_ = -1;
	}
	
	id_ = BlabbleCall::GetNextId();
	std::string str = "New call created. Global id: " + boost::lexical_cast<std::string>(id_);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	//BLABBLE_LOG_DEBUG("New call created. Global id: " << id_);

	registerMethod("answer", make_method(this, &BlabbleCall::Answer));
	registerMethod("hangup", make_method(this, &BlabbleCall::LocalEnd));
#if 0	// REITEK: Disabled
	registerMethod("hold", make_method(this, &BlabbleCall::Hold));
	registerMethod("unhold", make_method(this, &BlabbleCall::Unhold));
#endif
	registerMethod("sendDTMF", make_method(this, &BlabbleCall::SendDTMF));
#if 0	// REITEK: Disabled
	registerMethod("transferReplace", make_method(this, &BlabbleCall::TransferReplace));
	registerMethod("transfer", make_method(this, &BlabbleCall::Transfer));
#endif
	
	registerProperty("callerId", make_property(this, &BlabbleCall::caller_id));
	registerProperty("isActive", make_property(this, &BlabbleCall::is_active));
	registerProperty("status", make_property(this, &BlabbleCall::status));

	registerProperty("onCallConnected", make_write_only_property(this, &BlabbleCall::set_on_call_connected));
	registerProperty("onCallEnd", make_write_only_property(this, &BlabbleCall::set_on_call_end));
	registerProperty("onCallEndStatistics", make_write_only_property(this, &BlabbleCall::set_on_call_end_statistics));
}

void BlabbleCall::StopRinging()
{
	if (ringing_)
	{
		ringing_ = false;
		audio_manager_->StopRings();
	}
}

void BlabbleCall::StartInRinging()
{
	if (!ringing_)
	{
		ringing_ = true;
		audio_manager_->StartInRing();
	}
}

void BlabbleCall::StartOutRinging()
{
	if (!ringing_)
	{
		ringing_ = true;
		audio_manager_->StartOutRing();
	}
}

//Ended by us
void BlabbleCall::LocalEnd()
{
	pjsua_call_id old_id = INTERLOCKED_EXCHANGE((volatile long *)&call_id_, (long)INVALID_CALL);
	if (old_id == INVALID_CALL || 
		old_id < 0 || old_id >= (long)pjsua_call_get_max_count())
	{
		return;
	}

	StopRinging();

	pjsua_call_info info;
	if (pjsua_call_get_info(old_id, &info) == PJ_SUCCESS &&
		info.conf_slot > 0) 
	{
		//Kill the audio
		pjsua_conf_disconnect(info.conf_slot, 0);
		pjsua_conf_disconnect(0, info.conf_slot);
	}

	pjsua_call_hangup(old_id, 0, NULL, NULL);
	
	if (on_call_end_)
	{
		BlabbleCallPtr call = get_shared();
		on_call_end_->getHost()->ScheduleOnMainThread(call, boost::bind(&BlabbleCall::CallOnCallEnd, call));
	}

	BlabbleAccountPtr p = parent_.lock();
	if (p)
		p->OnCallEnd(get_shared());
}

void BlabbleCall::CallOnCallEnd()
{
	on_call_end_->Invoke("", FB::variant_list_of(BlabbleCallWeakPtr(get_shared())));
}

void BlabbleCall::CallOnCallEndStatus(pjsip_status_code status)
{
	on_call_end_->Invoke("", FB::variant_list_of(BlabbleCallWeakPtr(get_shared()))(status));
}

void BlabbleCall::CallOnCallEndStatistics(std::string statistics)
{
	on_call_end_statistics_->Invoke("", FB::variant_list_of(BlabbleCallWeakPtr(get_shared()))(statistics));
}

#if 0	// REITEK: Disabled
void BlabbleCall::CallOnTransferStatus(int status)
{
	on_transfer_status_->Invoke("", FB::variant_list_of(BlabbleCallWeakPtr(get_shared()))(status));
}
#endif

//Ended by remote, could be becuase of an error
void BlabbleCall::RemoteEnd(const pjsua_call_info &info)
{
	pjsua_call_id old_id = INTERLOCKED_EXCHANGE((volatile long *)&call_id_, (long)INVALID_CALL);
	if (old_id == INVALID_CALL || 
		old_id < 0 || old_id >= (long)pjsua_call_get_max_count())
	{
		return;
	}

	StopRinging();

	//Kill the audio
	if (info.conf_slot > 0) 
	{
		pjsua_conf_disconnect(info.conf_slot, 0);
		pjsua_conf_disconnect(0, info.conf_slot);
	}

	pjsua_call_hangup(old_id, 0, NULL, NULL);

	if (info.last_status > 400)
	{
		std::string callerId;
		if (info.remote_contact.ptr == NULL)
		{
			callerId =std::string(info.remote_info.ptr, info.remote_info.slen);
		} 
		else
		{
			callerId = std::string(info.remote_contact.ptr, info.remote_contact.slen);
		}

		if (on_call_end_)
		{
			BlabbleCallPtr call = get_shared();
			on_call_end_->getHost()->ScheduleOnMainThread(call, boost::bind(&BlabbleCall::CallOnCallEndStatus, call, info.last_status));
		}
	} else {
		if (on_call_end_)
		{
			BlabbleCallPtr call = get_shared();
			on_call_end_->getHost()->ScheduleOnMainThread(call, boost::bind(&BlabbleCall::CallOnCallEnd, call));
		}
	}

	BlabbleAccountPtr p = parent_.lock();
	if (p)
		p->OnCallEnd(get_shared());
}

BlabbleCall::~BlabbleCall(void)
{
	std::string str = "Call Deleted. Global id: " + boost::lexical_cast<std::string>(id_);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	//BLABBLE_LOG_DEBUG("Call Deleted. Global id: " << id_);
	on_call_end_.reset();
	LocalEnd();
}

bool BlabbleCall::RegisterIncomingCall(pjsua_call_id call_id)
{
	BlabbleAccountPtr p = parent_.lock();
	if (!p)
		return false;

	if (call_id_ == INVALID_CALL && call_id != INVALID_CALL &&
		call_id >= 0 && call_id < (long)pjsua_call_get_max_count())
	{
		call_id_ = call_id;
		pjsua_call_set_user_data(call_id, &id_);
		std::string str = "PJSIP call id " + boost::lexical_cast<std::string>(call_id)+" associated to BlabbleCall with Global id " + boost::lexical_cast<std::string>(id_);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_DEBUG("PJSIP call id " << call_id << " associated to BlabbleCall with Global id " << id_);

		return true;
	}
	std::string str = "BlabbleCall::RegisterIncomingCall called on BlabbleCall with Global id " + boost::lexical_cast<std::string>(id_)+" already associated to a PJSIP call, or invalid PJSIP call id specified";
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	//BLABBLE_LOG_ERROR("BlabbleCall::RegisterIncomingCall called on BlabbleCall with Global id " << id_ << " already associated to a PJSIP call, or invalid PJSIP call id specified");

	return false;
}

bool BlabbleCall::HandleIncomingCall(pjsip_rx_data *rdata)
{
	BlabbleAccountPtr p = parent_.lock();
	if (!p)
		return false;

	if (call_id_ == INVALID_CALL)
	{
		std::string str = "BlabbleCall::HandleIncomingCall called on BlabbleCall with Global id " + boost::lexical_cast<std::string>(id_)+" not associated to a PJSIP call";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_ERROR("BlabbleCall::HandleIncomingCall called on BlabbleCall with Global id " << id_ << " not associated to a PJSIP call");

		return false;
	}

	bool mustAnswerCall	= false;

	if (rdata != NULL)
	{
		const pj_str_t hdrName = { "Call-Info", 9 };
		pjsip_generic_string_hdr* hdr = (pjsip_generic_string_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &hdrName, NULL);
		if (hdr != NULL)
		{
			const char* hdrValue = "reitek;answer-after=";
			hdr->hvalue.ptr[hdr->hvalue.slen] = '\0';

			char* pos = strstr(hdr->hvalue.ptr, hdrValue);
			if (pos != NULL)
			{
				pos = pos += strlen(hdrValue);
				const int timeout = atoi(pos);
				if (timeout == 0)
				{
					std::string str = "PJSIP call id " + boost::lexical_cast<std::string>(call_id_) + ": auto answer header found";
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
					//BLABBLE_LOG_DEBUG("PJSIP call id " << call_id_ << ": auto answer header found");

					mustAnswerCall = true;
				}
			}
		}
	}

	if (mustAnswerCall)
	{
		pjsua_call_answer(call_id_, 180, NULL, NULL);

		//StartInRinging();

		// !!! NOTE: This flag must be set else the call will NOT be answered
		ringing_ = true;

		Answer();
	}
	else
	{
		pjsua_msg_data msg_data;

		pjsip_generic_string_hdr allowEvents;
		pj_str_t hname = pj_str("Allow-Events");
		pj_str_t hvalue = pj_str("talk");

		pjsua_msg_data_init(&msg_data);
		pjsip_generic_string_hdr_init2(&allowEvents, &hname, &hvalue);
		pj_list_push_back(&msg_data.hdr_list, &allowEvents);

		pjsua_call_answer(call_id_, 180, NULL, &msg_data);

		StartInRinging();
	}

	return true;
}

#if 0	// REITEK: Disabled
pj_status_t BlabbleCall::MakeCall(const std::string& dest, const std::string& identity)
{
	BlabbleAccountPtr p = parent_.lock();
	if (!p)
		return false;

	if (call_id_ != INVALID_CALL)
		return false;

	pj_status_t status;
	pj_str_t desturi;
	desturi.ptr = const_cast<char*>(dest.c_str());
	desturi.slen = dest.length();

	if (!identity.empty())
	{
		pjsua_msg_data msgData;
		pjsua_msg_data_init(&msgData);
		pjsip_generic_string_hdr cidHdr;
		pj_str_t name = pj_str(const_cast<char*>("P-Asserted-Identity"));
		pj_str_t value = pj_str(const_cast<char*>(identity.c_str()));

		pjsip_generic_string_hdr_init2(&cidHdr, &name, &value);
		pj_list_push_back(&msgData.hdr_list, &cidHdr);

		status = pjsua_call_make_call(acct_id_, &desturi, 0,
			&id_, &msgData, (pjsua_call_id*)&call_id_);
	}
	else 
	{
		status = pjsua_call_make_call(acct_id_, &desturi, 0,
			&id_, NULL, (pjsua_call_id*)&call_id_);
	}

	if (status == PJ_SUCCESS) 
	{
		destination_ = dest;
		StartOutRinging();
	} 

	return status;
}
#endif

bool BlabbleCall::Answer()
{
	BlabbleAccountPtr p = CheckAndGetParent();
	if (!p)
		return false;

	if (!ringing_)
		return false;

	// Stop playing the wav file not related to a call
	audio_manager_->StopWav();

	StopRinging();

	std::string str = "Answering PJSIP call id " + boost::lexical_cast<std::string>(call_id_)+" associated to BlabbleCall with Global id " + boost::lexical_cast<std::string>(id_);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	//BLABBLE_LOG_DEBUG("Answering PJSIP call id " << call_id_ << " associated to BlabbleCall with Global id " << id_);

	pj_status_t status = pjsua_call_answer(call_id_, 200, NULL, NULL);

	return status == PJ_SUCCESS;
}

#if 0	// REITEK: Disabled
bool BlabbleCall::Hold()
{
	BlabbleAccountPtr p = CheckAndGetParent();
	if (!p)
		return false;

	pj_status_t status = pjsua_call_set_hold(call_id_, NULL);
	return status == PJ_SUCCESS;
}
#endif

static bool IsValidDtmf(char c)
{
	const char lc = tolower(c);

	if ((lc >= '0' && lc <= '9') || (lc == '*') || (lc == '#') || (lc >= 'a' && lc <= 'd'))
		return true;

	return false;
}


bool BlabbleCall::SendDTMF(const std::string& dtmf)
{
#if 0	// REITEK: Sending more than 1 dtmf is valid
	if (dtmf.length() != 1)
	{
		throw FB::script_error("SendDTMF may only send one character!");
	}
#endif

#if 0	// REITEK: Allowed digits are those internally supported by pjmedia_stream_dial_dtmf ('0'-'9','*','#','a'-'d') except for 'r' (flash)
	char c;

	c = dtmf[0];
	if (c != '#' && c != '*' && c < '0' && c > '9')
	{
		throw FB::script_error("SendDTMF may only send numbers, # and *");
	}

	digits.ptr = &c;
	digits.slen = 1;
#endif

	std::string valid_digits;
	std::string invalid_digits;
	for (std::size_t i = 0, max = dtmf.length(); i < max; i++)
	{
		if (IsValidDtmf(dtmf[i]))
			valid_digits += dtmf[i];
		else
			invalid_digits += dtmf[i];
	}

	if (invalid_digits.length() > 0)
	{
		std::string str = "WARNING: Discarded characters not valid for SendDTMF: " + invalid_digits;
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	pj_str_t digits;

	digits.ptr = const_cast<char*>(valid_digits.c_str());
	digits.slen = valid_digits.length();

	BlabbleAccountPtr p;

	if (!(p = CheckAndGetParent()))
		return false;

	pj_status_t status;

	status = pjsua_call_dial_dtmf(call_id_, &digits);

	return status == PJ_SUCCESS;
}

#if 0	// REITEK: Disabled
bool BlabbleCall::Unhold()
{
	BlabbleAccountPtr p = CheckAndGetParent();
	if (!p)
		return false;

	pj_status_t status = pjsua_call_reinvite(call_id_, PJ_TRUE, NULL);
	return status == PJ_SUCCESS;
}

bool BlabbleCall::TransferReplace(const BlabbleCallPtr& otherCall)
{
	BlabbleAccountPtr p = CheckAndGetParent();
	if (!p)
		return false;

	if (!otherCall)
		return false;

	pj_status_t status = pjsua_call_xfer_replaces(call_id_, otherCall->call_id_, 
		PJSUA_XFER_NO_REQUIRE_REPLACES, NULL);
	if (status == PJ_SUCCESS)
	{
		LocalEnd();
	}

	return status == PJ_SUCCESS;
}

bool BlabbleCall::Transfer(const FB::VariantMap &params)
{
	std::string destination;
	pj_str_t desturi;
	BlabbleAccountPtr p = parent_.lock();
	if (!p)
		return false;

	if (call_id_ == INVALID_CALL)
		return false;

	FB::VariantMap::const_iterator iter = params.find("destination");
	if (iter == params.end())
	{
		throw FB::script_error("No destination given!");
	}
	else
	{
		destination = iter->second.cast<std::string>();
		if (destination.size() <= 4 || destination.substr(0, 4) != "sip:")
			destination = "sip:" + destination + "@" + p->server();

		if (p->use_tls() && destination.find("transport=TLS") == std::string::npos)
			destination += ";transport=TLS";
	}

	if ((iter = params.find("onStatusChange")) != params.end() &&
		iter->second.is_of_type<FB::JSObjectPtr>())
	{
		set_on_transfer_status(iter->second.cast<FB::JSObjectPtr>());
	}

	desturi.ptr = const_cast<char*>(destination.c_str());
	desturi.slen = destination.length();
	return pjsua_call_xfer(call_id_, &desturi, NULL) == PJ_SUCCESS;
}
#endif

std::string BlabbleCall::caller_id()
{
	BlabbleAccountPtr p = CheckAndGetParent();
	if (!p)
		return "INVALID CALL";

	pjsua_call_info info;
	if (pjsua_call_get_info(call_id_, &info) == PJ_SUCCESS) 
	{
		return std::string(info.remote_contact.ptr, info.remote_contact.slen);
	}
	return "";
}

bool BlabbleCall::get_valid()
{
	return (bool)CheckAndGetParent();
}

FB::VariantMap BlabbleCall::status()
{
	FB::VariantMap map = FB::VariantMap();
	map["state"] = (int)CALL_INVALID;
	pjsua_call_info info;
	pj_status_t status;

	if (call_id_ != INVALID_CALL &&
		(status = pjsua_call_get_info(call_id_, &info)) == PJ_SUCCESS)
	{
		if (info.media_status == PJSUA_CALL_MEDIA_LOCAL_HOLD ||
			info.media_status == PJSUA_CALL_MEDIA_REMOTE_HOLD)
		{
				map["state"] = (int)CALL_HOLD;
				map["callerId"] = std::string(info.remote_contact.ptr, info.remote_contact.slen);
		} 
		else if (info.media_status == PJSUA_CALL_MEDIA_ACTIVE ||
			info.media_status == PJSUA_CALL_MEDIA_ERROR ||
			(info.media_status == PJSUA_CALL_MEDIA_NONE && 
				info.state == PJSIP_INV_STATE_CONFIRMED))
		{
			map["state"] = (int)CALL_ACTIVE;
			map["duration"] = info.connect_duration.sec;
			map["callerId"] = std::string(info.remote_contact.ptr, info.remote_contact.slen);
		}
		else if (info.media_status == PJSUA_CALL_MEDIA_NONE &&
			(info.state == PJSIP_INV_STATE_CALLING ||
				info.state == PJSIP_INV_STATE_INCOMING ||
				info.state == PJSIP_INV_STATE_EARLY))
		{
			map["state"] = info.state == PJSIP_INV_STATE_INCOMING ? (int)CALL_RINGING_IN : (int)CALL_RINGING_OUT;
			map["duration"] = info.connect_duration.sec;
			if (info.remote_contact.ptr == NULL)
			{
				map["callerId"] = std::string(info.remote_info.ptr, info.remote_info.slen);
			} 
			else
			{
				map["callerId"] = std::string(info.remote_contact.ptr, info.remote_contact.slen);
			}
		} 
	}

	return map;
}

void BlabbleCall::OnCallMediaState()
{
	if (call_id_ == INVALID_CALL)
		return;

	pjsua_call_info info;
	pj_status_t status;
	if ((status = pjsua_call_get_info(call_id_, &info)) != PJ_SUCCESS) {
		std::string str = "Unable to get call info. PJSIP call id: " + boost::lexical_cast<std::string>(call_id_) + ", global id: " + boost::lexical_cast<std::string>(id_)+", pjsua_call_get_info returned " + boost::lexical_cast<std::string>(status);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_ERROR("Unable to get call info. PJSIP call id: " << call_id_ << ", global id: " << id_ << ", pjsua_call_get_info returned " << status);
		StopRinging();
		return;
	}
	std::string str = "PJSIP call id " + boost::lexical_cast<std::string>(call_id_)+": media state: " + boost::lexical_cast<std::string>(info.media_status);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	//BLABBLE_LOG_DEBUG("PJSIP call id " << call_id_ << ": media state: " << info.media_status);

	if (info.media_status == PJSUA_CALL_MEDIA_ACTIVE) 
	{
		StopRinging();

		// When media is active, connect call to sound device.
		pjsua_conf_connect(info.conf_slot, 0);
		pjsua_conf_connect(0, info.conf_slot);
	}
}

void BlabbleCall::OnCallState(pjsua_call_id call_id, pjsip_event *e)
{
	pjsua_call_info info;
	if (pjsua_call_get_info(call_id, &info) == PJ_SUCCESS)
	{
		std::string str = "PJSIP call id " + boost::lexical_cast<std::string>(call_id) + ": call state: " + boost::lexical_cast<std::string>(info.state);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
		//BLABBLE_LOG_DEBUG("PJSIP call id " << call_id << ": call state: " << info.state);

		if (info.state == PJSIP_INV_STATE_DISCONNECTED) 
		{
			// REITEK: Dump call statistics

			std::string dbgstr = "Dumping statistics for PJSIP call id: " + boost::lexical_cast<std::string>(call_id);
			BlabbleLogging::blabbleLog(0, dbgstr.c_str(), 0);

			char stats_buf[STATS_BUF_SIZE];
			memset(stats_buf, 0, STATS_BUF_SIZE);

			pjsua_call_dump(call_id, PJ_TRUE, stats_buf, STATS_BUF_SIZE, "  ");
			stats_buf[STATS_BUF_SIZE - 1] = '\0';

			BlabbleLogging::blabbleLog(0, stats_buf, 0);

			if (on_call_end_statistics_)
			{
				BlabbleCallPtr call = get_shared();
				on_call_end_statistics_->getHost()->ScheduleOnMainThread(call, boost::bind(&BlabbleCall::CallOnCallEndStatistics, call, stats_buf));
			}

			RemoteEnd(info);
		}
		else if (info.state == PJSIP_INV_STATE_CALLING)
		{
			if (on_call_ringing_)
				on_call_ringing_->InvokeAsync("", FB::variant_list_of(BlabbleCallWeakPtr(get_shared())));

			BlabbleAccountPtr p = parent_.lock();
			if (p)
				p->OnCallRingChange(get_shared(), info);
		}
		else if (info.state == PJSIP_INV_STATE_CONFIRMED)
		{
			if (on_call_connected_)
				on_call_connected_->InvokeAsync("", FB::variant_list_of(BlabbleCallWeakPtr(get_shared())));

			BlabbleAccountPtr p = parent_.lock();
			if (p)
				p->OnCallRingChange(get_shared(), info);
		}
	}
}

// REITEK: Method to handle transaction state changes
void BlabbleCall::OnCallTsxState(pjsua_call_id call_id, pjsip_transaction *tsx, pjsip_event *e)
{
	pjsua_call_info info;
	if (pjsua_call_get_info(call_id, &info) == PJ_SUCCESS)
	{
		if (pjsip_method_cmp(&tsx->method, &pjsip_options_method) == 0)
		{
			/*
			* Handle OPTIONS method.
			*/
			if (tsx->role == PJSIP_ROLE_UAS && tsx->state == PJSIP_TSX_STATE_TRYING)
			{
				/* Answer incoming OPTIONS with 200/OK */
				pjsip_rx_data *rdata;
				pjsip_tx_data *tdata;
				pj_status_t status;

				rdata = e->body.tsx_state.src.rdata;

				status = pjsip_endpt_create_response(tsx->endpt, rdata, 200, NULL, &tdata);
				if (status == PJ_SUCCESS)
				{
					status = pjsip_tsx_send_msg(tsx, tdata);
				}

				if (status == PJ_SUCCESS)
				{
					std::string str = "OPTIONS for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + " answered";
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
					//BLABBLE_LOG_DEBUG("OPTIONS for PJSIP call id " << call_id << " answered");
				}
				else
				{
					std::string str = "OPTIONS for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + " not answered";
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
					//BLABBLE_LOG_DEBUG("OPTIONS for PJSIP call id " << call_id << " not answered");
				}
			}
		}
		else if (pjsip_method_cmp(&tsx->method, &pjsip_notify_method) == 0)
		{
			/*
			* Handle NOTIFY method.
			*/
			if (tsx->role == PJSIP_ROLE_UAS && tsx->state == PJSIP_TSX_STATE_TRYING)
			{
				/* Answer incoming NOTIFY with 200/OK if they contains the Event: talk */
				pjsip_rx_data *rdata;
				pjsip_tx_data *tdata;
				pj_status_t status;

				rdata = e->body.tsx_state.src.rdata;

				const pj_str_t hdrName = { "Event", 5 };
				pjsip_generic_string_hdr* hdr = (pjsip_generic_string_hdr*)pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &hdrName, NULL);
				if (hdr != NULL)
				{
					const char* hdrValue = "talk";
					hdr->hvalue.ptr[hdr->hvalue.slen] = '\0';

					if (strcmp(hdr->hvalue.ptr, hdrValue) == 0)
					{
						status = pjsip_endpt_create_response(tsx->endpt, rdata, 200, NULL, &tdata);
						if (status == PJ_SUCCESS)
						{
							status = pjsip_tsx_send_msg(tsx, tdata);
						}

						if (status == PJ_SUCCESS)
						{
							std::string str = "NOTIFY (Event:Talk) for PJSIP call id " + boost::lexical_cast<std::string>(call_id)+" answered";
							BlabbleLogging::blabbleLog(0, str.c_str(), 0);
							//BLABBLE_LOG_DEBUG("NOTIFY (Event:Talk) for PJSIP call id " << call_id << " answered");
						}
						else
						{
							std::string str = "NOTIFY (Event:Talk) for PJSIP call id " + boost::lexical_cast<std::string>(call_id)+" not answered";
							BlabbleLogging::blabbleLog(0, str.c_str(), 0);
							//BLABBLE_LOG_ERROR("NOTIFY (Event:Talk) for PJSIP call id " << call_id << " not answered");
						}

						Answer();
					}
				}
			}
		}
	}
}

#if 0	// REITEK: Disabled
bool BlabbleCall::OnCallTransferStatus(int status)
{
	if (call_id_ != INVALID_CALL) 
	{
		if (on_transfer_status_)
		{
			BlabbleCallPtr call = get_shared();
			on_transfer_status_->getHost()->ScheduleOnMainThread(call, boost::bind(&BlabbleCall::CallOnTransferStatus, call, status));
		}
	}

	return false;
}
#endif

BlabbleAccountPtr BlabbleCall::CheckAndGetParent()
{
	if (call_id_ != INVALID_CALL)
	{
		return parent_.lock();
	}

	return BlabbleAccountPtr();
}

bool BlabbleCall::is_active()
{ 
	return ((bool)CheckAndGetParent() && pjsua_call_is_active(call_id_)); 
}
