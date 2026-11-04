#include "geometry/Bernstein.h"
#include "geometry/BezierSurface.h"
#include "math/TestUtils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using Grid = BezierSurface::ControlGrid;
using Heights = std::array<std::array<float, 4>, 4>;

/// x, y coordinate of the i-th control point on the regular grid -1, -1/3, 1/3, 1
float gridCoord(const int i) { return -1.0f + 2.0f * static_cast<float>(i) / 3.0f; }

/// Surface z(x, y) as in the project: regular x, y grid, heights z[i][j]
Grid heightGrid(const Heights &z) {
    Grid g;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            g[i][j] = {.x = gridCoord(i), .y = gridCoord(j), .z = z[i][j]};
    return g;
}

Grid flatGrid() { return heightGrid({}); }

/// Height field with hills and valleys, similar to the surfaces in assets/
Grid wavyGrid() {
    return heightGrid({{
        {0.0f, 0.4f, -0.3f, 0.1f},
        {0.5f, -0.6f, 0.8f, -0.2f},
        {-0.4f, 0.7f, -0.5f, 0.3f},
        {0.2f, -0.1f, 0.6f, 0.0f},
    }});
}

/// Control points not on a regular grid at all – for properties that must hold for any surface
Grid irregularGrid() {
    Grid g;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            const auto fi = static_cast<float>(i), fj = static_cast<float>(j);
            g[i][j] = {.x = fi + 0.3f * fj * fj, .y = fj - 0.2f * fi * fj, .z = std::sin(fi + 2 * fj)};
        }
    return g;
}

/// Reference cubic Bézier curve, independent of BezierSurface
Vec3 bezierCurve(const std::array<Vec3, 4> &p, const float t) {
    const auto b = bernstein3(t);
    return p[0] * b[0] + p[1] * b[1] + p[2] * b[2] + p[3] * b[3];
}

/// Parameter pairs in the interior and on the border of [0, 1]^2
std::vector<std::pair<float, float>> sampleUVs() {
    std::vector<std::pair<float, float>> uvs;
    for (int a = 0; a <= 4; ++a)
        for (int b = 0; b <= 4; ++b)
            uvs.emplace_back(static_cast<float>(a) / 4.0f, static_cast<float>(b) / 4.0f);
    return uvs;
}

std::string uvTrace(const float u, const float v) {
    return testing::PrintToString(u) + ", " + testing::PrintToString(v);
}
} // namespace

// ---------- control points ----------

TEST(BezierSurfaceTest, ControlPointReturnsStoredPoint) {
    const Grid g = irregularGrid();
    const BezierSurface s(g);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            EXPECT_VEC3_EQ(s.controlPoint(i, j), g[i][j]) << "i = " << i << ", j = " << j;
}

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(BezierSurfaceDeathTest, ControlPointOutOfRangeAsserts) {
    const BezierSurface s(flatGrid());
    EXPECT_DEATH(std::ignore = s.controlPoint(4, 0), "index out of range");
    EXPECT_DEATH(std::ignore = s.controlPoint(-1, 0), "index out of range");
    EXPECT_DEATH(std::ignore = s.controlPoint(0, 4), "index out of range");
    EXPECT_DEATH(std::ignore = s.controlPoint(0, -1), "index out of range");
}
#endif

// ---------- interpolation of corners and edges ----------

TEST(BezierSurfaceTest, PassesThroughCornerControlPoints) {
    const Grid g = irregularGrid();
    const BezierSurface s(g);
    EXPECT_VEC3_EQ(s.evaluate(0, 0).P, g[0][0]);
    EXPECT_VEC3_EQ(s.evaluate(1, 0).P, g[3][0]);
    EXPECT_VEC3_EQ(s.evaluate(0, 1).P, g[0][3]);
    EXPECT_VEC3_EQ(s.evaluate(1, 1).P, g[3][3]);
}

