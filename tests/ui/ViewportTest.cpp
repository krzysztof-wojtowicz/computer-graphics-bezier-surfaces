// Black-box tests of Viewport (stages 4.1-4.6): the widget is rendered with QWidget::grab()
// and the resulting image is inspected. Expected positions are computed from Viewport::rotation(),
// so the tests don't depend on the initial rotation chosen in Viewport.

#include "ui/Viewport.h"

#include "geometry/BezierSurface.h"
#include "geometry/Mesh.h"
#include "geometry/SurfaceParser.h"
#include "math/Vec3.h"
#include "renderer/MouseRotation.h"
#include "renderer/Projection.h"
#include "math/TestUtils.h"

#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QImage>
#include <QMouseEvent>
#include <QPointF>
#include <QPixmap>
#include <QRect>
#include <QSize>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>
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

// ---------- 4.4 / 4.5: rotation with the mouse ----------

namespace {
/// Sends a mouse event directly to the widget (QTest::mouseMove doesn't reliably pass the held buttons)
void sendMouse(Viewport &viewport, const QEvent::Type type, const QPoint &pos, const Qt::MouseButton button,
               const Qt::MouseButtons buttons) {
    const QPointF local(pos);
    QMouseEvent event(type, local, viewport.mapToGlobal(local), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&viewport, &event);
}

void press(Viewport &viewport, const QPoint &pos, const Qt::MouseButton button = Qt::LeftButton) {
    sendMouse(viewport, QEvent::MouseButtonPress, pos, button, button);
}

void move(Viewport &viewport, const QPoint &pos, const Qt::MouseButtons buttons = Qt::LeftButton) {
    sendMouse(viewport, QEvent::MouseMove, pos, Qt::NoButton, buttons);
}

void release(Viewport &viewport, const QPoint &pos, const Qt::MouseButton button = Qt::LeftButton) {
    sendMouse(viewport, QEvent::MouseButtonRelease, pos, button, Qt::NoButton);
}

/// Full drag with the left button: press at `from`, one move to `to`, release
void drag(Viewport &viewport, const QPoint &from, const QPoint &to) {
    press(viewport, from);
    move(viewport, to);
    release(viewport, to);
}
} // namespace

TEST(ViewportMouseTest, LeftDragAppliesDragRotation) {
    Viewport viewport;
    renderViewport(viewport, {400, 400});
    const Mat3 initial = viewport.rotation();
    drag(viewport, {100, 100}, {130, 90});
    EXPECT_MAT3_EQ(viewport.rotation(), applyDrag(initial, 30.0f, -10.0f));
}

TEST(ViewportMouseTest, EveryMoveEventAddsItsOwnDelta) {
    // Deltas are measured from the previous event, not from the press position
    Viewport viewport;
    renderViewport(viewport, {400, 400});
    const Mat3 initial = viewport.rotation();
    press(viewport, {100, 100});
    move(viewport, {110, 100});
    move(viewport, {110, 125});
    release(viewport, {110, 125});
    EXPECT_MAT3_EQ(viewport.rotation(), applyDrag(applyDrag(initial, 10.0f, 0.0f), 0.0f, 25.0f));
}

TEST(ViewportMouseTest, NewPressDoesNotJumpFromPreviousDrag) {
    // The second drag starts far from where the first one ended: only its own movement counts
    Viewport viewport;
    renderViewport(viewport, {400, 400});
    const Mat3 initial = viewport.rotation();
    drag(viewport, {100, 100}, {150, 100});
    drag(viewport, {300, 300}, {310, 300});
    EXPECT_MAT3_EQ(viewport.rotation(), applyDrag(applyDrag(initial, 50.0f, 0.0f), 10.0f, 0.0f));
}

TEST(ViewportMouseTest, ClickWithoutMovementKeepsRotation) {
    Viewport viewport;
    renderViewport(viewport, {400, 400});
    const Mat3 initial = viewport.rotation();
    press(viewport, {200, 200});
    release(viewport, {200, 200});
    EXPECT_MAT3_EQ(viewport.rotation(), initial);
}

TEST(ViewportMouseTest, MoveWithoutButtonKeepsRotation) {
    Viewport viewport;
    renderViewport(viewport, {400, 400});
    const Mat3 initial = viewport.rotation();
    move(viewport, {100, 100}, Qt::NoButton);
    move(viewport, {250, 300}, Qt::NoButton);
    EXPECT_MAT3_EQ(viewport.rotation(), initial);
}

TEST(ViewportMouseTest, RightDragKeepsRotation) {
    Viewport viewport;
    renderViewport(viewport, {400, 400});
    const Mat3 initial = viewport.rotation();
    press(viewport, {100, 100}, Qt::RightButton);
    move(viewport, {200, 150}, Qt::RightButton);
    release(viewport, {200, 150}, Qt::RightButton);
    EXPECT_MAT3_EQ(viewport.rotation(), initial);
}

TEST(ViewportMouseTest, DragBackAndForthRestoresRotation) {
    Viewport viewport;
    renderViewport(viewport, {400, 400});
    const Mat3 initial = viewport.rotation();
    press(viewport, {200, 200});
    move(viewport, {260, 200});
    move(viewport, {200, 200});
    release(viewport, {200, 200});
    EXPECT_MAT3_EQ(viewport.rotation(), initial);
}

