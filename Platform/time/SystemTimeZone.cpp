#include "SystemTimeZone.h"

#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

std::string SystemTimeZone::getPOSIXTimeZone() {
#ifdef _WIN32

    TIME_ZONE_INFORMATION tz;
    GetTimeZoneInformation(&tz);

    const int bias = tz.Bias;
    const int hours = -bias / 60;

    return "UTC" + std::string(hours >= 0 ? "+" : "") + std::to_string(hours);

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
