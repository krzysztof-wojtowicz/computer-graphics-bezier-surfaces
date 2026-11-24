#include "geometry/ClosedBezierCurve.h"
#include "geometry/Bernstein.h"

#include <cassert>
#include <cstddef>

ClosedBezierCurve::ClosedBezierCurve(const std::vector<Vec3> &points) : ClosedCurve(points) {
    assert(size() >= kMinPoints && "closed Bezier curve needs at least 2 points");
}

Vec3 ClosedBezierCurve::evaluate(const float t) const {
    const int n = degree();
    const auto B = bernstein(n, locate(t).second); // one segment: only the local parameter matters

    // Vn = V0
    Vec3 res;
    for (int i = 0; i <= n; ++i)
        res += B[static_cast<std::size_t>(i)] * _points[static_cast<std::size_t>(i % n)];
    return res;
}
