#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <string>

#include "../export.h"

class PLATFORM_API AppTime {
public:
    using UtcTime = boost::posix_time::ptime;
    using LocalTime = boost::local_time::local_date_time;

    static UtcTime UTCNow();

    static LocalTime localNow();

    static LocalTime toLocal(const UtcTime& utc);

    static UtcTime toUTC(const LocalTime& local);

    static std::string toStringUTC(const UtcTime& t);

    static std::string toStringLocal(const LocalTime& t);

private:
    static boost::local_time::time_zone_ptr getTimeZone();
};
