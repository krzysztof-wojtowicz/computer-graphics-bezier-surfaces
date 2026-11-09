#include "renderer/Rasterizer.h"
#include "renderer/Projection.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <random>
#include <set>
#include <span>
#include <utility>
#include <vector>

namespace {
using Pixel = std::pair<int, int>; // (x, y)

/// How many times each pixel of a w x h image was filled; also catches indices outside the image
struct Coverage {
    int width, height;
    std::vector<int> count;
    int outOfRange = 0;

    Coverage(const int w, const int h) : width(w), height(h), count(static_cast<std::size_t>(w * h), 0) {}

    void operator()(const int x, const int y) {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            ++outOfRange;
            return;
        }
        ++count[static_cast<std::size_t>(y * width + x)];
    }

    [[nodiscard]] int at(const int x, const int y) const { return count[static_cast<std::size_t>(y * width + x)]; }

    [[nodiscard]] int total() const {
        int sum = 0;
        for (const int c : count) sum += c;
        return sum;
    }
};

ScreenPoint pt(const float x, const float y) {
    return {.x = x, .y = y, .z = 0.0f};
}

std::set<Pixel> trianglePixels(const Rasterizer &raster, const ScreenPoint &a, const ScreenPoint &b,
                               const ScreenPoint &c) {
    std::set<Pixel> res;
    raster.fillTriangle(a, b, c, [&](const int x, const int y) { res.emplace(x, y); });
    return res;
}

std::set<Pixel> polygonPixels(const Rasterizer &raster, const std::span<const ScreenPoint> points) {
    std::set<Pixel> res;
    raster.fillPolygon(points, [&](const int x, const int y) { res.emplace(x, y); });
    return res;
}

/// All pixels of the integer rectangle [x0, x1) x [y0, y1)
std::set<Pixel> rectPixels(const int x0, const int y0, const int x1, const int y1) {
    std::set<Pixel> res;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            res.emplace(x, y);
    return res;
}

/// Signed distance of p from the line through a, b (sign tells the side)
double signedDistance(const ScreenPoint &a, const ScreenPoint &b, const double px, const double py) {
    const double ex = b.x - a.x, ey = b.y - a.y;
    return (ex * (py - a.y) - ey * (px - a.x)) / std::hypot(ex, ey);
}

enum class Side { Inside, Outside, OnBorder };

/// Independent reference: even-odd test of the point against the polygon, "on border" within eps
Side classify(const std::span<const ScreenPoint> poly, const double px, const double py, const double eps = 1e-3) {
    bool inside = false;
    const std::size_t n = poly.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const ScreenPoint &a = poly[j], &b = poly[i];
        const double minX = std::min(a.x, b.x) - eps, maxX = std::max(a.x, b.x) + eps;
        const double minY = std::min(a.y, b.y) - eps, maxY = std::max(a.y, b.y) + eps;
        const bool degenerate = a.x == b.x && a.y == b.y;
        if (!degenerate && px >= minX && px <= maxX && py >= minY && py <= maxY &&
            std::abs(signedDistance(a, b, px, py)) < eps)
            return Side::OnBorder;
        if ((a.y > py) != (b.y > py) && px < a.x + (py - a.y) * (b.x - a.x) / (b.y - a.y))
            inside = !inside;
    }
    return inside ? Side::Inside : Side::Outside;
}

/// Every pixel whose center is clearly inside is filled once, clearly outside is not filled;
/// centers on the border may go either way (that's what the half-open rule decides)
void expectMatchesReference(const std::span<const ScreenPoint> poly, const int w, const int h) {
    const Rasterizer raster(w, h);
    Coverage cov(w, h);
    raster.fillPolygon(poly, [&](const int x, const int y) { cov(x, y); });
    EXPECT_EQ(cov.outOfRange, 0);
    int wrong = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const Side side = classify(poly, x + 0.5, y + 0.5);
            const int c = cov.at(x, y);
            if ((side == Side::Inside && c != 1) || (side == Side::Outside && c != 0) || c > 1)
                ++wrong;
        }
    EXPECT_EQ(wrong, 0);
}

