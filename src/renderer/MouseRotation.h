#pragma once

#include "math/Mat3.h"

inline constexpr float kRadiansPerPixel = 0.01f;

[[nodiscard]] inline Mat3 dragRotation(const float dx, const float dy, const float radiansPerPixel = kRadiansPerPixel) {
    return rotationY(dx * radiansPerPixel) * rotationX(dy * radiansPerPixel);
}

[[nodiscard]] inline Mat3 applyDrag(const Mat3 &current, const float dx, const float dy, const float radiansPerPixel =
    kRadiansPerPixel) {
    return (dragRotation(dx, dy, radiansPerPixel) * current).orthonormalized();
}
