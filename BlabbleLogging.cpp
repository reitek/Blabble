#define CURL_STATICLIB

#include "BlabbleLogging.h"

#include <sstream>
#include <fstream>
#include <vector>

//#include "boost/thread.hpp"
#include "boost/filesystem.hpp"
#include "boost/date_time.hpp"
#include "boost/regex.hpp"
#include "boost/filesystem/operations.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/iterator/filter_iterator.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"
#include "ZipFile.h"
#include "ZipArchive.h"
#include "ZipArchiveEntry.h"
#include "streams/memstream.h"
#include "methods/Bzip2Method.h"
#ifdef WIN32
#include "curl.h"
#else
#include <curl/curl.h>
#endif
#include <fstream>
#include <cstdio>
//#include <time.h>

#ifdef WIN32
#include <Windows.h>
#endif

#include "simple_thread_safe_queue.h"


namespace BlabbleLogging {
	/**
	*	constants (multiple threads may freely access them)
	*/

	static const std::string extensionLOG = ".log";
	static const std::string extensionZIP = ".zip";

	static const std::string logSIP = "PluginSIP" + extensionLOG;
	static const std::string logAD = "AgentDesktop" + extensionLOG;

	/**
	*	!!! NOTE: Using XP_WIN/XP_UNIX defines could be avoided
	*
	*	(See: boost::filesystem::path::preferred_separator)
	*/

#if defined(XP_WIN)
	static const std::string nameAD = "\\AgentDesktop_";
	static const std::string nameSIP = "\\PluginSIP_";
	static const std::string nameZIP = "\\npPlugin_";
	static const std::string folder = "\\Reitek\\Contact\\BrowserPlugin";
	static const std::string appdata = getenv("AppData");
#elif defined(XP_UNIX)
	static const std::string nameAD = "/AgentDesktop_";
	static const std::string nameSIP = "/PluginSIP_";
	static const std::string nameZIP = "/npPlugin_";
	static const std::string folder = "/Reitek/Contact/BrowserPlugin";
	static const std::string appdata = getenv("HOME");
#endif

	/**
	*	variables (multiple threads need to access them using a mutex)
	*/

	/*! Keep track of whether or not logging has been initialized
	 */
	static std::atomic_bool logging_initialised(false);

	/**
	*	Base logdir for all log files
	*/
	static std::string logdir = appdata + folder;

	/**
	*	Filename of SIP log file (!!! NOTE: It is set by the setFilePaths function)
	*/
	static std::string filepathSIP;

	/**
	*	Filename of AD log file (!!! NOTE: It is set by the setFilePaths function)
	*/
	static std::string filepathAD;

	/**
	*	!!! CHECK: This is an atomic type because it is directly modified by the JS API
	*	(no task within the shared queue is used to modify it)
	*/
	static std::atomic_int logDimension(10);

	/**
	*	!!! CHECK: This is an atomic type because it is directly modified by the JS API
	*	(no task within the shared queue is used to modify it)
	*/
	static std::atomic_uint logNumber(3);

	/**
	*	Set SIP and AD log file paths according to the logdir variable
	*/
	static void setFilePaths()
	{
		/**
		*	!!! NOTE: Using XP_WIN/XP_UNIX defines could be avoided
		*
		*	(See: boost::filesystem::path::preferred_separator)
		*/

#if defined(XP_WIN)
		filepathSIP = logdir + "\\" + logSIP;
		filepathAD = logdir + "\\" + logAD;
#elif defined(XP_UNIX)
		filepathSIP = logdir + "/" + logSIP;
		filepathAD = logdir + "/" + logAD;
#endif
	}

	static std::string createDateLog(const std::string& pfx)
	{
		std::string tmp = pfx;
		const boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
		const boost::posix_time::time_facet *facet = new boost::posix_time::time_facet("%Y-%m-%d_%H%M%S");

		std::ostringstream stream;
		stream.imbue(std::locale(stream.getloc(), facet));
		stream << now;

		tmp.append(stream.str());

		return tmp;
	}

	static std::string createDateLogSIP()
	{
		return createDateLog(nameSIP);
	}

	static std::string createDateLogAD()
	{
		return createDateLog(nameAD);
	}

	static std::string createDateLogZIP()
	{
		return createDateLog(nameZIP);
	}