TEST(BezierSurfaceTest, EdgesAreBezierCurvesOfBorderControlPoints) {
    const Grid g = irregularGrid();
    const BezierSurface s(g);
    const std::array<Vec3, 4> v0{g[0][0], g[1][0], g[2][0], g[3][0]}; // v = 0, along u
    const std::array<Vec3, 4> v1{g[0][3], g[1][3], g[2][3], g[3][3]}; // v = 1, along u
    for (const float t : {0.0f, 0.2f, 0.5f, 0.9f, 1.0f}) {
        SCOPED_TRACE(testing::Message() << "t = " << t);
        EXPECT_VEC3_EQ(s.evaluate(t, 0).P, bezierCurve(v0, t));
        EXPECT_VEC3_EQ(s.evaluate(t, 1).P, bezierCurve(v1, t));
        EXPECT_VEC3_EQ(s.evaluate(0, t).P, bezierCurve(g[0], t)); // u = 0, along v
        EXPECT_VEC3_EQ(s.evaluate(1, t).P, bezierCurve(g[3], t)); // u = 1, along v
    }
}

TEST(BezierSurfaceTest, TangentsAtCornerFollowNeighbouringControlPoints) {
    // Pu(0,0) = 3(P10 - P00), Pv(0,0) = 3(P01 - P00)
    const Grid g = irregularGrid();
    const SurfacePoint p = BezierSurface(g).evaluate(0, 0);
    EXPECT_VEC3_EQ(p.Pu, (g[1][0] - g[0][0]) * 3.0f);
    EXPECT_VEC3_EQ(p.Pv, (g[0][1] - g[0][0]) * 3.0f);
}

// ---------- flat and planar surfaces ----------

TEST(BezierSurfaceTest, FlatGridIsLinearInUV) {
    const BezierSurface s(flatGrid());
    for (const auto [u, v] : sampleUVs()) {
        SCOPED_TRACE(uvTrace(u, v));
        const SurfacePoint p = s.evaluate(u, v);
        EXPECT_VEC3_EQ(p.P, (Vec3{-1 + 2 * u, -1 + 2 * v, 0}));
        EXPECT_VEC3_EQ(p.Pu, (Vec3{2, 0, 0}));
        EXPECT_VEC3_EQ(p.Pv, (Vec3{0, 2, 0}));
        EXPECT_VEC3_EQ(p.N, (Vec3{0, 0, 1}));
    }
}

TEST(BezierSurfaceTest, PlanarControlPointsGivePlane) {
    // z = 0.5x - 0.25y + 0.1 sampled at control points -> surface is exactly this plane
    Heights z;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            z[i][j] = 0.5f * gridCoord(i) - 0.25f * gridCoord(j) + 0.1f;
    const BezierSurface s(heightGrid(z));
    const Vec3 planeNormal = normalized(Vec3{-0.5f, 0.25f, 1});

    for (const auto [u, v] : sampleUVs()) {
        SCOPED_TRACE(uvTrace(u, v));
        const SurfacePoint p = s.evaluate(u, v);
        EXPECT_NEAR(p.P.z, 0.5f * p.P.x - 0.25f * p.P.y + 0.1f, kEps);
        EXPECT_VEC3_EQ(p.N, planeNormal);
    }
}

// ---------- derivatives and normal ----------

TEST(BezierSurfaceTest, TangentsMatchFiniteDifferences) {
    const BezierSurface s(irregularGrid());
    constexpr float h = 1e-3f;
    for (const auto [u, v] : sampleUVs()) {
        SCOPED_TRACE(uvTrace(u, v));
        const SurfacePoint p = s.evaluate(u, v);
        const Vec3 du = (s.evaluate(u + h, v).P - s.evaluate(u - h, v).P) / (2 * h);
        const Vec3 dv = (s.evaluate(u, v + h).P - s.evaluate(u, v - h).P) / (2 * h);
        EXPECT_VEC3_NEAR(p.Pu, du, 2e-2f);
        EXPECT_VEC3_NEAR(p.Pv, dv, 2e-2f);
    }
}

TEST(BezierSurfaceTest, NormalIsUnitAndPerpendicularToTangents) {
    const BezierSurface s(irregularGrid());
    for (const auto [u, v] : sampleUVs()) {
        SCOPED_TRACE(uvTrace(u, v));
        const SurfacePoint p = s.evaluate(u, v);
        EXPECT_NEAR(length(p.N), 1.0f, kEps);
        EXPECT_NEAR(dot(p.N, normalized(p.Pu)), 0.0f, 1e-4f);
        EXPECT_NEAR(dot(p.N, normalized(p.Pv)), 0.0f, 1e-4f);
    }
}

