#pragma once

#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

namespace AmnesiaHT {

struct Config {
    int   udpPort = 4242;

    float yawSens = 1.0f, pitchSens = 1.0f, rollSens = 1.0f;
    bool  invertYaw = false, invertPitch = false, invertRoll = false;
    // Smoothing is picked per connection from the packet source address: a
    // tracker on this machine (loopback) uses localSmoothing, a remote network
    // device uses remoteSmoothing. Both cover rotation and position.
    float localSmoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remoteSmoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    bool  positionEnabled = true;
    float posSensX = 1.0f, posSensY = 1.0f, posSensZ = 1.0f;
    float limitX     = cameraunlock::PositionSettings{}.limit_x;
    float limitY     = cameraunlock::PositionSettings{}.limit_y;
    float limitZ     = cameraunlock::PositionSettings{}.limit_z;
    float limitZBack = cameraunlock::PositionSettings{}.limit_z_back;
    bool  invertPosX = false, invertPosY = false, invertPosZ = false;

    int   toggleKey       = 0x23;  // End
    int   trackingModeKey = 0x21;  // Page Up

    bool  autoEnable = true;

    // Loads from an INI file. Returns false if the file does not exist (caller
    // then keeps these defaults).
    bool Load(const char* path);
};

}  // namespace AmnesiaHT
