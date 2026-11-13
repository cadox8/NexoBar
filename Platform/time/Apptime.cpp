#include "AppTime.h"
#include "AppTimeFormatter.h"
#include "SystemTimeZone.h"

using namespace boost::posix_time;
using namespace boost::local_time;

AppTime::UtcTime AppTime::UTCNow() {
	return microsec_clock::universal_time();
}

AppTime::LocalTime AppTime::localNow() {
	return toLocal(UTCNow());
}

AppTime::LocalTime AppTime::toLocal(const UtcTime& utc) {
	auto tz = getTimeZone();
	return local_date_time(utc, tz);
}

AppTime::UtcTime AppTime::toUTC(const LocalTime& local) {
	return local.utc_time();
}

std::string AppTime::toStringUTC(const UtcTime& t) {
	return AppTimeFormatter::ISO8601Utc(t);
}

std::string AppTime::toStringLocal(const LocalTime& t) {
	return AppTimeFormatter::ISO8601Local(t);
}

time_zone_ptr AppTime::getTimeZone() {
	static time_zone_ptr tz;

	if (!tz) {
		std::string posix = SystemTimeZone::getPOSIXTimeZone();
		tz.reset(new posix_time_zone(posix));
	}

	return tz;
}
