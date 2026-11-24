#include "geometry/ClosedCurve.h"

#include <algorithm>
#include <cassert>
#include <cmath>

ClosedCurve::ClosedCurve(std::vector<Vec3> points) : _points(std::move(points)) {
    assert(!_points.empty() && "curve needs control points");
    assert(std::ranges::all_of(_points, [this](const Vec3 &p) { return p.z == z(); })
           && "all points have to have the same z value");
}

void ClosedCurve::movePoint(const int i, const float x, const float y) {
    assert(i >= 0 && i < size() && "index out of range");

    _points[i].x = x;
    _points[i].y = y;
}

void ClosedCurve::setZ(const float z) {
    for (auto &p: _points)
        p.z = z;
}

std::vector<Vec3> ClosedCurve::sample(const int n) const {
    assert(n >= 1 && "n has to be >= 1");

    const int count = segments() * n;
    std::vector<Vec3> result;
    result.reserve(count);

    for (int k = 0; k < count; ++k)
        result.emplace_back(evaluate(static_cast<float>(k) / static_cast<float>(n)));

    return result;
}

std::pair<int, float> ClosedCurve::locate(const float t) const {
    const auto kf = static_cast<float>(segments());

    // wrap t into [0, K): fmod keeps the sign of t
    float s = std::fmod(t, kf);
    if (s < 0) s += kf;
    // tiny negative t + K rounds to exactly K in float
    if (!(s < kf)) s = 0;

    const int seg = std::min(static_cast<int>(s), segments() - 1);
    return {seg, s - static_cast<float>(seg)};
}