/// Grid of (n + 1)^2 points over [x0, x1] x [y0, y1], inner points moved randomly by up to `jitter`
std::vector<ScreenPoint> jitteredGrid(const int n, const float x0, const float y0, const float x1, const float y1,
                                      const float jitter, const unsigned seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> offset(-jitter, jitter);
    std::vector<ScreenPoint> res;
    for (int i = 0; i <= n; ++i)
        for (int j = 0; j <= n; ++j) {
            float x = x0 + (x1 - x0) * static_cast<float>(i) / static_cast<float>(n);
            float y = y0 + (y1 - y0) * static_cast<float>(j) / static_cast<float>(n);
            if (i > 0 && i < n) x += offset(gen);
            if (j > 0 && j < n) y += offset(gen);
            res.push_back(pt(x, y));
        }
    return res;
}
} // namespace

// ---------- known pixels ----------

TEST(RasterizerTest, RightTriangleFillsPixelsWithCentersInside) {
    // Hypotenuse x + y = 4 goes through pixel centers, those are on the right side -> excluded
    const Rasterizer raster(8, 8);
    const std::set<Pixel> expected = {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {0, 2}};
    EXPECT_EQ(trianglePixels(raster, pt(0, 0), pt(4, 0), pt(0, 4)), expected);
}

TEST(RasterizerTest, TwoTrianglesOfSquareCoverEveryPixelOnce) {
    const Rasterizer raster(8, 8);
    Coverage cov(8, 8);
    auto fn = [&](const int x, const int y) { cov(x, y); };
    raster.fillTriangle(pt(0, 0), pt(4, 0), pt(0, 4), fn);
    EXPECT_EQ(cov.total(), 6);
    raster.fillTriangle(pt(4, 0), pt(4, 4), pt(0, 4), fn);
    EXPECT_EQ(cov.total(), 16);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            EXPECT_EQ(cov.at(x, y), x < 4 && y < 4 ? 1 : 0) << "pixel " << x << ", " << y;
}

TEST(RasterizerTest, RectangleWithIntegerCornersFillsExactPixels) {
    const Rasterizer raster(16, 16);
    const std::array rect = {pt(2, 3), pt(7, 3), pt(7, 9), pt(2, 9)};
    EXPECT_EQ(polygonPixels(raster, rect), rectPixels(2, 3, 7, 9));
}

TEST(RasterizerTest, EdgesThroughPixelCentersAreClosedOnTopLeftOpenOnBottomRight) {
    // Centers x = 2.5, 3.5, 4.5 and y = 0.5, 1.5, 2.5 are in, x = 5.5 and y = 3.5 (on the border) are out
    const Rasterizer raster(16, 16);
    const std::array rect = {pt(2.5f, 0.5f), pt(5.5f, 0.5f), pt(5.5f, 3.5f), pt(2.5f, 3.5f)};
    EXPECT_EQ(polygonPixels(raster, rect), rectPixels(2, 0, 5, 3));
}

TEST(RasterizerTest, ConcavePolygonFillsExactPixels) {
    // L shape: [1, 11) x [1, 4) plus [1, 4) x [4, 9)
    const Rasterizer raster(16, 16);
    const std::array l = {pt(1, 1), pt(11, 1), pt(11, 4), pt(4, 4), pt(4, 9), pt(1, 9)};
    std::set<Pixel> expected = rectPixels(1, 1, 11, 4);
    expected.merge(rectPixels(1, 4, 4, 9));
    EXPECT_EQ(polygonPixels(raster, l), expected);
}

TEST(RasterizerTest, FlatTopAndFlatBottomTrianglesMatchReference) {
    const std::array flatTop = {pt(1.3f, 2.0f), pt(17.8f, 2.0f), pt(6.1f, 15.7f)};
    const std::array flatBottom = {pt(9.2f, 0.4f), pt(1.1f, 13.0f), pt(18.6f, 13.0f)};
    expectMatchesReference(flatTop, 20, 20);
    expectMatchesReference(flatBottom, 20, 20);
}

TEST(RasterizerTest, RandomPolygonsMatchReference) {
    std::mt19937 gen(7);
    std::uniform_real_distribution<float> coord(-5.0f, 45.0f);
    for (int k = 0; k < 50; ++k) {
        const std::array tri = {pt(coord(gen), coord(gen)), pt(coord(gen), coord(gen)), pt(coord(gen), coord(gen))};
        expectMatchesReference(tri, 40, 40);
    }
    // non-convex, possibly self-intersecting: the even-odd rule still holds
    for (int k = 0; k < 20; ++k) {
        std::vector<ScreenPoint> poly;
        for (int i = 0; i < 7; ++i)
            poly.push_back(pt(coord(gen), coord(gen)));
        expectMatchesReference(poly, 40, 40);
    }
}

