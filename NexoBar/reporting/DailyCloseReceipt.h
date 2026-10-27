#pragma once

#include "database/Database.h"
#include "database/hospitality/Types.h"
#include "i18n/I18n.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <optional>
#include <string>

class DailyCloseReceipt final {
public:
    static std::string build(Nearon::Database& database,
                             const boost::gregorian::date& date,
                             const std::string& businessName,
                             const platform::I18n& i18n);
    static bool print(Nearon::Database& database,
                      const boost::gregorian::date& date,
                      const std::string& businessName,
                      const platform::I18n& i18n,
                      bool closeSessions,
                      std::optional<nexobar::UserId> closedBy,
                      std::string& error);
};
