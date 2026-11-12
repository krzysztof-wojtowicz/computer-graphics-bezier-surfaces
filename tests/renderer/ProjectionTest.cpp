#include "renderer/Projection.h"
#include "math/TestUtils.h"

#include <array>
#include <cmath>
#include <utility>

namespace {
constexpr float kPixelEps = 1e-3f;

/// 2D cross product of (b - a) and (c - a) on the screen: > 0 counter-clockwise in a y-down system means clockwise
float screenCross(const ScreenPoint &a, const ScreenPoint &b, const ScreenPoint &c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}
} // namespace

// ---------- center ----------

TEST(ProjectionTest, OriginMapsToImageCenter) {
    const Projection proj(800, 600, 5.0f);
    const ScreenPoint sp = proj.project({0.0f, 0.0f, 0.0f});
    EXPECT_FLOAT_EQ(sp.x, 400.0f);
    EXPECT_FLOAT_EQ(sp.y, 300.0f);
}

TEST(ProjectionTest, CenterOfOddSizedImageIsNotTruncated) {
    // Integer division (401 / 2 = 200) would shift the whole picture by half a pixel
    const Projection proj(401, 301, 5.0f);
    const ScreenPoint sp = proj.project({0.0f, 0.0f, 0.0f});
    EXPECT_FLOAT_EQ(sp.x, 200.5f);
    EXPECT_FLOAT_EQ(sp.y, 150.5f);
}

// ---------- axes and scale ----------

TEST(ProjectionTest, SceneRadiusMapsToImageEdgesOfSquareImage) {
    const Projection proj(800, 800, 4.0f);
    EXPECT_FLOAT_EQ(proj.project({4.0f, 0.0f, 0.0f}).x, 800.0f);  // right edge
    EXPECT_FLOAT_EQ(proj.project({-4.0f, 0.0f, 0.0f}).x, 0.0f);   // left edge
    EXPECT_FLOAT_EQ(proj.project({0.0f, 4.0f, 0.0f}).y, 0.0f);    // +y is up: top edge
    EXPECT_FLOAT_EQ(proj.project({0.0f, -4.0f, 0.0f}).y, 800.0f); // bottom edge
}

TEST(ProjectionTest, PositiveXGoesRightPositiveYGoesUp) {
    const Projection proj(800, 600, 5.0f);
    const ScreenPoint center = proj.project({0.0f, 0.0f, 0.0f});
    const ScreenPoint right = proj.project({1.0f, 0.0f, 0.0f});
    const ScreenPoint up = proj.project({0.0f, 1.0f, 0.0f});
    EXPECT_GT(right.x, center.x);
    EXPECT_FLOAT_EQ(right.y, center.y);
    EXPECT_LT(up.y, center.y); // QImage y grows downwards
    EXPECT_FLOAT_EQ(up.x, center.x);
}

TEST(ProjectionTest, ScaleComesFromShorterSideOfWideImage) {
    const Projection proj(1000, 500, 5.0f); // scale = 250 / 5 = 50 px per unit
    EXPECT_FLOAT_EQ(proj.project({5.0f, 0.0f, 0.0f}).x, 750.0f);
    EXPECT_FLOAT_EQ(proj.project({0.0f, 5.0f, 0.0f}).y, 0.0f);
    EXPECT_FLOAT_EQ(proj.project({0.0f, -5.0f, 0.0f}).y, 500.0f);
}

TEST(ProjectionTest, ScaleComesFromShorterSideOfTallImage) {
    const Projection proj(500, 1000, 5.0f);
    EXPECT_FLOAT_EQ(proj.project({5.0f, 0.0f, 0.0f}).x, 500.0f);
    EXPECT_FLOAT_EQ(proj.project({-5.0f, 0.0f, 0.0f}).x, 0.0f);
    EXPECT_FLOAT_EQ(proj.project({0.0f, 5.0f, 0.0f}).y, 250.0f);
}