	static std::string createDateTime() 
	{
		const boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
		const boost::posix_time::time_facet *facet = new boost::posix_time::time_facet("%d/%m/%Y - %H:%M:%S.%f");

		std::ostringstream stream;
		stream.imbue(std::locale(stream.getloc(), facet));
		stream << now;
	
		/* Taglio la frazione al millisecondo */
		return stream.str().substr(0,25);
	}

	/**
	*	Write the passed data into the SIP log file
	*
	*	Forward declaration that makes easier to use it into functions defined within the namespace
	*	before its definition
	*/
	void writeLogSIPInternal(int level, const char* data, int /* len */);

	/**
	*	Write the passed data into the AD log file
	*
	*	The suppressCheckLogAD parameter is useful to avoid checking log size
	*	and rotating logs doing logging caused by internal operations (e.g. uploading logs)
	*
	*	Forward declaration that makes easier to use it into functions defined within the namespace
	*	before its definition
	*/
	void writeLogADInternal(const std::string& data, bool suppressCheckLogAD = true);

	bool setLogPathInternal(const std::string &logpath)
	{
		/**
		*	Don't allow an empty logpath
		*/

		if (logpath.length() <= 0)
		{
			writeLogADInternal(" [PLUGIN] Log Path indicato vuoto. Attuale Log Path non modificato: (" + logdir + ")");

			return false;
		}

		/**
		*	Check if logdir is the same as logpath (XP_WIN: case insensitive)
		*/

		#if defined(XP_WIN)
		// strcasecmp is only defined by POSIX, so this one must be used instead
		if (_stricmp(logdir.c_str(), logpath.c_str()) == 0)
		{
			writeLogADInternal(" [PLUGIN] Log Path indicato identico. Attuale Log Path non modificato (" + logdir + ")");

			return false;
		}
		#elif defined(XP_UNIX)
		if (logdir.compare(logpath) == 0)
		{
			writeLogADInternal(" [PLUGIN] Log Path indicato identico. Attuale Log Path non modificato (" + logdir + ")");

			return false;
		}
		#endif

		//logging_initialised = false;

		writeLogADInternal(" [PLUGIN] Log Path modificato: (" + logpath + ")");

		/**
		*	Update logdir
		*/

		logdir = logpath;

		/**
		*	Set new file paths
		*/

		setFilePaths();

		return true;
	}

	/**
	*	Ensure creation of the directory pointed by the logdir variable 
	*/
	static void makeLogDir()
	{
		boost::filesystem::create_directories(logdir);
	}

	static bool existsFile(const std::string &filepath)
	{
		const std::fstream fs = std::fstream(filepath, std::ios::in);
		const bool open = fs.is_open();
		//fs.flush();
		// !!! CHECK: Try not to close the stream and rely on the destructor to do it
		//fs.close();
		return open;
	}

	static std::ofstream createFile(const std::string &filepath)
	{
		return std::ofstream(filepath, std::ios::app);
	}

	static std::vector<std::string> countLog(const std::string& regex)
	{
		const boost::regex my_filter(regex /*"^AgentDesktop_.*\\.log$"*/ );
		std::vector<std::string> all_matching_files;

		boost::filesystem::directory_iterator end_itr; // Default ctor yields past-the-end
		for (boost::filesystem::directory_iterator i(logdir); i != end_itr; ++i)
		{
			// Skip if not a file
			if (!boost::filesystem::is_regular_file(i->status())) continue;

			// Skip if no match
			if (!boost::regex_match(i->path().filename().string(), my_filter)) continue;

			// File matches, store it
			all_matching_files.push_back(i->path().filename().string());
		}

		return all_matching_files;
	}

	static std::vector<std::string> countLogSIP()
	{
		return countLog("^PluginSIP_.*\\.log$");
	}

	static std::vector<std::string> countLogAD()
	{
		return countLog("^AgentDesktop_.*\\.log$");
	}

	static void checkHistoricalLog(const std::vector<std::string>& logFiles)
	{
		/* Check Storico */
		int count = logFiles.size();

		for (int i = 0; logNumber < count; ++i)
		{
			writeLogADInternal(" [PLUGIN] Next file: " + logFiles.at(i));

			/**
			*	!!! NOTE: Using XP_WIN/XP_UNIX defines could be avoided
			*
			*	(See: boost::filesystem::path::preferred_separator)
			*/

#if (XP_WIN)
			std::string path = logdir + "\\" + logFiles.at(i);
#elif (XP_UNIX)
			std::string path = logdir + "/" + logFiles.at(i);
#endif

			writeLogADInternal(" [PLUGIN] Removed: " + path);

			std::remove(path.c_str());

			count--;
		}
		//counter.clear();
	}

