#include "geometry/BezierCurve.h"
#include "math/TestUtils.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <tuple>
#include <vector>

namespace {
constexpr float kRadius = 4.0f;
constexpr float kZ = 8.0f;

/// 4/3·(√2 − 1): the midpoint of each quarter lies exactly on the circle
const float kExpectedHandle = 4.0f / 3.0f * (std::sqrt(2.0f) - 1.0f);

/// Irregular closed curve with 3 segments (no symmetry, so index mistakes show up)
BezierCurve wavyCurve() {
    return BezierCurve({
        {.x = 3.0f, .y = 0.0f, .z = 2.0f}, {.x = 4.0f, .y = 2.5f, .z = 2.0f}, {.x = 1.0f, .y = 3.5f, .z = 2.0f},
        {.x = -1.5f, .y = 2.0f, .z = 2.0f}, {.x = -4.0f, .y = 1.0f, .z = 2.0f}, {.x = -3.0f, .y = -2.0f, .z = 2.0f},
        {.x = -0.5f, .y = -3.0f, .z = 2.0f}, {.x = 1.0f, .y = -4.0f, .z = 2.0f}, {.x = 3.5f, .y = -2.5f, .z = 2.0f},
    });
}

/// Control point j (0..3) of segment s, the last segment wraps to P0
const Vec3 &segmentPoint(const BezierCurve &c, const int s, const int j) {
    return c.point((3 * s + j) % c.size());
}
} // namespace

// ---------- construction ----------

TEST(BezierCurveTest, StoresPointsAndCountsSegments) {
    const auto c = wavyCurve();
    EXPECT_EQ(c.size(), 9);
    EXPECT_EQ(c.segments(), 3);
    EXPECT_EQ(c.points().size(), 9u);
    EXPECT_VEC3_EQ(c.point(4), (Vec3{.x = -4.0f, .y = 1.0f, .z = 2.0f}));
    EXPECT_FLOAT_EQ(c.z(), 2.0f);
}

TEST(BezierCurveTest, SingleSegmentLoopIsAllowed) {
    // P0, P1, P2 and back to P0
    const BezierCurve c({{.x = 0, .y = 0, .z = 1}, {.x = 3, .y = 0, .z = 1}, {.x = 0, .y = 3, .z = 1}});
    EXPECT_EQ(c.segments(), 1);
    // (P0 + 3 P1 + 3 P2 + P0) / 8
    EXPECT_VEC3_EQ(c.evaluate(0.5f), (Vec3{.x = 9.0f / 8.0f, .y = 9.0f / 8.0f, .z = 1.0f}));
}

TEST(BezierCurveTest, CircleHasFourSegmentsOnPlane) {
    const auto c = BezierCurve::circle(kRadius, kZ);
    EXPECT_EQ(c.segments(), 4);
    EXPECT_EQ(c.size(), 12);
    EXPECT_FLOAT_EQ(c.z(), kZ);
    for (const auto &p: c.points())
        EXPECT_FLOAT_EQ(p.z, kZ);
}

TEST(BezierCurveTest, CircleControlPointLayout) {
    // anchors at 0°, 90°, 180°, 270°, handles tangent (counterclockwise) at distance k·r
    const auto c = BezierCurve::circle(kRadius, kZ);
    const float h = kExpectedHandle * kRadius;
    const std::vector<Vec3> expected = {
        {.x = kRadius, .y = 0, .z = kZ}, {.x = kRadius, .y = h, .z = kZ}, {.x = h, .y = kRadius, .z = kZ},
        {.x = 0, .y = kRadius, .z = kZ}, {.x = -h, .y = kRadius, .z = kZ}, {.x = -kRadius, .y = h, .z = kZ},
        {.x = -kRadius, .y = 0, .z = kZ}, {.x = -kRadius, .y = -h, .z = kZ}, {.x = -h, .y = -kRadius, .z = kZ},
        {.x = 0, .y = -kRadius, .z = kZ}, {.x = h, .y = -kRadius, .z = kZ}, {.x = kRadius, .y = -h, .z = kZ},
    };
    for (int i = 0; i < 12; ++i)
        EXPECT_VEC3_NEAR(c.point(i), expected[static_cast<std::size_t>(i)], 1e-5f) << "i = " << i;
}

// ---------- evaluate: known values ----------

TEST(BezierCurveTest, IntegerParameterGivesAnchor) {
    const auto c = wavyCurve();
    for (int s = 0; s < c.segments(); ++s)
        EXPECT_VEC3_EQ(c.evaluate(static_cast<float>(s)), c.point(3 * s)) << "s = " << s;
}

