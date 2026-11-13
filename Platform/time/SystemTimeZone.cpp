#include "SystemTimeZone.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

std::string SystemTimeZone::getPOSIXTimeZone() {
#ifdef _WIN32

    TIME_ZONE_INFORMATION tz;
    const DWORD state = GetTimeZoneInformation(&tz);
    LONG bias = tz.Bias;
    if (state == TIME_ZONE_ID_DAYLIGHT) bias += tz.DaylightBias;
    else if (state == TIME_ZONE_ID_STANDARD) bias += tz.StandardBias;
    const int offsetMinutes = -static_cast<int>(bias);
    const int absolute = std::abs(offsetMinutes);
    // boost::local_time uses POSIX's reversed sign convention (CET-1 == UTC+1).
    return "UTC" + std::string(offsetMinutes >= 0 ? "-" : "+") +
           std::to_string(absolute / 60) + ":" +
           (absolute % 60 < 10 ? "0" : "") + std::to_string(absolute % 60);

#else

    const char* tz = std::getenv("TZ");
    if (tz && std::string(tz).find(",") != std::string::npos) {
        return std::string(tz);
    }

    FILE* f = fopen("/etc/timezone", "r");
    if (f) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), f)) {
            fclose(f);

            std::string zone(buffer);
            zone.erase(zone.find_last_not_of(" \n\r\t") + 1);

            if (zone == "Europe/Madrid")
                return "CET-1CEST,M3.5.0/2,M10.5.0/3";
        }
        fclose(f);
    }
    return "UTC";

#endif
}
