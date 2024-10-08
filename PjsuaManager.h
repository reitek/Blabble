/**********************************************************\
Original Author: Andrew Ofisher (zaltar)

License:    GNU General Public License, version 3.0
            http://www.gnu.org/licenses/gpl-3.0.txt

Copyright 2012 Andrew Ofisher
\**********************************************************/

#ifndef H_PjsuaManagerPLUGIN
#define H_PjsuaManagerPLUGIN

#include "JSAPIAuto.h"
#include <string>
#include <map>
//#include <boost/smart_ptr/shared_ptr.hpp>
//#include <boost/optional.hpp>
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

FB_FORWARD_PTR(BlabbleCall)
FB_FORWARD_PTR(BlabbleAccount)
FB_FORWARD_PTR(BlabbleAudioManager)
FB_FORWARD_PTR(PjsuaManager)

typedef std::map<int, BlabbleAccountPtr> BlabbleAccountMap;

/*! @class  PjsuaManager
 *
 *  @brief  Singleton used to manage accounts, audio, and callbacks from PJSIP.
 *
 *  @author Andrew Ofisher (zaltar)
*/
class PjsuaManager : public boost::enable_shared_from_this<PjsuaManager>
{
public:
	// ENGHOUSE: OPTIONS keep-alive timeout
	static int optionskatimeout_;

	// ENGHOUSE: Periodic event timeout
	static int periodiceventtimeout_;

	// ENGHOUSE: Answer timeout
	static int answertimeout_;

	// REITEK: Get/parse parameters passed to the plugin upon manager creation

	static PjsuaManagerPtr GetManager(Blabble& pluginCore);
	virtual ~PjsuaManager();

	/*! @Brief Retrive the current audio manager.
	 *  The audio manager allows control of ringing, busy signals, and ringtones.
	 */
	BlabbleAudioManagerPtr audio_manager() { return audio_manager_; }

	void AddAccount(const BlabbleAccountPtr &account);
	void RemoveAccount(pjsua_acc_id acc_id);
	BlabbleAccountPtr FindAcc(int accId);

	// REITEK: Disable TLS flag (TLS is handled differently)
#if 0
	/*! Return true if we have TLS/SSL capability.
	 */
	bool has_tls() { return has_tls_; }
#endif

	static void SetCodecPriority(const char* codec, int value);

	//void SetCodecPriorityAll(std::vector<std::pair<std::string, int>> codecMap); ==> NON ESPOSTO
	
public:
	pjsua_transport_id GetUDPTransportID() const { return udp_transport; }
	pjsua_transport_id GetTLSTransportID() const { return tls_transport; }
	pjsua_transport_id GetUDP6TransportID() const { return udp6_transport; }
	pjsua_transport_id GetTLS6TransportID() const { return tls6_transport; }

	/*! @Brief Callback for PJSIP.
	 *  Called when an incoming call comes in on an account.
	 */
	static void OnIncomingCall(pjsua_acc_id acc_id, pjsua_call_id call_id, pjsip_rx_data *rdata);

	/*! @Brief Callback for PJSIP.
	 *  Called when the media state of a call changes.
	 */
	static void OnCallMediaState(pjsua_call_id call_id);

	/*! @Brief Callback for PJSIP.
	 *  Called when the state of a call changes.
	 */
	static void OnCallState(pjsua_call_id call_id, pjsip_event *e);

	/*! @Brief Callback for PJSIP.
	*  Called when the state of a transaction within a call changes.
	*/
	static void OnCallTsxState(pjsua_call_id call_id, pjsip_transaction *tsx, pjsip_event *e);

	/*! @Brief Callback for PJSIP.
	 *  Called when the registration status of an account changes.
	 */
	static void OnRegState(pjsua_acc_id acc_id);

	/*! @Brief Callback for PJSIP.
	 *  Called if there is a change in the transport. This is usually for TLS.
	 */
	static void OnTransportState(pjsip_transport *tp, pjsip_transport_state state, const pjsip_transport_state_info *info);

#if 0	// REITEK: Disabled
	/*! @Brief Callback for PJSIP.
	 *  Called after a call is transfer to report the status.
	 */
	static void OnCallTransferStatus(pjsua_call_id call_id, int st_code, const pj_str_t *st_text, pj_bool_t final, pj_bool_t *p_cont);
#endif

private:
	BlabbleAccountMap accounts_;
	BlabbleAudioManagerPtr audio_manager_;
	pjsua_transport_id udp_transport, tls_transport, udp6_transport, tls6_transport;

	// REITEK: Disable TLS flag (TLS is handled differently)
#if 0
	bool has_tls_;
#endif

	static PjsuaManagerWeakPtr instance_;

	//PjsuaManager is a singleton. Only one should ever exist so that PjSip callbacks work.

	// REITEK: Get/parse parameters passed to the plugin upon manager creation

	PjsuaManager(Blabble& pluginCore);
};

#endif // H_PjsuaManagerPLUGIN
