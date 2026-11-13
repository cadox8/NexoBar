#pragma once

#include "../hospitality/Types.h"
#include "../Orm.h"
#include "User.h"

#include <optional>
#include <string>

namespace nexobar {
    class AuditEvent {
    public:
        AuditEventId id{};
        Timestamp at;
        std::optional<UserId> userId;
        std::string type;
        std::string entity;
        std::optional<std::int64_t> entityId;
        std::string details;
    };

    using AuditEventTable = Nearon::Table<"audit_events", AuditEvent,
        Nearon::Column<"id", &AuditEvent::id, Nearon::PrimaryKey, Nearon::AutoIncrement>,
        Nearon::Column<"at", &AuditEvent::at>,
        Nearon::Column<"user_id", &AuditEvent::userId, Nearon::ForeignKey<&User::id> >,
        Nearon::Column<"type", &AuditEvent::type>,
        Nearon::Column<"entity", &AuditEvent::entity, Nearon::Default<"''"> >,
        Nearon::Column<"entity_id", &AuditEvent::entityId>,
        Nearon::Column<"details", &AuditEvent::details, Nearon::Default<"''"> >,
        Nearon::Index<"idx_audit_events_at", &AuditEvent::at>
    >;

    NEARON_TABLE(AuditEventTable);
} // namespace nexobar
