#include "renderer/Texture.h"
#include "math/TestUtils.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <ostream>
#include <vector>

// gtest uses it to print colors in failure messages
inline void PrintTo(const ShadingColor &c, std::ostream *os) {
    *os << "(" << c.r << ", " << c.g << ", " << c.b << ")";
}

namespace {
constexpr float kNan = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();

using Rgb = std::array<std::uint8_t, 3>;

/// Color the texture should return for the given bytes
ShadingColor fromBytes(const Rgb &c) {
    return {
        .r = static_cast<float>(c[0]) / 255.0f,
        .g = static_cast<float>(c[1]) / 255.0f,
        .b = static_cast<float>(c[2]) / 255.0f
    };
}

/// Texture from texels listed row by row, top row first
Texture makeTexture(const int width, const int height, const std::initializer_list<Rgb> texels) {
    std::vector<std::uint8_t> bytes;
    for (const auto &t: texels)
        bytes.insert(bytes.end(), t.begin(), t.end());
    return {width, height, bytes};
}

/// Bytes of texel (x, y) in gradient(): unique for every texel, every channel different
/// (so swapped channels, swapped axes or a wrong row stride all give a different color)
Rgb gradientBytes(const int x, const int y) {
    return {
        static_cast<std::uint8_t>(10 * x + 1),
        static_cast<std::uint8_t>(10 * y + 2),
        static_cast<std::uint8_t>(250 - x - 7 * y)
    };
}

Texture gradient(const int width, const int height) {
    std::vector<std::uint8_t> bytes;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const Rgb t = gradientBytes(x, y);
            bytes.insert(bytes.end(), t.begin(), t.end());
        }
    return {width, height, bytes};
}

testing::AssertionResult colorEq(const char *aExpr, const char *bExpr, const ShadingColor &a, const ShadingColor &b) {
    // neighbouring bytes differ by 1/255 ~ 0.004, so kEps tells every byte apart
    if (std::fabs(a.r - b.r) <= kEps && std::fabs(a.g - b.g) <= kEps && std::fabs(a.b - b.b) <= kEps)
        return testing::AssertionSuccess();
    return testing::AssertionFailure() << aExpr << " = " << testing::PrintToString(a) << "\n"
                                       << bExpr << " = " << testing::PrintToString(b);
}
} // namespace

#define EXPECT_COLOR_EQ(a, b) EXPECT_PRED_FORMAT2(colorEq, a, b)

// ---------- size and texels ----------

TEST(TextureTest, DefaultIsOneWhiteTexel) {
    const Texture t;
    EXPECT_EQ(t.width(), 1);
    EXPECT_EQ(t.height(), 1);
    for (const float u: {0.0f, 0.3f, 1.0f})
        for (const float v: {0.0f, 0.7f, 1.0f})
            EXPECT_COLOR_EQ(t.sample(u, v), (ShadingColor{1.0f, 1.0f, 1.0f})) << "u = " << u << ", v = " << v;
}

TEST(TextureTest, StoresSizeAndTexelsRowByRow) {
    // 3x2 is not square: a row stride of height instead of width gives other texels
    const Texture t = gradient(3, 2);
    EXPECT_EQ(t.width(), 3);
    EXPECT_EQ(t.height(), 2);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 3; ++x)
            EXPECT_COLOR_EQ(t.texel(x, y), fromBytes(gradientBytes(x, y))) << "texel " << x << ", " << y;
}

TEST(TextureTest, KeepsChannelOrder) {
    const Texture t = makeTexture(1, 1, {{10, 20, 30}});
    const ShadingColor c = t.texel(0, 0);
    EXPECT_FLOAT_EQ(c.r, 10.0f / 255.0f);
    EXPECT_FLOAT_EQ(c.g, 20.0f / 255.0f);
    EXPECT_FLOAT_EQ(c.b, 30.0f / 255.0f);
}

TEST(TextureTest, ByteRangeMapsToUnitRange) {
    const Texture t = makeTexture(2, 1, {{0, 0, 0}, {255, 255, 255}});
    EXPECT_COLOR_EQ(t.texel(0, 0), (ShadingColor{0.0f, 0.0f, 0.0f}));
    EXPECT_COLOR_EQ(t.texel(1, 0), (ShadingColor{1.0f, 1.0f, 1.0f}));
}

// ---------- axes: u -> column, v -> row, row 0 at the top ----------

TEST(TextureTest, UMapsToColumns) {
    // two texels side by side: only u decides
    const Rgb left = {200, 0, 0};
    const Rgb right = {0, 0, 200};
    const Texture t = makeTexture(2, 1, {left, right});
    for (const float v: {0.0f, 0.5f, 1.0f}) {
        EXPECT_COLOR_EQ(t.sample(0.25f, v), fromBytes(left)) << "v = " << v;
        EXPECT_COLOR_EQ(t.sample(0.75f, v), fromBytes(right)) << "v = " << v;
    }
}

