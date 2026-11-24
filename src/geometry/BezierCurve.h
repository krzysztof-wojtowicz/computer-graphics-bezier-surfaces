#pragma once

#include <vector>

#include "geometry/ClosedCurve.h"
#include "math/Vec3.h"

/// BezierCurve holds the closed curve consisting of K Bezier segments each
/// with 2 vertices and 2 control points
class BezierCurve : public ClosedCurve {
public:
    /// BezierCurve needs 3K points for K segments
    explicit BezierCurve(const std::vector<Vec3> &points);

    // --- GETTERS ---
    [[nodiscard]] int segments() const override { return size() / 3; }

    /// Anchors (every third point) are the ends of the segments
    [[nodiscard]] bool passesThrough(const int i) const override { return i % 3 == 0; }

    // --- METHODS ---
    /// Creates the default circle consisting of 4 Bezier segments
    [[nodiscard]] static BezierCurve circle(float radius, float z);

    /// Returns BezierCurve point for the parameter t; any t is accepted and wrapped modulo K,
    /// so an animation can keep increasing it (integer part = segment, fraction = local parameter)
    [[nodiscard]] Vec3 evaluate(float t) const override;

private:
    static constexpr float kCircleHandle = 0.55228475f;
    static constexpr int kDefaultSegments = 4;
};