TEST(BezierSurfaceTest, NormalHasSameOrientationAsPuCrossPv) {
    const BezierSurface s(irregularGrid());
    for (const auto [u, v] : sampleUVs()) {
        SCOPED_TRACE(uvTrace(u, v));
        const SurfacePoint p = s.evaluate(u, v);
        EXPECT_VEC3_EQ(p.N, normalized(cross(p.Pu, p.Pv)));
    }
}

TEST(BezierSurfaceTest, HeightFieldNormalsPointUp) {
    const BezierSurface s(wavyGrid());
    for (const auto [u, v] : sampleUVs())
        EXPECT_GT(s.evaluate(u, v).N.z, 0.0f) << uvTrace(u, v);
}

TEST(BezierSurfaceTest, DegenerateNormalFallsBackToPlusZ) {
    // Whole v = 0 edge collapsed into one point -> Pu = 0 there, Pu x Pv = 0
    Grid g = flatGrid();
    for (int i = 0; i < 4; ++i)
        g[i][0] = {0, -1, 0};
    const SurfacePoint p = BezierSurface(g).evaluate(0.5f, 0.0f);
    EXPECT_VEC3_EQ(p.Pu, Vec3{});
    EXPECT_VEC3_EQ(p.N, (Vec3{0, 0, 1}));
}

// ---------- convex hull ----------

TEST(BezierSurfaceTest, StaysWithinBoundingBoxOfControlPoints) {
    const Grid g = wavyGrid();
    float zMin = g[0][0].z, zMax = g[0][0].z;
    for (const auto &row : g)
        for (const Vec3 &p : row) {
            zMin = std::min(zMin, p.z);
            zMax = std::max(zMax, p.z);
        }

    const BezierSurface s(g);
    for (const auto [u, v] : sampleUVs()) {
        const Vec3 p = s.evaluate(u, v).P;
        EXPECT_GE(p.x, -1.0f - kEps) << uvTrace(u, v);
        EXPECT_LE(p.x, 1.0f + kEps) << uvTrace(u, v);
        EXPECT_GE(p.y, -1.0f - kEps) << uvTrace(u, v);
        EXPECT_LE(p.y, 1.0f + kEps) << uvTrace(u, v);
        EXPECT_GE(p.z, zMin - kEps) << uvTrace(u, v);
        EXPECT_LE(p.z, zMax + kEps) << uvTrace(u, v);
    }
}

// ---------- affine invariance (stage 3: rotating control points == rotating results) ----------

TEST(BezierSurfaceTest, RotatingControlPointsRotatesResult) {
    const Grid g = irregularGrid();
    const Mat3 r = rotationAxisAngle({0.3f, -1, 0.5f}, 1.1f);
    Grid rotated;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            rotated[i][j] = r * g[i][j];

    const BezierSurface s(g), sr(rotated);
    for (const auto [u, v] : sampleUVs()) {
        SCOPED_TRACE(uvTrace(u, v));
        const SurfacePoint p = s.evaluate(u, v);
        const SurfacePoint pr = sr.evaluate(u, v);
        EXPECT_VEC3_NEAR(pr.P, r * p.P, 1e-4f);
        EXPECT_VEC3_NEAR(pr.Pu, r * p.Pu, 1e-4f);
        EXPECT_VEC3_NEAR(pr.Pv, r * p.Pv, 1e-4f);
        EXPECT_VEC3_NEAR(pr.N, r * p.N, 1e-4f);
    }
}

TEST(BezierSurfaceTest, TranslatingControlPointsTranslatesOnlyPosition) {
    const Grid g = irregularGrid();
    const Vec3 offset{0.5f, -2, 3};
    Grid moved;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            moved[i][j] = g[i][j] + offset;

    const BezierSurface s(g), sm(moved);
    for (const auto [u, v] : sampleUVs()) {
        SCOPED_TRACE(uvTrace(u, v));
        const SurfacePoint p = s.evaluate(u, v);
        const SurfacePoint pm = sm.evaluate(u, v);
        EXPECT_VEC3_NEAR(pm.P, p.P + offset, 1e-4f);
        EXPECT_VEC3_NEAR(pm.Pu, p.Pu, 1e-4f);
        EXPECT_VEC3_NEAR(pm.Pv, p.Pv, 1e-4f);
        EXPECT_VEC3_NEAR(pm.N, p.N, 1e-4f);
    }
}
