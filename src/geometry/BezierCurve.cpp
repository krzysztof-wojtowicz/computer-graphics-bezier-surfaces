#include "geometry/BezierCurve.h"
#include "geometry/Bernstein.h"

#include <cassert>
#include <numbers>
#include <cmath>
#include <algorithm>

BezierCurve::BezierCurve(const std::vector<Vec3> &points) : _points(points){
    assert(_points.size() % 3 == 0 && !_points.empty() && "points count must be a positive multiple of 3");
    assert(std::ranges::all_of(points, [this](const Vec3 &p) { return p.z == z(); }) && "all points have to have the "
                                                                                       "same z value");
}

BezierCurve BezierCurve::circle(const float radius, const float z) {
    std::vector<Vec3> points;
    points.reserve(kDefaultSegments * 3);

    constexpr float step = 2 * std::numbers::pi_v<float> / kDefaultSegments;
    const float distance = radius * kCircleHandle;

    for (int i = 0; i < kDefaultSegments; ++i) {
        const float angle = step * static_cast<float>(i);

        const Vec3 point = {
            .x = radius * std::cos(angle),
            .y = radius * std::sin(angle),
            .z = z
        };

        const Vec3 tangent = {
            .x = -std::sin(angle),
            .y = std::cos(angle),
            .z = 0
        };

        points.emplace_back(point - distance * tangent);
        points.emplace_back(point);
        points.emplace_back(point + distance * tangent);
    }

    std::ranges::rotate(points, points.begin() + 1);

    return BezierCurve(points);
}

Vec3 BezierCurve::evaluate(const float t) const {
    const int k = segments();
    const auto kf = static_cast<float>(k);

    // wrap t into [0, K): fmod keeps the sign of t
    float s = std::fmod(t, kf);
    if (s < 0) s += kf;
    // tiny negative t + K rounds to exactly K in float
    if (!(s < kf)) s = 0;

    const int seg = static_cast<int>(s);
    const auto B = bernstein3(s - static_cast<float>(seg));

    // the last segment ends at P[0]: indices wrap modulo 3K
    Vec3 res;
    for (int j = 0; j < 4; ++j)
        res += B[j] * _points[(3 * seg + j) % size()];
    return res;
}

void BezierCurve::movePoint(const int i, const float x, const float y) {
    assert(i >= 0 && i < static_cast<int>(_points.size()) && "index out of range");

    _points[i].x = x;
    _points[i].y = y;
}

void BezierCurve::setZ(const float z) {
    for (auto &p : _points)
        p.z = z;
}

std::vector<Vec3> BezierCurve::sample(const int n) const {
    assert(n >= 1 && "n has to be >= 1");

    std::vector<Vec3> result;
    result.reserve(segments() * n);

    for (int k = 0; k < segments() * n; ++k) {
        result.emplace_back(evaluate(static_cast<float>(k) / static_cast<float>(n)));
    }

    return result;
}
