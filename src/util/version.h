#pragma once

#include <string>

namespace macro {

// Semantic version (override at build time via -DMACRO_VERSION="x.y.z").
#ifndef MACRO_VERSION
#define MACRO_VERSION "0.2.0"
#endif

#ifndef MACRO_GIT_SHA
#define MACRO_GIT_SHA "unknown"
#endif

#ifndef MACRO_BUILD_DATE
#define MACRO_BUILD_DATE __DATE__
#endif

inline std::string version()      { return MACRO_VERSION;    }
inline std::string git_sha()      { return MACRO_GIT_SHA;    }
inline std::string build_date()   { return MACRO_BUILD_DATE; }
inline std::string version_string() {
    return "macro_risk " + std::string(MACRO_VERSION) + " (" + MACRO_GIT_SHA + ", " + MACRO_BUILD_DATE + ")";
}

} // namespace macro
