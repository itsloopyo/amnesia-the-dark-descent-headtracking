#pragma once

namespace AmnesiaHT {

// Single source of truth for the mod version. scripts/package-release.ps1 and
// scripts/release-nightly.ps1 parse VERSION_MAJOR/MINOR/PATCH from this file.
constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;

constexpr const char* VERSION_STRING = "0.0.0";

}  // namespace AmnesiaHT