TEST(BezierCurveTest, SegmentMidpointMatchesFormula) {
    // B(1/2) = (P0 + 3 P1 + 3 P2 + P3) / 8, also for the last segment that wraps to P0
    const auto c = wavyCurve();
    for (int s = 0; s < c.segments(); ++s) {
        const Vec3 expected = (segmentPoint(c, s, 0) + 3.0f * segmentPoint(c, s, 1) + 3.0f * segmentPoint(c, s, 2)
                               + segmentPoint(c, s, 3)) / 8.0f;
        EXPECT_VEC3_EQ(c.evaluate(static_cast<float>(s) + 0.5f), expected) << "segment " << s;
    }
}

TEST(BezierCurveTest, EvenlySpacedCollinearPointsGiveLinearMotion) {
    // segment 0: (0,0) → (3,0) with handles at thirds, so B(u) = (3u, 0)
    const BezierCurve c({
        {.x = 0, .y = 0, .z = 5}, {.x = 1, .y = 0, .z = 5}, {.x = 2, .y = 0, .z = 5},
        {.x = 3, .y = 0, .z = 5}, {.x = 3, .y = 3, .z = 5}, {.x = 0, .y = 3, .z = 5},
    });
    for (const float u: {0.0f, 0.1f, 0.25f, 0.5f, 0.8f, 0.99f})
        EXPECT_VEC3_EQ(c.evaluate(u), (Vec3{.x = 3.0f * u, .y = 0, .z = 5})) << "u = " << u;
}

TEST(BezierCurveTest, CircleQuarterPoints) {
    const auto c = BezierCurve::circle(kRadius, kZ);
    EXPECT_VEC3_EQ(c.evaluate(0.0f), (Vec3{.x = kRadius, .y = 0, .z = kZ}));
    EXPECT_VEC3_EQ(c.evaluate(1.0f), (Vec3{.x = 0, .y = kRadius, .z = kZ}));
    EXPECT_VEC3_EQ(c.evaluate(2.0f), (Vec3{.x = -kRadius, .y = 0, .z = kZ}));
    EXPECT_VEC3_EQ(c.evaluate(3.0f), (Vec3{.x = 0, .y = -kRadius, .z = kZ}));
}

TEST(BezierCurveTest, CircleQuarterMidpointIsOnCircle) {
    // the condition k was derived from: B(1/2) at 45°
    const auto c = BezierCurve::circle(kRadius, kZ);
    const float d = kRadius * std::sqrt(2.0f) / 2.0f;
    EXPECT_VEC3_NEAR(c.evaluate(0.5f), (Vec3{.x = d, .y = d, .z = kZ}), 1e-5f);
    EXPECT_VEC3_NEAR(c.evaluate(2.5f), (Vec3{.x = -d, .y = -d, .z = kZ}), 1e-5f);
}

// ---------- evaluate: properties ----------

TEST(BezierCurveTest, CircleApproximationIsAccurate) {
    // the 4-segment cubic circle deviates from the true radius by at most ~0.027%
    const auto c = BezierCurve::circle(kRadius, kZ);
    float maxError = 0;
    for (int k = 0; k <= 4000; ++k) {
        const Vec3 p = c.evaluate(static_cast<float>(k) / 1000.0f);
        maxError = std::max(maxError, std::fabs(std::hypot(p.x, p.y) - kRadius));
        EXPECT_FLOAT_EQ(p.z, kZ);
    }
    EXPECT_LT(maxError / kRadius, 3e-4f);
}

TEST(BezierCurveTest, CircleAngleGrowsWithParameter) {
    // counterclockwise, roughly 90° per segment
    const auto c = BezierCurve::circle(kRadius, kZ);
    float prev = -1.0f;
    for (int k = 0; k < 400; ++k) {
        const float t = static_cast<float>(k) / 100.0f;
        const Vec3 p = c.evaluate(t);
        float angle = std::atan2(p.y, p.x);
        if (angle < 0) angle += 2 * kPi;
        EXPECT_GT(angle, prev) << "t = " << t;
        EXPECT_NEAR(angle, t * kPi / 2, 0.02f) << "t = " << t;
        prev = angle;
    }
}

TEST(BezierCurveTest, EvaluateIsPeriodic) {
    const auto c = wavyCurve();
    const auto k = static_cast<float>(c.segments());
    for (const float t: {0.0f, 0.3f, 1.0f, 1.7f, 2.5f, 2.99f}) {
        EXPECT_VEC3_NEAR(c.evaluate(t + k), c.evaluate(t), 1e-4f) << "t = " << t;
        EXPECT_VEC3_NEAR(c.evaluate(t + 5 * k), c.evaluate(t), 1e-4f) << "t = " << t;
        EXPECT_VEC3_NEAR(c.evaluate(t - k), c.evaluate(t), 1e-4f) << "t = " << t;
        EXPECT_VEC3_NEAR(c.evaluate(t - 3 * k), c.evaluate(t), 1e-4f) << "t = " << t;
    }
}

