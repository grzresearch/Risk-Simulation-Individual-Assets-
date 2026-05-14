#pragma once

#include <set>
#include <string>
#include <vector>

namespace macro::governance {

enum class Permission {
    ReadReports,
    RunScan,
    Export,
    ManageConfig,
};

const char* to_string(Permission p);

struct Role {
    std::string name;
    std::set<Permission> permissions;
    std::set<std::string> allowed_asset_ids;  // empty = all
    std::set<std::string> allowed_event_kinds; // empty = all
};

class Rbac {
public:
    void add_role(Role r);
    bool has_permission(const std::string& role_name, Permission p) const;
    bool can_view_asset(const std::string& role_name, const std::string& asset_id) const;
    bool can_view_event(const std::string& role_name, const std::string& event_kind) const;

    static Rbac default_roles();

private:
    std::vector<Role> roles_;
    const Role* find(const std::string& name) const;
};

} // namespace macro::governance
