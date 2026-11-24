#include "geometry/ClosedBezierCurve.h"
#include "geometry/BezierCurve.h"
#include "geometry/BSplineCurve.h"
#include "math/TestUtils.h"

#include <array>
#include <cmath>
#include <tuple>
#include <vector>

namespace {
constexpr float kZ = 3.0f;

/// Irregular loop V0..V5 (no symmetry, so index mistakes show up)
ClosedBezierCurve wavyLoop() {
    return ClosedBezierCurve({
        {.x = 3.0f, .y = 0.5f, .z = kZ}, {.x = 4.0f, .y = 3.0f, .z = kZ}, {.x = 0.5f, .y = 4.5f, .z = kZ},
        {.x = -3.5f, .y = 1.0f, .z = kZ}, {.x = -2.0f, .y = -3.5f, .z = kZ}, {.x = 2.5f, .y = -4.0f, .z = kZ},
    });
}

/// V0..Vn with Vn = V0
Vec3 controlPoint(const ClosedBezierCurve &c, const int i) { return c.point(i % c.size()); }

/// de Casteljau on the n + 1 points: an independent way to evaluate the curve
Vec3 deCasteljau(const ClosedBezierCurve &c, const float t) {
    const int n = c.degree();
    std::vector<Vec3> p;
    for (int i = 0; i <= n; ++i) p.push_back(controlPoint(c, i));
    for (int level = n; level > 0; --level)
        for (int i = 0; i < level; ++i)
            p[i] = (1 - t) * p[i] + t * p[i + 1];
    return p[0];
}

/// The 12 points of the default light polygon
ClosedBezierCurve circleLoop() { return ClosedBezierCurve(BezierCurve::circle(4.3f, kZ).points()); }
} // namespace

TEST(ClosedBezierCurveTest, StoresSixPointsAsOneSegment) {
    const auto c = wavyLoop();
    EXPECT_EQ(c.size(), 6);
    EXPECT_EQ(c.degree(), 6);
    EXPECT_EQ(c.segments(), 1);
    EXPECT_FLOAT_EQ(c.z(), kZ);
    EXPECT_TRUE(c.passesThrough(0));
    for (int i = 1; i < 6; ++i)
        EXPECT_FALSE(c.passesThrough(i)) << i;
}

TEST(ClosedBezierCurveTest, StartsAndEndsAtV0) {
    const auto c = wavyLoop();
    EXPECT_VEC3_EQ(c.evaluate(0.0f), c.point(0));
    EXPECT_VEC3_NEAR(c.evaluate(0.9999f), c.point(0), 1e-2f);
}

TEST(ClosedBezierCurveTest, MatchesDeCasteljau) {
    for (const auto &c : {wavyLoop(), circleLoop()}) {
        SCOPED_TRACE(c.size());
        for (int k = 0; k <= 20; ++k) {
            const float t = static_cast<float>(k) / 20.0f;
            EXPECT_VEC3_NEAR(c.evaluate(t), deCasteljau(c, t), 1e-4f) << "t = " << t;
        }
    }
}

TEST(ClosedBezierCurveTest, TwelvePointsGiveDegreeTwelve) {
    const auto c = circleLoop();
    EXPECT_EQ(c.size(), 12);
    EXPECT_EQ(c.degree(), 12);
    EXPECT_EQ(c.segments(), 1);
    EXPECT_VEC3_EQ(c.evaluate(0.0f), c.point(0));
    // midpoint: sum C(12, i) V_i / 2^12 with V12 = V0
    Vec3 expected;
    float binomial = 1.0f;
    for (int i = 0; i <= 12; ++i) {
        expected += binomial / 4096.0f * controlPoint(c, i);
        binomial = binomial * static_cast<float>(12 - i) / static_cast<float>(i + 1);
    }
    EXPECT_VEC3_NEAR(c.evaluate(0.5f), expected, 1e-4f);
}

