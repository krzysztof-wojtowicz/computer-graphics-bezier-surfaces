#include "renderer/NormalMapping.h"
#include "renderer/Texture.h"
#include "math/TestUtils.h"

#include <gtest/gtest.h>

#include <cmath>
#include <random>

namespace {
// Pu and Pv are NOT perpendicular and none of the vectors is unit:
// only then a transposed M or a missing normalization gives a different result
constexpr Vec3 kPu = {.x = 3.0f, .y = 0.0f, .z = 0.0f};
constexpr Vec3 kPv = {.x = 0.5f, .y = 0.5f, .z = 0.0f};
constexpr Vec3 kN = {.x = 0.0f, .y = 0.0f, .z = 4.0f};

constexpr Vec3 kUp = {.x = 0.0f, .y = 0.0f, .z = 1.0f};

/// Random vector with length in [0.2, 5] (far from a zero vector)
Vec3 randomVector(std::mt19937 &rng) {
    std::uniform_real_distribution<float> coord(-1.0f, 1.0f);
    std::uniform_real_distribution<float> len(0.2f, 5.0f);
    Vec3 v;
    do {
        v = {.x = coord(rng), .y = coord(rng), .z = coord(rng)};
    } while (length(v) < 0.1f);
    return normalized(v) * len(rng);
}

/// Normal map vector with Nz > 0, as the task requires (Blue in 128-255)
Vec3 randomMapNormal(std::mt19937 &rng) {
    std::uniform_real_distribution<float> xy(-1.0f, 1.0f);
    std::uniform_real_distribution<float> z(0.05f, 1.0f);
    return {.x = xy(rng), .y = xy(rng), .z = z(rng)};
}
} // namespace

// ---------- known values ----------

TEST(NormalMappingTest, FlatMapKeepsSurfaceNormal) {
    EXPECT_VEC3_EQ(perturbNormal(kPu, kPv, kN, kUp), normalized(kN));
}

TEST(NormalMappingTest, XComponentFollowsPu) {
    EXPECT_VEC3_EQ(perturbNormal(kPu, kPv, kN, {1.0f, 0.0f, 0.0f}), normalized(kPu));
    EXPECT_VEC3_EQ(perturbNormal(kPu, kPv, kN, {-1.0f, 0.0f, 0.0f}), -normalized(kPu));
}

TEST(NormalMappingTest, YComponentFollowsPv) {
    EXPECT_VEC3_EQ(perturbNormal(kPu, kPv, kN, {0.0f, 1.0f, 0.0f}), normalized(kPv));
    EXPECT_VEC3_EQ(perturbNormal(kPu, kPv, kN, {0.0f, -1.0f, 0.0f}), -normalized(kPv));
}

TEST(NormalMappingTest, CombinesNormalizedFrameVectors) {
    // N' = normalize(x * Pu/|Pu| + y * Pv/|Pv| + z * N/|N|), computed here without Mat3
    const Vec3 T = kPu / length(kPu);
    const Vec3 B = kPv / length(kPv);
    const Vec3 Nn = kN / length(kN);
    for (const Vec3 &t: {Vec3{0.3f, -0.2f, 0.9f}, Vec3{-0.7f, 0.1f, 0.4f}, Vec3{0.5f, 0.5f, 0.5f}}) {
        const Vec3 expected = normalized(t.x * T + t.y * B + t.z * Nn);
        EXPECT_VEC3_EQ(perturbNormal(kPu, kPv, kN, t), expected) << "Ntex = " << testing::PrintToString(t);
    }
}

TEST(NormalMappingTest, TiltsNormalTowardsPu) {
    // a 45 degree bump on a flat patch lying in the xy plane
    const Vec3 tilted = perturbNormal({1, 0, 0}, {0, 1, 0}, kUp, {1.0f, 0.0f, 1.0f});
    const float h = 1.0f / std::sqrt(2.0f);
    EXPECT_VEC3_EQ(tilted, (Vec3{h, 0.0f, h}));
}

// ---------- properties ----------

TEST(NormalMappingTest, ResultIsUnitLength) {
    std::mt19937 rng(7);
    for (int i = 0; i < 1000; ++i) {
        const Vec3 n = perturbNormal(randomVector(rng), randomVector(rng), randomVector(rng), randomMapNormal(rng));
        ASSERT_NEAR(length(n), 1.0f, 1e-4f) << "sample " << i;
    }
}

TEST(NormalMappingTest, DoesNotDependOnInputLengths) {
    std::mt19937 rng(11);
    std::uniform_real_distribution<float> scale(0.1f, 10.0f);
    for (int i = 0; i < 200; ++i) {
        const Vec3 Pu = randomVector(rng), Pv = randomVector(rng), N = randomVector(rng);
        const Vec3 t = randomMapNormal(rng);
        EXPECT_VEC3_NEAR(perturbNormal(Pu, Pv, N, t),
                         perturbNormal(Pu * scale(rng), Pv * scale(rng), N * scale(rng), t * scale(rng)), 1e-4f)
            << "sample " << i;
    }
}

TEST(NormalMappingTest, RotatingFrameRotatesResult) {
    // the map is in the surface's own frame: rotating the surface rotates the bumped normal with it
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> angle(-kPi, kPi);
    for (int i = 0; i < 200; ++i) {
        const Mat3 R = rotationZ(angle(rng)) * rotationY(angle(rng)) * rotationX(angle(rng));
        const Vec3 Pu = randomVector(rng), Pv = randomVector(rng), N = randomVector(rng);
        const Vec3 t = randomMapNormal(rng);
        EXPECT_VEC3_NEAR(perturbNormal(R * Pu, R * Pv, R * N, t), R * perturbNormal(Pu, Pv, N, t), 1e-4f)
            << "sample " << i;
    }
}

TEST(NormalMappingTest, PositiveMapZStaysOnSurfaceSideForOrthonormalFrame) {
    // Blue in 128-255 (Nz > 0): the bumped normal never points under the surface
    std::mt19937 rng(5);
    std::uniform_real_distribution<float> angle(-kPi, kPi);
    for (int i = 0; i < 200; ++i) {
        const Mat3 R = rotationZ(angle(rng)) * rotationY(angle(rng)) * rotationX(angle(rng));
        const Vec3 N = R.column(2);
        EXPECT_GT(dot(perturbNormal(R.column(0), R.column(1), N, randomMapNormal(rng)), N), 0.0f) << "sample " << i;
    }
}

// ---------- with colors from a normal map ----------

TEST(NormalMappingTest, FlatColorFromTaskKeepsNormal) {
    // RGB(127, 127, 255) means N = [0, 0, 1] in the map's frame
    const Texture map(1, 1, {127, 127, 255});
    const Vec3 n = perturbNormal(kPu, kPv, kN, colorToNormal(map.sample(0.5f, 0.5f)));
    EXPECT_VEC3_NEAR(n, normalized(kN), 0.01f);
}

TEST(NormalMappingTest, GrayColorGivesZeroVectorWithoutNan) {
    // (128, 128, 128) is not a valid normal (Nz = 0); normalized() keeps the zero vector instead of dividing by 0
    const Vec3 n = perturbNormal(kPu, kPv, kN, colorToNormal({0.5f, 0.5f, 0.5f}));
    EXPECT_FALSE(std::isnan(n.x) || std::isnan(n.y) || std::isnan(n.z));
    EXPECT_VEC3_EQ(n, (Vec3{0.0f, 0.0f, 0.0f}));
}