TEST(RasterizerTest, PixelCountApproximatesArea) {
    std::mt19937 gen(3);
    std::uniform_real_distribution<float> coord(0.0f, 200.0f);
    const Rasterizer raster(200, 200);
    for (int k = 0; k < 20; ++k) {
        const ScreenPoint a = pt(coord(gen), coord(gen)), b = pt(coord(gen), coord(gen)), c = pt(coord(gen), coord(gen));
        int count = 0;
        raster.fillTriangle(a, b, c, [&](int, int) { ++count; });
        const float area = 0.5f * std::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y));
        const float perimeter = std::hypot(b.x - a.x, b.y - a.y) + std::hypot(c.x - b.x, c.y - b.y) +
                                std::hypot(a.x - c.x, a.y - c.y);
        EXPECT_NEAR(static_cast<float>(count), area, perimeter);
    }
}

// ---------- no gaps and no overlaps ----------

TEST(RasterizerTest, JitteredMeshCoversEveryPixelExactlyOnce) {
    // Split like Mesh: cell (a, b, c, d) -> (a, b, d) and (a, d, c). Shared edges must not leave gaps
    // nor be filled twice; the outer border is an axis-aligned rectangle, so the expected set is exact.
    constexpr int w = 64, h = 48, n = 9;
    constexpr float x0 = 3.3f, y0 = 2.1f, x1 = 57.7f, y1 = 44.9f;
    const Rasterizer raster(w, h);
    for (const unsigned seed : {1u, 2u, 3u}) {
        const std::vector<ScreenPoint> v = jitteredGrid(n, x0, y0, x1, y1, 1.5f, seed);
        auto id = [&](const int i, const int j) { return static_cast<std::size_t>(i * (n + 1) + j); };
        Coverage cov(w, h);
        auto fn = [&](const int x, const int y) { cov(x, y); };
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j) {
                const ScreenPoint &a = v[id(i, j)], &b = v[id(i + 1, j)], &c = v[id(i, j + 1)], &d = v[id(i + 1, j + 1)];
                raster.fillTriangle(a, b, d, fn);
                raster.fillTriangle(a, d, c, fn);
            }
        int wrong = 0;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const float cx = static_cast<float>(x) + 0.5f, cy = static_cast<float>(y) + 0.5f;
                const bool inside = cx >= x0 && cx < x1 && cy >= y0 && cy < y1;
                if (cov.at(x, y) != (inside ? 1 : 0))
                    ++wrong;
            }
        EXPECT_EQ(wrong, 0) << "seed " << seed;
        EXPECT_EQ(cov.outOfRange, 0);
    }
}

TEST(RasterizerTest, FanAroundVertexCoversEveryPixelAtMostOnce) {
    // Many edges meet in one vertex; the fan covers a convex polygon
    constexpr int w = 60, h = 60, sides = 13;
    const ScreenPoint center = pt(30.27f, 29.81f);
    std::vector<ScreenPoint> rim;
    for (int k = 0; k < sides; ++k) {
        const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(k) / sides + 0.1f;
        rim.push_back(pt(center.x + 25.0f * std::cos(angle), center.y + 25.0f * std::sin(angle)));
    }
    const Rasterizer raster(w, h);
    Coverage cov(w, h);
    for (int k = 0; k < sides; ++k)
        raster.fillTriangle(center, rim[k], rim[(k + 1) % sides], [&](const int x, const int y) { cov(x, y); });
    int wrong = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const Side side = classify(rim, x + 0.5, y + 0.5);
            const int c = cov.at(x, y);
            if (c > 1 || (side == Side::Inside && c != 1) || (side == Side::Outside && c != 0))
                ++wrong;
        }
    EXPECT_EQ(wrong, 0);
}

TEST(RasterizerTest, VertexOrderDoesNotMatter) {
    const Rasterizer raster(40, 40);
    const std::array<ScreenPoint, 3> p = {pt(3.7f, 31.2f), pt(25.1f, 4.4f), pt(36.9f, 22.6f)};
    const std::set<Pixel> expected = trianglePixels(raster, p[0], p[1], p[2]);
    ASSERT_FALSE(expected.empty());
    std::array<int, 3> perm = {0, 1, 2};
    do {
        EXPECT_EQ(trianglePixels(raster, p[perm[0]], p[perm[1]], p[perm[2]]), expected)
            << "order " << perm[0] << perm[1] << perm[2];
    } while (std::next_permutation(perm.begin(), perm.end()));
}

