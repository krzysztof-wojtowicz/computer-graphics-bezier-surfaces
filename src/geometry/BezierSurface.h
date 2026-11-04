#pragma once

#include <array>

#include "math/Vec3.h"

/// Represent Bezier surface point P with tangent vectors Pu, Pv and normal vector N
struct SurfacePoint {
    Vec3 P;
    Vec3 Pu;
    Vec3 Pv;
    Vec3 N;
};

class BezierSurface {
public:
    using ControlGrid = std::array<std::array<Vec3, 4>, 4>;

    constexpr explicit BezierSurface(const ControlGrid &points) : _points(points) {}

    [[nodiscard]] const Vec3 &controlPoint(int i, int j) const;

    [[nodiscard]] SurfacePoint evaluate(float u, float v) const;

private:
    ControlGrid _points;
};
