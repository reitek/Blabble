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


/* OPTIONS keep-alive timer callback */
static void options_ka_timer(pj_timer_heap_t *th, pj_timer_entry *e)
{
	const std::string str = "OPTIONS keep-alive timer callback (user_data: " + boost::lexical_cast<std::string>(e->user_data) + ")";
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

	BlabbleCall * call = (BlabbleCall *) e->user_data;

	call->SendOptionsKA();
}

/* Periodic event timer callback */
static void periodic_event_timer(pj_timer_heap_t *th, pj_timer_entry *e)
{
	const std::string str = "Periodic event timer callback (user_data: " + boost::lexical_cast<std::string>(e->user_data) + ")";
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

	BlabbleCall * call = (BlabbleCall *) e->user_data;

	call->OnPeriodicEventTimer();
}

/* answer timer callback */
static void answer_timer(pj_timer_heap_t *th, pj_timer_entry *e)
{
	const std::string str = "Answer timer callback (user_data: " + boost::lexical_cast<std::string>(e->user_data) + ")";
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

	BlabbleCall * call = (BlabbleCall *) e->user_data;

	call->OnAnswerTimer();
}


/*! @Brief Static call counter to keep track of calls.
 */
unsigned int BlabbleCall::id_counter_ = 0;
unsigned int BlabbleCall::GetNextId()
{
	return ATOMIC_INCREMENT(&BlabbleCall::id_counter_);
}

