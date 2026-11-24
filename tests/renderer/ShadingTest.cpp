#include "renderer/Shading.h"
#include "math/TestUtils.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <span>

namespace {
constexpr ShadingColor kWhite{.r = 1.0f, .g = 1.0f, .b = 1.0f};
constexpr Vec3 kViewer{.x = 0.0f, .y = 0.0f, .z = 1.0f};
/// Shaded point for the direction tests: with P = 0 the light position is also the direction to the light
constexpr Vec3 kOrigin{.x = 0.0f, .y = 0.0f, .z = 0.0f};

/// Shading with only one of the two terms switched on, so each can be checked against its own formula
Shading diffuseOnly(const Vec3 &light) {
    Shading s;
    s.setKd(1.0f);
    s.setKs(0.0f);
    s.setLightPosition(light);
    return s;
}

Shading specularOnly(const Vec3 &light, const float m) {
    Shading s;
    s.setKd(0.0f);
    s.setKs(1.0f);
    s.setM(m);
    s.setLightPosition(light);
    return s;
}

/// Unit vector in the xz plane, `angle` radians away from +z towards +x
Vec3 tilted(const float angle) {
    return {.x = std::sin(angle), .y = 0.0f, .z = std::cos(angle)};
}

void expectColorNear(const ShadingColor &actual, const ShadingColor &expected, const float eps = kEps) {
    EXPECT_NEAR(actual.r, expected.r, eps);
    EXPECT_NEAR(actual.g, expected.g, eps);
    EXPECT_NEAR(actual.b, expected.b, eps);
}

void expectGray(const ShadingColor &actual, const float value, const float eps = kEps) {
    expectColorNear(actual, {.r = value, .g = value, .b = value}, eps);
}
} // namespace

// ---------- diffuse term: kd * IL * IO * cos(N, L) ----------

TEST(ShadingTest, NormalPointingAtLightGivesFullDiffuse) {
    Shading s = diffuseOnly({.x = 1.0f, .y = 2.0f, .z = 3.0f});
    s.setKd(0.7f);
    s.setIl({.r = 1.0f, .g = 0.5f, .b = 0.25f});
    const ShadingColor io{.r = 0.2f, .g = 0.5f, .b = 1.0f};
    expectColorNear(s.calculateColor(io, {.x = 1.0f, .y = 2.0f, .z = 3.0f}, kOrigin),
                    {.r = 0.7f * 0.2f, .g = 0.7f * 0.5f * 0.5f, .b = 0.7f * 0.25f});
}

TEST(ShadingTest, DiffuseFollowsLambertCosine) {
    const Shading s = diffuseOnly(kViewer);
    for (const float angle : {0.0f, 0.3f, kPi / 4, 1.2f, kPi / 2 - 0.01f}) {
        SCOPED_TRACE(angle);
        expectGray(s.calculateColor(kWhite, tilted(angle), kOrigin), std::cos(angle));
    }
}

TEST(ShadingTest, LightAlongTheSurfaceGivesNoDiffuse) {
    const Shading s = diffuseOnly({.x = 1.0f, .y = 0.0f, .z = 0.0f});
    expectGray(s.calculateColor(kWhite, kViewer, kOrigin), 0.0f);
}

TEST(ShadingTest, LightBehindTheSurfaceGivesNoDiffuse) {
    // negative cosine is clamped to 0, not subtracted
    const Shading s = diffuseOnly({.x = 1.0f, .y = 0.0f, .z = -1.0f});
    expectGray(s.calculateColor(kWhite, kViewer, kOrigin), 0.0f);
}

TEST(ShadingTest, DiffuseIsLinearInKd) {
    Shading s = diffuseOnly(tilted(0.4f));
    const ShadingColor full = s.calculateColor(kWhite, kViewer, kOrigin);
    s.setKd(0.25f);
    expectGray(s.calculateColor(kWhite, kViewer, kOrigin), 0.25f * full.r);
}

// ---------- specular term: ks * IL * IO * cos^m(V, R), R = 2<N,L>N - L ----------

