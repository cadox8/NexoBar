#include "database/hospitality/HospitalityDatabase.h"
#include "sales/SaleService.h"

#include <gtest/gtest.h>

TEST(SaleServiceTests, PersistsNumberedTicketsLinesAndTaxesAtomically) {
  const platform::I18n i18n(NEXOBAR_TEST_LANGUAGE_DIR, "es-ES");
  const auto database = nexobar::openHospitalityDatabase(":memory:", i18n);
  const auto user = database->query<nexobar::User>().first();
  ASSERT_TRUE(user.has_value());
  nexobar::Business business;
  business.legalName = "NexoBar Tests SL";
  business.tradeName = "NexoBar Tests";
  business.nif = "B12345678";
  business.address = "Calle Test 1";
  nexobar::SaleService service(*database);
  const std::vector<nexobar::SaleItem> items = {
      {std::nullopt, "Café", 2000, 150, 1000},
      {std::nullopt, "Agua", 1000, 100, 2100}};
  const auto first = service.issue(business, user->id, items);
  const auto second = service.issue(business, user->id, items);
  EXPECT_EQ(first.ticket.fullNumber,
            "S-" +
                std::to_string(
                    static_cast<int>(first.ticket.issuedAt.date().year())) +
                "-000001");
  EXPECT_EQ(second.ticket.number, 2);
  EXPECT_EQ(first.ticket.total, 400);
  EXPECT_EQ(first.ticket.taxBase + first.ticket.taxAmount, first.ticket.total);
  EXPECT_EQ(database->count<nexobar::Ticket>(), 2);
  EXPECT_EQ(database->count<nexobar::TicketLine>(), 4);
  EXPECT_EQ(database->count<nexobar::TicketTax>(), 4);
  EXPECT_EQ(database->count<nexobar::TicketSeries>(), 1);
}
