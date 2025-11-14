#pragma once

#include <cstdint>

inline float FixedAngleToDegrees(int32_t angle_fixed)
{
    return angle_fixed * (360.0f / (2048.0f * 256.0f));
}

inline float FixedAngleToRadians(int ang) {
    return ang * (2.0f * PI / (2048.0f * 256.0f));
}