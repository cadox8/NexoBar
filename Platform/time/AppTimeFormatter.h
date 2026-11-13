#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <string>

#include "../export.h"

class PLATFORM_API AppTimeFormatter {
public:
    static std::string ISO8601Utc(const boost::posix_time::ptime& t);

    static std::string ISO8601Local(const boost::local_time::local_date_time& t);

private:
    static std::string FormatUTC(const boost::posix_time::ptime& pt);

    static std::string FormatLocal(const boost::local_time::local_date_time& ldt);

    static std::string Format(const boost::posix_time::ptime& pt, bool utc);

    static void WriteInt(char* buf, int value, int digits);
};