TEST(BezierCurveTest, NegativeParameterRunsBackwardsFromStart) {
    // half a step before the start = middle of the last segment
    const auto c = wavyCurve();
    EXPECT_VEC3_EQ(c.evaluate(-0.5f), c.evaluate(2.5f));
}

TEST(BezierCurveTest, CurveIsClosed) {
    const auto c = wavyCurve();
    const auto k = static_cast<float>(c.segments());
    EXPECT_VEC3_NEAR(c.evaluate(k - 1e-4f), c.point(0), 1e-2f);
    EXPECT_VEC3_EQ(c.evaluate(k), c.point(0));
}

TEST(BezierCurveTest, TinyNegativeParameterGivesStart) {
    // fmod(-1e-9, K) + K rounds to exactly K in float, which must not select segment K
    const auto c = wavyCurve();
    EXPECT_VEC3_NEAR(c.evaluate(-1e-9f), c.point(0), 1e-4f);
    EXPECT_VEC3_NEAR(c.evaluate(-1e-30f), c.point(0), 1e-4f);
}

TEST(BezierCurveTest, NanParameterGivesStart) {
    const auto c = wavyCurve();
    EXPECT_VEC3_EQ(c.evaluate(std::nanf("")), c.point(0));
}

TEST(BezierCurveTest, CurveIsContinuousAtSegmentJoins) {
    const auto c = wavyCurve();
    constexpr float d = 1e-3f;
    for (int s = 0; s < c.segments(); ++s) {
        const auto t = static_cast<float>(s);
        EXPECT_VEC3_NEAR(c.evaluate(t - d), c.evaluate(t), 2e-2f) << "join " << s;
        EXPECT_VEC3_NEAR(c.evaluate(t + d), c.evaluate(t), 2e-2f) << "join " << s;
    }
}

TEST(BezierCurveTest, CircleIsSmoothAtSegmentJoins) {
    // handles collinear with the anchor: no kink (finite differences on both sides)
    const auto c = BezierCurve::circle(kRadius, kZ);
    constexpr float h = 1e-2f;
    for (int s = 0; s < 4; ++s) {
        const auto t = static_cast<float>(s);
        const Vec3 before = (c.evaluate(t) - c.evaluate(t - h)) / h;
        const Vec3 after = (c.evaluate(t + h) - c.evaluate(t)) / h;
        EXPECT_VEC3_NEAR(before, after, 0.15f) << "join " << s;
    }
}

