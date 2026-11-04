#include "geometry/Bernstein.h"
#include "math/TestUtils.h"

#include <array>
#include <vector>

namespace {
/// Sample parameters covering [0, 1], including both ends
std::vector<float> sampleTs() {
    std::vector<float> ts;
    for (int k = 0; k <= 20; ++k)
        ts.push_back(static_cast<float>(k) / 20.0f);
    return ts;
}

float sum(const std::array<float, 4> &a) { return a[0] + a[1] + a[2] + a[3]; }
} // namespace

// ---------- compile-time checks (constexpr API) ----------

static_assert(bernstein3(0.0f)[0] == 1.0f && bernstein3(0.0f)[3] == 0.0f);
static_assert(bernstein3(1.0f)[0] == 0.0f && bernstein3(1.0f)[3] == 1.0f);
static_assert(bernstein3Deriv(0.0f)[0] == -3.0f && bernstein3Deriv(0.0f)[1] == 3.0f);

// ---------- bernstein3 ----------

TEST(BernsteinTest, ValuesAtEndpoints) {
    const auto b0 = bernstein3(0.0f);
    const auto b1 = bernstein3(1.0f);
    for (int i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(b0[i], i == 0 ? 1.0f : 0.0f) << "B" << i << "(0)";
        EXPECT_FLOAT_EQ(b1[i], i == 3 ? 1.0f : 0.0f) << "B" << i << "(1)";
    }
}

TEST(BernsteinTest, ValuesAtMidpoint) {
    const auto b = bernstein3(0.5f);
    EXPECT_FLOAT_EQ(b[0], 0.125f);
    EXPECT_FLOAT_EQ(b[1], 0.375f);
    EXPECT_FLOAT_EQ(b[2], 0.375f);
    EXPECT_FLOAT_EQ(b[3], 0.125f);
}

TEST(BernsteinTest, WeightsSumToOne) {
    for (const float t : sampleTs())
        EXPECT_NEAR(sum(bernstein3(t)), 1.0f, kEps) << "t = " << t;
}

TEST(BernsteinTest, WeightsAreNonNegativeOnUnitInterval) {
    for (const float t : sampleTs())
        for (const float w : bernstein3(t))
            EXPECT_GE(w, 0.0f) << "t = " << t;
}

TEST(BernsteinTest, WeightsAreSymmetric) {
    // B_i(t) = B_{3-i}(1 - t)
    for (const float t : sampleTs()) {
        const auto a = bernstein3(t);
        const auto b = bernstein3(1.0f - t);
        for (int i = 0; i < 4; ++i)
            EXPECT_NEAR(a[i], b[3 - i], kEps) << "t = " << t << ", i = " << i;
    }
}

TEST(BernsteinTest, EachWeightPeaksAtItsIndex) {
    // B_i has its maximum at t = i/3
    for (int i = 0; i < 4; ++i) {
        const float peak = bernstein3(static_cast<float>(i) / 3.0f)[i];
        for (const float t : sampleTs())
            EXPECT_LE(bernstein3(t)[i], peak + kEps) << "i = " << i << ", t = " << t;
    }
}

// ---------- bernstein3Deriv ----------

TEST(BernsteinTest, DerivativesAtEndpoints) {
    // B'(0) = 3(P1 - P0), B'(1) = 3(P3 - P2)
    const auto d0 = bernstein3Deriv(0.0f);
    const auto d1 = bernstein3Deriv(1.0f);
    const std::array expected0{-3.0f, 3.0f, 0.0f, 0.0f};
    const std::array expected1{0.0f, 0.0f, -3.0f, 3.0f};
    for (int i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(d0[i], expected0[i]) << "B" << i << "'(0)";
        EXPECT_FLOAT_EQ(d1[i], expected1[i]) << "B" << i << "'(1)";
    }
}

TEST(BernsteinTest, DerivativesSumToZero) {
    // derivative of a constant (sum of weights = 1)
    for (const float t : sampleTs())
        EXPECT_NEAR(sum(bernstein3Deriv(t)), 0.0f, kEps) << "t = " << t;
}

TEST(BernsteinTest, DerivativesMatchFiniteDifferences) {
    constexpr float h = 1e-3f;
    for (const float t : sampleTs()) {
        const auto d = bernstein3Deriv(t);
        const auto plus = bernstein3(t + h);
        const auto minus = bernstein3(t - h);
        for (int i = 0; i < 4; ++i)
            EXPECT_NEAR(d[i], (plus[i] - minus[i]) / (2 * h), 1e-2f) << "t = " << t << ", i = " << i;
    }
}

TEST(BernsteinTest, DerivativesAreAntisymmetric) {
    // from B_i(t) = B_{3-i}(1 - t):  B_i'(t) = -B_{3-i}'(1 - t)
    for (const float t : sampleTs()) {
        const auto a = bernstein3Deriv(t);
        const auto b = bernstein3Deriv(1.0f - t);
        for (int i = 0; i < 4; ++i)
            EXPECT_NEAR(a[i], -b[3 - i], kEps) << "t = " << t << ", i = " << i;
    }
}
