// Black-box tests of Viewport (stage 4.1 and 4.3): the widget is rendered with QWidget::grab()
// and the resulting image is inspected. Only rotation-independent properties are checked,
// so the tests don't depend on the initial rotation chosen in Viewport.

#include "ui/Viewport.h"

#include "geometry/BezierSurface.h"
#include "geometry/SurfaceParser.h"
#include "math/Vec3.h"
#include "renderer/Projection.h"

#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QSize>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
// Upper bound for the radius of a drawn point (Viewport::kPointRadius) plus antialiasing
constexpr float kMaxDotRadius = 5.0f;

/// Surface with all 16 control points in p: every mesh vertex lies in p, so the mesh is drawn as a single dot
BezierSurface pointSurface(const Vec3 &p) {
    BezierSurface::ControlGrid g;
    for (auto &row : g)
        row.fill(p);
    return BezierSurface(g);
}

/// Surface with control points in the corners of the [-a, a]^3 cube, |P| = a * sqrt(3) (worst case for the scene radius)
BezierSurface cubeCornerSurface(const float a) {
    BezierSurface::ControlGrid g;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            g[i][j] = {.x = (i % 2 == 0) ? -a : a, .y = (j % 2 == 0) ? -a : a, .z = ((i + j) % 2 == 0) ? a : -a};
    return BezierSurface(g);
}

std::vector<BezierSurface> assetSurfaces() {
    std::ifstream file(BEZIER_ASSETS_DIR "/surfaces.txt", std::ios::binary);
    std::stringstream buffer;
    buffer << file.rdbuf();
    ParseResult result = parseSurfaces(buffer.str());
    EXPECT_TRUE(result.ok()) << result.error;
    return result.surfaces;
}

/// Resizes the viewport and renders it like on screen (resizeEvent + paintEvent)
QImage renderViewport(Viewport &viewport, const QSize &size) {
    viewport.resize(size);
    return viewport.grab().toImage().convertToFormat(QImage::Format_RGB32);
}

bool isWhite(const QImage &img, const int x, const int y) {
    return (img.pixel(x, y) & 0xFFFFFFu) == 0xFFFFFFu;
}

/// All non-white pixels of the image
std::vector<QPoint> drawnPixels(const QImage &img) {
    std::vector<QPoint> res;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (!isWhite(img, x, y))
                res.emplace_back(x, y);
    return res;
}

QRect boundingBox(const std::vector<QPoint> &points) {
    QRect box;
    for (const QPoint &p : points)
        box |= QRect(p, QSize(1, 1));
    return box;
}

/// Largest distance of a drawn pixel center from the image center
float maxDistanceFromCenter(const QImage &img, const std::vector<QPoint> &points) {
    const float cx = 0.5f * static_cast<float>(img.width()), cy = 0.5f * static_cast<float>(img.height());
    float res = 0.0f;
    for (const QPoint &p : points)
        res = std::max(res, std::hypot(static_cast<float>(p.x()) + 0.5f - cx, static_cast<float>(p.y()) + 0.5f - cy));
    return res;
}

void expectDotAtCenter(const QImage &img) {
    const std::vector<QPoint> pixels = drawnPixels(img);
    ASSERT_FALSE(pixels.empty()) << "nothing drawn";
    const QRect box = boundingBox(pixels);
    const float boxCx = static_cast<float>(box.left()) + 0.5f * static_cast<float>(box.width());
    const float boxCy = static_cast<float>(box.top()) + 0.5f * static_cast<float>(box.height());
    EXPECT_NEAR(boxCx, 0.5f * static_cast<float>(img.width()), 1.0f);
    EXPECT_NEAR(boxCy, 0.5f * static_cast<float>(img.height()), 1.0f);
    EXPECT_LE(box.width(), static_cast<int>(2 * kMaxDotRadius)) << "expected a single small dot";
    EXPECT_LE(box.height(), static_cast<int>(2 * kMaxDotRadius)) << "expected a single small dot";
}
} // namespace

// ---------- 4.1: meshes in Viewport ----------

TEST(ViewportTest, WithoutSurfacesImageIsWhite) {
    Viewport viewport;
    const QImage img = renderViewport(viewport, {400, 400});
    ASSERT_EQ(img.size(), QSize(400, 400));
    EXPECT_TRUE(drawnPixels(img).empty());
}

TEST(ViewportTest, EmptySurfaceListGivesWhiteImage) {
    Viewport viewport;
    viewport.setSurfaces({});
    EXPECT_TRUE(drawnPixels(renderViewport(viewport, {400, 400})).empty());
}

TEST(ViewportTest, SetSurfacesBeforeFirstResizeDrawsAfterResize) {
    // MainWindow calls setSurfaces in its constructor, before the first resizeEvent (m_frame is still null)
    Viewport viewport;
    viewport.setSurfaces({pointSurface({0.0f, 0.0f, 0.0f})});
    expectDotAtCenter(renderViewport(viewport, {400, 400}));
}

TEST(ViewportTest, SetSurfacesAfterResizeRedraws) {
    Viewport viewport;
    ASSERT_TRUE(drawnPixels(renderViewport(viewport, {400, 400})).empty());
    viewport.setSurfaces({pointSurface({0.0f, 0.0f, 0.0f})});
    expectDotAtCenter(viewport.grab().toImage().convertToFormat(QImage::Format_RGB32));
}

