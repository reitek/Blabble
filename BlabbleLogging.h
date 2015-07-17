/**********************************************************\
Original Author: Andrew Ofisher (zaltar)

License:    GNU General Public License, version 3.0
            http://www.gnu.org/licenses/gpl-3.0.txt

Copyright 2012 Andrew Ofisher
\**********************************************************/

#ifndef H_BlabbleLoggingPLUGIN
#define H_BlabbleLoggingPLUGIN

#include <string>
#include <sstream>
#include <fstream>
#include "log4cplus/logger.h"
#include "log4cplus/loggingmacros.h"

namespace BlabbleLogging {

	/*! @Brief Initilize log4cplus
	 *  Sets up log4cplus with a rolling file appender. 
	 *  Currently it is hardcoded to keep 5 files of 10MB each.
	 *
	 *  @ToDo Make this more configurable.
	 */
	void initLogging();
	
	/*! This is used by PJSIP to log via log4plus.
	 */
	void blabbleLog(int level, const char* data, int len);

	/*! @Brief Return the path to the log file
	 *  Currently the log file will be stored in the userprofile path on windows, 
	 *  directly on the C drive if that fails, or under /tmp on unix platforms.
	 *
	 *  @ToDo Make this more configureable.
	 */
	std::wstring getLogFilename();

	/*! @Brief Map between PJSIP log levels and log4cplus
	 */
	log4cplus::LogLevel mapPJSIPLogLevel(int pjsipLevel);

	/*! @Brief REITEK
	 *	Set Log File dimension
	 */
	void setLogDimension(int dimension);

	/*! @Brief REITEK
	 *  Set Log File Number
	 */
	void setLogNumber(int number);

	/*! @Brief REITEK
	 *  Get Log File dimension
	 */
	int getLogDimension();

	/*! @Brief REITEK
	*   Get Log File Number
	*/
	int getLogNumber();

	/*! @Brief REITEK
	*   Set Log Path for ALL Log Operation
	*/
	void setLogPath(const std::string &logpath);

	/*! @Brief REITEK
	 *  Create Log file for AgentDesktop
	 */
	void getLogAD();
	
	void checkLogAD();

	void checkLogSIP();

	void checkHistoricalLogAD();

	void checkHistoricalLogSIP();

	void writeLogAD(std::string data);

	void sendZip(std::string host);

	void ZipSender(std::string host);

	bool existFile(const std::string &filepath);

	std::string createZIP();

	std::string createDateLogAD();

	std::string createDateLogSIP();

	std::string createDateLogZIP();

	std::string createDateTimeString();

	std::vector<std::string> countLogAD();

	std::vector<std::string> countLogSIP();

	std::ofstream createFile(const std::string &filepath);

	extern bool logging_started;
	extern log4cplus::Logger blabble_logger;
	extern log4cplus::Logger js_logger;

}

#define BLABBLE_LOG_TRACE(what)								\
	do {													\
			if (BlabbleLogging::logging_started) {			\
				BlabbleLogging::blabbleLog(0, what, 0);		\
				/*LOG4CPLUS_TRACE(							\
					BlabbleLogging::blabble_logger, what);	\
					*/										\
						}												\
		} while(0)

#define BLABBLE_LOG_DEBUG(what)								\
	do {													\
			if (BlabbleLogging::logging_started) {			\
				BlabbleLogging::blabbleLog(0, what, 0);		\
				/*LOG4CPLUS_DEBUG(							\
					BlabbleLogging::blabble_logger, what);	\
					*/										\
						}												\
		} while(0)

#define BLABBLE_LOG_WARN(what)								\
	do {													\
			if (BlabbleLogging::logging_started) {			\
				BlabbleLogging::blabbleLog(0, what, 0);		\
				/*LOG4CPLUS_WARN(							\
					BlabbleLogging::blabble_logger, what);	\
					*/										\
						}												\
		} while(0)

#define BLABBLE_LOG_ERROR(what)								\
	do {													\
			if (BlabbleLogging::logging_started) {			\
				BlabbleLogging::blabbleLog(0, what, 0);		\
				/*LOG4CPLUS_ERROR(							\
					BlabbleLogging::blabble_logger, what);	\
					*/										\
						}												\
		} while(0)

#define BLABBLE_JS_LOG(level, what)							\
    do {													\
			BlabbleLogging::blabbleLog(0, what, 0);			\
		/*log4cplus::LogLevel log4level =						\
			BlabbleLogging::mapPJSIPLogLevel(level);		\
        if(BlabbleLogging::js_logger.isEnabledFor(log4level)) {		\
            log4cplus::tostringstream _log4cplus_buf;		\
            _log4cplus_buf << what;							\
            BlabbleLogging::js_logger.						\
				forcedLog(log4level,						\
                _log4cplus_buf.str(), __FILE__, __LINE__);*/	\
															\
        }													\
    } while (0)

#endif // H_BlabbleLoggingPLUGIN
