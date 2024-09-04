/**********************************************************\
Original Author: Andrew Ofisher (zaltar)

License:    GNU General Public License, version 3.0
            http://www.gnu.org/licenses/gpl-3.0.txt

Copyright 2012 Andrew Ofisher
\**********************************************************/

#ifndef H_BlabbleLoggingPLUGIN
#define H_BlabbleLoggingPLUGIN

#include <string>

namespace BlabbleLogging {

	/**
	*	!!! NOTE: Functions declared here must be public
	*	because they are either called from the JS API or from other source files
	*/

	/*! @Brief Initialize logging
	 *
	 * If loggingAsync is true, a dedicated thread is used
	 */
	void init(bool loggingAsync);

	/**
	*	Deinitialise logging
	*/
	void deinit();

	/*! @Brief This is used to write to the log file
	 *
	 * It has this signature because it is a callback function used by PJSIP
	 * The len parameter is unused
	 */
	void blabbleLog(int level, const char* data, int len);

	/*! @Brief REITEK - Called from the JS API
	 *	Set Log File dimension
	 */
	void setLogDimension(int dimension);

	/*! @Brief REITEK - Called from the JS API
	 *  Set Log File Number
	 */
	void setLogNumber(int number);

#if 0	// REITEK: Disabled
	/*! @Brief REITEK - Called from the JS API
	 *  Get Log File dimension
	 */
	int getLogDimension();

	/*! @Brief REITEK - Called from the JS API
	*   Get Log File Number
	*/
	int getLogNumber();
#endif

	/*! @Brief REITEK - Called from the JS API
	*   Set Log Path for ALL Log Operation
	*/
	bool setLogPath(const std::string &logpath);

	/*! @Brief REITEK - Called from the JS API
	*/
	void writeLogAD(const std::string& data);

	/*! @Brief REITEK - Called from the JS API
	*/
	void logSender(const std::string& url);
}

// !!! TODO: Handle TRACE
#define BLABBLE_LOG_TRACE(what)							\
	do {												\
			BlabbleLogging::blabbleLog(0, what, 0);		\
	} while(0)

// !!! TODO: Handle DEBUG
#define BLABBLE_LOG_DEBUG(what)							\
	do {												\
			BlabbleLogging::blabbleLog(0, what, 0);		\
	} while(0)

// !!! TODO: Handle WARN
#define BLABBLE_LOG_WARN(what)							\
	do {												\
			BlabbleLogging::blabbleLog(0, what, 0);		\
	} while(0)

// !!! TODO: Handle ERROR
#define BLABBLE_LOG_ERROR(what)							\
	do {												\
			BlabbleLogging::blabbleLog(0, what, 0);		\
	} while(0)

#endif // H_BlabbleLoggingPLUGIN
