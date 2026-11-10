#pragma once

#include <vector>
#include <cassert>

#include "math/Vec3.h"

/// BezierCurve holds the closed curve consisting of K Bezier segments each
/// with 2 vertices and 2 control points
class BezierCurve {
public:
    /// BezierCurve needs 3K points for K segments
    explicit BezierCurve(const std::vector<Vec3> &points);

    // --- GETTERS ---
    [[nodiscard]] float z() const { return _points.front().z; }
    [[nodiscard]] int segments() const { return static_cast<int>(_points.size()) / 3; }
    [[nodiscard]] int size() const { return static_cast<int>(_points.size()); }
    [[nodiscard]] const std::vector<Vec3> &points() const { return _points; }

    [[nodiscard]] const Vec3 &point(const int i) const {
        assert(i >= 0 && i < static_cast<int>(_points.size()) && "index out of range");
        return _points[i];
    }

    // --- METHODS ---
    /// Creates the default circle consisting of 4 Bezier segments
    [[nodiscard]] static BezierCurve circle(float radius, float z);

    /// Returns BezierCurve point for the parameter t; any t is accepted and wrapped modulo K,
    /// so an animation can keep increasing it (integer part = segment, fraction = local parameter)
    [[nodiscard]] Vec3 evaluate(float t) const;

    /// Moves i point to the (x,y) coordinates
    void movePoint(int i, float x, float y);

    /// Set the z coordinate of all points
    void setZ(float z);

    /// Returns n*K points from BezierCurve
    [[nodiscard]] std::vector<Vec3> sample(int n) const;

private:
    static constexpr float kCircleHandle = 0.55228475f;
    static constexpr int kDefaultSegments = 4;

    std::vector<Vec3> _points;
};