	static void checkHistoricalLogSIP() {
		writeLogADInternal(" [PLUGIN] Check Historical Log SIP");

		checkHistoricalLog(countLogSIP());
	}

	static void checkHistoricalLogAD()
	{
		writeLogADInternal(" [PLUGIN] Check Historical Log AD");

		checkHistoricalLog(countLogAD());
	}

	static void checkLogSIP()
	{
		/*
		*	Controllo della presenza del SIP log file e della sua dimensione;
		*	se ha superato quella massima, viene rinominato e viene effettuata l'archiviazione
		*/
		if (existsFile(filepathSIP))
		{
			if (boost::filesystem::file_size(filepathSIP) >= (logDimension * 1024 * 1024))
			{
				const std::string logPath = logdir + createDateLogSIP() + extensionLOG;

				// !!! CHECK: This should be useless
				//logging_started = false;

				boost::filesystem::rename(filepathSIP, logPath.c_str());

				// !!! CHECK: This should be useless
				//initLogging();

				/* Check Storico */
				checkHistoricalLogSIP();
			}
		}
	}

	static void checkLogAD()
	{
		/*
		*	Controllo della presenza del AD log file e della sua dimensione;
		*	se ha superato quella massima, viene rinominato e viene effettuata l'archiviazione
		*/
		if (existsFile(filepathAD))
		{
			if (boost::filesystem::file_size(filepathAD) >= (logDimension * 1024 * 1024))
			{
				const std::string logPath = logdir + createDateLogAD() + extensionLOG;
				boost::filesystem::rename(filepathAD, logPath);

				/* Check Storico */
				checkHistoricalLogAD();
			}
		}
	}

	static std::string createZIP()
	{
		writeLogADInternal(" [" + boost::lexical_cast<std::string>(std::this_thread::get_id()) + "] [PLUGIN] " + "Inizio createZIP");

		/*
		 * Creazione del percorso completo del file zip
		 */

		/**
		*	!!! NOTE: Using XP_WIN/XP_UNIX defines could be avoided
		*
		*	(See: boost::filesystem::path::preferred_separator)
		*/

#if (XP_WIN)
		std::string filepathZIP = logdir + "\\" + createDateLogZIP() + extensionZIP;
#elif (XP_UNIX)
		std::string filepathZIP = logdir + "/" + createDateLogZIP() + extensionZIP;
#endif
	
		/*
		 * Costruzione dell'elenco dei file da zippare
		 */

		const std::vector<std::string>& logSIP = countLogSIP();
		const std::vector<std::string>& logAD = countLogAD();

		/*
		 * Rename del file di log corrente
		 */

		/**
		*	Use a common timestamp for all files
		*/
		std::string thisDateLog = createDateLog("");

		std::string thisSIP;
		std::string thisAD;
		std::string logPathSIP;
		std::string logPathAD;


		/* Log PluginSIP */
		if (existsFile(filepathSIP))
		{
			thisSIP = nameSIP + thisDateLog /* createDateLogSIP() */ + extensionLOG;
			logPathSIP = logdir + thisSIP;

			// !!! CHECK: This should be useless
			//logging_started = false;

			std::rename(filepathSIP.c_str(), logPathSIP.c_str());

			// !!! CHECK: This should be useless
			//initLogging();
		}

		/* Log AD */
		if (existsFile(filepathAD))
		{
			thisAD = nameAD + thisDateLog /* createDateLogAD() */ + extensionLOG;
			logPathAD = logdir + thisAD;

			std::rename(filepathAD.c_str(), logPathAD.c_str());
		}

		/*
		 * Add file da zippare
		 */

		writeLogADInternal(" [PLUGIN] Add File da Zippare");

		try {
			ZipFile::AddEncryptedFile(filepathZIP, logPathSIP, thisSIP, std::string());
		}
		catch (std::exception& e) {
			writeLogADInternal(" [PLUGIN] [NOTICE]: Impossibile aggiungere il file " + logPathSIP + " al file " + filepathZIP + ": " + e.what());
		}

		try {
			ZipFile::AddEncryptedFile(filepathZIP, logPathAD, thisAD, std::string());
		}
		catch (std::exception& e) {
			writeLogADInternal(" [PLUGIN] [NOTICE]: Impossibile aggiungere il file " + logPathAD + " al file " + filepathZIP + ": " + e.what());
		}

		int i = 0;
		while (i < logSIP.size()) 
		{
			/**
			*	!!! NOTE: Using XP_WIN/XP_UNIX defines could be avoided
			*
			*	(See: boost::filesystem::path::preferred_separator)
			*/

#if (XP_WIN)
			const std::string thiszipSIP = logdir + "\\" + logSIP[i];
#elif (XP_UNIX)
			const std::string thiszipSIP = logdir + "/" + logSIP[i];
#endif

			try {
				ZipFile::AddEncryptedFile(filepathZIP, thiszipSIP, logSIP[i], std::string());
			}
			catch (std::exception& e) {
				writeLogADInternal(" [PLUGIN] [NOTICE]: Impossibile aggiungere il file " + thiszipSIP + " al file " + filepathZIP + ": " + e.what());
			}

			i++;
		}

		i = 0;
		while (i < logAD.size()) 
		{
			/**
			*	!!! NOTE: Using XP_WIN/XP_UNIX defines could be avoided
			*
			*	(See: boost::filesystem::path::preferred_separator)
			*/

#if (XP_WIN)
			const std::string thiszipAD = logdir + "\\" + logAD[i];
#elif (XP_UNIX)
			const std::string thiszipAD = logdir + "/" + logAD[i];
#endif

			try {
				ZipFile::AddEncryptedFile(filepathZIP, thiszipAD, logAD[i], std::string());
			}
			catch (std::exception& e) {
				writeLogADInternal(" [PLUGIN] [NOTICE]: Impossibile aggiungere il file " + thiszipAD + " al file " + filepathZIP + ": " + e.what());
			}

			i++;
		}

		//elimino eventuali log in sovrannumero

		checkHistoricalLogSIP();
		checkHistoricalLogAD();

		writeLogADInternal(" [" + boost::lexical_cast<std::string>(std::this_thread::get_id()) + "] [PLUGIN] " + "Fine createZIP");

		return filepathZIP;
	}

