#define CURL_STATICLIB

#include "BlabbleLogging.h"
//#include "log4cplus/config/defines.hxx"
#include "log4cplus/loglevel.h"
#include "log4cplus/layout.h"
#include "log4cplus/ndc.h"
#include "log4cplus/fileappender.h"
#include "log4cplus/hierarchy.h"
#include "utf8_tools.h"
#include "boost/thread.hpp"
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
#include "curl.h"
#include <fstream>
#include <cstdio>
//#include <time.h>

#ifdef WIN32
#include <Windows.h>
#endif

namespace BlabbleLogging {
	/*! Keep track of whether or not logging has been initilized
	 */
	bool logging_started = false;

	/*! Keep the logger around for reference
	 */
	log4cplus::Logger blabble_logger, js_logger;

	std::string extensionLOG = ".log";
	std::string extensionZIP = ".zip";
	#if defined(XP_WIN)
	std::string nameAD = "\\AgentDesktop_";
	std::string nameSIP = "\\PluginSIP_";
	std::string nameZIP = "\\npPlugin_";
	std::string folder = "\\Reitek\\Contact\\BrowserPlugin";
	std::string appdata = getenv("AppData");
	std::string filepath = appdata + folder;
	#elif defined(XP_UNIX)
	std::string nameAD = "/AgentDesktop_";
	std::string nameSIP = "/PluginSIP_";
	std::string nameZIP = "/npPlugin_";
	std::string folder = "/Reitek/Contact/BrowserPlugin";
	std::string appdata = getenv("HOME");
	std::string filepath = appdata + folder;
	#endif

	std::string filepathAD;
	std::string filepathSIP;
	std::string filepathZIP;
	
	int logDimension = 10;
	unsigned int logNumber = 3;
}

void BlabbleLogging::initLogging()
{
	if (BlabbleLogging::logging_started)
        return;
	/*
	log4cplus::Logger logger = log4cplus::Logger::getDefaultHierarchy().getRoot();
	log4cplus::SharedAppenderPtr fileAppender(new log4cplus::RollingFileAppender(BlabbleLogging::getLogFilename(),BlabbleLogging::logDimension*1024*1024,BlabbleLogging::logNumber, true));
	std::auto_ptr<log4cplus::Layout> layout(new log4cplus::PatternLayout(L"%D{%d-%m-%Y %H:%M:%S,%q} [%t] %c - %m%n"));
    fileAppender->setLayout(layout);
    logger.addAppender(fileAppender);
    
	BlabbleLogging::blabble_logger = log4cplus::Logger::getInstance(L"PluginSIP");
	BlabbleLogging::js_logger = log4cplus::Logger::getInstance(L"JavaScript");
	*/
	BlabbleLogging::getLogFilename();
    BlabbleLogging::logging_started = true;
}

std::wstring BlabbleLogging::getLogFilename()
{
	BlabbleLogging::filepathSIP = BlabbleLogging::filepath + "/PluginSIP.log";
	
	boost::filesystem::path dir(BlabbleLogging::filepath.c_str());

	if (!BlabbleLogging::existFile(BlabbleLogging::filepathSIP))
	{
		boost::filesystem::create_directories(dir);
		BlabbleLogging::createFile(BlabbleLogging::filepathSIP);

	}

	std::wstring wAppData(BlabbleLogging::filepathSIP.begin(), BlabbleLogging::filepathSIP.end());
	
	return wAppData;
}

void BlabbleLogging::blabbleLog(int level, const char* data, int len)
{
	BlabbleLogging::checkLogSIP();
	std::string str = BlabbleLogging::createDateTimeString() + " " + boost::lexical_cast<std::string>(data);
	if (BlabbleLogging::logging_started)
	{
		std::ofstream fs = BlabbleLogging::createFile(BlabbleLogging::filepathSIP);
		fs << str << std::endl;
		fs.flush();
		fs.close();
		/*
		wchar_t* wdata = new wchar_t[len + 1];
		for (int i = 0; i < len; i++) 
		{
			wdata[i] = static_cast<wchar_t>(data[i]);
		}
		wdata[len] = 0;
		LOG4CPLUS_DEBUG(log4cplus::Logger::getInstance(L"PluginSIP"), std::wstring(wdata, 0, len));
		delete[] wdata;
		*/
	}
}