TEST(ClosedBezierCurveTest, SamePointsAsBSplineGiveDifferentCurve) {
    // one polygon, two curves: both inside the polygon, the Bezier one only touches it at V0
    const auto points = BezierCurve::circle(4.3f, kZ).points();
    const ClosedBezierCurve bezier(points);
    const BSplineCurve spline(points);
    EXPECT_GT(length(bezier.evaluate(0.5f) - spline.evaluate(6.0f)), 0.1f);
}

TEST(ClosedBezierCurveTest, MidpointMatchesFormula) {
    // sum C(6, i) V_i / 64 with V6 = V0
    const auto c = wavyLoop();
    const std::array<float, 7> binomials{1, 6, 15, 20, 15, 6, 1};
    Vec3 expected;
    for (int i = 0; i < 7; ++i)
        expected += binomials[i] / 64.0f * controlPoint(c, i);
    EXPECT_VEC3_NEAR(c.evaluate(0.5f), expected, 1e-5f);
}

TEST(ClosedBezierCurveTest, EvaluateIsPeriodicWithPeriodOne) {
    const auto c = wavyLoop();
    for (const float t : {0.1f, 0.37f, 0.8f}) {
        EXPECT_VEC3_NEAR(c.evaluate(t + 1.0f), c.evaluate(t), 1e-4f);
        EXPECT_VEC3_NEAR(c.evaluate(t - 2.0f), c.evaluate(t), 1e-4f);
    }
}

TEST(ClosedBezierCurveTest, StaysInPlane) {
    for (const Vec3 &p : wavyLoop().sample(50))
        EXPECT_FLOAT_EQ(p.z, kZ);
}

TEST(ClosedBezierCurveTest, MovingV0MovesBothEnds) {
    auto c = wavyLoop();
    c.movePoint(0, -1.0f, 2.0f);
    EXPECT_VEC3_EQ(c.evaluate(0.0f), (Vec3{.x = -1.0f, .y = 2.0f, .z = kZ}));
    EXPECT_VEC3_NEAR(c.evaluate(0.99999f), (Vec3{.x = -1.0f, .y = 2.0f, .z = kZ}), 1e-3f);
}

TEST(ClosedBezierCurveTest, EveryPointPullsTheWholeCurve) {
    // one segment of degree 6: moving any point changes the curve everywhere inside (0, 1)
    for (int i = 0; i < 6; ++i) {
        auto c = wavyLoop();
        const Vec3 before = c.evaluate(0.3f);
        c.movePoint(i, c.point(i).x + 1.0f, c.point(i).y);
        EXPECT_GT(length(c.evaluate(0.3f) - before), 1e-3f) << "point " << i;
    }
}

TEST(ClosedBezierCurveTest, CircleLoopGoesAroundTheCentre) {
    // the angle of the curve point grows monotonically through a full turn
    const auto c = circleLoop();
    float turned = 0.0f;
    const auto samples = c.sample(64);
    for (std::size_t k = 0; k < samples.size(); ++k) {
        const Vec3 &a = samples[k];
        const Vec3 &b = samples[(k + 1) % samples.size()];
        const float step = std::atan2(a.x * b.y - a.y * b.x, a.x * b.x + a.y * b.y);
        EXPECT_GT(step, 0.0f) << k;
        turned += step;
    }
    EXPECT_NEAR(turned, 2 * std::numbers::pi_v<float>, 1e-3f);
}

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(ClosedBezierCurveDeathTest, RejectsBadPointCount) {
    EXPECT_DEATH(ClosedBezierCurve(std::vector<Vec3>(1)), "at least 2 points");
    EXPECT_DEATH(ClosedBezierCurve(std::vector<Vec3>{}), "needs control points");
}

TEST(ClosedBezierCurveDeathTest, PointIndexOutOfRange) {
    auto c = wavyLoop();
    EXPECT_DEATH(std::ignore = c.point(6), "index out of range");
    EXPECT_DEATH(c.movePoint(6, 0, 0), "index out of range");
}
#endif