	static void sendZip(const std::string url)
	{
		writeLogADInternal(" [" + boost::lexical_cast<std::string>(std::this_thread::get_id()) + "] [PLUGIN] " + "Inizio sendZip");

		writeLogADInternal(" [PLUGIN] Upload URL: " + url);

		/* Creazione del file ZIP da uplodare e memorizzazione del path */
		const std::string zip = createZIP();

		/* Creazione dello stream per l'invio del file */
		writeLogADInternal(" [PLUGIN] Creazione dello stream per l'upload del file");

		FILE *zipFile = fopen(zip.c_str(), "rb");

		/* Catch e Log eventuale impossibilità apertura file */
		if (zipFile == NULL)
		{
			writeLogADInternal(" [PLUGIN] [ERROR] Impossibile aprire il file " + zip);

			writeLogADInternal(" [" + boost::lexical_cast<std::string>(std::this_thread::get_id()) + "] [PLUGIN] " + "Fine sendZip");

			return;
		}

		/* Memorizzazione della dimensione del file */
		writeLogADInternal(" [PLUGIN] Memorizzazione della dimensione del file");

		fseek(zipFile, 0, SEEK_END);
		const unsigned long zipSize = ftell(zipFile);
		fseek(zipFile, 0, SEEK_SET);

		/* Preparazione del file da inviare */
		writeLogADInternal(" [PLUGIN] Preparazione del file da inviare");

		struct curl_slist *headers = NULL;

		CURL *easyhandle = curl_easy_init();
		if (easyhandle != NULL)
		{
			/* no progress meter please */
			curl_easy_setopt(easyhandle, CURLOPT_NOPROGRESS, 1L);

			/* Abilito Upload */
			curl_easy_setopt(easyhandle, CURLOPT_UPLOAD, 1L);
	
			/* PUT */
			curl_easy_setopt(easyhandle, CURLOPT_PUT, 1L);
	
			/* URL */
			curl_easy_setopt(easyhandle, CURLOPT_URL, url.c_str());
	
			/* Set Content Type Header */
			headers = curl_slist_append(headers, "Content-Type: application/zip");

			curl_easy_setopt(easyhandle, CURLOPT_HTTPHEADER, headers);

			/* Inserisco puntatore a file */
			curl_easy_setopt(easyhandle, CURLOPT_INFILE, zipFile);

			/* Indico dimensione del file */
			curl_easy_setopt(easyhandle, CURLOPT_INFILESIZE_LARGE, (curl_off_t)zipSize);

			/* Disable SSL certificates checking */
			curl_easy_setopt(easyhandle, CURLOPT_SSL_VERIFYPEER, 0L);
		}
		else
		{
			writeLogADInternal(" [PLUGIN] [ERROR] curl_easy_init fallita");

			fclose(zipFile);
			std::remove(zip.c_str());

			writeLogADInternal(" [" + boost::lexical_cast<std::string>(std::this_thread::get_id()) + "] [PLUGIN] " + "Fine sendZip");

			return;
		}

		/* Invio File */
		writeLogADInternal(" [PLUGIN] Upload File @ " + url);

		CURLcode result = curl_easy_perform(easyhandle);

		// Free the headers
		curl_slist_free_all(headers);

		/* Check Errori di Invio */
		if (result != CURLE_OK)
		{
			writeLogADInternal(" [PLUGIN] [ERROR] Errore nell'upload del file: " + boost::lexical_cast<std::string>(result)+" " + boost::lexical_cast<std::string>(curl_easy_strerror(result)));
		}
		else
		{
			writeLogADInternal(" [PLUGIN] Upload Terminato");
		}

		/* Cleanup */

		curl_easy_cleanup(easyhandle);

		/* Chiusura stream invio file */
		//writeLogADInternal(" [PLUGIN] Chiusura stream upload file");

		fclose(zipFile);

		//writeLogADInternal(" [PLUGIN] Procedura di upload terminata");

		/* Rimozione File Zip */
		writeLogADInternal(" [PLUGIN] Rimozione File: " + zip);

		std::remove(zip.c_str());

		//writeLogADInternal(" [PLUGIN] Rimozione avvenuta con successo");

		writeLogADInternal(" [" + boost::lexical_cast<std::string>(std::this_thread::get_id()) + "] [PLUGIN] " + "Fine sendZip");
	}