TEST(ShadingTest, SpecularFollowsCosinePowerM) {
    // N = V = +z and L tilted by angle, so R is tilted by -angle and cos(V, R) = cos(angle)
    for (const float m : {1.0f, 10.0f, 50.0f}) {
        for (const float angle : {0.1f, 0.3f, 0.6f, 1.0f}) {
            SCOPED_TRACE(testing::Message() << "m = " << m << ", angle = " << angle);
            const Shading s = specularOnly(tilted(angle), m);
            expectGray(s.calculateColor(kWhite, kViewer, kOrigin), std::pow(std::cos(angle), m));
        }
    }
}

TEST(ShadingTest, ReflectionTowardsViewerGivesFullSpecularWithoutNaN) {
    // N halfway between L and V reflects L exactly into V: the centre of the highlight.
    // With acos(dot / lengths) this produced NaN (and a black pixel) for ~25% of directions.
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> coord(-1.0f, 1.0f);
    std::uniform_real_distribution<float> up(0.1f, 1.0f);
    for (int i = 0; i < 1000; ++i) {
        const Vec3 light{.x = coord(gen), .y = coord(gen), .z = up(gen)};
        const Shading s = specularOnly(light, 100.0f);
        const ShadingColor c = s.calculateColor(kWhite, normalized(normalized(light) + kViewer), kOrigin);
        ASSERT_FALSE(std::isnan(c.r)) << "light " << testing::PrintToString(light);
        ASSERT_NEAR(c.r, 1.0f, 1e-3f) << "light " << testing::PrintToString(light);
    }
}

TEST(ShadingTest, SpecularIsSharperForBiggerM) {
    const Vec3 light = tilted(0.3f);
    const float soft = specularOnly(light, 2.0f).calculateColor(kWhite, kViewer, kOrigin).r;
    const float sharp = specularOnly(light, 80.0f).calculateColor(kWhite, kViewer, kOrigin).r;
    EXPECT_GT(soft, sharp);
}

// ---------- point light: L = normalize(lightPosition - P) ----------

TEST(ShadingTest, LightDirectlyAbovePointGivesFullDiffuse) {
    const Vec3 P{.x = 2.0f, .y = -1.0f, .z = 0.5f};
    const Shading s = diffuseOnly(P + Vec3{.x = 0.0f, .y = 0.0f, .z = 4.0f});
    expectGray(s.calculateColor(kWhite, kViewer, P), 1.0f);
}

TEST(ShadingTest, DiffuseOnPlaneFollowsGeometryOfLightPosition) {
    // light at height h above the origin, point x away on the plane z = 0: cos = h / sqrt(h^2 + x^2)
    constexpr float h = 3.0f;
    const Shading s = diffuseOnly({.x = 0.0f, .y = 0.0f, .z = h});
    for (const float x : {0.0f, 1.0f, 3.0f, -5.0f, 20.0f}) {
        SCOPED_TRACE(x);
        expectGray(s.calculateColor(kWhite, kViewer, {.x = x, .y = 0.0f, .z = 0.0f}), h / std::hypot(h, x));
    }
}

TEST(ShadingTest, SameNormalIsLitDifferentlyAtDifferentPoints) {
    // with a constant light direction the whole plane would have one color
    const Shading s = diffuseOnly({.x = 0.0f, .y = 0.0f, .z = 2.0f});
    const float below = s.calculateColor(kWhite, kViewer, kOrigin).r;
    const float aside = s.calculateColor(kWhite, kViewer, {.x = 3.0f, .y = 0.0f, .z = 0.0f}).r;
    EXPECT_GT(below, aside + 0.1f);
}