log4cplus::LogLevel BlabbleLogging::mapPJSIPLogLevel(int pjsipLevel) {
	switch (pjsipLevel) {
		case 0:
			return log4cplus::FATAL_LOG_LEVEL;
		case 1:
			return log4cplus::ERROR_LOG_LEVEL;
		case 2:
			return log4cplus::WARN_LOG_LEVEL;
		case 3:
			return log4cplus::INFO_LOG_LEVEL;
		case 4:
		case 5:
			return log4cplus::DEBUG_LOG_LEVEL;
		default:
			return log4cplus::TRACE_LOG_LEVEL;
	}
}

void BlabbleLogging::setLogDimension(int dimension) 
{
	BlabbleLogging::logDimension = dimension;
}

void BlabbleLogging::setLogNumber(int number)
{
	BlabbleLogging::logNumber = number;
}

int BlabbleLogging::getLogDimension() 
{
	return BlabbleLogging::logDimension;
}

int BlabbleLogging::getLogNumber()
{
	return BlabbleLogging::logNumber;
}

void BlabbleLogging::setLogPath(const std::string &logpath) 
{
	BlabbleLogging::logging_started = false;
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Log Path Modificato: " + logpath);
	BlabbleLogging::filepath = logpath;
	BlabbleLogging::getLogAD();
	BlabbleLogging::getLogFilename();
	BlabbleLogging::initLogging();
}

std::ofstream BlabbleLogging::createFile(const std::string &filepath)
{
	return std::ofstream(filepath, std::ios::app);
}

bool BlabbleLogging::existFile(const std::string &filepath)
{
	std::fstream fs = std::fstream(filepath, std::ios::app);
	bool open = fs.is_open();
	fs.flush();
	fs.close();
	return open;
}

void BlabbleLogging::getLogAD()
{
	BlabbleLogging::filepathAD = BlabbleLogging::filepath + "/AgentDesktop.log";

	 // const char* folderPath = BlabbleLogging::folder.c_str();
	boost::filesystem::path dir(BlabbleLogging::filepath.c_str());

	if (!BlabbleLogging::existFile(BlabbleLogging::filepathAD))
	{
		boost::filesystem::create_directories(dir);
		BlabbleLogging::createFile(BlabbleLogging::filepathAD);

	}
	BlabbleLogging::checkLogAD();
	BlabbleLogging::checkLogSIP();
}

void BlabbleLogging::writeLogAD(std::string data)
{
	BlabbleLogging::checkLogAD();
	std::string str = BlabbleLogging::createDateTimeString() + data;

	std::ofstream fs = BlabbleLogging::createFile(BlabbleLogging::filepathAD);
	fs << str << std::endl;
	fs.flush();
	fs.close();
}

std::string BlabbleLogging::createDateLogAD()
{
	std::string tmp = BlabbleLogging::nameAD;
	boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
	boost::posix_time::time_facet *facet = new boost::posix_time::time_facet("%Y-%m-%d_%H%M%S");
	
	std::ostringstream stream;
	stream.imbue(std::locale(stream.getloc(), facet));
	stream << now;

	tmp.append(stream.str());

	return tmp;
}

std::string BlabbleLogging::createDateLogSIP()
{
	std::string tmp = BlabbleLogging::nameSIP;
	boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
	boost::posix_time::time_facet *facet = new boost::posix_time::time_facet("%Y-%m-%d_%H%M%S");

	std::ostringstream stream;
	stream.imbue(std::locale(stream.getloc(), facet));
	stream << now;

	tmp.append(stream.str());

	return tmp;
}

std::string BlabbleLogging::createDateLogZIP()
{
	std::string tmp = BlabbleLogging::nameZIP;
	boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
	boost::posix_time::time_facet *facet = new boost::posix_time::time_facet("%Y-%m-%d_%H%M%S");

	std::ostringstream stream;
	stream.imbue(std::locale(stream.getloc(), facet));
	stream << now;

	tmp.append(stream.str());

	return tmp;
}

