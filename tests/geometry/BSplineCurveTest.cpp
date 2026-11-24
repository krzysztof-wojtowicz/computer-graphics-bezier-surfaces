#include "geometry/BSplineCurve.h"
#include "geometry/BezierCurve.h"
#include "math/TestUtils.h"

#include <cmath>
#include <tuple>
#include <vector>

namespace {
constexpr float kZ = 2.0f;

/// Irregular closed control polygon with 5 points
BSplineCurve wavySpline() {
    return BSplineCurve({
        {.x = 3.0f, .y = 0.0f, .z = kZ}, {.x = 2.0f, .y = 3.5f, .z = kZ}, {.x = -2.5f, .y = 2.0f, .z = kZ},
        {.x = -3.0f, .y = -2.5f, .z = kZ}, {.x = 1.5f, .y = -3.5f, .z = kZ},
    });
}

/// Index i wrapped into [0, n)
int wrap(const int i, const int n) { return ((i % n) + n) % n; }

/// One-sided numerical derivative d^order/dt^order at t: from the left (dir = -1) or from the right (dir = +1),
/// so at a join each side sees only its own segment
Vec3 sideDerivative(const ClosedCurve &c, const float t, const int order, const float dir) {
    constexpr float h = 1e-2f;
    const Vec3 p0 = c.evaluate(t), p1 = c.evaluate(t + dir * h), p2 = c.evaluate(t + 2 * dir * h);
    if (order == 1) return (dir / h) * (p1 - p0);
    return (1.0f / (h * h)) * (p2 - 2.0f * p1 + p0);
}
} // namespace

static_assert(bspline3(0.0f)[3] == 0.0f && bspline3(1.0f)[0] == 0.0f);

TEST(BSplineBasisTest, KnownValuesAtEnds) {
    const auto b0 = bspline3(0.0f);
    EXPECT_FLOAT_EQ(b0[0], 1.0f / 6.0f);
    EXPECT_FLOAT_EQ(b0[1], 4.0f / 6.0f);
    EXPECT_FLOAT_EQ(b0[2], 1.0f / 6.0f);
    EXPECT_FLOAT_EQ(b0[3], 0.0f);
    const auto b1 = bspline3(1.0f);
    EXPECT_FLOAT_EQ(b1[0], 0.0f);
    EXPECT_FLOAT_EQ(b1[1], 1.0f / 6.0f);
    EXPECT_FLOAT_EQ(b1[2], 4.0f / 6.0f);
    EXPECT_FLOAT_EQ(b1[3], 1.0f / 6.0f);
}

TEST(BSplineBasisTest, WeightsSumToOneNonNegativeAndSymmetric) {
    for (int k = 0; k <= 20; ++k) {
        const float t = static_cast<float>(k) / 20.0f;
        const auto a = bspline3(t);
        const auto b = bspline3(1.0f - t);
        EXPECT_NEAR(a[0] + a[1] + a[2] + a[3], 1.0f, kEps) << t;
        for (int i = 0; i < 4; ++i) {
            EXPECT_GE(a[i], 0.0f);
            EXPECT_NEAR(a[i], b[3 - i], kEps) << "t = " << t << ", i = " << i;
        }
    }
}

TEST(BSplineCurveTest, OneSegmentPerPoint) {
    const auto c = wavySpline();
    EXPECT_EQ(c.size(), 5);
    EXPECT_EQ(c.segments(), 5);
    EXPECT_FLOAT_EQ(c.z(), kZ);
    for (int i = 0; i < c.size(); ++i)
        EXPECT_FALSE(c.passesThrough(i));
}

TEST(BSplineCurveTest, SegmentStartIsAverageAroundItsPoint) {
    // t = i: (P[i-1] + 4 P[i] + P[i+1]) / 6
    const auto c = wavySpline();
    const int n = c.size();
    for (int i = 0; i < n; ++i) {
        const Vec3 expected = (1.0f / 6.0f) * (c.point(wrap(i - 1, n)) + 4.0f * c.point(i) + c.point(wrap(i + 1, n)));
        EXPECT_VEC3_NEAR(c.evaluate(static_cast<float>(i)), expected, 1e-5f) << i;
    }
}

TEST(BSplineCurveTest, SegmentMidpointMatchesFormula) {
    // t = 1/2: (P[i-1] + 23 P[i] + 23 P[i+1] + P[i+2]) / 48
    const auto c = wavySpline();
    const int n = c.size();
    for (int i = 0; i < n; ++i) {
        const Vec3 expected = (1.0f / 48.0f) * (c.point(wrap(i - 1, n)) + 23.0f * c.point(i)
                                                + 23.0f * c.point(wrap(i + 1, n)) + c.point(wrap(i + 2, n)));
        EXPECT_VEC3_NEAR(c.evaluate(static_cast<float>(i) + 0.5f), expected, 1e-5f) << i;
    }
}

