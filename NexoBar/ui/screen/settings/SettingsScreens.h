#pragma once

#include "SettingsWorkspaceScreen.h"

class UsersSettingsScreen final : public SettingsWorkspaceScreen {
public: explicit UsersSettingsScreen(std::function<void()> back) : SettingsWorkspaceScreen(Kind::Users, std::move(back)) {}
};
class RolesSettingsScreen final : public SettingsWorkspaceScreen {
public: explicit RolesSettingsScreen(std::function<void()> back) : SettingsWorkspaceScreen(Kind::Roles, std::move(back)) {}
};
class BusinessSettingsScreen final : public SettingsWorkspaceScreen {
public: explicit BusinessSettingsScreen(std::function<void()> back) : SettingsWorkspaceScreen(Kind::Business, std::move(back)) {}
};
class TicketTemplateSettingsScreen final : public SettingsWorkspaceScreen {
public: explicit TicketTemplateSettingsScreen(std::function<void()> back) : SettingsWorkspaceScreen(Kind::Template, std::move(back)) {}
};
class SystemStatusSettingsScreen final : public SettingsWorkspaceScreen {
public: explicit SystemStatusSettingsScreen(std::function<void()> back) : SettingsWorkspaceScreen(Kind::Status, std::move(back)) {}
};
class BillingHistorySettingsScreen final : public SettingsWorkspaceScreen {
public: explicit BillingHistorySettingsScreen(std::function<void()> back) : SettingsWorkspaceScreen(Kind::History, std::move(back)) {}
};