TEST(TextureTest, VMapsToRows) {
    // two texels one above the other: only v decides
    const Rgb top = {200, 0, 0};
    const Rgb bottom = {0, 0, 200};
    const Texture t = makeTexture(1, 2, {top, bottom});
    for (const float u: {0.0f, 0.5f, 1.0f}) {
        EXPECT_COLOR_EQ(t.sample(u, 0.25f), fromBytes(top)) << "u = " << u;
        EXPECT_COLOR_EQ(t.sample(u, 0.75f), fromBytes(bottom)) << "u = " << u;
    }
}

TEST(TextureTest, CornersOfParameterSquareGiveCornerTexels) {
    const Texture t = gradient(3, 2);
    EXPECT_COLOR_EQ(t.sample(0.0f, 0.0f), t.texel(0, 0)); // first bytes = top left
    EXPECT_COLOR_EQ(t.sample(1.0f, 0.0f), t.texel(2, 0));
    EXPECT_COLOR_EQ(t.sample(0.0f, 1.0f), t.texel(0, 1));
    EXPECT_COLOR_EQ(t.sample(1.0f, 1.0f), t.texel(2, 1)); // last bytes = bottom right
}

// ---------- nearest texel ----------

TEST(TextureTest, TexelCentersGiveThatTexel) {
    for (const auto &[w, h]: {std::array{4, 4}, std::array{3, 2}, std::array{1, 5}}) {
        const Texture t = gradient(w, h);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(w);
                const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(h);
                EXPECT_COLOR_EQ(t.sample(u, v), t.texel(x, y)) << w << "x" << h << ", texel " << x << ", " << y;
            }
    }
}

TEST(TextureTest, TexelCoversHalfOpenRange) {
    // texel x covers u in [x/w, (x+1)/w): the boundary belongs to the next texel
    const Texture t = gradient(4, 1);
    EXPECT_COLOR_EQ(t.sample(0.25f, 0.0f), t.texel(1, 0));
    EXPECT_COLOR_EQ(t.sample(std::nextafter(0.25f, 0.0f), 0.0f), t.texel(0, 0));
    EXPECT_COLOR_EQ(t.sample(0.5f, 0.0f), t.texel(2, 0));
    EXPECT_COLOR_EQ(t.sample(std::nextafter(1.0f, 0.0f), 0.0f), t.texel(3, 0));
}

TEST(TextureTest, ScanAcrossReachesEveryTexelInOrder) {
    // u from 0 to 1: the column never goes back and no column is skipped
    constexpr int kW = 7;
    const Texture t = gradient(kW, 1);
    int column = 0;
    constexpr int kSteps = 1000;
    for (int i = 0; i <= kSteps; ++i) {
        const float u = static_cast<float>(i) / kSteps;
        const ShadingColor c = t.sample(u, 0.5f);
        if (std::fabs(c.r - t.texel(column, 0).r) > kEps) {
            ++column;
            ASSERT_LT(column, kW) << "u = " << u;
            ASSERT_PRED_FORMAT2(colorEq, c, t.texel(column, 0)) << "column skipped at u = " << u;
        }
    }
    EXPECT_EQ(column, kW - 1);
}

// ---------- coordinates outside [0, 1] ----------

TEST(TextureTest, UAndVEqualToOneGiveLastTexel) {
    // vertices on the surface border have u = 1 or v = 1: u * w = w would be past the array
    const Texture t = gradient(5, 3);
    EXPECT_COLOR_EQ(t.sample(1.0f, 0.5f), t.texel(4, 1));
    EXPECT_COLOR_EQ(t.sample(0.5f, 1.0f), t.texel(2, 2));
    EXPECT_COLOR_EQ(t.sample(1.0f, 1.0f), t.texel(4, 2));
}

TEST(TextureTest, CoordinatesSlightlyOutsideAreClamped) {
    // barycentric weights on a triangle edge can give u a bit below 0 or above 1
    const Texture t = gradient(3, 2);
    EXPECT_COLOR_EQ(t.sample(-1e-7f, 0.0f), t.texel(0, 0));
    EXPECT_COLOR_EQ(t.sample(0.0f, -1e-7f), t.texel(0, 0));
    EXPECT_COLOR_EQ(t.sample(-0.0f, -0.0f), t.texel(0, 0));
    EXPECT_COLOR_EQ(t.sample(1.000001f, 0.0f), t.texel(2, 0));
    EXPECT_COLOR_EQ(t.sample(0.0f, 1.000001f), t.texel(0, 1));
}