std::vector<std::string> BlabbleLogging::countLogAD()
{
	const std::string path = BlabbleLogging::filepath;
	const boost::regex my_filter("^AgentDesktop_.*\\.log$");
	std::vector<std::string> all_matching_files;

	boost::filesystem::directory_iterator end_itr; // Default ctor yields past-the-end
	for (boost::filesystem::directory_iterator i(BlabbleLogging::filepath); i != end_itr; ++i)
	{
		// Skip if not a file
		if (!boost::filesystem::is_regular_file(i->status())) continue;

		boost::smatch what;
		// Skip if no match
		if (!boost::regex_match(i->path().filename().string(), my_filter)) continue;

		// File matches, store it
		all_matching_files.push_back(i->path().filename().string());
	}

	return all_matching_files;
}

std::vector<std::string> BlabbleLogging::countLogSIP()
{
	const std::string path = BlabbleLogging::filepath;
	const boost::regex my_filter("^PluginSIP_.*\\.log$");
	//const boost::regex my_filter("^PluginSIP\\.log\\.[0-9]*$");
	std::vector<std::string> all_matching_files;

	boost::filesystem::directory_iterator end_itr; // Default ctor yields past-the-end
	for (boost::filesystem::directory_iterator i(BlabbleLogging::filepath); i != end_itr; ++i)
	{
		// Skip if not a file
		if (!boost::filesystem::is_regular_file(i->status())) continue;

		boost::smatch what;
		// Skip if no match
		if (!boost::regex_match(i->path().filename().string(), my_filter)) continue;

		// File matches, store it
		all_matching_files.push_back(i->path().filename().string());
	}

	return all_matching_files;
}

void BlabbleLogging::checkLogAD()
{

	/* Controllo la presenza di un file attuale e la sua dimensione */
	if (BlabbleLogging::existFile(BlabbleLogging::filepathAD))
	{
		if (boost::filesystem::file_size(BlabbleLogging::filepathAD) >= (BlabbleLogging::logDimension * 1024 * 1024))
		{
			std::string logPath = BlabbleLogging::filepath + BlabbleLogging::createDateLogAD() + BlabbleLogging::extensionLOG;
			boost::filesystem::rename(BlabbleLogging::filepathAD, logPath);
			BlabbleLogging::createFile(BlabbleLogging::filepathAD);

			/* Check Storico */
			BlabbleLogging::checkHistoricalLogAD();
		}
	}
}

void BlabbleLogging::checkLogSIP()
{
	/* Controllo la presenza di un file attuale e la sua dimensione */
	if (BlabbleLogging::existFile(BlabbleLogging::filepathSIP))
	{
		if (boost::filesystem::file_size(BlabbleLogging::filepathSIP) >= (BlabbleLogging::logDimension * 1024 * 1024))
		{
			std::string logPath = BlabbleLogging::filepath + BlabbleLogging::createDateLogSIP() + ".log";
			BlabbleLogging::logging_started = false;
			//BlabbleLogging::blabble_logger.shutdown();
			//BlabbleLogging::js_logger.shutdown();
			boost::filesystem::rename(BlabbleLogging::filepathSIP, logPath.c_str());
			BlabbleLogging::initLogging();

			/* Check Storico */
			BlabbleLogging::checkHistoricalLogSIP();

		}
	}
}

void BlabbleLogging::checkHistoricalLogAD()
{
	/* Check Storico */
	std::vector<std::string> counter = BlabbleLogging::countLogAD();
	int count = counter.size();
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Check Historical Log AD");
	for (int i = 0; BlabbleLogging::logNumber < count; ++i)
	{
		BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Next file: " + counter.at(i));
		#if (XP_WIN)
		std::string path = BlabbleLogging::filepath + "\\" + counter.at(i);
		BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Removed: " + path);
		std::remove(path.c_str());
		#elif (XP_UNIX)
		std::string path = BlabbleLogging::filepath + "/" + counter.at(i);
		std::remove(path.c_str());
		#endif
		count--;
	}
	counter.clear();
}

