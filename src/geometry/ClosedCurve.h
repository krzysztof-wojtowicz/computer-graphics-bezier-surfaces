#pragma once

#include <cassert>
#include <utility>
#include <vector>

#include "math/Vec3.h"

/// Closed curve lying in the plane z = const, defined by its control points.
/// Base of the light paths: the viewport edits, samples and draws any of them the same way.
class ClosedCurve {
public:
    virtual ~ClosedCurve() = default;

    // --- GETTERS ---
    [[nodiscard]] float z() const { return _points.front().z; }
    [[nodiscard]] int size() const { return static_cast<int>(_points.size()); }
    [[nodiscard]] const std::vector<Vec3> &points() const { return _points; }

    [[nodiscard]] const Vec3 &point(const int i) const {
        assert(i >= 0 && i < size() && "index out of range");
        return _points[i];
    }

    /// Number of segments: the curve parameter runs over [0, segments())
    [[nodiscard]] virtual int segments() const = 0;

    /// Curve point for the parameter t; any t is accepted and wrapped modulo segments(),
    /// so an animation can keep increasing it (integer part = segment, fraction = local parameter)
    [[nodiscard]] virtual Vec3 evaluate(float t) const = 0;

    /// Whether control point i always lies on the curve (drawn differently from the points that only pull it)
    [[nodiscard]] virtual bool passesThrough(int i) const = 0;

    // --- METHODS ---
    /// Moves point i to (x, y); z stays, so the curve stays in its plane
    void movePoint(int i, float x, float y);

    /// Sets the z coordinate of all points
    void setZ(float z);

    /// Returns n * segments() points of the curve, n per segment
    [[nodiscard]] std::vector<Vec3> sample(int n) const;

protected:
    explicit ClosedCurve(std::vector<Vec3> points);
    // protected: copying through the base would slice off the kind of the curve
    ClosedCurve(const ClosedCurve &) = default;
    ClosedCurve(ClosedCurve &&) = default;
    ClosedCurve &operator=(const ClosedCurve &) = default;
    ClosedCurve &operator=(ClosedCurve &&) = default;

    /// Wraps t into [0, segments()) and splits it into the segment index and the local parameter in [0, 1)
    [[nodiscard]] std::pair<int, float> locate(float t) const;

    std::vector<Vec3> _points;
};
