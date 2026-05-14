#include "governance/rbac.h"

namespace macro::governance {

const char* to_string(Permission p) {
    switch (p) {
        case Permission::ReadReports:  return "read_reports";
        case Permission::RunScan:      return "run_scan";
        case Permission::Export:       return "export";
        case Permission::ManageConfig: return "manage_config";
    }
    return "unknown";
}

void Rbac::add_role(Role r) { roles_.push_back(std::move(r)); }

const Role* Rbac::find(const std::string& name) const {
    for (const auto& r : roles_) if (r.name == name) return &r;
    return nullptr;
}

bool Rbac::has_permission(const std::string& role_name, Permission p) const {
    auto* r = find(role_name);
    return r && r->permissions.count(p) > 0;
}

bool Rbac::can_view_asset(const std::string& role_name, const std::string& asset_id) const {
    auto* r = find(role_name);
    if (!r) return false;
    return r->allowed_asset_ids.empty() || r->allowed_asset_ids.count(asset_id) > 0;
}

bool Rbac::can_view_event(const std::string& role_name, const std::string& event_kind) const {
    auto* r = find(role_name);
    if (!r) return false;
    return r->allowed_event_kinds.empty() || r->allowed_event_kinds.count(event_kind) > 0;
}

Rbac Rbac::default_roles() {
    Rbac rb;
    rb.add_role({"viewer",
                 {Permission::ReadReports},
                 {}, {}});
    rb.add_role({"analyst",
                 {Permission::ReadReports, Permission::RunScan, Permission::Export},
                 {}, {}});
    rb.add_role({"admin",
                 {Permission::ReadReports, Permission::RunScan, Permission::Export,
                  Permission::ManageConfig},
                 {}, {}});
    return rb;
}

} // namespace macro::governance