TEST(ProjectionTest, SameScaleOnBothAxes) {
    // Different scales would turn circles into ellipses and deform the scene while rotating
    const Projection proj(1000, 500, 5.0f);
    const ScreenPoint c = proj.project({0.0f, 0.0f, 0.0f});
    const ScreenPoint px = proj.project({1.0f, 0.0f, 0.0f});
    const ScreenPoint py = proj.project({0.0f, 1.0f, 0.0f});
    EXPECT_FLOAT_EQ(px.x - c.x, c.y - py.y);
}

TEST(ProjectionTest, DefaultSceneRadiusIsUsed) {
    const Projection byDefault(600, 600);
    const Projection explicitRadius(600, 600, Projection::kSceneRadius);
    const Vec3 p{1.0f, -2.0f, 0.5f};
    EXPECT_FLOAT_EQ(byDefault.project(p).x, explicitRadius.project(p).x);
    EXPECT_FLOAT_EQ(byDefault.project(p).y, explicitRadius.project(p).y);
}

TEST(ProjectionTest, ExampleFromPlan) {
    // 800x600, R = 5: scale 60, center (400, 300)
    const Projection proj(800, 600, 5.0f);
    const ScreenPoint sp = proj.project({3.0f, -1.0f, 2.0f});
    EXPECT_FLOAT_EQ(sp.x, 580.0f);
    EXPECT_FLOAT_EQ(sp.y, 360.0f);
    EXPECT_FLOAT_EQ(sp.z, 2.0f);
}

// ---------- z (orthographic projection) ----------

TEST(ProjectionTest, ZIsPassedThroughUnchanged) {
    const Projection proj(800, 600, 5.0f);
    for (const float z : {-4.0f, -0.5f, 0.0f, 1.25f, 4.9f})
        EXPECT_FLOAT_EQ(proj.project({1.0f, 2.0f, z}).z, z) << "z = " << z;
}

TEST(ProjectionTest, ZDoesNotChangeScreenPosition) {
    const Projection proj(800, 600, 5.0f);
    const ScreenPoint closer = proj.project({1.5f, -2.0f, 3.0f});
    const ScreenPoint farther = proj.project({1.5f, -2.0f, -3.0f});
    EXPECT_FLOAT_EQ(closer.x, farther.x);
    EXPECT_FLOAT_EQ(closer.y, farther.y);
}

// ---------- linearity and orientation ----------

TEST(ProjectionTest, ScreenDistanceIsScaledDistanceInXYPlane) {
    const Projection proj(800, 600, 5.0f); // scale 60
    const Vec3 a{-1.0f, 2.0f, 0.3f}, b{2.0f, -2.0f, -1.7f};
    const ScreenPoint pa = proj.project(a), pb = proj.project(b);
    const float screenDist = std::hypot(pb.x - pa.x, pb.y - pa.y);
    const float sceneDistXY = std::hypot(b.x - a.x, b.y - a.y); // = 5
    EXPECT_NEAR(screenDist, 60.0f * sceneDistXY, kPixelEps);
}

// ---------- unproject ----------

TEST(ProjectionTest, UnprojectInvertsProjectForGivenDepth) {
    for (const auto &[w, h] : {std::pair{800, 600}, std::pair{301, 557}}) {
        const Projection proj(w, h);
        for (const Vec3 p : {Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.5f, -2.0f, 8.0f}, Vec3{-5.2f, 5.2f, -3.0f}}) {
            const ScreenPoint sp = proj.project(p);
            EXPECT_VEC3_NEAR(proj.unproject(sp.x, sp.y, p.z), p, 1e-4f) << w << "x" << h;
        }
    }
}

TEST(ProjectionTest, ProjectInvertsUnproject) {
    const Projection proj(640, 480);
    for (const auto &[sx, sy] : {std::pair{0.0f, 0.0f}, std::pair{320.0f, 240.0f}, std::pair{17.5f, 433.25f}}) {
        const ScreenPoint sp = proj.project(proj.unproject(sx, sy, 2.0f));
        EXPECT_NEAR(sp.x, sx, kPixelEps);
        EXPECT_NEAR(sp.y, sy, kPixelEps);
        EXPECT_FLOAT_EQ(sp.z, 2.0f);
    }
}