TEST(ShadingTest, MovingLightAndPointTogetherKeepsColor) {
    Shading s;
    s.setKd(0.6f);
    s.setKs(0.4f);
    s.setM(5.0f);
    const Vec3 light{.x = 1.0f, .y = 2.0f, .z = 6.0f};
    const Vec3 P{.x = -0.5f, .y = 0.3f, .z = 0.2f};
    const Vec3 n = normalized(Vec3{.x = 0.2f, .y = -0.1f, .z = 1.0f});
    s.setLightPosition(light);
    const ShadingColor expected = s.calculateColor(kWhite, n, P);
    for (const Vec3 d : {Vec3{.x = 3.0f, .y = 0.0f, .z = 0.0f}, Vec3{.x = -1.0f, .y = 4.0f, .z = 2.0f}}) {
        SCOPED_TRACE(testing::PrintToString(d));
        s.setLightPosition(light + d);
        expectColorNear(s.calculateColor(kWhite, n, P + d), expected);
    }
}

TEST(ShadingTest, HighlightOnPlaneIsDirectlyBelowLight) {
    // N = V = +z: R = V exactly where L = N, i.e. under the light
    constexpr float lightX = 1.5f;
    const Shading s = specularOnly({.x = lightX, .y = 0.0f, .z = 4.0f}, 20.0f);
    float bestX = 0.0f;
    float best = -1.0f;
    for (int k = -60; k <= 60; ++k) {
        const float x = static_cast<float>(k) * 0.1f;
        const float value = s.calculateColor(kWhite, kViewer, {.x = x, .y = 0.0f, .z = 0.0f}).r;
        if (value > best) {
            best = value;
            bestX = x;
        }
    }
    EXPECT_NEAR(bestX, lightX, 1e-4f);
    EXPECT_NEAR(best, 1.0f, 1e-5f);
}

TEST(ShadingTest, LightAtThePointGivesBlackWithoutNaN) {
    // L = normalize(0) = 0: no direction, no light (and no NaN from dividing by zero)
    Shading s;
    s.setM(3.0f);
    const Vec3 P{.x = 1.0f, .y = 1.0f, .z = 1.0f};
    s.setLightPosition(P);
    const ShadingColor c = s.calculateColor(kWhite, kViewer, P);
    ASSERT_FALSE(std::isnan(c.r));
    expectGray(c, 0.0f);
}

// ---------- invariants ----------

TEST(ShadingTest, ResultDoesNotDependOnNormalLength) {
    // the interpolated normal is shorter than 1 inside a triangle
    Shading s;
    s.setKd(0.6f);
    s.setKs(0.4f);
    s.setM(5.0f);
    s.setLightPosition(tilted(0.5f));
    const Vec3 n = normalized(Vec3{.x = -0.2f, .y = 0.1f, .z = 1.0f});
    const ShadingColor expected = s.calculateColor(kWhite, n, kOrigin);
    for (const float scale : {0.2f, 0.9f, 3.0f}) {
        SCOPED_TRACE(scale);
        expectColorNear(s.calculateColor(kWhite, n * scale, kOrigin), expected);
    }
}

TEST(ShadingTest, ResultDoesNotDependOnLightDistance) {
    // no attenuation in the model: only the direction to the light matters
    const Vec3 light{.x = 0.3f, .y = -0.2f, .z = 0.9f};
    const Vec3 n = normalized(Vec3{.x = 0.1f, .y = 0.2f, .z = 1.0f});
    Shading unit;
    unit.setKs(0.5f);
    unit.setM(3.0f);
    unit.setLightPosition(light);
    Shading longer = unit;
    longer.setLightPosition(light * 7.0f);
    expectColorNear(longer.calculateColor(kWhite, n, kOrigin), unit.calculateColor(kWhite, n, kOrigin));
}

TEST(ShadingTest, BackSideIsLitLikeFrontSide) {
    // surfaces are open, after a rotation the viewer sees their underside (N.z < 0)
    Shading s;
    s.setKs(0.5f);
    s.setM(5.0f);
    s.setLightPosition(tilted(0.4f));
    const Vec3 n = normalized(Vec3{.x = 0.3f, .y = -0.1f, .z = 0.8f});
    const ShadingColor front = s.calculateColor(kWhite, n, kOrigin);
    ASSERT_GT(front.r, 0.1f);
    expectColorNear(s.calculateColor(kWhite, -n, kOrigin), front);
}

