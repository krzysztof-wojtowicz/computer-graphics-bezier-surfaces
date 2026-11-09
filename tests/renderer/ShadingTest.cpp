#include "renderer/Shading.h"
#include "math/TestUtils.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <random>

namespace {
constexpr ShadingColor kWhite{.r = 1.0f, .g = 1.0f, .b = 1.0f};
constexpr Vec3 kViewer{.x = 0.0f, .y = 0.0f, .z = 1.0f};

/// Shading with only one of the two terms switched on, so each can be checked against its own formula
Shading diffuseOnly(const Vec3 &light) {
    Shading s;
    s.setKd(1.0f);
    s.setKs(0.0f);
    s.setL(light);
    return s;
}

Shading specularOnly(const Vec3 &light, const float m) {
    Shading s;
    s.setKd(0.0f);
    s.setKs(1.0f);
    s.setM(m);
    s.setL(light);
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
    expectColorNear(s.calculateColor(io, {.x = 1.0f, .y = 2.0f, .z = 3.0f}),
                    {.r = 0.7f * 0.2f, .g = 0.7f * 0.5f * 0.5f, .b = 0.7f * 0.25f});
}

TEST(ShadingTest, DiffuseFollowsLambertCosine) {
    const Shading s = diffuseOnly(kViewer);
    for (const float angle : {0.0f, 0.3f, kPi / 4, 1.2f, kPi / 2 - 0.01f}) {
        SCOPED_TRACE(angle);
        expectGray(s.calculateColor(kWhite, tilted(angle)), std::cos(angle));
    }
}

TEST(ShadingTest, LightAlongTheSurfaceGivesNoDiffuse) {
    const Shading s = diffuseOnly({.x = 1.0f, .y = 0.0f, .z = 0.0f});
    expectGray(s.calculateColor(kWhite, kViewer), 0.0f);
}

TEST(ShadingTest, LightBehindTheSurfaceGivesNoDiffuse) {
    // negative cosine is clamped to 0, not subtracted
    const Shading s = diffuseOnly({.x = 1.0f, .y = 0.0f, .z = -1.0f});
    expectGray(s.calculateColor(kWhite, kViewer), 0.0f);
}

TEST(ShadingTest, DiffuseIsLinearInKd) {
    Shading s = diffuseOnly(tilted(0.4f));
    const ShadingColor full = s.calculateColor(kWhite, kViewer);
    s.setKd(0.25f);
    expectGray(s.calculateColor(kWhite, kViewer), 0.25f * full.r);
}

// ---------- specular term: ks * IL * IO * cos^m(V, R), R = 2<N,L>N - L ----------

TEST(ShadingTest, SpecularFollowsCosinePowerM) {
    // N = V = +z and L tilted by angle, so R is tilted by -angle and cos(V, R) = cos(angle)
    for (const float m : {1.0f, 10.0f, 50.0f}) {
        for (const float angle : {0.1f, 0.3f, 0.6f, 1.0f}) {
            SCOPED_TRACE(testing::Message() << "m = " << m << ", angle = " << angle);
            const Shading s = specularOnly(tilted(angle), m);
            expectGray(s.calculateColor(kWhite, kViewer), std::pow(std::cos(angle), m));
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
        const ShadingColor c = s.calculateColor(kWhite, normalized(normalized(light) + kViewer));
        ASSERT_FALSE(std::isnan(c.r)) << "light " << testing::PrintToString(light);
        ASSERT_NEAR(c.r, 1.0f, 1e-3f) << "light " << testing::PrintToString(light);
    }
}

TEST(ShadingTest, SpecularIsSharperForBiggerM) {
    const Vec3 light = tilted(0.3f);
    const float soft = specularOnly(light, 2.0f).calculateColor(kWhite, kViewer).r;
    const float sharp = specularOnly(light, 80.0f).calculateColor(kWhite, kViewer).r;
    EXPECT_GT(soft, sharp);
}

// ---------- invariants ----------

TEST(ShadingTest, ResultDoesNotDependOnNormalLength) {
    // the interpolated normal is shorter than 1 inside a triangle
    Shading s;
    s.setKd(0.6f);
    s.setKs(0.4f);
    s.setM(5.0f);
    s.setL(tilted(0.5f));
    const Vec3 n = normalized(Vec3{.x = -0.2f, .y = 0.1f, .z = 1.0f});
    const ShadingColor expected = s.calculateColor(kWhite, n);
    for (const float scale : {0.2f, 0.9f, 3.0f}) {
        SCOPED_TRACE(scale);
        expectColorNear(s.calculateColor(kWhite, n * scale), expected);
    }
}

TEST(ShadingTest, ResultDoesNotDependOnLightVectorLength) {
    const Vec3 light{.x = 0.3f, .y = -0.2f, .z = 0.9f};
    const Vec3 n = normalized(Vec3{.x = 0.1f, .y = 0.2f, .z = 1.0f});
    Shading unit;
    unit.setKs(0.5f);
    unit.setM(3.0f);
    unit.setL(light);
    Shading longer = unit;
    longer.setL(light * 7.0f);
    expectColorNear(longer.calculateColor(kWhite, n), unit.calculateColor(kWhite, n));
}

TEST(ShadingTest, BackSideIsLitLikeFrontSide) {
    // surfaces are open, after a rotation the viewer sees their underside (N.z < 0)
    Shading s;
    s.setKs(0.5f);
    s.setM(5.0f);
    s.setL(tilted(0.4f));
    const Vec3 n = normalized(Vec3{.x = 0.3f, .y = -0.1f, .z = 0.8f});
    const ShadingColor front = s.calculateColor(kWhite, n);
    ASSERT_GT(front.r, 0.1f);
    expectColorNear(s.calculateColor(kWhite, -n), front);
}

TEST(ShadingTest, LightAndObjectColorsMultiplyPerComponent) {
    Shading s;
    s.setKs(0.5f);
    s.setM(5.0f);
    s.setL(tilted(0.2f));
    const Vec3 n = tilted(-0.1f);
    const float gray = s.calculateColor(kWhite, n).r;
    ASSERT_GT(gray, 0.1f);

    s.setIl({.r = 1.0f, .g = 0.0f, .b = 0.5f});
    const ShadingColor io{.r = 0.4f, .g = 1.0f, .b = 1.0f};
    expectColorNear(s.calculateColor(io, n), {.r = 0.4f * gray, .g = 0.0f, .b = 0.5f * gray});
}

TEST(ShadingTest, ZeroCoefficientsGiveBlack) {
    Shading s;
    s.setKd(0.0f);
    s.setKs(0.0f);
    expectGray(s.calculateColor(kWhite, kViewer), 0.0f);
}

TEST(ShadingTest, ColorIsNeverNegative) {
    std::mt19937 gen(7);
    std::uniform_real_distribution<float> coord(-1.0f, 1.0f);
    Shading s;
    s.setKs(0.5f);
    s.setM(3.0f);
    for (int i = 0; i < 1000; ++i) {
        s.setL({.x = coord(gen), .y = coord(gen), .z = coord(gen) + 1.5f});
        const ShadingColor c = s.calculateColor(kWhite, {.x = coord(gen), .y = coord(gen), .z = coord(gen) + 1.5f});
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

TEST(ShadingDeathTest, RejectsLightColorOutsideUnitRange) {
    Shading s;
    EXPECT_DEATH(s.setIl({.r = 1.5f, .g = 0.0f, .b = 0.0f}), "");
    EXPECT_DEATH(s.setIl({.r = 0.0f, .g = -0.1f, .b = 0.0f}), "");
    EXPECT_DEATH(s.setIl({.r = 0.0f, .g = 0.0f, .b = 2.0f}), "");
}
#endif
