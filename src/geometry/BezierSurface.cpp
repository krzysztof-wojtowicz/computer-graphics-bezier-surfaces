#include "geometry/BezierSurface.h"

#include <cassert>

#include "geometry/Bernstein.h"

/// Returns [i][j] control point from the control grid
const Vec3 &BezierSurface::controlPoint(const int i, const int j) const {
    assert(i >= 0 && i < 4 && j >= 0 && j < 4 && "BezierSurface::controlPoint: index out of range");
    return _points[i][j];
}

/// Calculates and returns SurfacePoint for a given parameters u, v
SurfacePoint BezierSurface::evaluate(const float u, const float v) const {
    const auto Bu = bernstein3(u);
    const auto Bv = bernstein3(v);

    const auto dBu = bernstein3Deriv(u);
    const auto dBv = bernstein3Deriv(v);

    Vec3 P, Pu, Pv;

    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            const Vec3 &cp = _points[i][j];
            P += cp * Bu[i] * Bv[j];
            Pu += cp * dBu[i] * Bv[j];
            Pv += cp * Bu[i] * dBv[j];
        }

    const Vec3 c = cross(Pu, Pv);
    const float len = length(c);
    const Vec3 N = len > 1e-8f ? c / len : Vec3{.x = 0, .y = 0, .z = 1};

    return {
        .P = P,
        .Pu = Pu,
        .Pv = Pv,
        .N = N
    };
}
