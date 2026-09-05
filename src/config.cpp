#include "config.h"
#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/logging/file_log.h"

#include <cmath>

namespace AmnesiaHT {
namespace {

// Smoothing reaches cameraunlock::math::CalculateSmoothingFactor, which runs
// exp() on it. strtod parses "nan" and "inf" without complaint, and every
// comparison-based clamp downstream is skipped by NaN because each comparison
// against it is false, so "LocalSmoothing=nan" would otherwise poison the
// smoothed pose for the rest of the session with nothing in the log. Reject it
// here instead. Validation only, never a floor: a configured 0.0 stays 0.0.
float SanitizeSmoothing(const char* key, float value, float fallback) {
    if (!std::isfinite(value)) {
        cameraunlock::logging::Line("config: %s is not a finite number, using %.2f",
                                    key, fallback);
        return fallback;
    }
    if (value < 0.0f || value > 1.0f) {
        const float clamped = (value < 0.0f) ? 0.0f : 1.0f;
        cameraunlock::logging::Line("config: %s=%g is outside [0,1], clamped to %.2f",
                                    key, static_cast<double>(value), clamped);
        return clamped;
    }
    return value;
}

// The old value is deliberately NOT migrated into the new keys. The single
// Smoothing value carried a hidden 0.15 floor, so the number in an existing
// config does not mean what it used to: copying it across would hand a local
// user smoothing they never chose under the new semantics, and copying it into
// only one of the two keys would be a guess about which connection they were on.
void WarnRetiredSmoothingKey(const cameraunlock::IniReader& reader,
                             const char* section, const char* key) {
    if (reader.ReadString(section, key, "").empty()) return;
    cameraunlock::logging::Line(
        "Config key [%s] %s has been retired and is IGNORED. Smoothing is now two "
        "keys: LocalSmoothing (default 0, applies to a tracker on this machine) and "
        "RemoteSmoothing (default 0.15, applies to a tracker on the network). The "
        "old value is not migrated because the semantics changed - it carried a "
        "hidden 0.15 floor that no longer exists. Set the two new keys.",
        section, key);
}

}  // namespace

bool Config::Load(const char* path) {
    cameraunlock::IniReader ini;
    if (!ini.Open(path)) return false;

    udpPort = ini.ReadInt("Network", "UDPPort", udpPort);

    yawSens   = ini.ReadFloat("Sensitivity", "YawMultiplier", yawSens);
    pitchSens = ini.ReadFloat("Sensitivity", "PitchMultiplier", pitchSens);
    rollSens  = ini.ReadFloat("Sensitivity", "RollMultiplier", rollSens);
    invertYaw   = ini.ReadBool("Sensitivity", "InvertYaw", invertYaw);
    invertPitch = ini.ReadBool("Sensitivity", "InvertPitch", invertPitch);
    invertRoll  = ini.ReadBool("Sensitivity", "InvertRoll", invertRoll);
    // Each key falls back to its own default (local 0.0, remote 0.15), never to
    // a shared one: a bad RemoteSmoothing dropping to the local default would
    // leave a phone's network jitter entirely unsmoothed.
    const float localDefault  = localSmoothing;
    const float remoteDefault = remoteSmoothing;
    localSmoothing  = SanitizeSmoothing(
        "LocalSmoothing",  ini.ReadFloat("Sensitivity", "LocalSmoothing", localDefault),
        localDefault);
    remoteSmoothing = SanitizeSmoothing(
        "RemoteSmoothing", ini.ReadFloat("Sensitivity", "RemoteSmoothing", remoteDefault),
        remoteDefault);
    WarnRetiredSmoothingKey(ini, "Sensitivity", "Smoothing");
    WarnRetiredSmoothingKey(ini, "Position", "Smoothing");

    positionEnabled = ini.ReadBool("Position", "Enabled", positionEnabled);
    posSensX = ini.ReadFloat("Position", "SensitivityX", posSensX);
    posSensY = ini.ReadFloat("Position", "SensitivityY", posSensY);
    posSensZ = ini.ReadFloat("Position", "SensitivityZ", posSensZ);
    limitX     = ini.ReadFloat("Position", "LimitX", limitX);
    limitY     = ini.ReadFloat("Position", "LimitY", limitY);
    limitZ     = ini.ReadFloat("Position", "LimitZ", limitZ);
    limitZBack = ini.ReadFloat("Position", "LimitZBack", limitZBack);
    invertPosX = ini.ReadBool("Position", "InvertX", invertPosX);
    invertPosY = ini.ReadBool("Position", "InvertY", invertPosY);
    invertPosZ = ini.ReadBool("Position", "InvertZ", invertPosZ);

    toggleKey       = ini.ReadHex("Hotkeys", "ToggleKey", toggleKey);
    trackingModeKey = ini.ReadHex("Hotkeys", "TrackingModeKey", trackingModeKey);

    autoEnable = ini.ReadBool("General", "AutoEnable", autoEnable);

    return true;
}

}  // namespace AmnesiaHT
