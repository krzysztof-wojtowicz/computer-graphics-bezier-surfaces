#pragma once

#include <vector>

#include "geometry/ClosedCurve.h"
#include "math/Vec3.h"

/// One closed Bezier segment on n control points: degree n, points V0..Vn with Vn = V0.
/// Only V0..V(n-1) are stored, so V0 and Vn are one point and can't be dragged apart.
/// The light path uses it on the same points as the B-spline (12 by default: degree 12).
class ClosedBezierCurve : public ClosedCurve {
public:
    static constexpr int kMinPoints = 2;

    /// Takes V0..V(n-1)
    explicit ClosedBezierCurve(const std::vector<Vec3> &points);

    // --- GETTERS ---
    [[nodiscard]] int degree() const { return size(); }
    [[nodiscard]] int segments() const override { return 1; }

    /// Only V0 = Vn lies on the curve (its start and end)
    [[nodiscard]] bool passesThrough(const int i) const override { return i == 0; }

    // --- METHODS ---
    /// Any t is accepted and wrapped modulo 1
    [[nodiscard]] Vec3 evaluate(float t) const override;
};