BlabbleCall::BlabbleCall(const BlabbleAccountPtr& parent_account)
	: call_id_(INVALID_CALL), ringing_(false), firstconfirmedstate_(true)
{
	if (parent_account) 
	{
		acct_id_ = parent_account->id();
		audio_manager_ = parent_account->GetManager()->audio_manager();
		parent_ = BlabbleAccountWeakPtr(parent_account);
		// ENGHOUSE: OPTIONS keep-alive timeout
		optionskatimeout_ = parent_account->GetManager()->optionskatimeout_;
		// ENGHOUSE: Periodic event timeout
		periodiceventtimeout_ = parent_account->GetManager()->periodiceventtimeout_;
		// ENGHOUSE: Maximum timeout for answering the call
		answertimeout_ = parent_account->GetManager()->answertimeout_;
	}
	else 
	{
		acct_id_ = -1;
	}
	
	id_ = BlabbleCall::GetNextId();

	{
		const std::string str = "Created new call with global id " + boost::lexical_cast<std::string>(id_);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	pj_timer_entry_init(&options_ka_timer_, 0, (void *)this, &options_ka_timer);
	pj_timer_entry_init(&periodic_event_timer_, 1, (void *)this, &periodic_event_timer);
	pj_timer_entry_init(&answer_timer_, 2, (void *)this, &answer_timer);

	{
		const std::string str = "Set OPTIONS keep-alive user_data: " + boost::lexical_cast<std::string>(this);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

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
	registerProperty("statistics", make_property(this, &BlabbleCall::statistics));

	registerProperty("onCallConnected", make_write_only_property(this, &BlabbleCall::set_on_call_connected));
	registerProperty("onCallEnd", make_write_only_property(this, &BlabbleCall::set_on_call_end));
	registerProperty("onCallPeriodicEvent", make_write_only_property(this, &BlabbleCall::set_on_call_periodic_event));
#if 0	// !!! REMOVE ME
	registerProperty("onCallEndStatistics", make_write_only_property(this, &BlabbleCall::set_on_call_end_statistics));
#endif
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

	StopOptionsKATimer(old_id);

	StopPeriodicEventTimer(old_id);

	StopAnswerTimer(old_id);

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
		const std::string str = "Scheduling execution of onCallEnd handler for PJSIP call id " + boost::lexical_cast<std::string>(old_id);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		BlabbleCallPtr call = get_shared();

		/**
		*	Use a status code equal to 0
		*/
		on_call_end_->getHost()->ScheduleOnMainThread(call, std::bind(&BlabbleCall::CallOnCallEnd, call, old_id, (pjsip_status_code) 0));
	} else {
		/**
		*	No onCallEnd callback: the call can be removed immediately
		*/

		BlabbleAccountPtr p = parent_.lock();
		if (p)
			p->OnCallEnd(old_id, get_shared());
	}
}

void BlabbleCall::CallOnCallEnd(pjsua_call_id call_id, pjsip_status_code status)
{
	const std::string str = "Executing onCallEnd handler for PJSIP call id " + boost::lexical_cast<std::string>(call_id);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

	auto promise = on_call_end_->Invoke("", { BlabbleCallWeakPtr(get_shared()), status } );
	promise.then<void>([this, call_id] (FB::variant) -> void {
		const std::string str = "Executed onCallEnd handler for PJSIP call id " + boost::lexical_cast<std::string>(call_id);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		BlabbleAccountPtr p = parent_.lock();
		if (p)
			p->OnCallEnd(call_id, get_shared());
	}, [this, call_id](std::exception_ptr eptr) -> void {
		std::string str = "Exception during execution of onCallEnd handler for PJSIP call id " + boost::lexical_cast<std::string>(call_id);

		try {
			if (eptr) {
				std::rethrow_exception(eptr);
			}
	    } catch(const std::exception& e) {
			str += ": ";
			str += e.what();
		}

		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		BlabbleAccountPtr p = parent_.lock();
		if (p)
			p->OnCallEnd(call_id, get_shared());
	});
}

#if 0	// !!! REMOVE ME
void BlabbleCall::CallOnCallEndStatistics(std::string statistics)
{
	on_call_end_statistics_->Invoke("", { BlabbleCallWeakPtr(get_shared()), statistics });
}
#endif

#if 0	// REITEK: Disabled
void BlabbleCall::CallOnTransferStatus(int status)
{
	on_transfer_status_->Invoke("", { BlabbleCallWeakPtr(get_shared()), status } );
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

	StopOptionsKATimer(old_id);

	StopPeriodicEventTimer(old_id);

	StopAnswerTimer(old_id);

	StopRinging();

	//Kill the audio
	if (info.conf_slot > 0) 
	{
		pjsua_conf_disconnect(info.conf_slot, 0);
		pjsua_conf_disconnect(0, info.conf_slot);
	}

	pjsua_call_hangup(old_id, 0, NULL, NULL);

	if (on_call_end_)
	{
		const std::string str = "Scheduling execution of onCallEnd handler for PJSIP call id " + boost::lexical_cast<std::string>(old_id);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		BlabbleCallPtr call = get_shared();
		on_call_end_->getHost()->ScheduleOnMainThread(call, std::bind(&BlabbleCall::CallOnCallEnd, call, old_id, info.last_status));
	} else {
		/**
		*	No onCallEnd callback: the call can be removed immediately
		*/

		BlabbleAccountPtr p = parent_.lock();
		if (p)
			p->OnCallEnd(old_id, get_shared());
	}
}

BlabbleCall::~BlabbleCall(void)
{
	const std::string str = "Call with global id " + boost::lexical_cast<std::string>(id_) + " deleted";
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);
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
		const std::string str = "PJSIP call id " + boost::lexical_cast<std::string>(call_id)+" associated to call with global id " + boost::lexical_cast<std::string>(id_);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		return true;
	}
	const std::string str = "RegisterIncomingCall called on call with global id " + boost::lexical_cast<std::string>(id_)+" already associated to a PJSIP call, or invalid PJSIP call id specified";
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

	return false;
}

bool BlabbleCall::HandleIncomingCall(pjsip_rx_data *rdata)
{
	BlabbleAccountPtr p = parent_.lock();
	if (!p)
		return false;

	if (call_id_ == INVALID_CALL)
	{
		const std::string str = "HandleIncomingCall called on call with global id " + boost::lexical_cast<std::string>(id_)+" not associated to a PJSIP call";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		return false;
	}

	// Start the periodic event timer
	StartPeriodicEventTimer();

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
					const std::string str = "PJSIP call id " + boost::lexical_cast<std::string>(call_id_) + ": auto answer header found";
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);

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

bool BlabbleCall::StartOptionsKATimer()
{
	if (optionskatimeout_ > 0)
	{
		const std::string str = "Start " + boost::lexical_cast<std::string>(optionskatimeout_) + "s OPTIONS keep-alive timer for PJSIP call id " + boost::lexical_cast<std::string>(call_id_) + " (user_data: " + boost::lexical_cast<std::string>(this) + ")";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		pj_time_val delay = { 0 };
		delay.sec = optionskatimeout_;

		const pj_status_t status = pjsip_endpt_schedule_timer(pjsua_get_pjsip_endpt(), &options_ka_timer_, &delay);
		if (status != PJ_SUCCESS)
		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " ERROR:                Could not schedule OPTIONS keep-alive timer";
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);

			return false;
		}
	}

	return true;
}