void BlabbleLogging::checkHistoricalLogSIP() {
	/* Check Storico */
	std::vector<std::string> counter = BlabbleLogging::countLogSIP();
	int count = counter.size();
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Check Historical Log SIP");
	for (int i = 0; BlabbleLogging::logNumber < count; ++i)
	{
		BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Next file: " + counter.at(i));
		#if (XP_WIN)
		std::string path = BlabbleLogging::filepath + "\\" + counter.at(i);
		BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Removed: " + path);
		std::remove(path.c_str());
		#elif (XP_UNIX)
		std::string path = BlabbleLogging::filepath + "/" + counter.at(i);
		std::remove(path.c_str());
		#endif
		count--;
	}
	counter.clear();
}

std::string BlabbleLogging::createZIP()
{
	/*
	 * Creazione del nome del file zip e del path
	 */
	std::string zip = BlabbleLogging::createDateLogZIP();
	zip.append(BlabbleLogging::extensionZIP);
	BlabbleLogging::filepathZIP = BlabbleLogging::filepath + zip;
	
	/*
	 * Check file da zippare
	 */
	std::vector<std::string> logAD = BlabbleLogging::countLogAD();
	std::vector<std::string> logSIP = BlabbleLogging::countLogSIP();

	/*
	 * Force creazione nuovo file di log + rename ultimo
	 */
	std::string thisAD;
	std::string thisSIP;
	std::string logPathAD;
	std::string logPathSIP;
	
	/* Log AD */
	if (BlabbleLogging::existFile(BlabbleLogging::filepathAD))
	{
		thisAD = BlabbleLogging::createDateLogAD() + BlabbleLogging::extensionLOG;
		logPathAD = BlabbleLogging::filepath + thisAD;
		std::rename(BlabbleLogging::filepathAD.c_str(), logPathAD.c_str());
		BlabbleLogging::createFile(BlabbleLogging::filepathAD);
		//std::remove(BlabbleLogging::filepathAD.c_str()); ==> questa chiamata è inutile nella versione attuale
	}
	
	/* Log PluginSIP */
	if (BlabbleLogging::existFile(BlabbleLogging::filepathSIP))
	{
		thisSIP = BlabbleLogging::createDateLogSIP() + BlabbleLogging::extensionLOG;
		logPathSIP = BlabbleLogging::filepath + thisSIP;
		BlabbleLogging::logging_started = false;
		//BlabbleLogging::blabble_logger.shutdown();
		//BlabbleLogging::js_logger.shutdown();
		std::rename(BlabbleLogging::filepathSIP.c_str(), logPathSIP.c_str());
		BlabbleLogging::initLogging();
	}

	/*
	 * Add file da zippare
	 */
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Add File da Zippare");

	try {
		ZipFile::AddEncryptedFile(BlabbleLogging::filepathZIP, logPathAD, thisAD, std::string());
	}
	catch (std::exception& e) {
		BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] [NOTICE]: Impossibile aggiungere il file " + logPathAD + " al file " + BlabbleLogging::filepathZIP + ": " + e.what());
	}

	try {
		ZipFile::AddEncryptedFile(BlabbleLogging::filepathZIP, logPathSIP, thisSIP, std::string());
	}
	catch (std::exception& e) {
		BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] [NOTICE]: Impossibile aggiungere il file " + logPathSIP + " al file " + BlabbleLogging::filepathZIP + ": " + e.what());
	}

	int i = 0;
	while (i < logAD.size()) 
	{
		#if (XP_WIN)
		std::string thiszipAD = BlabbleLogging::filepath + "\\" + logAD[i];
		#elif (XP_UNIX)
		std::string thiszipAD = BlabbleLogging::filepath + "/" + logAD[i];
		#endif

		try {
			ZipFile::AddEncryptedFile(BlabbleLogging::filepathZIP, thiszipAD, logAD[i], std::string());
		}
		catch (std::exception& e) {
			BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] [NOTICE]: Impossibile aggiungere il file " + thiszipAD + " al file " + BlabbleLogging::filepathZIP + ": " + e.what());
		}

		i++;
	}
	i = 0;
	//il numero dei log potrebbe essere diverso
	while (i < logSIP.size()) 
	{
		#if (XP_WIN)
		std::string thiszipSIP = BlabbleLogging::filepath + "\\" + logSIP[i];
		#elif (XP_UNIX)
		std::string thiszipSIP = BlabbleLogging::filepath + "/" + logSIP[i];
		#endif

		try {
			ZipFile::AddEncryptedFile(BlabbleLogging::filepathZIP, thiszipSIP, logSIP[i], std::string());
		}
		catch (std::exception& e) {
			BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] [NOTICE]: Impossibile aggiungere il file " + thiszipSIP + " al file " + BlabbleLogging::filepathZIP + ": " + e.what());
		}

		i++;
	}

	//elimino eventuali log in sovrannumero
	BlabbleLogging::checkHistoricalLogAD();
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] CheckLogAD Terminato!");
	BlabbleLogging::checkHistoricalLogSIP();
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] CheckLogSIP Terminato!");
	
	return BlabbleLogging::filepathZIP;
}