TEST(BezierCurveTest, SegmentStaysInBoundingBoxOfItsControlPoints) {
    // convex hull property, checked with the axis-aligned box of the hull
    const auto c = wavyCurve();
    for (int s = 0; s < c.segments(); ++s) {
        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
        for (int j = 0; j < 4; ++j) {
            const Vec3 &p = segmentPoint(c, s, j);
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        for (int k = 0; k < 100; ++k) {
            const Vec3 p = c.evaluate(static_cast<float>(s) + static_cast<float>(k) / 100.0f);
            EXPECT_GE(p.x, minX - kEps) << "segment " << s << ", k = " << k;
            EXPECT_LE(p.x, maxX + kEps) << "segment " << s << ", k = " << k;
            EXPECT_GE(p.y, minY - kEps) << "segment " << s << ", k = " << k;
            EXPECT_LE(p.y, maxY + kEps) << "segment " << s << ", k = " << k;
            EXPECT_FLOAT_EQ(p.z, c.z());
        }
    }
}

// ---------- editing ----------

TEST(BezierCurveTest, MovePointChangesOnlyXY) {
    auto c = wavyCurve();
    c.movePoint(5, 7.0f, -8.0f);
    EXPECT_VEC3_EQ(c.point(5), (Vec3{.x = 7.0f, .y = -8.0f, .z = 2.0f}));
    EXPECT_FLOAT_EQ(c.z(), 2.0f);
}

TEST(BezierCurveTest, MovingPointAffectsOnlySegmentsThatUseIt) {
    // a handle (3s+1, 3s+2) belongs to one segment, an anchor 3s to two: segment s and the previous one
    const auto original = wavyCurve();
    const int k = original.segments();
    for (int i = 0; i < original.size(); ++i) {
        auto moved = original;
        moved.movePoint(i, original.point(i).x + 1.0f, original.point(i).y - 2.0f);

        const int own = i / 3;
        const int previous = (own + k - 1) % k;
        for (int s = 0; s < k; ++s) {
            const bool affected = s == own || (i % 3 == 0 && s == previous);
            const float t = static_cast<float>(s) + 0.5f;
            const float diff = length(moved.evaluate(t) - original.evaluate(t));
            if (affected)
                EXPECT_GT(diff, 0.1f) << "point " << i << ", segment " << s;
            else
                EXPECT_LT(diff, kEps) << "point " << i << ", segment " << s;
        }
        // whatever was moved, the curve stays closed
        EXPECT_VEC3_EQ(moved.evaluate(static_cast<float>(k)), moved.point(0)) << "point " << i;
        EXPECT_VEC3_NEAR(moved.evaluate(static_cast<float>(k) - 1e-4f), moved.point(0), 1e-2f) << "point " << i;
    }
}

TEST(BezierCurveTest, SetZMovesWholeCurveVertically) {
    const auto original = wavyCurve();
    auto c = original;
    c.setZ(-3.5f);
    EXPECT_FLOAT_EQ(c.z(), -3.5f);
    for (int i = 0; i < c.size(); ++i)
        EXPECT_VEC3_EQ(c.point(i), (Vec3{.x = original.point(i).x, .y = original.point(i).y, .z = -3.5f}));
    for (const float t: {0.2f, 1.4f, 2.9f}) {
        const Vec3 q = original.evaluate(t);
        EXPECT_VEC3_EQ(c.evaluate(t), (Vec3{.x = q.x, .y = q.y, .z = -3.5f})) << "t = " << t;
    }
}

// ---------- sample ----------

TEST(BezierCurveTest, SampleMatchesEvaluate) {
    const auto c = wavyCurve();
    for (const int n: {1, 4, 32}) {
        const auto pts = c.sample(n);
        ASSERT_EQ(pts.size(), static_cast<std::size_t>(c.segments() * n)) << "n = " << n;
        for (std::size_t k = 0; k < pts.size(); ++k)
            EXPECT_VEC3_EQ(pts[k], c.evaluate(static_cast<float>(k) / static_cast<float>(n)))
                << "n = " << n << ", k = " << k;
    }
}

TEST(BezierCurveTest, SampleWithOnePerSegmentGivesAnchors) {
    const auto c = wavyCurve();
    const auto pts = c.sample(1);
    ASSERT_EQ(pts.size(), 3u);
    for (int s = 0; s < c.segments(); ++s)
        EXPECT_VEC3_EQ(pts[static_cast<std::size_t>(s)], c.point(3 * s)) << "s = " << s;
}

TEST(BezierCurveTest, SampleDoesNotRepeatFirstPoint) {
    // closed curve: the polyline is closed by the caller, a duplicate would add a zero-length edge
    const auto c = BezierCurve::circle(kRadius, kZ);
    const auto pts = c.sample(32);
    EXPECT_GT(length(pts.back() - pts.front()), 0.1f);
    // ... and the gap back to the start is as long as every other step
    const float step = length(pts[1] - pts[0]);
    EXPECT_NEAR(length(pts.front() - pts.back()), step, 0.01f * step);
}

// ---------- invariants ----------

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(BezierCurveDeathTest, RejectsBadPointCount) {
    EXPECT_DEATH(BezierCurve(std::vector<Vec3>{}), "positive multiple of 3");
    EXPECT_DEATH(BezierCurve(std::vector<Vec3>(2)), "positive multiple of 3");
    EXPECT_DEATH(BezierCurve(std::vector<Vec3>(4)), "positive multiple of 3");
}

TEST(BezierCurveDeathTest, RejectsPointsOutsidePlane) {
    EXPECT_DEATH(BezierCurve({{.x = 0, .y = 0, .z = 1}, {.x = 1, .y = 0, .z = 1}, {.x = 0, .y = 1, .z = 2}}),
                 "same z value");
}

TEST(BezierCurveDeathTest, PointIndexOutOfRange) {
    auto c = wavyCurve();
    EXPECT_DEATH(std::ignore = c.point(-1), "index out of range");
    EXPECT_DEATH(std::ignore = c.point(9), "index out of range");
    EXPECT_DEATH(c.movePoint(-1, 0, 0), "index out of range");
    EXPECT_DEATH(c.movePoint(9, 0, 0), "index out of range");
}

TEST(BezierCurveDeathTest, SampleRejectsNonPositiveCount) {
    const auto c = wavyCurve();
    EXPECT_DEATH(std::ignore = c.sample(0), "n has to be >= 1");
    EXPECT_DEATH(std::ignore = c.sample(-3), "n has to be >= 1");
}
#endif