TEST(ProjectionTest, UnprojectImageCenterGivesAxis) {
    const Projection proj(800, 600);
    EXPECT_VEC3_EQ(proj.unproject(400.0f, 300.0f, 3.0f), (Vec3{0.0f, 0.0f, 3.0f}));
    // y down on the screen is -y in the scene
    EXPECT_GT(proj.unproject(400.0f, 100.0f, 0.0f).y, 0.0f);
    EXPECT_GT(proj.unproject(700.0f, 300.0f, 0.0f).x, 0.0f);
}

TEST(ProjectionTest, FlipsTriangleOrientation) {
    // Counter-clockwise in the scene (looking from +z) becomes clockwise on the y-down screen.
    // Stage 5 relies on this when deciding which side of a triangle faces the viewer.
    const Projection proj(800, 600, 5.0f);
    const Vec3 a{0.0f, 0.0f, 0.0f}, b{1.0f, 0.0f, 0.0f}, c{0.0f, 1.0f, 0.0f};
    ASSERT_GT(cross(b - a, c - a).z, 0.0f); // counter-clockwise in the scene
    EXPECT_LT(screenCross(proj.project(a), proj.project(b), proj.project(c)), 0.0f);
}

// ---------- everything inside the scene sphere fits in the image ----------

class ProjectionFitTest : public testing::TestWithParam<std::pair<int, int>> {};

TEST_P(ProjectionFitTest, PointsOnSceneSphereStayInsideImage) {
    const auto [w, h] = GetParam();
    constexpr float R = 5.0f;
    const Projection proj(w, h, R);
    constexpr float step = kPi / 12.0f; // 15 degrees
    for (int a = 0; a <= 12; ++a) {     // polar angle 0..180
        for (int b = 0; b < 24; ++b) {  // azimuth 0..345
            const float theta = static_cast<float>(a) * step, phi = static_cast<float>(b) * step;
            const Vec3 p{R * std::sin(theta) * std::cos(phi), R * std::sin(theta) * std::sin(phi), R * std::cos(theta)};
            const ScreenPoint sp = proj.project(p);
            EXPECT_GE(sp.x, -kPixelEps) << "p = " << testing::PrintToString(p);
            EXPECT_LE(sp.x, static_cast<float>(w) + kPixelEps) << "p = " << testing::PrintToString(p);
            EXPECT_GE(sp.y, -kPixelEps) << "p = " << testing::PrintToString(p);
            EXPECT_LE(sp.y, static_cast<float>(h) + kPixelEps) << "p = " << testing::PrintToString(p);
        }
    }
}

TEST_P(ProjectionFitTest, RotatedCubeCornerStaysInsideImage) {
    // Corner (3, 3, 3) of the [-3, 3]^3 box, |P| = 3*sqrt(3) ~ 5.196 <= kSceneRadius, rotated in many ways
    const auto [w, h] = GetParam();
    const Projection proj(w, h);
    const Vec3 corner{3.0f, 3.0f, 3.0f};
    ASSERT_LE(length(corner), Projection::kSceneRadius);
    for (int a = 0; a < 24; ++a)
        for (int b = 0; b < 24; ++b) {
            const float ax = static_cast<float>(a) * kPi / 12.0f, ay = static_cast<float>(b) * kPi / 12.0f;
            const ScreenPoint sp = proj.project(rotationY(ay) * rotationX(ax) * corner);
            EXPECT_GE(sp.x, -kPixelEps);
            EXPECT_LE(sp.x, static_cast<float>(w) + kPixelEps);
            EXPECT_GE(sp.y, -kPixelEps);
            EXPECT_LE(sp.y, static_cast<float>(h) + kPixelEps);
        }
}

INSTANTIATE_TEST_SUITE_P(ImageSizes, ProjectionFitTest,
                         testing::Values(std::pair{800, 600}, std::pair{600, 800}, std::pair{401, 401},
                                         std::pair{1920, 400}));

// ---------- invalid arguments ----------

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(ProjectionDeathTest, RejectsNonPositiveArguments) {
    EXPECT_DEATH(Projection(0, 600), "must be > 0");
    EXPECT_DEATH(Projection(800, 0), "must be > 0");
    EXPECT_DEATH(Projection(800, 600, 0.0f), "must be > 0");
    EXPECT_DEATH(Projection(800, 600, -1.0f), "must be > 0");
}
#endif