TEST(TextureTest, FarOutsideAndInfinityAreClamped) {
    const Texture t = gradient(3, 2);
    for (const float low: {-0.5f, -5.0f, -1e30f, -kInf}) {
        EXPECT_COLOR_EQ(t.sample(low, 0.0f), t.texel(0, 0)) << "u = " << low;
        EXPECT_COLOR_EQ(t.sample(0.0f, low), t.texel(0, 0)) << "v = " << low;
    }
    // 1e30 * width cast to int would be undefined behaviour: clamping must happen before the cast
    for (const float high: {1.5f, 5.0f, 1e30f, kInf}) {
        EXPECT_COLOR_EQ(t.sample(high, 0.0f), t.texel(2, 0)) << "u = " << high;
        EXPECT_COLOR_EQ(t.sample(0.0f, high), t.texel(0, 1)) << "v = " << high;
    }
}

TEST(TextureTest, NanGivesFirstTexel) {
    const Texture t = gradient(3, 2);
    EXPECT_COLOR_EQ(t.sample(kNan, 1.0f), t.texel(0, 1));
    EXPECT_COLOR_EQ(t.sample(1.0f, kNan), t.texel(2, 0));
    EXPECT_COLOR_EQ(t.sample(kNan, kNan), t.texel(0, 0));
}

// ---------- normal map colors ----------

TEST(TextureTest, ColorToNormalMapsUnitRangeToMinusOneOne) {
    EXPECT_VEC3_EQ(colorToNormal({0.5f, 0.5f, 1.0f}), (Vec3{0.0f, 0.0f, 1.0f}));
    EXPECT_VEC3_EQ(colorToNormal({1.0f, 0.5f, 0.5f}), (Vec3{1.0f, 0.0f, 0.0f}));
    EXPECT_VEC3_EQ(colorToNormal({0.5f, 1.0f, 0.5f}), (Vec3{0.0f, 1.0f, 0.0f}));
    EXPECT_VEC3_EQ(colorToNormal({0.0f, 0.0f, 0.0f}), (Vec3{-1.0f, -1.0f, -1.0f}));
    EXPECT_VEC3_EQ(colorToNormal({1.0f, 1.0f, 1.0f}), (Vec3{1.0f, 1.0f, 1.0f}));
}

TEST(TextureTest, ColorToNormalDoesNotNormalize) {
    // normalizing happens after M * N_texture; gray would be a zero vector here
    EXPECT_VEC3_EQ(colorToNormal({0.5f, 0.5f, 0.5f}), (Vec3{0.0f, 0.0f, 0.0f}));
    EXPECT_VEC3_EQ(colorToNormal({0.75f, 0.5f, 0.75f}), (Vec3{0.5f, 0.0f, 0.5f}));
}

TEST(TextureTest, FlatNormalMapColorFromTaskIsAlmostUp) {
    // the task: RGB(127, 127, 255) means N = [0, 0, 1]; 127 / 255 * 2 - 1 = -0.004
    const Texture t = makeTexture(1, 1, {{127, 127, 255}});
    EXPECT_VEC3_NEAR(colorToNormal(t.sample(0.5f, 0.5f)), (Vec3{0.0f, 0.0f, 1.0f}), 0.01f);
}

TEST(TextureTest, ColorToNormalIsConstexpr) {
    constexpr Vec3 n = colorToNormal({0.5f, 0.5f, 1.0f});
    static_assert(n.x == 0.0f && n.y == 0.0f && n.z == 1.0f);
    SUCCEED();
}

// ---------- invariants checked by assert ----------

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(TextureDeathTest, RejectsWrongNumberOfBytes) {
    EXPECT_DEATH(Texture(2, 2, std::vector<std::uint8_t>(11)), "");
    EXPECT_DEATH(Texture(2, 2, std::vector<std::uint8_t>(13)), "");
    EXPECT_DEATH(Texture(2, 2, std::vector<std::uint8_t>(4)), ""); // 1 byte per texel instead of 3
}

TEST(TextureDeathTest, RejectsNonPositiveSize) {
    EXPECT_DEATH(Texture(0, 1, {}), "");
    EXPECT_DEATH(Texture(1, 0, {}), "");
    EXPECT_DEATH(Texture(-1, -1, std::vector<std::uint8_t>(3)), "");
}

TEST(TextureDeathTest, RejectsTexelOutOfRange) {
    const Texture t = gradient(3, 2);
    EXPECT_DEATH(static_cast<void>(t.texel(3, 0)), "");
    EXPECT_DEATH(static_cast<void>(t.texel(0, 2)), "");
    EXPECT_DEATH(static_cast<void>(t.texel(-1, 0)), "");
    EXPECT_DEATH(static_cast<void>(t.texel(0, -1)), "");
}
#endif