	/**
	*	Abstract base class for all tasks that must be processed by the LogHandler
	*/

	class LogHandler;

	class LogTask
	{
	public:
		enum Type {
			exit,
			//logSender,
			//setLogDimension,
			//setLogNumber,
			setLogPath,
			writeLogAD,
			writeLogSIP
		};

		LogTask(Type type)
			: type_(type)
		{
		}

		LogTask(Type type, const std::string& data)
			: type_(type), strdata_(data)
		{
		}

		LogTask(Type type, int data)
			: type_(type), intdata_(data)
		{
		}

		std::string getTypeStr()
		{
			switch (getType())
			{
			case exit:
				return "exit";
			//case logSender:
			//	return "logSender";
			//case setLogDimension:
			//	return "setLogDimension";
			//case setLogNumber:
			//	return "setLogNumber";
			case setLogPath:
				return "setLogPath";
			case writeLogAD:
				return "writeLogAD";
			case writeLogSIP:
				return "writeLogSIP";
			default:
				return "???";
			}
		}

		Type getType() const { return type_; }
		const std::string& getStrData() const { return strdata_; }
		int getIntData() const { return intdata_; }

	private:
		Type type_;
		std::string strdata_;
		int intdata_;
	};

	using LogTaskPtr = std::unique_ptr<LogTask>;
	using LogTaskQueue = util::SimpleThreadSafeQueue<LogTaskPtr>;

	/**
	*	Class that implements a thread dedicated to handle operations related to the log files
	*	in order to not block PJSIP/JS ones
	*/
	class LogHandler
	{
	public:
		LogHandler()
		{
			//writeLogSIPInternal(0, "LogHandler::LogHandler", 0);
		}

		virtual ~LogHandler()
		{
			//writeLogSIPInternal(0, "LogHandler::~LogHandler", 0);

			Stop();
		}

		void PushTask(LogTask * logTask)
		{
			if (logTask == nullptr)
			{
				//writeLogSIPInternal(0, "Could not queue a null LogTask", 0);
				return;
			}

			//{
			//	const std::string str = std::string("Queue a LogTask type ") + logTask->getTypeStr();
			//	writeLogSIPInternal(0, str.c_str(), 0);
			//}

			logtaskqueue_.push(LogTaskPtr(logTask));
		}

