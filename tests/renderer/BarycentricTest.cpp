#include "renderer/Barycentric.h"
#include "renderer/Projection.h"
#include "renderer/Rasterizer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <utility>

namespace {
constexpr float kEps = 1e-4f;

ScreenPoint pt(const float x, const float y, const float z = 0.0f) {
    return {.x = x, .y = y, .z = z};
}

void expectWeights(const Weights &w, const float a, const float b, const float c, const float eps = kEps) {
    EXPECT_NEAR(w.a, a, eps);
    EXPECT_NEAR(w.b, b, eps);
    EXPECT_NEAR(w.c, c, eps);
}

std::array<ScreenPoint, 3> randomTriangle(std::mt19937 &gen) {
    std::uniform_real_distribution<float> coord(0.0f, 100.0f);
    while (true) {
        const std::array t = {pt(coord(gen), coord(gen)), pt(coord(gen), coord(gen)), pt(coord(gen), coord(gen))};
        // skip slivers: with a tiny area the weights get large and the float tolerances below don't hold
        const float area2 = (t[1].x - t[0].x) * (t[2].y - t[0].y) - (t[1].y - t[0].y) * (t[2].x - t[0].x);
        if (std::abs(area2) > 200.0f)
            return t;
    }
}
} // namespace

// ---------- known values ----------

TEST(BarycentricTest, VerticesHaveUnitWeights) {
    const ScreenPoint a = pt(1, 2), b = pt(9, 3), c = pt(4, 8);
    const Barycentric bary(a, b, c);
    expectWeights(bary.at(a.x, a.y), 1, 0, 0);
    expectWeights(bary.at(b.x, b.y), 0, 1, 0);
    expectWeights(bary.at(c.x, c.y), 0, 0, 1);
}

TEST(BarycentricTest, CentroidAndEdgeMidpoints) {
    const ScreenPoint a = pt(0, 0), b = pt(6, 0), c = pt(0, 6);
    const Barycentric bary(a, b, c);
    expectWeights(bary.at(2, 2), 1.0f / 3, 1.0f / 3, 1.0f / 3);
    expectWeights(bary.at(3, 0), 0.5f, 0.5f, 0);
    expectWeights(bary.at(3, 3), 0, 0.5f, 0.5f);
    expectWeights(bary.at(0, 3), 0.5f, 0, 0.5f);
}

TEST(BarycentricTest, PointsOutsideHaveNegativeWeights) {
    // Mirror of a in the edge b-c: weight of a is -1
    const Barycentric bary(pt(0, 0), pt(6, 0), pt(0, 6));
    expectWeights(bary.at(6, 6), -1, 1, 1);
    expectWeights(bary.at(-6, 0), 2, -1, 0);
}

TEST(BarycentricTest, InterpolateMixesValuesWithWeights) {
    EXPECT_FLOAT_EQ(interpolate({1, 0, 0}, 3, 5, 7), 3);
    EXPECT_FLOAT_EQ(interpolate({0, 1, 0}, 3, 5, 7), 5);
    EXPECT_FLOAT_EQ(interpolate({0, 0, 1}, 3, 5, 7), 7);
    EXPECT_FLOAT_EQ(interpolate({0.5f, 0.25f, 0.25f}, 4, 8, 12), 7);
}

// ---------- properties ----------

TEST(BarycentricTest, WeightsSumToOne) {
    std::mt19937 gen(1);
    std::uniform_real_distribution<float> coord(-50.0f, 150.0f);
    for (int k = 0; k < 100; ++k) {
        const auto t = randomTriangle(gen);
        const Barycentric bary(t[0], t[1], t[2]);
        const Weights w = bary.at(coord(gen), coord(gen));
        EXPECT_NEAR(w.a + w.b + w.c, 1.0f, kEps);
    }
}

TEST(BarycentricTest, WeightsReconstructThePoint) {
    // p = wa * a + wb * b + wc * c, also outside the triangle
    std::mt19937 gen(2);
    std::uniform_real_distribution<float> coord(-50.0f, 150.0f);
    for (int k = 0; k < 100; ++k) {
        const auto t = randomTriangle(gen);
        const Barycentric bary(t[0], t[1], t[2]);
        const float px = coord(gen), py = coord(gen);
        const Weights w = bary.at(px, py);
        EXPECT_NEAR(interpolate(w, t[0].x, t[1].x, t[2].x), px, 1e-2f);
        EXPECT_NEAR(interpolate(w, t[0].y, t[1].y, t[2].y), py, 1e-2f);
    }
}