TEST(RasterizerTest, SharedEdgeGivenInAnyOrderIsFilledOnce) {
    // Quad a, b, c, d split along b-d; every vertex order of both triangles covers the quad exactly once
    const ScreenPoint a = pt(2.3f, 3.1f), b = pt(27.6f, 5.9f), c = pt(24.2f, 28.7f), d = pt(4.9f, 22.4f);
    const std::array quad = {a, b, c, d};
    const std::array<ScreenPoint, 3> t1 = {a, b, d}, t2 = {b, c, d};
    const Rasterizer raster(32, 32);
    std::array<int, 3> p1 = {0, 1, 2};
    do {
        std::array<int, 3> p2 = {0, 1, 2};
        do {
            Coverage cov(32, 32);
            auto fn = [&](const int x, const int y) { cov(x, y); };
            raster.fillTriangle(t1[p1[0]], t1[p1[1]], t1[p1[2]], fn);
            raster.fillTriangle(t2[p2[0]], t2[p2[1]], t2[p2[2]], fn);
            int wrong = 0;
            for (int y = 0; y < 32; ++y)
                for (int x = 0; x < 32; ++x) {
                    const Side side = classify(quad, x + 0.5, y + 0.5);
                    const int count = cov.at(x, y);
                    if (count > 1 || (side == Side::Inside && count != 1) || (side == Side::Outside && count != 0))
                        ++wrong;
                }
            EXPECT_EQ(wrong, 0);
        } while (std::next_permutation(p2.begin(), p2.end()));
    } while (std::next_permutation(p1.begin(), p1.end()));
}

TEST(RasterizerTest, EdgeThroughPixelCenterIsSharedExactly) {
    // A random edge through the pixel center C: with float rounding, x computed from one end can land
    // just left of C and from the other end just right of it. Both triangles must compute the edge
    // the same way, otherwise C is filled twice or not at all. The triangles list the shared
    // vertices in opposite orders, like neighbouring cells of a mesh.
    std::mt19937 gen(11);
    std::uniform_real_distribution<float> angle(0.2f, std::numbers::pi_v<float> - 0.2f);
    std::uniform_real_distribution<float> length(3.0f, 14.0f);
    const Rasterizer raster(64, 64);
    const ScreenPoint c = pt(31.5f, 32.5f);
    int wrong = 0;
    for (int k = 0; k < 2000; ++k) {
        const float a = angle(gen), t1 = length(gen), t2 = length(gen);
        const float dx = std::cos(a), dy = std::sin(a); // dy > 0, the edge is never horizontal
        const ScreenPoint p = pt(c.x - t1 * dx, c.y - t1 * dy), q = pt(c.x + t2 * dx, c.y + t2 * dy);
        const ScreenPoint left = pt(c.x - 20.0f, c.y), right = pt(c.x + 20.0f, c.y);
        Coverage cov(64, 64);
        auto fn = [&](const int x, const int y) { cov(x, y); };
        raster.fillTriangle(p, q, left, fn);
        raster.fillTriangle(q, p, right, fn);
        if (cov.at(31, 32) != 1)
            ++wrong;
    }
    EXPECT_EQ(wrong, 0);
}

// ---------- degenerate input ----------

TEST(RasterizerTest, LessThanThreePointsFillNothing) {
    const Rasterizer raster(10, 10);
    EXPECT_TRUE(polygonPixels(raster, std::vector<ScreenPoint>{}).empty());
    EXPECT_TRUE(polygonPixels(raster, std::array{pt(1, 1)}).empty());
    EXPECT_TRUE(polygonPixels(raster, std::array{pt(1, 1), pt(8, 8)}).empty());
}

TEST(RasterizerTest, ZeroAreaTrianglesFillNothing) {
    const Rasterizer raster(20, 20);
    EXPECT_TRUE(trianglePixels(raster, pt(5, 5), pt(5, 5), pt(5, 5)).empty()) << "single point";
    EXPECT_TRUE(trianglePixels(raster, pt(2, 5.5f), pt(9, 5.5f), pt(15, 5.5f)).empty()) << "horizontal";
    EXPECT_TRUE(trianglePixels(raster, pt(5.5f, 2), pt(5.5f, 9), pt(5.5f, 15)).empty()) << "vertical";
    EXPECT_TRUE(trianglePixels(raster, pt(1, 1), pt(5, 5), pt(9, 9)).empty()) << "diagonal";
    EXPECT_TRUE(trianglePixels(raster, pt(1, 1), pt(9, 9), pt(1, 1)).empty()) << "repeated vertex";
}