TEST(ViewportMouseTest, DragRedrawsImage) {
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    const QImage before = renderViewport(viewport, {400, 400});
    drag(viewport, {100, 100}, {160, 130});
    const QImage after = viewport.grab().toImage().convertToFormat(QImage::Format_RGB32);
    EXPECT_NE(before, after);
}

TEST(ViewportMouseTest, DragBackAndForthRestoresImage) {
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    const QImage before = renderViewport(viewport, {400, 400});
    press(viewport, {200, 200});
    move(viewport, {260, 200});
    move(viewport, {200, 200});
    release(viewport, {200, 200});
    const QImage after = viewport.grab().toImage().convertToFormat(QImage::Format_RGB32);
    // Same rotation up to float noise: at most a few antialiased pixels may differ slightly
    int differentPixels = 0;
    for (int y = 0; y < before.height(); ++y)
        for (int x = 0; x < before.width(); ++x)
            if (before.pixel(x, y) != after.pixel(x, y))
                ++differentPixels;
    EXPECT_LE(differentPixels, 50);
}

TEST(ViewportMouseTest, DragBeforeSurfacesAndBeforeResizeDoesNotCrash) {
    Viewport viewport;
    drag(viewport, {10, 10}, {50, 40}); // no meshes, null frame
    viewport.setSurfaces(assetSurfaces());
    EXPECT_FALSE(drawnPixels(renderViewport(viewport, {400, 400})).empty());
}

TEST(ViewportMouseTest, RotationAppliesToSurfacesSetAfterDrag) {
    // setSurfaces uses the current rotation, not the initial one: a point surface away from the origin
    // must be drawn where the dragged rotation projects it
    Viewport viewport;
    renderViewport(viewport, {400, 400});
    drag(viewport, {100, 100}, {180, 140});
    const Vec3 p{3.0f, 0.0f, 0.0f};
    viewport.setSurfaces({pointSurface(p)});
    const QImage img = viewport.grab().toImage().convertToFormat(QImage::Format_RGB32);
    const ScreenPoint sp = Projection(400, 400).project(viewport.rotation() * p);
    const QRect box = boundingBox(drawnPixels(img));
    ASSERT_FALSE(box.isEmpty());
    EXPECT_NEAR(static_cast<float>(box.left()) + 0.5f * static_cast<float>(box.width()), sp.x, 1.0f);
    EXPECT_NEAR(static_cast<float>(box.top()) + 0.5f * static_cast<float>(box.height()), sp.y, 1.0f);
}

TEST(ViewportMouseTest, CursorIsClosedHandOnlyWhileDragging) {
    Viewport viewport;
    renderViewport(viewport, {400, 400});
    press(viewport, {100, 100});
    EXPECT_EQ(viewport.cursor().shape(), Qt::ClosedHandCursor);
    release(viewport, {100, 100});
    EXPECT_NE(viewport.cursor().shape(), Qt::ClosedHandCursor);
}

// After many drags in all directions the scene still fits: the rotation stays orthonormal (4.5)
TEST_P(ViewportFitTest, WorstCaseSurfaceStaysInsideImageAfterManyDrags) {
    const float a = Projection::kSceneRadius / std::sqrt(3.0f);
    Viewport viewport;
    viewport.setSurfaces({cubeCornerSurface(a)});
    const QSize size = GetParam();
    renderViewport(viewport, size);
    press(viewport, {0, 0});
    for (int i = 1; i <= 2000; ++i)
        move(viewport, {(i * 37) % 300, (i * 53) % 200});
    release(viewport, {0, 0});
    const QImage img = viewport.grab().toImage().convertToFormat(QImage::Format_RGB32);
    const std::vector<QPoint> pixels = drawnPixels(img);
    ASSERT_FALSE(pixels.empty());
    const float limit = 0.5f * static_cast<float>(std::min(size.width(), size.height())) + kMaxDotRadius;
    EXPECT_LE(maxDistanceFromCenter(img, pixels), limit);
}

// ---------- 4.6: triangle mesh ----------

