#include "geometry/BezierCurve.h"
#include "geometry/Bernstein.h"

#include <cassert>
#include <numbers>
#include <cmath>
#include <algorithm>

BezierCurve::BezierCurve(const std::vector<Vec3> &points) : ClosedCurve(points) {
    assert(_points.size() % 3 == 0 && "points count must be a positive multiple of 3");
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
    const auto [seg, local] = locate(t);
    const auto B = bernstein3(local);

    // the last segment ends at P[0]: indices wrap modulo 3K
    Vec3 res;
    for (int j = 0; j < 4; ++j)
        res += B[j] * _points[(3 * seg + j) % size()];
    return res;
}