void BlabbleLogging::ZipSender(std::string host)
{ 
	boost::thread logSender(boost::bind(BlabbleLogging::sendZip, host));
}

void BlabbleLogging::sendZip(std::string host)
{
	/* Creazione del file ZIP da uplodare e memorizzazione del path */
	std::string zip = BlabbleLogging::createZIP();
	/* Creazione dello stream per l'invio del file */
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Creazione dello stream per l'upload del file");
	FILE *zipFile = fopen(zip.c_str(), "rb");
	/* Catch e Log eventuale impossibilità apertura file */
	if (!zipFile)
	{
		BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] [ERROR] Impossibile aprire il file " + zip);
		BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] [ERROR] Impossibile eseguire l'upload del file");
		exit(0);
	}
	/* Memorizzazione della dimensione del file */
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Memorizzazione della dimensione del file");

	fseek(zipFile, 0, SEEK_END);
	unsigned long zipSize = ftell(zipFile);
	fseek(zipFile, 0, SEEK_SET);

	/* Preparazione del file da inviare */
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Preparazione del file da inviare");

	struct curl_slist *headers = NULL;

	CURL *easyhandle = curl_easy_init();
	if (easyhandle)
	{
		/* no progress meter please */
		curl_easy_setopt(easyhandle, CURLOPT_NOPROGRESS, 1L);

		/* Abilito Upload */
		curl_easy_setopt(easyhandle, CURLOPT_UPLOAD, 1L);
	
		/* PUT */
		curl_easy_setopt(easyhandle, CURLOPT_PUT, 1L);
	
		/* URL */
		curl_easy_setopt(easyhandle, CURLOPT_URL, host.c_str());
	
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

	/* Invio File */
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Upload File @ " + host);

	CURLcode result = curl_easy_perform(easyhandle);

	// Free the headers
	curl_slist_free_all(headers);

	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Upload Terminato!");

	/* Check Errori di Invio */
	if (result != CURLE_OK)
	{
		BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] [ERROR] Errore nell'upload del file: " + boost::lexical_cast<std::string>(result)+" " + boost::lexical_cast<std::string>(curl_easy_strerror(result)));
	}

	/* Cleanup */
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Cleanup");

	curl_easy_cleanup(easyhandle);

	/* Chiusura stream invio file */
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Chiusura stream upload file");

	fclose(zipFile);

	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Procedura di upload terminata!");

	/* Rimozione File Zip */
	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Rimozione File: " + zip);

	std::remove(zip.c_str());

	BlabbleLogging::writeLogAD(" [" + boost::lexical_cast<std::string>(boost::this_thread::get_id()) + "] [PLUGIN] Rimozione avvenuta con successo!");
}

std::string BlabbleLogging::createDateTimeString() 
{
	boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
	boost::posix_time::time_facet *facet = new boost::posix_time::time_facet("%d/%m/%Y - %H:%M:%S.%f");

	std::ostringstream stream;
	stream.imbue(std::locale(stream.getloc(), facet));
	stream << now;
	
	/* Taglio la frazione al millisecondo */
	return stream.str().substr(0,25);
}