TEST(ViewportTest, SetSurfacesReplacesPreviousMeshes) {
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    viewport.setSurfaces({pointSurface({0.0f, 0.0f, 0.0f})});
    expectDotAtCenter(renderViewport(viewport, {400, 400})); // only the new surface, old meshes cleared
}

TEST(ViewportTest, SetSurfacesWithEmptyListClearsImage) {
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    ASSERT_FALSE(drawnPixels(renderViewport(viewport, {400, 400})).empty());
    viewport.setSurfaces({});
    EXPECT_TRUE(drawnPixels(renderViewport(viewport, {400, 400})).empty());
}

TEST(ViewportTest, EverySurfaceGetsAMesh) {
    // Two single-dot surfaces: one at the origin (always drawn in the center), one far from it.
    // Removing the first surface must remove its dot.
    Viewport viewport;
    viewport.setSurfaces({pointSurface({0.0f, 0.0f, 0.0f}), pointSurface({3.0f, 3.0f, 3.0f})});
    const QImage both = renderViewport(viewport, {400, 400});
    EXPECT_FALSE(isWhite(both, 200, 200));

    viewport.setSurfaces({pointSurface({3.0f, 3.0f, 3.0f})});
    const QImage onlySecond = renderViewport(viewport, {400, 400});
    EXPECT_FALSE(drawnPixels(onlySecond).empty());
    EXPECT_GT(drawnPixels(both).size(), drawnPixels(onlySecond).size());
}

// ---------- 4.3: drawing mesh points ----------

TEST(ViewportTest, PointAtOriginIsDrawnInCenterOfSquareImage) {
    Viewport viewport;
    viewport.setSurfaces({pointSurface({0.0f, 0.0f, 0.0f})});
    expectDotAtCenter(renderViewport(viewport, {400, 400}));
}

TEST(ViewportTest, PointAtOriginIsDrawnInCenterOfWideAndTallImage) {
    Viewport viewport;
    viewport.setSurfaces({pointSurface({0.0f, 0.0f, 0.0f})});
    expectDotAtCenter(renderViewport(viewport, {700, 400}));
    expectDotAtCenter(renderViewport(viewport, {400, 700}));
}

TEST(ViewportTest, ResizeRecreatesFrameWithNewSize) {
    Viewport viewport;
    viewport.setSurfaces({pointSurface({0.0f, 0.0f, 0.0f})});
    EXPECT_EQ(renderViewport(viewport, {400, 400}).size(), QSize(400, 400));
    const QImage resized = renderViewport(viewport, {640, 480});
    EXPECT_EQ(resized.size(), QSize(640, 480));
    expectDotAtCenter(resized); // the dot follows the new center
}

TEST(ViewportTest, AssetSurfacesAreDrawn) {
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    const QImage img = renderViewport(viewport, {400, 400});
    const std::vector<QPoint> pixels = drawnPixels(img);
    // (n + 1)^2 = 121 dots, partly overlapping: certainly much more than a single dot
    EXPECT_GT(pixels.size(), 500u);
    // Corners of the image stay white: the scene is in the middle, not stretched to the edges
    EXPECT_TRUE(isWhite(img, 0, 0));
    EXPECT_TRUE(isWhite(img, 399, 0));
    EXPECT_TRUE(isWhite(img, 0, 399));
    EXPECT_TRUE(isWhite(img, 399, 399));
}

TEST(ViewportTest, MeshIsNotDrawnFlatFromAbove) {
    // With the identity rotation the camera looks from the top and the dome's x, y grid is perfectly regular:
    // the bounding box of the drawn points would be a square. The initial rotation tilts the scene, so it is not.
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    const QRect box = boundingBox(drawnPixels(renderViewport(viewport, {400, 400})));
    ASSERT_FALSE(box.isEmpty());
    EXPECT_GT(std::abs(box.width() - box.height()), 4);
}

// The drawn scene always fits inside the circle of radius min(w, h) / 2 around the image center,
// whatever the rotation is, as long as all control points lie within Projection::kSceneRadius.
class ViewportFitTest : public testing::TestWithParam<QSize> {};

TEST_P(ViewportFitTest, AssetSurfacesStayInsideSceneCircle) {
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    const QSize size = GetParam();
    const QImage img = renderViewport(viewport, size);
    const std::vector<QPoint> pixels = drawnPixels(img);
    ASSERT_FALSE(pixels.empty());
    const float limit = 0.5f * static_cast<float>(std::min(size.width(), size.height())) + kMaxDotRadius;
    EXPECT_LE(maxDistanceFromCenter(img, pixels), limit);
}

TEST_P(ViewportFitTest, WorstCaseSurfaceStaysInsideImage) {
    // Control points as far as allowed: |P| = kSceneRadius (corners of a cube)
    const float a = Projection::kSceneRadius / std::sqrt(3.0f);
    Viewport viewport;
    viewport.setSurfaces({cubeCornerSurface(a)});
    const QSize size = GetParam();
    const QImage img = renderViewport(viewport, size);
    const std::vector<QPoint> pixels = drawnPixels(img);
    ASSERT_FALSE(pixels.empty());
    const float limit = 0.5f * static_cast<float>(std::min(size.width(), size.height())) + kMaxDotRadius;
    EXPECT_LE(maxDistanceFromCenter(img, pixels), limit);
}

INSTANTIATE_TEST_SUITE_P(Sizes, ViewportFitTest,
                         testing::Values(QSize(400, 400), QSize(800, 450), QSize(450, 800), QSize(1001, 601)));