TEST(ShadingTest, LightAndObjectColorsMultiplyPerComponent) {
    Shading s;
    s.setKs(0.5f);
    s.setM(5.0f);
    s.setLightPosition(tilted(0.2f));
    const Vec3 n = tilted(-0.1f);
    const float gray = s.calculateColor(kWhite, n, kOrigin).r;
    ASSERT_GT(gray, 0.1f);

    s.setIl({.r = 1.0f, .g = 0.0f, .b = 0.5f});
    const ShadingColor io{.r = 0.4f, .g = 1.0f, .b = 1.0f};
    expectColorNear(s.calculateColor(io, n, kOrigin), {.r = 0.4f * gray, .g = 0.0f, .b = 0.5f * gray});
}

TEST(ShadingTest, ZeroCoefficientsGiveBlack) {
    Shading s;
    s.setKd(0.0f);
    s.setKs(0.0f);
    expectGray(s.calculateColor(kWhite, kViewer, kOrigin), 0.0f);
}

TEST(ShadingTest, ColorIsNeverNegative) {
    std::mt19937 gen(7);
    std::uniform_real_distribution<float> coord(-1.0f, 1.0f);
    Shading s;
    s.setKs(0.5f);
    s.setM(3.0f);
    for (int i = 0; i < 1000; ++i) {
        s.setLightPosition({.x = coord(gen), .y = coord(gen), .z = coord(gen) + 1.5f});
        const Vec3 n{.x = coord(gen), .y = coord(gen), .z = coord(gen) + 1.5f};
        const ShadingColor c = s.calculateColor(kWhite, n, kOrigin);
        ASSERT_GE(c.r, 0.0f);
    }
}

// ---------- conversions ----------

TEST(ShadingTest, Float2IntRoundsKnownValues) {
    EXPECT_EQ(Shading::float2int(0.0f), 0);
    EXPECT_EQ(Shading::float2int(1.0f), 255);
    EXPECT_EQ(Shading::float2int(0.5f), 128);       // 127.5 rounds up
    EXPECT_EQ(Shading::float2int(254.6f / 255.0f), 255); // rounded, not truncated
}

TEST(ShadingTest, Float2IntClampsOutOfRangeValues) {
    // kd + ks can be up to 2, so the sum goes above 1
    EXPECT_EQ(Shading::float2int(-1.0f), 0);
    EXPECT_EQ(Shading::float2int(2.0f), 255);
    EXPECT_EQ(Shading::float2int(std::numeric_limits<float>::infinity()), 255);
    EXPECT_EQ(Shading::float2int(-std::numeric_limits<float>::infinity()), 0);
}

TEST(ShadingTest, Float2IntMapsNaNToZero) {
    EXPECT_EQ(Shading::float2int(std::numeric_limits<float>::quiet_NaN()), 0);
}

TEST(ShadingTest, Int2FloatAndBackIsIdentity) {
    EXPECT_FLOAT_EQ(Shading::int2float(0), 0.0f);
    EXPECT_FLOAT_EQ(Shading::int2float(255), 1.0f);
    for (int c = 0; c <= 255; ++c)
        EXPECT_EQ(Shading::float2int(Shading::int2float(c)), c);
}

// ---------- several lights ----------

TEST(ShadingTest, OneLightByDefaultAndAfterSetLightPosition) {
    Shading s;
    EXPECT_EQ(s.lightCount(), 1);
    const std::array<Vec3, 3> lights{Vec3{1, 0, 1}, Vec3{-1, 0, 1}, Vec3{0, 1, 1}};
    s.setLightPositions(lights);
    EXPECT_EQ(s.lightCount(), 3);
    for (int i = 0; i < 3; ++i)
        EXPECT_VEC3_EQ(s.lightPosition(i), lights[static_cast<std::size_t>(i)]);

    s.setLightPosition({0, 0, 5}); // back to a single light: the other two must stop lighting
    EXPECT_EQ(s.lightCount(), 1);
    EXPECT_VEC3_EQ(s.lightPosition(), (Vec3{0, 0, 5}));
    Shading single;
    single.setLightPosition({0, 0, 5});
    expectColorNear(s.calculateColor(kWhite, kViewer, kOrigin), single.calculateColor(kWhite, kViewer, kOrigin));
}

