#pragma once
#include "database/hospitality/HospitalityDatabase.h"
#include "printer/ReceiptLayout.h"
#include <string>
#include <vector>
#include <map>

class TicketTemplate {
public:
    struct PrintResult {
        std::string rendered;
        bool printed = false;
        std::string error;
    };

    struct TemplateData {
        std::map<std::string, std::string> values;
        std::map<std::string, std::vector<TemplateData> > sections;
    };

    // Generic placeholders use {{key}}. Repeating sections use {{#each rows}}...{{/each}}.
    static std::string render(std::string source, const TemplateData& data);

    static std::string renderFile(const std::string& path,
                                  const nexobar::Ticket& invoice,
                                  const std::vector<nexobar::TicketLine>& lines,
                                  const std::string& businessName, const platform::I18n& i18n,
                                  const std::string& footer = {},
                                  const ReceiptLayout& layout = ReceiptLayout{},
                                  const TemplateData& custom = {});

    // The template owns the render -> log -> print workflow. Device diagnostics remain separate.
    static PrintResult printFile(const std::string& path,
                                 const nexobar::Ticket& invoice,
                                 const std::vector<nexobar::TicketLine>& lines,
                                 const std::string& businessName, const platform::I18n& i18n,
                                 const std::string& footer = {},
                                 const ReceiptLayout& layout = ReceiptLayout{},
                                 const TemplateData& custom = {});

    static std::string renderDefault(const nexobar::Ticket& invoice,
                                     const std::vector<nexobar::TicketLine>& lines,
                                     const std::string& businessName, const platform::I18n& i18n,
                                     const std::string& footer = {},
                                     const ReceiptLayout& layout = ReceiptLayout{},
                                     const TemplateData& custom = {});
};