TEST(BSplineCurveTest, EvaluateIsPeriodic) {
    const auto c = wavySpline();
    for (const float t : {0.2f, 1.7f, 4.9f}) {
        EXPECT_VEC3_NEAR(c.evaluate(t + 5.0f), c.evaluate(t), 1e-4f);
        EXPECT_VEC3_NEAR(c.evaluate(t - 10.0f), c.evaluate(t), 1e-4f);
    }
}

TEST(BSplineCurveTest, IsC2AtEveryJoinIncludingTheClosingOne) {
    // first and second derivatives from both sides of each integer t agree (up to the O(h) error of the
    // one-sided differences; a break in a derivative of this polygon is of order 1 or more)
    const auto c = wavySpline();
    for (int i = 0; i < c.segments(); ++i) {
        const auto t = static_cast<float>(i);
        for (const int order : {1, 2}) {
            const Vec3 left = sideDerivative(c, t, order, -1.0f);
            const Vec3 right = sideDerivative(c, t, order, 1.0f);
            EXPECT_GT(length(right), 1.0f) << "join " << i << ", order " << order; // not trivially zero
            EXPECT_LT(length(right - left), order == 1 ? 0.1f : 0.6f) << "join " << i << ", order " << order;
        }
    }
}

TEST(BSplineCurveTest, MovingPointChangesOnlyFourSegments) {
    // local control: P[k] pulls segments k-2 .. k+1
    const int k = 2;
    auto c = wavySpline();
    const auto before = c.sample(8);
    c.movePoint(k, c.point(k).x + 1.0f, c.point(k).y - 1.0f);
    const auto after = c.sample(8);
    const int n = c.size();
    for (int s = 0; s < n; ++s) {
        const bool affected = s == wrap(k - 2, n) || s == wrap(k - 1, n) || s == k || s == wrap(k + 1, n);
        bool changed = false;
        for (int j = 1; j < 8; ++j) // inner samples; the segment ends are shared with neighbours
            changed |= length(after[s * 8 + j] - before[s * 8 + j]) > 1e-5f;
        EXPECT_EQ(changed, affected) << "segment " << s;
    }
}

TEST(BSplineCurveTest, StaysInsideConvexHull) {
    // every point is a convex combination of 4 points: inside their bounding box
    const auto c = wavySpline();
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (const Vec3 &p : c.points()) {
        minX = std::min(minX, p.x), maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y), maxY = std::max(maxY, p.y);
    }
    for (const Vec3 &p : c.sample(16)) {
        EXPECT_GE(p.x, minX - kEps);
        EXPECT_LE(p.x, maxX + kEps);
        EXPECT_GE(p.y, minY - kEps);
        EXPECT_LE(p.y, maxY + kEps);
        EXPECT_FLOAT_EQ(p.z, kZ);
    }
}

TEST(BSplineCurveTest, FromBezierTakesItsPointsInOrder) {
    const auto bezier = BezierCurve::circle(4.0f, 5.0f);
    const auto spline = BSplineCurve::fromBezier(bezier);
    ASSERT_EQ(spline.size(), bezier.size());
    EXPECT_EQ(spline.segments(), 12);
    for (int i = 0; i < bezier.size(); ++i)
        EXPECT_VEC3_EQ(spline.point(i), bezier.point(i)) << i;
}

TEST(BSplineCurveTest, FromCircleIsRoundAndInsideIt) {
    // control points of the Bezier circle: the spline is a slightly smaller, nearly round loop
    const auto spline = BSplineCurve::fromBezier(BezierCurve::circle(4.0f, 5.0f));
    for (const Vec3 &p : spline.sample(8)) {
        const float r = std::hypot(p.x, p.y);
        EXPECT_GT(r, 3.6f);
        EXPECT_LT(r, 4.4f);
    }
}

TEST(BSplineCurveTest, SetZMovesWholeCurve) {
    auto c = wavySpline();
    c.setZ(7.0f);
    EXPECT_FLOAT_EQ(c.z(), 7.0f);
    for (const Vec3 &p : c.sample(4))
        EXPECT_FLOAT_EQ(p.z, 7.0f);
}

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(BSplineCurveDeathTest, RejectsTooFewPoints) {
    EXPECT_DEATH(BSplineCurve(std::vector<Vec3>(2)), "at least 3");
    EXPECT_DEATH(BSplineCurve(std::vector<Vec3>{}), "needs control points");
}

TEST(BSplineCurveDeathTest, RejectsPointsOutsidePlane) {
    EXPECT_DEATH(BSplineCurve({{.x = 0, .y = 0, .z = 1}, {.x = 1, .y = 0, .z = 1}, {.x = 0, .y = 1, .z = 2}}),
                 "same z value");
}
#endif