TEST(ShadingTest, LightsAddUp) {
    // each light has its own L and R; diffuse and specular of all of them are summed
    constexpr ShadingColor io{.r = 0.2f, .g = 0.1f, .b = 0.05f}; // small: the sum stays below 1, nothing clamps
    const std::array<Vec3, 3> lights{Vec3{3, 0, 4}, Vec3{-2, 1, 5}, Vec3{0.5f, -3, 2}};
    const Vec3 N = normalized(Vec3{0.2f, -0.1f, 1.0f});
    const Vec3 P{0.3f, 0.4f, -0.2f};

    Shading all;
    all.setKd(0.6f);
    all.setKs(0.7f);
    all.setM(5.0f);
    all.setLightPositions(lights);

    ShadingColor sum{0, 0, 0};
    for (const Vec3 &light : lights) {
        Shading one = all;
        one.setLightPosition(light);
        const ShadingColor c = one.calculateColor(io, N, P);
        EXPECT_GT(c.r, 0.0f) << "every light must contribute, else the test checks less";
        sum = {.r = sum.r + c.r, .g = sum.g + c.g, .b = sum.b + c.b};
    }
    expectColorNear(all.calculateColor(io, N, P), sum);
}

TEST(ShadingTest, LightsOnOppositeSidesLightBothSlopes) {
    // two slopes facing +x and -x: one light lights only one of them, a light on each side lights both
    const Vec3 right = tilted(0.8f), left = tilted(-0.8f);
    const std::array<Vec3, 2> lights{Vec3{10, 0, 1}, Vec3{-10, 0, 1}};
    Shading s = diffuseOnly(lights[0]);
    EXPECT_GT(s.calculateColor(kWhite, right, kOrigin).r, 0.1f);
    EXPECT_FLOAT_EQ(s.calculateColor(kWhite, left, kOrigin).r, 0.0f);

    s.setLightPositions(lights);
    expectColorNear(s.calculateColor(kWhite, left, kOrigin), s.calculateColor(kWhite, right, kOrigin));
    EXPECT_GT(s.calculateColor(kWhite, left, kOrigin).r, 0.1f);
}

// ---------- invariants checked by assert ----------

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(ShadingDeathTest, RejectsCoefficientsOutsideUnitRange) {
    Shading s;
    EXPECT_DEATH(s.setKd(-0.1f), "");
    EXPECT_DEATH(s.setKd(1.1f), "");
    EXPECT_DEATH(s.setKs(-0.1f), "");
    EXPECT_DEATH(s.setKs(1.1f), "");
}

TEST(ShadingDeathTest, RejectsMOutsideRange) {
    Shading s;
    EXPECT_DEATH(s.setM(0.5f), "");
    EXPECT_DEATH(s.setM(101.0f), "");
}

TEST(ShadingDeathTest, RejectsLightCountOutsideRange) {
    Shading s;
    const std::array<Vec3, Shading::kMaxLights + 1> tooMany{};
    EXPECT_DEATH(s.setLightPositions(std::span<const Vec3>()), "1 to kMaxLights lights");
    EXPECT_DEATH(s.setLightPositions(tooMany), "1 to kMaxLights lights");
    EXPECT_DEATH(static_cast<void>(s.lightPosition(1)), "light index out of range");
}

TEST(ShadingDeathTest, RejectsLightColorOutsideUnitRange) {
    Shading s;
    EXPECT_DEATH(s.setIl({.r = 1.5f, .g = 0.0f, .b = 0.0f}), "");
    EXPECT_DEATH(s.setIl({.r = 0.0f, .g = -0.1f, .b = 0.0f}), "");
    EXPECT_DEATH(s.setIl({.r = 0.0f, .g = 0.0f, .b = 2.0f}), "");
}
#endif