namespace {
constexpr float kHalfSide = 3.5f; // |corner| = 3.5 * sqrt(2) < kSceneRadius
constexpr QSize kMeshImageSize{600, 600};

/// Flat square [-kHalfSide, kHalfSide]^2 at z = 0 with evenly spaced control points:
/// the surface is linear in (u, v), so mesh vertex (i, j) lies exactly at gridPoint(i, j)
BezierSurface flatSquare() {
    BezierSurface::ControlGrid g;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            g[i][j] = {.x = -kHalfSide + 2.0f * kHalfSide * static_cast<float>(i) / 3.0f,
                       .y = -kHalfSide + 2.0f * kHalfSide * static_cast<float>(j) / 3.0f,
                       .z = 0.0f};
    return BezierSurface(g);
}

Vec3 gridPoint(const int i, const int j) {
    const auto n = static_cast<float>(Mesh::kDefaultResolution);
    return {.x = -kHalfSide + 2.0f * kHalfSide * static_cast<float>(i) / n,
            .y = -kHalfSide + 2.0f * kHalfSide * static_cast<float>(j) / n,
            .z = 0.0f};
}

QPointF screenPoint(const Viewport &viewport, const Vec3 &p) {
    const ScreenPoint sp = Projection(kMeshImageSize.width(), kMeshImageSize.height()).project(viewport.rotation() * p);
    return {sp.x, sp.y};
}

/// A gray/black pixel (a mesh line), not white and not a colored vertex dot, in the 3x3 neighbourhood of p
bool lineNear(const QImage &img, const QPointF &p) {
    const int px = static_cast<int>(p.x()), py = static_cast<int>(p.y());
    for (int y = py - 1; y <= py + 1; ++y)
        for (int x = px - 1; x <= px + 1; ++x) {
            const QColor c = img.pixelColor(x, y);
            const bool gray = std::abs(c.red() - c.green()) < 40 && std::abs(c.red() - c.blue()) < 40;
            if (gray && c.red() < 200)
                return true;
        }
    return false;
}

QPointF midpoint(const QPointF &a, const QPointF &b) {
    return 0.5 * (a + b);
}

/// Checks the three edges (a, b), (a, c), (a, d) and both triangle interiors of the cell (i, j)
void expectCellDrawnAsTwoTriangles(const Viewport &viewport, const QImage &img, const int i, const int j) {
    const QPointF a = screenPoint(viewport, gridPoint(i, j));
    const QPointF b = screenPoint(viewport, gridPoint(i + 1, j));
    const QPointF c = screenPoint(viewport, gridPoint(i, j + 1));
    const QPointF d = screenPoint(viewport, gridPoint(i + 1, j + 1));
    EXPECT_TRUE(lineNear(img, midpoint(a, b))) << "edge a-b of cell " << i << ", " << j;
    EXPECT_TRUE(lineNear(img, midpoint(a, c))) << "edge a-c of cell " << i << ", " << j;
    EXPECT_TRUE(lineNear(img, midpoint(a, d))) << "diagonal a-d of cell " << i << ", " << j;
    EXPECT_TRUE(lineNear(img, midpoint(b, d))) << "edge b-d of cell " << i << ", " << j;
    EXPECT_TRUE(lineNear(img, midpoint(c, d))) << "edge c-d of cell " << i << ", " << j;
    // Triangles are only outlined, not filled (filling comes in stage 5)
    const QPointF abd = (a + b + d) / 3.0, adc = (a + d + c) / 3.0;
    EXPECT_TRUE(isWhite(img, static_cast<int>(abd.x()), static_cast<int>(abd.y()))) << "triangle abd filled";
    EXPECT_TRUE(isWhite(img, static_cast<int>(adc.x()), static_cast<int>(adc.y()))) << "triangle adc filled";
    // The other diagonal (b, c) is not an edge. Its midpoint lies on a-d, so check at 1/4 and 3/4 instead.
    EXPECT_FALSE(lineNear(img, b + 0.25 * (c - b))) << "unexpected diagonal b-c of cell " << i << ", " << j;
    EXPECT_FALSE(lineNear(img, b + 0.75 * (c - b))) << "unexpected diagonal b-c of cell " << i << ", " << j;
}
} // namespace

TEST(ViewportMeshTest, CellsAreDrawnAsOutlinedTriangles) {
    Viewport viewport;
    viewport.setSurfaces({flatSquare()});
    const QImage img = renderViewport(viewport, kMeshImageSize);
    for (const auto &[i, j] : {std::pair{0, 0}, std::pair{4, 5}, std::pair{9, 9}, std::pair{2, 7}})
        expectCellDrawnAsTwoTriangles(viewport, img, i, j);
}

TEST(ViewportMeshTest, TriangleEdgesFollowRotation) {
    Viewport viewport;
    viewport.setSurfaces({flatSquare()});
    renderViewport(viewport, kMeshImageSize);
    drag(viewport, {100, 100}, {150, 70});
    const QImage img = viewport.grab().toImage().convertToFormat(QImage::Format_RGB32);
    for (const auto &[i, j] : {std::pair{0, 0}, std::pair{4, 5}, std::pair{9, 9}})
        expectCellDrawnAsTwoTriangles(viewport, img, i, j);
}

TEST(ViewportMeshTest, NothingIsDrawnOutsideTheMesh) {
    // Pixels well outside the projected square stay white: lines connect only mesh vertices
    Viewport viewport;
    viewport.setSurfaces({flatSquare()});
    const QImage img = renderViewport(viewport, kMeshImageSize);
    const QPointF corner = screenPoint(viewport, gridPoint(10, 10));
    const QPointF center = screenPoint(viewport, gridPoint(5, 5));
    const QPointF dir = corner - center;
    const double len = std::hypot(dir.x(), dir.y());
    const QPointF outside = corner + (15.0 / len) * dir; // 15 px beyond the corner, on the diagonal
    ASSERT_TRUE(img.rect().contains(outside.toPoint()));
    EXPECT_TRUE(isWhite(img, static_cast<int>(outside.x()), static_cast<int>(outside.y())));
}