		// Start the thread
		bool Start()
		{
			//writeLogSIPInternal(0, "LogHandler::Start", 0);

			// This method is not re-entryable.
			std::lock_guard<std::mutex> lock(general_mutex_);

			if (thread_) {
				return false;
			}

			// Starts a new thread.
			stop_flag_.store(false);
			done_flag_.store(false);

			thread_.reset(new std::thread(
				// All parameters are passed by value to the new thread
				[ this ] {
				LogHandlerThread();
			}));

			return true;
		}

		// Test whether the thread is still running
		bool IsRunning()
		{
			// Methods are not re-entryable.
			std::lock_guard<std::mutex> lock(general_mutex_);

			// If the thread is stopped, join the thread and destroy the thread object.
			if (done_flag_.load() && thread_) {
				thread_->join();
				thread_.reset(nullptr);
			}

			if (thread_) {
				return true;
			}
			return false;
		}

		// Stop the thread and wait for its termination
		void Stop()
		{
			//writeLogSIPInternal(0, "LogHandler::Stop", 0);

			// Methods are not re-entryable.
			std::lock_guard<std::mutex> lock(general_mutex_);

			/**
			*	If the thread is still running, set the stop flag,
			*	queue an "exit" task, and wait for it to stop.
			*/
			if (thread_) {
				stop_flag_.store(true);

				PushTask(new LogTask(LogTask::exit));

				thread_->join();
				thread_.reset(nullptr);
			}
		}

	private:
		// Private function that runs in a separate thread
		void LogHandlerThread()
		{
			//writeLogSIPInternal(0, "LogHandlerThread begin", 0);

			do {
				LogTaskPtr task = logtaskqueue_.blocking_pop();

				if (task.get() == nullptr)
				{
					//writeLogSIPInternal(0, "Got null LogTask", 0);

					continue;
				}

				//{
				//	const std::string str = "Got LogTask type " + task->getTypeStr();

				//	writeLogSIPInternal(0, str.c_str(), 0);
				//}

				switch (task->getType())
				{
				case LogTask::exit:
					//writeLogSIPInternal(0, "Handling LogTask exit", 0);

					done_flag_.store(true);

					//writeLogSIPInternal(0, "LogHandlerThread end", 0);

					return;
				break;
				case LogTask::setLogPath:
					setLogPathInternal(task->getStrData());
				break;
				case LogTask::writeLogAD:
					// !!! NOTE: Don't suppress CheckLogAD here !!!
					writeLogADInternal(task->getStrData(), false);
				break;
				case LogTask::writeLogSIP:
					writeLogSIPInternal(0, task->getStrData().c_str(), 0);
				break;
				default:
				break;
				}
			} while (!stop_flag_.load());

			//writeLogSIPInternal(0, "LogHandlerThread end", 0);

			done_flag_.store(true);
		}

		/**
		*	Mutex to serialize access to the thread_ member
		*/
		std::mutex general_mutex_;

		/**
		*	Flag to signal to the thread to stop its execution
		*/
		std::atomic_bool stop_flag_ { false };

		/**
		*	Flag to signal that the thread actually stopped its excution
		*/
		std::atomic_bool done_flag_ { false };

		std::unique_ptr<std::thread> thread_;

		LogTaskQueue logtaskqueue_;
	};

	static std::unique_ptr<LogHandler> loghandler_;

	/**
	*	Mutex to serialize access to the loghandler_ variable above
	*/
	std::mutex loghandler_mutex_;

	/**
	*	Mutex to serialize access when writing to the logSIP file
	*/
	std::mutex logSIP_mutex_;

	/**
	*	Mutex to serialize access when writing to the logAD file
	*/
	std::mutex logAD_mutex_;
}

/*! @Brief Initialize logging
	*
	* If loggingAsync is true, a dedicated thread is used
	*/
void BlabbleLogging::init(bool loggingAsync)
{
	if (logging_initialised.load())
		return;

	/**
	*	Set default file paths
	*/

	setFilePaths();

	//getLogFilename();

	//writeLogSIPInternal(0, "Init logging", 0);

	if (loggingAsync)
	{
		// Handle loggingAsync init

		LogHandler * loghandler = new LogHandler();

		if ((loghandler != nullptr) && loghandler->Start())
		{
			// Use a mutex to handle a possible race condition while accessing the loghandler
			std::lock_guard<std::mutex> lock(loghandler_mutex_);
			loghandler_.reset(loghandler);
		}
	}

	logging_initialised.store(true);
}

