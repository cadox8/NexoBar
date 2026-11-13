#pragma once

#include "database/Database.h"
#include "database/hospitality/Types.h"
#include "database/models/CashSession.h"
#include "database/models/Payment.h"
#include "database/models/Ticket.h"
#include "i18n/I18n.h"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <filesystem>
#include <string>
#include <vector>

class DailyCloseReceipt final {
public:
    static std::string build(Nearon::Database& database,
                             const boost::gregorian::date& date,
                             const std::string& businessName,
                             const platform::I18n& i18n,
                             const std::filesystem::path& templatePath = {});
    static std::string buildReport(Nearon::Database& database,
                                   const std::vector<nexobar::Ticket>& tickets,
                                   const std::vector<nexobar::Payment>& payments,
                                   const std::vector<nexobar::CashSession>& sessions,
                                   const std::string& period,
                                   const std::string& businessName,
                                   const platform::I18n& i18n,
                                   const std::filesystem::path& templatePath = {});
    static std::size_t close(Nearon::Database& database,
                             const boost::gregorian::date& date,
                             nexobar::UserId closedBy);
    static bool print(Nearon::Database& database,
                      const boost::gregorian::date& date,
                      const std::string& businessName,
                      const platform::I18n& i18n,
                      std::string& error,
                      const std::filesystem::path& templatePath = {});
};
