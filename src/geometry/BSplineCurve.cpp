#include "geometry/BSplineCurve.h"

#include <cassert>

BSplineCurve::BSplineCurve(const std::vector<Vec3> &points) : ClosedCurve(points) {
    assert(size() >= kMinPoints && "closed cubic B-spline needs at least 3 points");
}

BSplineCurve BSplineCurve::fromBezier(const BezierCurve &bezier) {
    return BSplineCurve(bezier.points());
}

Vec3 BSplineCurve::evaluate(const float t) const {
    const auto [seg, local] = locate(t);
    const auto B = bspline3(local);

    // points seg-1 .. seg+2, wrapped: + size() keeps seg-1 non-negative
    Vec3 res;
    for (int j = 0; j < 4; ++j)
        res += B[j] * _points[(seg - 1 + j + size()) % size()];
    return res;
}
