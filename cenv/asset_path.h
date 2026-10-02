#pragma once

#include <cstdlib>
#include <string>

// Texture paths in the games are "assets/...". Those are relative to the
// process cwd, which is wrong after `pip install` / running from another
// directory. Python sets PROCGEN2_ASSET_ROOT to the repo (or package) root.
inline std::string procgen2_resolve_asset(const std::string &name) {
    if (name.empty())
        return name;
    if (name[0] == '/' || name[0] == '\\' || (name.size() >= 2 && name[1] == ':'))
        return name;

    const char *root = std::getenv("PROCGEN2_ASSET_ROOT");
    if (root == nullptr || root[0] == '\0')
        return name;

    std::string base(root);
    if (base.back() != '/' && base.back() != '\\')
        base.push_back('/');
    return base + name;
}