bool BlabbleCall::StopOptionsKATimer(pjsua_call_id call_id)
{
	if (optionskatimeout_ > 0)
	{
		const std::string str = "Stop OPTIONS keep-alive timer for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + " (user_data: " + boost::lexical_cast<std::string>(this) + ")";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		pjsip_endpt_cancel_timer(pjsua_get_pjsip_endpt(), &options_ka_timer_);
	}

	return true;
}

bool BlabbleCall::SendOptionsKA()
{
	std::string str = "Send OPTIONS keep-alive PJSIP call id " + boost::lexical_cast<std::string>(call_id_);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

	const pj_str_t SIP_OPTIONS = pj_str("OPTIONS");

	pj_status_t status = pjsua_call_send_request(call_id_, &SIP_OPTIONS, NULL);
	if (status != PJ_SUCCESS)
	{
		// !!! UGLY (should automatically conform to pjsip formatting)
		str = " ERROR:                Could not send OPTIONS keep-alive";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		return false;
	}

	return true;
}

/*! @Brief ENGHOUSE: Called to start the periodic event timer
*/
bool BlabbleCall::StartPeriodicEventTimer()
{
	if (periodiceventtimeout_ > 0)
	{
		const std::string str = "Start " + boost::lexical_cast<std::string>(periodiceventtimeout_) + "s periodic event timer for PJSIP call id " + boost::lexical_cast<std::string>(call_id_) + " (user_data: " + boost::lexical_cast<std::string>(this) + ")";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		pj_time_val delay = { 0 };
		delay.sec = periodiceventtimeout_;

		const pj_status_t status = pjsip_endpt_schedule_timer(pjsua_get_pjsip_endpt(), &periodic_event_timer_, &delay);
		if (status != PJ_SUCCESS)
		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " ERROR:                Could not schedule periodic event timer";
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);

			return false;
		}
	}

	return true;
}

/*! @Brief ENGHOUSE: Called to stop the periodic event timer
	*/
bool BlabbleCall::StopPeriodicEventTimer(pjsua_call_id call_id)
{
	if (periodiceventtimeout_ > 0)
	{
		const std::string str = "Stop periodic event timer for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + " (user_data: " + boost::lexical_cast<std::string>(this) + ")";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		pjsip_endpt_cancel_timer(pjsua_get_pjsip_endpt(), &periodic_event_timer_);
	}

	return true;
}

