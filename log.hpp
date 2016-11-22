#pragma once

#include <iostream>

#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/global_logger_storage.hpp>


// These macros use the global variable lg
#define LOGDEB(msg) BOOST_LOG_SEV(general_log::logger::get(), general_log::SeverityLevel::debug)   << msg
#define LOGINF(msg) BOOST_LOG_SEV(general_log::logger::get(), general_log::SeverityLevel::info)    << msg
#define LOGWAR(msg) BOOST_LOG_SEV(general_log::logger::get(), general_log::SeverityLevel::warning) << msg
#define LOGERR(msg) BOOST_LOG_SEV(general_log::logger::get(), general_log::SeverityLevel::error)   << msg

// Note that LOGFAT inmediately exits
#define LOGFAT(msg) do {BOOST_LOG_SEV(general_log::logger::get(), general_log::SeverityLevel::fatal) << msg; exit(EXIT_FAILURE);} while(0)


namespace general_log
{
	enum class SeverityLevel
	{
		debug = 0,
		info,
		warning,
		error,
		fatal,
	};

	// Declare a global logger
	BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(logger, boost::log::sources::severity_logger_mt<general_log::SeverityLevel>);

	// Initialize the general application events log
	void init(const std::string &logfile, SeverityLevel clog_min, SeverityLevel flog_min);

	// SeverityLevel from integer
	SeverityLevel severity_level(size_t level);

	// SeverityLevel from string
	SeverityLevel severity_level(const std::string &level);

	// Pipe a SeverityLevel into an ostream
	std::ostream& operator<< (std::ostream& strm, SeverityLevel level);

	// SeverityLevel from string coming from istream
	std::istream& operator>> (std::istream& in, SeverityLevel& unit);
}