TEST(RasterizerTest, TriangleSmallerThanPixelMayFillNothing) {
    // No pixel center inside -> nothing; that's correct, neighbours cover the area
    const Rasterizer raster(10, 10);
    EXPECT_TRUE(trianglePixels(raster, pt(3.1f, 3.1f), pt(3.4f, 3.1f), pt(3.1f, 3.4f)).empty());
    EXPECT_EQ(trianglePixels(raster, pt(3.1f, 3.1f), pt(3.95f, 3.1f), pt(3.1f, 3.95f)), (std::set<Pixel>{{3, 3}}));
}

// ---------- clipping ----------

TEST(RasterizerTest, PartlyOutsideTrianglesAreClippedToImage) {
    constexpr int w = 30, h = 20;
    const std::array<std::array<ScreenPoint, 3>, 5> triangles = {{
        {pt(-12.3f, 3.1f), pt(14.2f, 6.7f), pt(-4.9f, 17.6f)},  // left
        {pt(18.4f, 2.2f), pt(41.7f, 9.3f), pt(22.6f, 18.1f)},   // right
        {pt(3.3f, -9.8f), pt(26.1f, -4.2f), pt(13.7f, 11.4f)},  // top
        {pt(4.2f, 12.6f), pt(27.3f, 31.5f), pt(9.1f, 26.6f)},   // bottom
        {pt(-20.5f, -15.2f), pt(55.1f, -3.3f), pt(7.7f, 41.9f)} // all sides
    }};
    for (const auto &t : triangles)
        expectMatchesReference(t, w, h); // also checks that nothing lands outside [0, w) x [0, h)
}

TEST(RasterizerTest, TriangleOutsideImageFillsNothing) {
    const Rasterizer raster(30, 20);
    EXPECT_TRUE(trianglePixels(raster, pt(-10, -10), pt(-1, -10), pt(-5, -2)).empty());
    EXPECT_TRUE(trianglePixels(raster, pt(31, 5), pt(40, 2), pt(45, 15)).empty());
    EXPECT_TRUE(trianglePixels(raster, pt(5, 21), pt(25, 21), pt(15, 30)).empty());
}

TEST(RasterizerTest, HugeTriangleFillsWholeImageOnce) {
    constexpr int w = 37, h = 23;
    const Rasterizer raster(w, h);
    Coverage cov(w, h);
    raster.fillTriangle(pt(-1000, -1000), pt(2000, -1000), pt(-1000, 2000), [&](const int x, const int y) { cov(x, y); });
    EXPECT_EQ(cov.outOfRange, 0);
    EXPECT_EQ(cov.total(), w * h);
    EXPECT_EQ(*std::ranges::max_element(cov.count), 1);
}

TEST(RasterizerTest, VerticesOnImageBorderStayInside) {
    // x = w and y = h are already past the last pixel; Projection can give exactly those values
    constexpr int w = 20, h = 10;
    const Rasterizer raster(w, h);
    Coverage cov(w, h);
    raster.fillTriangle(pt(0, 0), pt(w, 0), pt(w, h), [&](const int x, const int y) { cov(x, y); });
    raster.fillTriangle(pt(0, 0), pt(w, h), pt(0, h), [&](const int x, const int y) { cov(x, y); });
    EXPECT_EQ(cov.outOfRange, 0);
    EXPECT_EQ(cov.total(), w * h);
}

// ---------- invalid arguments ----------

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(RasterizerDeathTest, RejectsEmptyImage) {
    EXPECT_DEATH(Rasterizer(0, 10), "must be > 0");
    EXPECT_DEATH(Rasterizer(10, 0), "must be > 0");
    EXPECT_DEATH(Rasterizer(-5, 10), "must be > 0");
}

TEST(RasterizerDeathTest, RejectsNonFiniteVertex) {
    const Rasterizer raster(10, 10);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_DEATH(raster.fillTriangle(pt(nan, 1), pt(5, 1), pt(1, 5), [](int, int) {}), "must be finite");
    EXPECT_DEATH(raster.fillTriangle(pt(1, 1), pt(5, inf), pt(1, 5), [](int, int) {}), "must be finite");
}
#endif