/*! @Brief ENGHOUSE: Handle the periodic event timer
*/
bool BlabbleCall::OnPeriodicEventTimer()
{
	const std::string str = "Periodic event timer for PJSIP call id " + boost::lexical_cast<std::string>(call_id_);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

	if (on_call_periodic_event_)
	{
		const std::string str = "Calling callback function for PJSIP call id " + boost::lexical_cast<std::string>(call_id_);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		on_call_periodic_event_->InvokeAsync("", { BlabbleCallWeakPtr(get_shared()) });
	}
	else
	{
		const std::string str = "Callback function not set for PJSIP call id " + boost::lexical_cast<std::string>(call_id_);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	// Restart the periodic event timer
	StartPeriodicEventTimer();

	return true;
}

bool BlabbleCall::StartAnswerTimer()
{
	if (answertimeout_ > 0)
	{
		const std::string str = "Start " + boost::lexical_cast<std::string>(answertimeout_) + "s answer timer for PJSIP call id " + boost::lexical_cast<std::string>(call_id_) + " (user_data: " + boost::lexical_cast<std::string>(this) + ")";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		pj_time_val delay = { 0 };
		delay.sec = answertimeout_;

		const pj_status_t status = pjsip_endpt_schedule_timer(pjsua_get_pjsip_endpt(), &answer_timer_, &delay);
		if (status != PJ_SUCCESS)
		{
			// !!! UGLY (should automatically conform to pjsip formatting)
			const std::string str = " ERROR:                Could not schedule answer timer";
			BlabbleLogging::blabbleLog(0, str.c_str(), 0);

			return false;
		}
	}

	return true;
}

bool BlabbleCall::StopAnswerTimer(pjsua_call_id call_id)
{
	if (answertimeout_ > 0)
	{
		const std::string str = "Stop answer timer for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + " (user_data: " + boost::lexical_cast<std::string>(this) + ")";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		pjsip_endpt_cancel_timer(pjsua_get_pjsip_endpt(), &answer_timer_);
	}

	return true;
}

bool BlabbleCall::OnAnswerTimer()
{
	const std::string str = "Answer timer for PJSIP call id " + boost::lexical_cast<std::string>(call_id_);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

	LocalEnd();

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
	{
		const std::string str = "answer JS method called for PJSIP call id " + boost::lexical_cast<std::string>(call_id_);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);
	}

	BlabbleAccountPtr p = CheckAndGetParent();
	if (!p)
		return false;

	if (!ringing_)
		return false;

	// Stop playing the wav file not related to a call
	audio_manager_->StopWav();

	StopRinging();

	const std::string str = "Answering PJSIP call id " + boost::lexical_cast<std::string>(call_id_)+" associated to call with global id " + boost::lexical_cast<std::string>(id_);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

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
		const std::string str = "WARNING: Discarded characters not valid for SendDTMF: " + invalid_digits;
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

const std::string BlabbleCall::statistics()
{
	const std::string str = "statistics JS method called for PJSIP call id " + boost::lexical_cast<std::string>(call_id_);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

	if (call_id_ == INVALID_CALL)
	{
		return "Invalid PJSIP call";
	}

	char stats_buf[STATS_BUF_SIZE];
	memset(stats_buf, 0, STATS_BUF_SIZE);

	pjsua_call_dump(call_id_, PJ_TRUE, stats_buf, STATS_BUF_SIZE, "  ");
	stats_buf[STATS_BUF_SIZE - 1] = '\0';

	return stats_buf;
}

void BlabbleCall::OnCallMediaState()
{
	if (call_id_ == INVALID_CALL)
		return;

	pjsua_call_info info;
	pj_status_t status;
	if ((status = pjsua_call_get_info(call_id_, &info)) != PJ_SUCCESS) {
		const std::string str = "Unable to get call info. PJSIP call id " + boost::lexical_cast<std::string>(call_id_) + ", global id " + boost::lexical_cast<std::string>(id_)+", pjsua_call_get_info returned " + boost::lexical_cast<std::string>(status);
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		StopRinging();
		return;
	}
	const std::string str = "PJSIP call id " + boost::lexical_cast<std::string>(call_id_)+": media state: " + boost::lexical_cast<std::string>(info.media_status);
	BlabbleLogging::blabbleLog(0, str.c_str(), 0);

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
		const std::string str = "PJSIP call id " + boost::lexical_cast<std::string>(call_id) +
						": call state: " + boost::lexical_cast<std::string>(info.state) +
						" (" + std::string(pjsip_inv_state_name(info.state)) + ")";
		BlabbleLogging::blabbleLog(0, str.c_str(), 0);

		if (info.state == PJSIP_INV_STATE_DISCONNECTED)
		{
#if 0	// !!! REMOVE ME
			/**
			*	ENGHOUSE: !!! CHECK: This is not reliable anymore, because it is made after media is already deinitialised;
			*	anyway, pjsip automatically dumps statistics before that happens
			*/

			// REITEK: Dump call statistics

			const std::string dbgstr = "Dumping statistics for PJSIP call id " + boost::lexical_cast<std::string>(call_id);
			BlabbleLogging::blabbleLog(0, dbgstr.c_str(), 0);

			char stats_buf[STATS_BUF_SIZE];
			memset(stats_buf, 0, STATS_BUF_SIZE);

			pjsua_call_dump(call_id, PJ_TRUE, stats_buf, STATS_BUF_SIZE, "  ");
			stats_buf[STATS_BUF_SIZE - 1] = '\0';

			BlabbleLogging::blabbleLog(0, stats_buf, 0);

			if (on_call_end_statistics_)
			{
				BlabbleCallPtr call = get_shared();
				on_call_end_statistics_->getHost()->ScheduleOnMainThread(call, std::bind(&BlabbleCall::CallOnCallEndStatistics, call, stats_buf));
			}
#endif

			RemoteEnd(info);
		}
		else if (info.state == PJSIP_INV_STATE_CALLING)
		{
			if (on_call_ringing_)
				on_call_ringing_->InvokeAsync("", { BlabbleCallWeakPtr(get_shared()) } );

			BlabbleAccountPtr p = parent_.lock();
			if (p)
				p->OnCallRingChange(get_shared(), info);
		}
		else if (info.state == PJSIP_INV_STATE_EARLY)
		{
			// ENGHOUSE: Start the answer timer
			StartAnswerTimer();
		}
		else if (info.state == PJSIP_INV_STATE_CONNECTING)
		{
			// ENGHOUSE: Stop the answer timer
			StopAnswerTimer(call_id_);
		}
		else if (info.state == PJSIP_INV_STATE_CONFIRMED)
		{
			if (on_call_connected_)
				on_call_connected_->InvokeAsync("", { BlabbleCallWeakPtr(get_shared()) } );

			BlabbleAccountPtr p = parent_.lock();
			if (p)
				p->OnCallRingChange(get_shared(), info);

			// ENGHOUSE: If this is the first ACK, schedule the OPTIONS keep-alive timer
			if (firstconfirmedstate_)
			{
				firstconfirmedstate_ = false;

				const std::string str = "PJSIP call id " + boost::lexical_cast<std::string>(call_id) + ": first ACK";
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);

				StartOptionsKATimer();
			}

			// ENGHOUSE: Stop the answer timer
			StopAnswerTimer(call_id_);
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
			* Handle incoming OPTIONS request
			*/
			if (tsx->role == PJSIP_ROLE_UAS && tsx->state == PJSIP_TSX_STATE_TRYING)
			{
				/* Answer incoming OPTIONS request with 200 OK */
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
					const std::string str = "Incoming OPTIONS keep-alive for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + " answered";
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
				}
				else
				{
					// !!! UGLY (should automatically conform to pjsip formatting)
					const std::string str = " ERROR:                Incoming OPTIONS keep-alive for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + " not answered";
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
				}
			}
			else if ((tsx->role == PJSIP_ROLE_UAC) && (tsx->state == PJSIP_TSX_STATE_COMPLETED))
			{
				// Final response for sent OPTIONS keep-alive request

				const std::string str = "Final response for sent OPTIONS keep-alive request for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + " received";
				BlabbleLogging::blabbleLog(0, str.c_str(), 0);

				pjsip_msg *msg = e->body.tsx_state.src.rdata->msg_info.msg;
				if (msg->type == PJSIP_RESPONSE_MSG)
				{
					if (msg->line.status.code == PJSIP_SC_OK)
					{
						const std::string str = "Final response for sent OPTIONS keep-alive request for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + " status code: 200";
						BlabbleLogging::blabbleLog(0, str.c_str(), 0);

						// Restart the OPTIONS keep-alive timer
						StartOptionsKATimer();
					}
					else
					{
						// !!! UGLY (should automatically conform to pjsip formatting)
						const std::string str = " ERROR:                Final response for sent OPTIONS keep-alive request for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + " status code: " + boost::lexical_cast<std::string>(msg->line.status.code);
						BlabbleLogging::blabbleLog(0, str.c_str(), 0);

						// Must hangup the call

						LocalEnd();
					}
				}
				else
				{
					// !!! UGLY (should automatically conform to pjsip formatting)
					const std::string str = " ERROR:                Message is not a SIP response !!!???";
					BlabbleLogging::blabbleLog(0, str.c_str(), 0);
				}
			}
		}
		else if (pjsip_method_cmp(&tsx->method, &pjsip_notify_method) == 0)
		{
			/*
			* Handle incoming NOTIFY request
			*/
			if (tsx->role == PJSIP_ROLE_UAS && tsx->state == PJSIP_TSX_STATE_TRYING)
			{
				// !!! FIXME: The incoming NOTIFY request must always be answered

				/* Answer incoming NOTIFY request with 200 OK if it contains the Event: talk */
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
							const std::string str = "Incoming NOTIFY (Event:Talk) for PJSIP call id " + boost::lexical_cast<std::string>(call_id)+" answered";
							BlabbleLogging::blabbleLog(0, str.c_str(), 0);
						}
						else
						{
							// !!! UGLY (should automatically conform to pjsip formatting)
							const std::string str = " ERROR:                Incoming NOTIFY (Event:Talk) for PJSIP call id " + boost::lexical_cast<std::string>(call_id) + " not answered";
							BlabbleLogging::blabbleLog(0, str.c_str(), 0);
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
			on_transfer_status_->getHost()->ScheduleOnMainThread(call, std::bind(&BlabbleCall::CallOnTransferStatus, call, status));
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
