#pragma once

#include <array>
#include <vector>

#include "geometry/ClosedCurve.h"
#include "geometry/BezierCurve.h"
#include "math/Vec3.h"

/// Weights of the 4 control points of a uniform cubic B-spline segment for parameter t in [0, 1]
constexpr std::array<float, 4> bspline3(const float t) {
    const float t1 = 1 - t;
    const float t2 = t * t;
    const float t3 = t2 * t;

    return {
        {
            t1 * t1 * t1 / 6,
            (3 * t3 - 6 * t2 + 4) / 6,
            (-3 * t3 + 3 * t2 + 3 * t + 1) / 6,
            t3 / 6
        }};
}

/// Closed uniform cubic B-spline: n control points give n segments, C2 everywhere (also where it closes).
/// Segment i is pulled by points i-1, i, i+1, i+2 (modulo n), so t = i is the point of the curve nearest to P[i].
class BSplineCurve : public ClosedCurve {
public:
    static constexpr int kMinPoints = 3;

    explicit BSplineCurve(const std::vector<Vec3> &points);

    // --- GETTERS ---
    [[nodiscard]] int segments() const override { return size(); }

    /// A B-spline doesn't pass through its control points
    [[nodiscard]] bool passesThrough(int) const override { return false; }

    // --- METHODS ---
    /// B-spline with the control points of a (piecewise cubic) Bezier curve, in the same order
    [[nodiscard]] static BSplineCurve fromBezier(const BezierCurve &bezier);

    [[nodiscard]] Vec3 evaluate(float t) const override;
};