/**
*	Deinitialise logging
*/
void BlabbleLogging::deinit()
{
	if (!logging_initialised.load())
		return;

	{
		// Use a mutex to handle a possible race condition while accessing the loghandler
		std::lock_guard<std::mutex> lock(loghandler_mutex_);
		loghandler_.reset(nullptr);
	}

	//writeLogSIPInternal(0, "Deinit logging", 0);

	logging_initialised.store(false);
}

/*! @Brief This is used to write to the log file
 *
 * It has this signature because it is a callback function used by PJSIP
 * The len parameter is unused
 */
void BlabbleLogging::blabbleLog(int level, const char* data, int len)
{
	if (!logging_initialised.load())
		return;

	// Use a mutex to handle a possible race condition while accessing the loghandler
	std::lock_guard<std::mutex> lock(loghandler_mutex_);
	LogHandler * loghandler = loghandler_.get();
	if ((loghandler == nullptr) || !loghandler->IsRunning())
	{
		writeLogSIPInternal(level, data, len);
		return;
	}

	loghandler->PushTask(new LogTask(LogTask::writeLogSIP, std::string(data)));
}

/**
*	Write the passed data into the SIP log file
*/
void BlabbleLogging::writeLogSIPInternal(int level, const char* data, int /* len */)
{
	// !!! CHECK: Don't do this for internal debugging logs
	checkLogSIP();

	makeLogDir();

	const std::string str = createDateTime() + " " + boost::lexical_cast<std::string>(data);
	// !!! CHECK: Only one thread at a time must do this !!!
	std::lock_guard<std::mutex> lock(logSIP_mutex_);
	std::ofstream fs = createFile(filepathSIP);
	fs << str << std::endl;
	fs.flush();
	fs.close();
}

void BlabbleLogging::setLogDimension(int dimension)
{
	logDimension = dimension;
}

void BlabbleLogging::setLogNumber(int number)
{
	logNumber = number;
}

#if 0	// REITEK: Disabled
int BlabbleLogging::getLogDimension()
{
	return logDimension;
}

int BlabbleLogging::getLogNumber()
{
	return logNumber;
}
#endif

bool BlabbleLogging::setLogPath(const std::string &logpath)
{
	if (!logging_initialised.load())
		return false;

	// Use a mutex to handle a possible race condition while accessing the loghandler
	std::lock_guard<std::mutex> lock(loghandler_mutex_);
	LogHandler * loghandler = loghandler_.get();
	if ((loghandler == NULL) || !loghandler->IsRunning())
	{
		return setLogPathInternal(logpath);
	}

	loghandler->PushTask(new LogTask(LogTask::setLogPath, logpath));

	return true;
}

void BlabbleLogging::writeLogADInternal(const std::string& data, bool suppressCheckLogAD)
{
	if (!suppressCheckLogAD)
	{
		checkLogAD();
	}

	makeLogDir();

	const std::string str = createDateTime() + data;
	// !!! CHECK: Only one thread at a time must do this !!!
	std::lock_guard<std::mutex> lock(logAD_mutex_);
	std::ofstream fs = createFile(filepathAD);
	fs << str << std::endl;
	fs.flush();
	fs.close();
}

void BlabbleLogging::writeLogAD(const std::string& data)
{
	if (!logging_initialised.load())
		return;

	// Use a mutex to handle a possible race condition while accessing the loghandler
	std::lock_guard<std::mutex> lock(loghandler_mutex_);
	LogHandler * loghandler = loghandler_.get();
	if ((loghandler == NULL) || !loghandler->IsRunning())
	{
		// !!! NOTE: Don't suppress CheckLogAD here !!!
		writeLogADInternal(data, false);
		return;
	}

	loghandler->PushTask(new LogTask(LogTask::writeLogAD, data));
}

void BlabbleLogging::logSender(const std::string& url)
{
	// !!! TODO: The logSender thread should be able to also run on Linux, it must be tested !!!
	// !!! NOTE: Allow running the logSender thread on Win32 (somehow the crash is avoided with the trick of using static runtime libraries
#ifndef WIN32
	return;
#endif

	std::thread logSender(std::bind(sendZip, url));
	logSender.detach();
}