TEST(BarycentricTest, LinearFunctionIsInterpolatedExactly) {
    // z given by a plane z = alpha*x + beta*y + gamma in the vertices is the same plane everywhere
    // (orthographic projection: screen-space interpolation of z is exact)
    std::mt19937 gen(3);
    std::uniform_real_distribution<float> coord(-50.0f, 150.0f), coef(-0.1f, 0.1f);
    for (int k = 0; k < 100; ++k) {
        const auto t = randomTriangle(gen);
        const float alpha = coef(gen), beta = coef(gen), gamma = coef(gen) * 10;
        auto f = [&](const float x, const float y) { return alpha * x + beta * y + gamma; };
        const Barycentric bary(t[0], t[1], t[2]);
        const float px = coord(gen), py = coord(gen);
        EXPECT_NEAR(interpolate(bary.at(px, py), f(t[0].x, t[0].y), f(t[1].x, t[1].y), f(t[2].x, t[2].y)), f(px, py),
                    1e-3f);
    }
}

TEST(BarycentricTest, OrientationDoesNotMatter) {
    const ScreenPoint a = pt(1.5f, 2.5f), b = pt(19.0f, 4.0f), c = pt(7.0f, 17.5f);
    const Barycentric ccw(a, b, c), cw(a, c, b);
    for (const auto &[px, py] : {std::pair{5.0f, 6.0f}, std::pair{30.0f, -4.0f}, std::pair{a.x, a.y}}) {
        const Weights w1 = ccw.at(px, py), w2 = cw.at(px, py);
        EXPECT_NEAR(w1.a, w2.a, kEps);
        EXPECT_NEAR(w1.b, w2.c, kEps); // b and c swapped places
        EXPECT_NEAR(w1.c, w2.b, kEps);
    }
}

TEST(BarycentricTest, WeightsFollowVertexPermutation) {
    // Each weight belongs to its point, not to the argument position
    const std::array<ScreenPoint, 3> p = {pt(3.3f, 1.1f), pt(21.7f, 9.4f), pt(8.2f, 25.6f)};
    const Weights ref = Barycentric(p[0], p[1], p[2]).at(10.0f, 11.0f);
    const std::array<float, 3> refByPoint = {ref.a, ref.b, ref.c};
    std::array<int, 3> perm = {0, 1, 2};
    do {
        const Weights w = Barycentric(p[perm[0]], p[perm[1]], p[perm[2]]).at(10.0f, 11.0f);
        EXPECT_NEAR(w.a, refByPoint[perm[0]], kEps);
        EXPECT_NEAR(w.b, refByPoint[perm[1]], kEps);
        EXPECT_NEAR(w.c, refByPoint[perm[2]], kEps);
    } while (std::next_permutation(perm.begin(), perm.end()));
}

TEST(BarycentricTest, RasterizedPixelsHaveWeightsInUnitRange) {
    // Every pixel center the rasterizer fills lies in the triangle: weights in [0, 1] up to rounding
    std::mt19937 gen(4);
    const Rasterizer raster(100, 100);
    for (int k = 0; k < 30; ++k) {
        const auto t = randomTriangle(gen);
        const Barycentric bary(t[0], t[1], t[2]);
        int outside = 0, filled = 0;
        raster.fillTriangle(t[0], t[1], t[2], [&](const int x, const int y) {
            ++filled;
            const Weights w = bary.at(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
            for (const float v : {w.a, w.b, w.c})
                if (v < -1e-5f || v > 1.0f + 1e-5f)
                    ++outside;
        });
        EXPECT_GT(filled, 0);
        EXPECT_EQ(outside, 0);
    }
}

// ---------- degenerate triangles ----------

TEST(BarycentricTest, ZeroAreaTrianglesAreDegenerate) {
    EXPECT_TRUE(Barycentric(pt(5, 5), pt(5, 5), pt(5, 5)).degenerate()) << "single point";
    EXPECT_TRUE(Barycentric(pt(1, 1), pt(9, 9), pt(1, 1)).degenerate()) << "repeated vertex";
    EXPECT_TRUE(Barycentric(pt(1, 1), pt(5, 5), pt(9, 9)).degenerate()) << "collinear";
    EXPECT_TRUE(Barycentric(pt(2, 3), pt(8, 3), pt(20, 3)).degenerate()) << "horizontal";
    EXPECT_TRUE(Barycentric(pt(0, 0), pt(1e-4f, 0), pt(0, 1e-4f)).degenerate()) << "area below kMinArea2";
}

TEST(BarycentricTest, ProperTrianglesAreNotDegenerateInEitherOrientation) {
    EXPECT_FALSE(Barycentric(pt(0, 0), pt(4, 0), pt(0, 4)).degenerate());
    EXPECT_FALSE(Barycentric(pt(0, 0), pt(0, 4), pt(4, 0)).degenerate()) << "clockwise";
    EXPECT_FALSE(Barycentric(pt(0, 0), pt(0.1f, 0), pt(0, 0.1f)).degenerate()) << "smaller than a pixel";
}
