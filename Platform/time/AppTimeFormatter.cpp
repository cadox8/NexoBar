#include "AppTimeFormatter.h"
#include <ctime>

std::string AppTimeFormatter::ISO8601Utc(const boost::posix_time::ptime& t) {
    return FormatUTC(t);
}

std::string AppTimeFormatter::ISO8601Local(const boost::local_time::local_date_time& t) {
    return FormatLocal(t);
}

std::string AppTimeFormatter::FormatUTC(const boost::posix_time::ptime& pt) {
    return Format(pt, true);
}

std::string AppTimeFormatter::FormatLocal(const boost::local_time::local_date_time& ldt) {
    return Format(ldt.local_time(), false);
}

std::string AppTimeFormatter::Format(const boost::posix_time::ptime& pt, bool utc) {
    static const boost::gregorian::date epoch(1970, 1, 1);
    static const boost::posix_time::ptime epoch_pt(epoch);

    auto diff = pt - epoch_pt;

    int64_t micros = diff.total_microseconds();

    time_t seconds = static_cast<time_t>(micros / 1000000);
    int micro_part = static_cast<int>(micros % 1000000);

    struct tm tmv;

#ifdef _WIN32
    if (utc)
        gmtime_s(&tmv, &seconds);
    else
        gmtime_s(&tmv, &seconds); 
#else
    if (utc)
        gmtime_r(&seconds, &tmv);
    else
        gmtime_r(&seconds, &tmv);
#endif

    char buf[32];

    WriteInt(buf + 0, tmv.tm_year + 1900, 4);
    buf[4] = '-';
    WriteInt(buf + 5, tmv.tm_mon + 1, 2);
    buf[7] = '-';
    WriteInt(buf + 8, tmv.tm_mday, 2);
    buf[10] = 'T';
    WriteInt(buf + 11, tmv.tm_hour, 2);
    buf[13] = ':';
    WriteInt(buf + 14, tmv.tm_min, 2);
    buf[16] = ':';
    WriteInt(buf + 17, tmv.tm_sec, 2);
    buf[19] = '.';
    WriteInt(buf + 20, micro_part, 6);

    int end = 26;

    if (utc) {
        buf[26] = 'Z';
        end = 27;
    }

    return std::string(buf, end);
}

void AppTimeFormatter::WriteInt(char* buf, int value, int digits) {
    for (int i = digits - 1; i >= 0; --i) {
        buf[i] = '0' + (value % 10);
        value /= 10;
    }
}
