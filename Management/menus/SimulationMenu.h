#pragma once

#include "MenuBase.h"

namespace Nearon { class Database; }

class SimulationMenu final : public MenuBase {
public:
    explicit SimulationMenu(Nearon::Database& database) : database_(database) {}
    void init() override;
    std::function<void()> on_back;
private:
    std::string seed();
    Nearon::Database& database_;
};
