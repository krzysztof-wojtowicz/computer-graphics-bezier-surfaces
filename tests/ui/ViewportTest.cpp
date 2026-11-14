// Black-box tests of Viewport (stages 4.1-4.7): the widget is rendered with QWidget::grab()
// and the resulting image is inspected. Expected positions are computed from Viewport::rotation(),
// so the tests don't depend on the initial rotation chosen in Viewport.

#include "ui/Viewport.h"

#include "geometry/BezierSurface.h"
#include "geometry/Mesh.h"
#include "geometry/SurfaceParser.h"
#include "math/Vec3.h"
#include "renderer/MouseRotation.h"
#include "renderer/Projection.h"
#include "geometry/BezierCurve.h"
#include "math/TestUtils.h"
#include "ui/ImageLoader.h"
#include "renderer/Texture.h"
#include "renderer/NormalMapping.h"

#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QMouseEvent>
#include <QPointF>
#include <QPixmap>
#include <QRect>
#include <QSize>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <tuple>
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

QImage grabImage(Viewport &viewport) {
    return viewport.grab().toImage().convertToFormat(QImage::Format_RGB32);
}

/// Shows the viewport (offscreen) and returns its image. grab() of a hidden widget sends it a resizeEvent,
/// which calls render() itself; a shown widget only gets paintEvent, so later grabs reveal a setter that forgot render()
QImage showViewport(Viewport &viewport, const QSize &size) {
    viewport.resize(size);
    viewport.show();
    QCoreApplication::processEvents();
    return grabImage(viewport);
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

/// Mesh vertex (i, j) of flatSquare() at resolution n
Vec3 gridPoint(const int i, const int j, const int n = Mesh::kDefaultResolution) {
    const auto fn = static_cast<float>(n);
    return {.x = -kHalfSide + 2.0f * kHalfSide * static_cast<float>(i) / fn,
            .y = -kHalfSide + 2.0f * kHalfSide * static_cast<float>(j) / fn,
            .z = 0.0f};
}

/// Control point P[i][j] of flatSquare()
Vec3 controlPointOfFlatSquare(const int i, const int j) {
    return gridPoint(i, j, 3);
}

/// Viewport with only the triangle mesh visible, so every drawn pixel belongs to a mesh line
void showOnlyMesh(Viewport &viewport) {
    viewport.setShowFill(false);
    viewport.setShowMesh(true);
    viewport.setShowPolygon(false);
    viewport.setShowPoints(false);
}

QPointF screenPoint(const Viewport &viewport, const Vec3 &p) {
    const ScreenPoint sp = Projection(kMeshImageSize.width(), kMeshImageSize.height()).project(viewport.rotation() * p);
    return {sp.x, sp.y};
}

/// A drawn (non-white) pixel in the 3x3 neighbourhood of p. Callers hide the layers they don't check.
bool lineNear(const QImage &img, const QPointF &p) {
    const int px = static_cast<int>(p.x()), py = static_cast<int>(p.y());
    for (int y = py - 1; y <= py + 1; ++y)
        for (int x = px - 1; x <= px + 1; ++x)
            if (img.rect().contains(x, y) && !isWhite(img, x, y))
                return true;
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
    // Only the outline is drawn here (showOnlyMesh hides the fill)
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
    showOnlyMesh(viewport);
    viewport.setSurfaces({flatSquare()});
    const QImage img = renderViewport(viewport, kMeshImageSize);
    for (const auto &[i, j] : {std::pair{0, 0}, std::pair{4, 5}, std::pair{9, 9}, std::pair{2, 7}})
        expectCellDrawnAsTwoTriangles(viewport, img, i, j);
}

TEST(ViewportMeshTest, TriangleEdgesFollowRotation) {
    Viewport viewport;
    showOnlyMesh(viewport);
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

// ---------- 4.6: Bezier polygon and drawing order ----------

namespace {
/// Some pixel within `radius` px of p along the segment direction is drawn: dashed lines have gaps,
/// so a single sample at the midpoint may land in one of them
bool dashedLineNear(const QImage &img, const QPointF &p, const QPointF &dir, const double radius = 6.0) {
    const double len = std::hypot(dir.x(), dir.y());
    for (double t = -radius; t <= radius; t += 1.0)
        if (lineNear(img, p + (t / len) * dir))
            return true;
    return false;
}

// Colors differ per surface and are private to Viewport, so the tests don't check them directly:
// layers are told apart by hiding the others, dots by their size, drawing order by comparing two images.

QColor colorAt(const QImage &img, const QPointF &p) {
    return img.pixelColor(static_cast<int>(p.x()), static_cast<int>(p.y()));
}

/// The pixel containing p is drawn
bool drawnAt(const QImage &img, const QPointF &p) {
    return !isWhite(img, static_cast<int>(p.x()), static_cast<int>(p.y()));
}

/// The whole 3x3 neighbourhood of p is drawn: a filled control point dot (radius 3), not just a 1 px line through p
bool dotAt(const QImage &img, const QPointF &p) {
    const int px = static_cast<int>(p.x()), py = static_cast<int>(p.y());
    for (int y = py - 1; y <= py + 1; ++y)
        for (int x = px - 1; x <= px + 1; ++x)
            if (!img.rect().contains(x, y) || isWhite(img, x, y))
                return false;
    return true;
}

/// The 3x3 neighbourhoods of p in both images are identical
bool sameAround(const QImage &a, const QImage &b, const QPointF &p) {
    const int px = static_cast<int>(p.x()), py = static_cast<int>(p.y());
    for (int y = py - 1; y <= py + 1; ++y)
        for (int x = px - 1; x <= px + 1; ++x)
            if (a.pixel(x, y) != b.pixel(x, y))
                return false;
    return true;
}

void showOnlyPolygon(Viewport &viewport) {
    viewport.setShowFill(false);
    viewport.setShowMesh(false);
    viewport.setShowPolygon(true);
    viewport.setShowPoints(false);
}

void showOnlyPoints(Viewport &viewport) {
    viewport.setShowFill(false);
    viewport.setShowMesh(false);
    viewport.setShowPolygon(false);
    viewport.setShowPoints(true);
}

void showOnlyFill(Viewport &viewport) {
    viewport.setShowFill(true);
    viewport.setShowMesh(false);
    viewport.setShowPolygon(false);
    viewport.setShowPoints(false);
}

/// All 24 edges P[i][j]-P[i+1][j] and P[i][j]-P[i][j+1] are drawn, the diagonals P[i][j]-P[i+1][j+1] are not
void expectPolygonDrawn(const Viewport &viewport, const QImage &img) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            const QPointF p = screenPoint(viewport, controlPointOfFlatSquare(i, j));
            if (i + 1 < 4) {
                const QPointF q = screenPoint(viewport, controlPointOfFlatSquare(i + 1, j));
                EXPECT_TRUE(dashedLineNear(img, midpoint(p, q), q - p)) << "edge P" << i << j << "-P" << i + 1 << j;
            }
            if (j + 1 < 4) {
                const QPointF q = screenPoint(viewport, controlPointOfFlatSquare(i, j + 1));
                EXPECT_TRUE(dashedLineNear(img, midpoint(p, q), q - p)) << "edge P" << i << j << "-P" << i << j + 1;
            }
            if (i + 1 < 4 && j + 1 < 4) {
                const QPointF q = screenPoint(viewport, controlPointOfFlatSquare(i + 1, j + 1));
                EXPECT_FALSE(lineNear(img, midpoint(p, q))) << "unexpected diagonal from P" << i << j;
            }
        }
}
} // namespace

TEST(ViewportPolygonTest, AllPolygonEdgesAreDrawnWithoutDiagonals) {
    Viewport viewport;
    showOnlyPolygon(viewport);
    viewport.setSurfaces({flatSquare()});
    expectPolygonDrawn(viewport, renderViewport(viewport, kMeshImageSize));
}

TEST(ViewportPolygonTest, PolygonFollowsRotation) {
    Viewport viewport;
    showOnlyPolygon(viewport);
    viewport.setSurfaces({flatSquare()});
    renderViewport(viewport, kMeshImageSize);
    drag(viewport, {100, 100}, {160, 140});
    expectPolygonDrawn(viewport, viewport.grab().toImage().convertToFormat(QImage::Format_RGB32));
}

TEST(ViewportPolygonTest, ControlPointsAreDrawnAsDots) {
    Viewport viewport;
    showOnlyPolygon(viewport);
    viewport.setSurfaces({flatSquare()});
    const QImage img = renderViewport(viewport, kMeshImageSize);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            EXPECT_TRUE(dotAt(img, screenPoint(viewport, controlPointOfFlatSquare(i, j)))) << "control point P" << i << j;
}

TEST(ViewportPolygonTest, ControlPointsUseTheirOwnPositionsNotMeshVertices) {
    // Lifting one inner control point moves only the polygon: P11 is drawn where it is, above the flat mesh
    BezierSurface::ControlGrid g;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            g[i][j] = controlPointOfFlatSquare(i, j);
    g[1][1].z = 3.0f;
    Viewport viewport;
    showOnlyPolygon(viewport);
    viewport.setSurfaces({BezierSurface(g)});
    const QImage img = renderViewport(viewport, kMeshImageSize);
    EXPECT_TRUE(dotAt(img, screenPoint(viewport, g[1][1])));
}

TEST(ViewportPolygonTest, PointsOfOneSurfaceAreDrawnOverMeshOfAnother) {
    // Layers go over all meshes at once (mesh, polygon, points), so the control points of the first surface
    // stay on top of the triangle edges of the second one: around the dot the image looks exactly as if
    // the first surface were alone. The point lies on the middle of an edge a-b of the second surface.
    const Vec3 onEdge = 0.5f * (gridPoint(4, 5) + gridPoint(5, 5));
    Viewport both;
    both.setSurfaces({pointSurface(onEdge), flatSquare()});
    const QImage img = renderViewport(both, kMeshImageSize);
    Viewport alone;
    alone.setSurfaces({pointSurface(onEdge)});
    const QPointF p = screenPoint(both, onEdge);
    ASSERT_TRUE(dotAt(img, p));
    EXPECT_TRUE(sameAround(img, renderViewport(alone, kMeshImageSize), p));
}

TEST(ViewportPolygonTest, NeighbouringSurfacesHaveDifferentColors) {
    const Vec3 p1{-2.0f, 0.0f, 0.0f}, p2{2.0f, 0.0f, 0.0f};
    for (auto *show : {&showOnlyPolygon, &showOnlyPoints}) {
        Viewport viewport;
        show(viewport);
        viewport.setSurfaces({pointSurface(p1), pointSurface(p2)});
        const QImage img = renderViewport(viewport, kMeshImageSize);
        const QPointF s1 = screenPoint(viewport, p1), s2 = screenPoint(viewport, p2);
        ASSERT_TRUE(drawnAt(img, s1));
        ASSERT_TRUE(drawnAt(img, s2));
        EXPECT_NE(colorAt(img, s1), colorAt(img, s2));
    }
}

// ---------- 5.5: triangle fill ----------

namespace {
/// Flat square at z = 0 with the given center and half side, evenly spaced control points
BezierSurface flatSquareAround(const Vec3 &center, const float halfSide) {
    BezierSurface::ControlGrid g;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            g[i][j] = center + Vec3{.x = -halfSide + 2.0f * halfSide * static_cast<float>(i) / 3.0f,
                                    .y = -halfSide + 2.0f * halfSide * static_cast<float>(j) / 3.0f,
                                    .z = 0.0f};
    return BezierSurface(g);
}

/// Point of flatSquare() at parameters (s, t), s, t in [0, 1] cover the square
Vec3 squarePoint(const float s, const float t) {
    return {.x = -kHalfSide + 2.0f * kHalfSide * s, .y = -kHalfSide + 2.0f * kHalfSide * t, .z = 0.0f};
}

/// Number of samples (s, t) in [margin, 1 - margin]^2 whose pixel is not drawn
int undrawnSamplesInside(const Viewport &viewport, const QImage &img, const float margin) {
    int undrawn = 0;
    for (float s = margin; s <= 1.0f - margin; s += 0.01f)
        for (float t = margin; t <= 1.0f - margin; t += 0.01f)
            if (!drawnAt(img, screenPoint(viewport, squarePoint(s, t))))
                ++undrawn;
    return undrawn;
}

/// p lies in the convex polygon (any orientation), up to `tolerance` px outside its edges
bool insideConvex(const std::vector<QPointF> &polygon, const QPointF &p, const double tolerance) {
    bool anyNegative = false, anyPositive = false;
    for (size_t k = 0; k < polygon.size(); ++k) {
        const QPointF a = polygon[k], b = polygon[(k + 1) % polygon.size()];
        const QPointF e = b - a, ap = p - a;
        const double signedDistance = (e.x() * ap.y() - e.y() * ap.x()) / std::hypot(e.x(), e.y());
        anyNegative |= signedDistance < -tolerance;
        anyPositive |= signedDistance > tolerance;
    }
    return !(anyNegative && anyPositive);
}
} // namespace

TEST(ViewportFillTest, FillCoversWholeSurface) {
    // 0.02 of the side is ~8 px from the border, so every sampled pixel center is inside the square
    Viewport viewport;
    showOnlyFill(viewport);
    viewport.setSurfaces({flatSquare()});
    EXPECT_EQ(undrawnSamplesInside(viewport, renderViewport(viewport, kMeshImageSize), 0.02f), 0);
}

TEST(ViewportFillTest, FillFollowsRotation) {
    Viewport viewport;
    showOnlyFill(viewport);
    viewport.setSurfaces({flatSquare()});
    renderViewport(viewport, kMeshImageSize);
    drag(viewport, {100, 100}, {150, 70});
    EXPECT_EQ(undrawnSamplesInside(viewport, grabImage(viewport), 0.02f), 0);
}

TEST(ViewportFillTest, FilledPixelsHaveCentersInsideSurface) {
    // A pixel is filled only when its center lies in the projected square (on its border at most)
    Viewport viewport;
    showOnlyFill(viewport);
    viewport.setSurfaces({flatSquare()});
    const QImage img = renderViewport(viewport, kMeshImageSize);
    const std::vector<QPointF> square = {
        screenPoint(viewport, squarePoint(0, 0)), screenPoint(viewport, squarePoint(1, 0)),
        screenPoint(viewport, squarePoint(1, 1)), screenPoint(viewport, squarePoint(0, 1))};
    const std::vector<QPoint> pixels = drawnPixels(img);
    ASSERT_FALSE(pixels.empty());
    int outside = 0;
    for (const QPoint &p : pixels)
        if (!insideConvex(square, QPointF(p.x() + 0.5, p.y() + 0.5), 1e-3))
            ++outside;
    EXPECT_EQ(outside, 0);
}

TEST(ViewportFillTest, LinesAreDrawnOverFill) {
    // If the fill were drawn after the mesh it would paint over the inner edges,
    // so around an edge the image would look the same as with the fill alone
    Viewport fillOnly;
    showOnlyFill(fillOnly);
    fillOnly.setSurfaces({flatSquare()});
    Viewport fillAndMesh;
    showOnlyFill(fillAndMesh);
    fillAndMesh.setShowMesh(true);
    fillAndMesh.setSurfaces({flatSquare()});
    const QPointF ab = midpoint(screenPoint(fillOnly, gridPoint(4, 5)), screenPoint(fillOnly, gridPoint(5, 5)));
    EXPECT_FALSE(sameAround(renderViewport(fillOnly, kMeshImageSize), renderViewport(fillAndMesh, kMeshImageSize), ab));
}

TEST(ViewportFillTest, HiddenFillLeavesImageWhite) {
    Viewport viewport;
    showOnlyFill(viewport);
    viewport.setSurfaces({flatSquare()});
    ASSERT_TRUE(drawnAt(showViewport(viewport, kMeshImageSize), screenPoint(viewport, squarePoint(0.3f, 0.6f))));
    viewport.setShowFill(false);
    EXPECT_TRUE(drawnPixels(grabImage(viewport)).empty());
}

TEST(ViewportFillTest, NeighbouringSurfacesHaveDifferentFillColors) {
    const Vec3 c1{-2.0f, 0.0f, 0.0f}, c2{2.0f, 0.0f, 0.0f};
    Viewport viewport;
    showOnlyFill(viewport);
    viewport.setSurfaces({flatSquareAround(c1, 1.0f), flatSquareAround(c2, 1.0f)});
    const QImage img = renderViewport(viewport, kMeshImageSize);
    const QPointF s1 = screenPoint(viewport, c1), s2 = screenPoint(viewport, c2);
    ASSERT_TRUE(drawnAt(img, s1));
    ASSERT_TRUE(drawnAt(img, s2));
    EXPECT_NE(colorAt(img, s1), colorAt(img, s2));
}

TEST(ViewportFillTest, DegenerateSurfacesDoNotCrash) {
    // A surface collapsed to a point and a flat one seen exactly edge-on (all triangles have zero area on screen):
    // before = R^T * q gives after = q, a square in the plane x = 0 of the scene
    Viewport viewport;
    showOnlyFill(viewport);
    const Mat3 back = viewport.rotation().transposed();
    BezierSurface::ControlGrid g;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            g[i][j] = back * Vec3{.x = 0.0f,
                                  .y = -2.0f + 4.0f * static_cast<float>(i) / 3.0f,
                                  .z = -2.0f + 4.0f * static_cast<float>(j) / 3.0f};
    viewport.setSurfaces({pointSurface({0.0f, 0.0f, 0.0f}), BezierSurface(g)});
    const QImage img = renderViewport(viewport, kMeshImageSize);
    // at most a 1 px wide vertical line through the center
    const std::vector<QPoint> pixels = drawnPixels(img);
    if (!pixels.empty())
        EXPECT_LE(boundingBox(pixels).width(), 1);
}

// ---------- 5.6: Z-buffer ----------

namespace {
/// Flat rectangle given directly in scene coordinates (after rotation): center q, half sizes, z = q.z + slope * (x - q.x).
/// Control points are R^T * scene point, so after Viewport applies R the mesh lies exactly there,
/// whatever the initial rotation is.
BezierSurface sceneRect(const Viewport &viewport, const Vec3 &q, const float halfX, const float halfY,
                        const float slope = 0.0f) {
    const Mat3 back = viewport.rotation().transposed();
    BezierSurface::ControlGrid g;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            const float dx = -halfX + 2.0f * halfX * static_cast<float>(i) / 3.0f;
            const float dy = -halfY + 2.0f * halfY * static_cast<float>(j) / 3.0f;
            g[i][j] = back * Vec3{.x = q.x + dx, .y = q.y + dy, .z = q.z + slope * dx};
        }
    return BezierSurface(g);
}

/// Screen position of a point given in scene coordinates (already rotated)
QPointF sceneToScreen(const Vec3 &q) {
    const ScreenPoint sp = Projection(kMeshImageSize.width(), kMeshImageSize.height()).project(q);
    return {sp.x, sp.y};
}

// Two overlapping squares: the near one (z = 1) on the left, the far one (z = -1) on the right
const Vec3 kNearCenter{-1.0f, 0.0f, 1.0f}, kFarCenter{1.0f, 0.0f, -1.0f};
const Vec3 kOverlap{0.0f, 0.0f, 0.0f}, kNearOnly{-2.5f, 0.0f, 1.0f}, kFarOnly{2.5f, 0.0f, -1.0f};

/// Which surface covers the pixel: the fill colors differ in their dominant channel (0 = red, 1 = green, 2 = blue).
/// With a point light the brightness changes across a flat surface, the dominant channel does not; -1 for white
/// background, black or a tie. Callers switch the highlight off (ks = 0), it could saturate all channels to white.
int surfaceAt(const QImage &img, const QPointF &p) {
    const QColor c = colorAt(img, p);
    const int r = c.red(), g = c.green(), b = c.blue();
    if (r > g && r > b) return 0;
    if (g > r && g > b) return 1;
    if (b > r && b > g) return 2;
    return -1;
}

/// Fill only, diffuse only: surfaceAt() can tell the surfaces apart everywhere
void showOnlyDiffuseFill(Viewport &viewport) {
    showOnlyFill(viewport);
    viewport.setKs(0.0f);
}

/// In the overlap the near square is visible: same surface as where it is alone, not the far one
void expectNearSquareVisible(const QImage &img) {
    const int overlap = surfaceAt(img, sceneToScreen(kOverlap));
    const int nearOnly = surfaceAt(img, sceneToScreen(kNearOnly));
    const int farOnly = surfaceAt(img, sceneToScreen(kFarOnly));
    ASSERT_NE(nearOnly, -1) << "near square not found";
    ASSERT_NE(farOnly, -1) << "far square not found";
    ASSERT_NE(nearOnly, farOnly) << "surfaces must have different colors for this test";
    EXPECT_EQ(overlap, nearOnly);
}
} // namespace

TEST(ViewportDepthTest, NearerSurfaceCoversFartherOneInAnyOrder) {
    // Without a depth test the surface drawn last would always win, so one of the orders would fail
    for (const bool nearFirst : {true, false}) {
        Viewport viewport;
        showOnlyDiffuseFill(viewport);
        const BezierSurface nearSquare = sceneRect(viewport, kNearCenter, 2.0f, 2.0f);
        const BezierSurface farSquare = sceneRect(viewport, kFarCenter, 2.0f, 2.0f);
        viewport.setSurfaces(nearFirst ? std::vector{nearSquare, farSquare} : std::vector{farSquare, nearSquare});
        SCOPED_TRACE(nearFirst ? "near surface first" : "far surface first");
        expectNearSquareVisible(renderViewport(viewport, kMeshImageSize));
    }
}

TEST(ViewportDepthTest, IntersectingSurfacesSwitchAlongIntersection) {
    // A: plane z = 0, y in [-3, 1]; B: plane z = 0.5 * x, y in [-1, 3]. They cross along x = 0:
    // for x > 0 B is closer, for x < 0 A is. A alone at y = -2, B alone at y = 2.
    for (const bool aFirst : {true, false}) {
        Viewport viewport;
        showOnlyDiffuseFill(viewport);
        const BezierSurface a = sceneRect(viewport, {0.0f, -1.0f, 0.0f}, 3.0f, 2.0f);
        const BezierSurface b = sceneRect(viewport, {0.0f, 1.0f, 0.0f}, 3.0f, 2.0f, 0.5f);
        viewport.setSurfaces(aFirst ? std::vector{a, b} : std::vector{b, a});
        const QImage img = renderViewport(viewport, kMeshImageSize);
        SCOPED_TRACE(aFirst ? "A first" : "B first");
        const int aOnly = surfaceAt(img, sceneToScreen({0.0f, -2.0f, 0.0f}));
        const int bOnly = surfaceAt(img, sceneToScreen({0.0f, 2.0f, 0.0f}));
        ASSERT_NE(aOnly, -1);
        ASSERT_NE(bOnly, -1);
        ASSERT_NE(aOnly, bOnly);
        EXPECT_EQ(surfaceAt(img, sceneToScreen({2.0f, 0.0f, 0.0f})), bOnly) << "right of the intersection";
        EXPECT_EQ(surfaceAt(img, sceneToScreen({-2.0f, 0.0f, 0.0f})), aOnly) << "left of the intersection";
        EXPECT_EQ(surfaceAt(img, sceneToScreen({0.5f, 0.0f, 0.0f})), bOnly) << "just right of the intersection";
        EXPECT_EQ(surfaceAt(img, sceneToScreen({-0.5f, 0.0f, 0.0f})), aOnly) << "just left of the intersection";
    }
}

TEST(ViewportDepthTest, HalfTurnShowsTheOtherSurface) {
    // 314 px = 3.14 rad around the screen Y axis: (x, y, z) -> (-x, y, -z), the far square comes to the front
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    const Mat3 back = viewport.rotation().transposed();
    viewport.setSurfaces({sceneRect(viewport, kNearCenter, 2.0f, 2.0f), sceneRect(viewport, kFarCenter, 2.0f, 2.0f)});
    const QImage before = renderViewport(viewport, kMeshImageSize);
    ASSERT_NO_FATAL_FAILURE(expectNearSquareVisible(before));

    // object coordinates of the sample points, to find them again after the turn
    const Vec3 overlap = back * kOverlap, nearOnly = back * kNearOnly, farOnly = back * kFarOnly;
    drag(viewport, {100, 300}, {414, 300});
    const QImage after = grabImage(viewport);
    const int wasFar = surfaceAt(after, screenPoint(viewport, farOnly));
    ASSERT_NE(wasFar, -1);
    ASSERT_NE(wasFar, surfaceAt(after, screenPoint(viewport, nearOnly)));
    EXPECT_EQ(surfaceAt(after, screenPoint(viewport, overlap)), wasFar);
}

TEST(ViewportDepthTest, DepthIsResetEveryFrame) {
    // Without clearing, the depths of the previous frame would block the next one (equal z is rejected),
    // leaving holes; dragging there and back must give the same image again
    Viewport viewport;
    showOnlyFill(viewport);
    viewport.setSurfaces({flatSquare()});
    const QImage before = renderViewport(viewport, kMeshImageSize);
    // horizontal only: rotationY(a) * rotationY(-a) = identity, a diagonal drag would not come back exactly
    drag(viewport, {100, 100}, {160, 100});
    EXPECT_EQ(undrawnSamplesInside(viewport, grabImage(viewport), 0.02f), 0) << "holes after the first drag";
    drag(viewport, {160, 100}, {100, 100});
    // same rotation up to float noise (see DragBackAndForthRestoresImage): a few border pixels may differ
    const QImage after = grabImage(viewport);
    int differentPixels = 0;
    for (int y = 0; y < before.height(); ++y)
        for (int x = 0; x < before.width(); ++x)
            if (before.pixel(x, y) != after.pixel(x, y))
                ++differentPixels;
    EXPECT_LE(differentPixels, 50);
}

TEST(ViewportDepthTest, RedrawWithoutChangesGivesSameImage) {
    // Exactly the same depths again: without clearing the buffer every pixel would be rejected
    Viewport viewport;
    showOnlyFill(viewport);
    viewport.setSurfaces({flatSquare()});
    const QImage before = showViewport(viewport, kMeshImageSize);
    viewport.setShowFill(true); // setter re-renders with unchanged data
    EXPECT_EQ(grabImage(viewport), before);
}

TEST(ViewportDepthTest, DepthTestWorksAfterResize) {
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    viewport.setSurfaces({sceneRect(viewport, kNearCenter, 2.0f, 2.0f), sceneRect(viewport, kFarCenter, 2.0f, 2.0f)});
    renderViewport(viewport, {200, 150}); // smaller first: a buffer not resized with the image would be too small
    expectNearSquareVisible(renderViewport(viewport, kMeshImageSize));
}

// ---------- 4.7: display flags and resolution ----------

TEST(ViewportControlsTest, DefaultsShowEverythingAtDefaultResolution) {
    const Viewport viewport;
    EXPECT_TRUE(viewport.showFill());
    EXPECT_TRUE(viewport.showMesh());
    EXPECT_TRUE(viewport.showPolygon());
    EXPECT_TRUE(viewport.showPoints());
    EXPECT_EQ(viewport.resolution(), Mesh::kDefaultResolution);
}

TEST(ViewportControlsTest, LightPathIsHiddenByDefaultAndToggles) {
    // hidden in a bare Viewport (MainWindow switches it on), so the other tests see the scene alone
    Viewport viewport;
    EXPECT_FALSE(viewport.showLightPath());
    viewport.setShowFill(false);
    viewport.setShowMesh(false);
    viewport.setShowPolygon(false);
    viewport.setShowPoints(false);
    viewport.setLightColor(QColor(255, 0, 0)); // the light dot is filled with the light color, white by default
    viewport.setSurfaces({flatSquare()});
    const QPointF light = sceneToScreen(viewport.lightPosition());
    EXPECT_TRUE(drawnPixels(showViewport(viewport, kMeshImageSize)).empty());

    viewport.setShowLightPath(true);
    EXPECT_TRUE(viewport.showLightPath());
    EXPECT_TRUE(dotAt(grabImage(viewport), light)) << "light not drawn";

    viewport.setShowLightPath(false);
    EXPECT_TRUE(drawnPixels(grabImage(viewport)).empty());
}

TEST(ViewportControlsTest, SettersUpdateFlags) {
    Viewport viewport;
    viewport.setShowMesh(false);
    EXPECT_FALSE(viewport.showMesh());
    viewport.setShowPolygon(false);
    EXPECT_FALSE(viewport.showPolygon());
    viewport.setShowPoints(false);
    EXPECT_FALSE(viewport.showPoints());
    viewport.setShowFill(false);
    EXPECT_FALSE(viewport.showFill());
    viewport.setShowMesh(true);
    EXPECT_TRUE(viewport.showMesh());
    EXPECT_FALSE(viewport.showPolygon());
    EXPECT_FALSE(viewport.showPoints());
    EXPECT_FALSE(viewport.showFill());
}

TEST(ViewportControlsTest, HidingAllLayersGivesWhiteImage) {
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    ASSERT_FALSE(drawnPixels(showViewport(viewport, {500, 500})).empty());
    viewport.setShowFill(false);
    viewport.setShowMesh(false);
    viewport.setShowPolygon(false);
    viewport.setShowPoints(false);
    EXPECT_TRUE(drawnPixels(grabImage(viewport)).empty());
}

TEST(ViewportControlsTest, EveryLayerAloneDrawsSomething) {
    for (auto *show : {&showOnlyFill, &showOnlyMesh, &showOnlyPolygon, &showOnlyPoints}) {
        Viewport viewport;
        show(viewport);
        viewport.setSurfaces({flatSquare()});
        EXPECT_FALSE(drawnPixels(renderViewport(viewport, kMeshImageSize)).empty());
    }
}

TEST(ViewportControlsTest, HiddenMeshLeavesEdgesWhite) {
    Viewport viewport;
    viewport.setShowFill(false); // the fill covers the edges too
    viewport.setSurfaces({flatSquare()});
    const QPointF ab = midpoint(screenPoint(viewport, gridPoint(4, 5)), screenPoint(viewport, gridPoint(5, 5)));
    ASSERT_TRUE(lineNear(showViewport(viewport, kMeshImageSize), ab));
    viewport.setShowMesh(false);
    const QImage img = grabImage(viewport);
    EXPECT_FALSE(lineNear(img, ab));
    // the other layers are still there
    EXPECT_TRUE(drawnAt(img, screenPoint(viewport, gridPoint(4, 5))));
    EXPECT_TRUE(dotAt(img, screenPoint(viewport, controlPointOfFlatSquare(1, 2))));
}

TEST(ViewportControlsTest, HiddenPolygonHidesControlPoints) {
    Viewport viewport;
    showOnlyPolygon(viewport);
    viewport.setSurfaces({flatSquare()});
    ASSERT_FALSE(drawnPixels(showViewport(viewport, kMeshImageSize)).empty());
    viewport.setShowPolygon(false);
    EXPECT_TRUE(drawnPixels(grabImage(viewport)).empty());
}

TEST(ViewportControlsTest, PointsAreDrawnInMeshVertices) {
    Viewport viewport;
    showOnlyPoints(viewport);
    viewport.setSurfaces({flatSquare()});
    const QImage img = renderViewport(viewport, kMeshImageSize);
    for (const auto &[i, j] : {std::pair{0, 0}, std::pair{3, 7}, std::pair{10, 10}, std::pair{5, 0}})
        EXPECT_TRUE(drawnAt(img, screenPoint(viewport, gridPoint(i, j)))) << "vertex " << i << ", " << j;
    // between vertices nothing is drawn
    EXPECT_FALSE(lineNear(img, midpoint(screenPoint(viewport, gridPoint(4, 5)), screenPoint(viewport, gridPoint(5, 5)))));
}

TEST(ViewportControlsTest, ToggledLayerBackOnRestoresImage) {
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    const QImage before = showViewport(viewport, {500, 500});
    viewport.setShowFill(false);
    viewport.setShowMesh(false);
    viewport.setShowPolygon(false);
    viewport.setShowPoints(false);
    ASSERT_NE(grabImage(viewport), before);
    viewport.setShowFill(true);
    viewport.setShowMesh(true);
    viewport.setShowPolygon(true);
    viewport.setShowPoints(true);
    EXPECT_EQ(grabImage(viewport), before);
}

TEST(ViewportResolutionTest, SetResolutionRetriangulatesEveryMesh) {
    Viewport viewport;
    viewport.setSurfaces({flatSquare(), pointSurface({1.0f, 2.0f, 0.0f})});
    for (const int n : {Mesh::kMinResolution, 3, 17, Mesh::kMaxResolution}) {
        viewport.setResolution(n);
        EXPECT_EQ(viewport.resolution(), n);
        for (const Mesh &mesh : viewport.meshes()) {
            EXPECT_EQ(mesh.resolution(), n);
            EXPECT_EQ(mesh.vertices().size(), static_cast<size_t>((n + 1) * (n + 1)));
            EXPECT_EQ(mesh.triangles().size(), static_cast<size_t>(2 * n * n));
        }
    }
}

TEST(ViewportResolutionTest, ResolutionSetBeforeSurfacesIsUsedForNewMeshes) {
    Viewport viewport;
    viewport.setResolution(4);
    viewport.setSurfaces({flatSquare()});
    ASSERT_EQ(viewport.meshes().size(), 1u);
    EXPECT_EQ(viewport.meshes()[0].resolution(), 4);
    // and it stays when the surfaces are replaced
    viewport.setSurfaces(assetSurfaces());
    for (const Mesh &mesh : viewport.meshes())
        EXPECT_EQ(mesh.resolution(), 4);
}

TEST(ViewportResolutionTest, SetResolutionKeepsCurrentRotation) {
    Viewport viewport;
    viewport.setSurfaces({flatSquare()});
    renderViewport(viewport, kMeshImageSize);
    drag(viewport, {100, 100}, {170, 60});
    viewport.setResolution(7);
    for (const Vertex &v : viewport.meshes()[0].vertices())
        EXPECT_VEC3_EQ(v.after.P, viewport.rotation() * v.before.P);
}

TEST(ViewportResolutionTest, SetResolutionRedrawsLikeFreshViewport) {
    Viewport changed;
    changed.setSurfaces({flatSquare()});
    showViewport(changed, kMeshImageSize);
    changed.setResolution(4);
    const QImage img = grabImage(changed);

    Viewport fresh;
    fresh.setResolution(4);
    fresh.setSurfaces({flatSquare()});
    EXPECT_EQ(img, renderViewport(fresh, kMeshImageSize));
}

TEST(ViewportResolutionTest, VerticesAreDrawnAtNewResolution) {
    Viewport viewport;
    showOnlyPoints(viewport);
    viewport.setSurfaces({flatSquare()});
    showViewport(viewport, kMeshImageSize);
    viewport.setResolution(2);
    const QImage img = grabImage(viewport);
    EXPECT_TRUE(drawnAt(img, screenPoint(viewport, gridPoint(1, 1, 2))));
    // vertex (1, 0) at n = 10 is gone
    EXPECT_TRUE(isWhite(img, static_cast<int>(screenPoint(viewport, gridPoint(1, 0)).x()),
                        static_cast<int>(screenPoint(viewport, gridPoint(1, 0)).y())));
}

// ---------- lighting parameters (stage 6) ----------

TEST(ViewportLightingTest, SettersStoreValuesAndRedraw) {
    // each setter must re-render, otherwise the image keeps the old shading until the next drag
    // point light: a plane flat on the screen (N = V) seen halfway between the screen centre and the point below
    // the light; directly below it cos(V, R) = 1 for every m, far away ~0 for every m, so setM would not show
    Viewport viewport;
    showOnlyFill(viewport);
    const Vec3 light = viewport.lightPosition();
    const Vec3 center{0.5f * light.x, 0.5f * light.y, 0.0f};
    viewport.setSurfaces({sceneRect(viewport, center, 1.0f, 1.0f)});
    const QPointF p = sceneToScreen(center);
    const QColor initial = colorAt(showViewport(viewport, kMeshImageSize), p);

    viewport.setKd(0.2f);
    EXPECT_FLOAT_EQ(viewport.kd(), 0.2f);
    const QColor dimmer = colorAt(grabImage(viewport), p);
    EXPECT_NE(dimmer, initial) << "setKd";

    // with the default m = 50 the highlight is ~0 away from its centre, so widen it first
    viewport.setM(1.0f);
    EXPECT_FLOAT_EQ(viewport.m(), 1.0f);
    const QColor wide = colorAt(grabImage(viewport), p);
    EXPECT_NE(wide, dimmer) << "setM";

    viewport.setKs(0.0f);
    EXPECT_FLOAT_EQ(viewport.ks(), 0.0f);
    EXPECT_NE(colorAt(grabImage(viewport), p), wide) << "setKs";
}

TEST(ViewportLightingTest, PointLightUsesRotatedPositions) {
    // diffuse on a plane flat on the screen: brightness ~ cos(N, L) = h / |light - P| with P after rotation.
    // The ratio of two samples doesn't depend on the (private) fill color.
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    const Vec3 light = viewport.lightPosition();
    viewport.setSurfaces({sceneRect(viewport, {0.0f, 0.0f, 0.0f}, 4.0f, 4.0f)});
    const QImage img = showViewport(viewport, kMeshImageSize);

    const Vec3 towards{0.5f * light.x, 0.5f * light.y, 0.0f};
    const Vec3 away{-0.5f * light.x, -0.5f * light.y, 0.0f};
    const auto cosAt = [&](const Vec3 &p) { return light.z / length(light - p); };
    const int near = colorAt(img, sceneToScreen(towards)).red();
    const int far = colorAt(img, sceneToScreen(away)).red();
    ASSERT_GT(far, 50);
    EXPECT_NEAR(static_cast<float>(near) / static_cast<float>(far), cosAt(towards) / cosAt(away), 0.02f);
}

TEST(ViewportLightingTest, ZeroCoefficientsGiveBlackFill) {
    const Vec3 center{0.0f, 0.0f, 0.0f};
    Viewport viewport;
    showOnlyFill(viewport);
    viewport.setSurfaces({flatSquareAround(center, 1.0f)});
    viewport.setKd(0.0f);
    viewport.setKs(0.0f);
    const QImage img = renderViewport(viewport, kMeshImageSize);
    EXPECT_EQ(colorAt(img, screenPoint(viewport, center)), QColor(Qt::black));
    EXPECT_TRUE(isWhite(img, 0, 0)) << "background stays white";
}

TEST(ViewportLightingTest, LightIsWhiteByDefault) {
    const Viewport viewport;
    EXPECT_EQ(viewport.lightColor(), QColor(Qt::white));
}

TEST(ViewportLightingTest, LightColorFiltersFillPerComponent) {
    // with a pure green light only the green part of the (reddish) fill remains
    const Vec3 center{0.0f, 0.0f, 0.0f};
    Viewport viewport;
    showOnlyFill(viewport);
    viewport.setSurfaces({flatSquareAround(center, 1.0f)});
    const QPointF p = screenPoint(viewport, center);
    const QColor white = colorAt(showViewport(viewport, kMeshImageSize), p);
    ASSERT_GT(white.green(), 0);

    viewport.setLightColor(QColor(0, 255, 0));
    EXPECT_EQ(viewport.lightColor(), QColor(0, 255, 0));
    const QColor green = colorAt(grabImage(viewport), p); // shown widget: only a setter with render() changes it
    EXPECT_EQ(green.red(), 0);
    EXPECT_EQ(green.blue(), 0);
    EXPECT_EQ(green.green(), white.green());
}

TEST(ViewportLightingTest, LightColorRoundTripsThroughShading) {
    Viewport viewport;
    for (const QColor &c : {QColor(0, 0, 0), QColor(12, 128, 255), QColor(200, 7, 99)}) {
        viewport.setLightColor(c);
        EXPECT_EQ(viewport.lightColor(), c);
    }
}

namespace {
/// Dome bulging towards the viewer, built in scene coordinates (like sceneRect) so it faces the screen at start
BezierSurface sceneDome(const Viewport &viewport) {
    constexpr std::array<float, 4> kBulge{0.0f, 1.0f, 1.0f, 0.0f};
    const Mat3 back = viewport.rotation().transposed();
    BezierSurface::ControlGrid g;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            g[i][j] = back * Vec3{.x = -3.0f + 2.0f * static_cast<float>(i),
                                  .y = -3.0f + 2.0f * static_cast<float>(j),
                                  .z = 3.0f * kBulge[i] * kBulge[j]};
    return BezierSurface(g);
}

struct Highlight {
    float u, v;
    int brightness;
};

/// Brightest point of the surface, sampled on a (u, v) grid: tracks the highlight on the surface itself,
/// not on the screen (for a dome under a directional light it stays at almost the same screen spot)
Highlight brightestPoint(const Viewport &viewport, const QImage &img, const BezierSurface &surface) {
    constexpr int kSamples = 40;
    Highlight best{.u = 0.0f, .v = 0.0f, .brightness = -1};
    // edge samples skipped: on the silhouette the pixel can be the white background
    for (int a = 1; a < kSamples; ++a)
        for (int b = 1; b < kSamples; ++b) {
            const float u = static_cast<float>(a) / kSamples, v = static_cast<float>(b) / kSamples;
            const QColor c = colorAt(img, screenPoint(viewport, surface.evaluate(u, v).P));
            if (c == QColor(Qt::white)) continue;
            const int brightness = c.red() + c.green() + c.blue();
            if (brightness > best.brightness)
                best = {.u = u, .v = v, .brightness = brightness};
        }
    return best;
}
} // namespace

TEST(ViewportLightingTest, HighlightMovesAcrossSurfaceWhenRotated) {
    Viewport viewport;
    showOnlyFill(viewport);
    viewport.setKd(0.0f); // only the specular term, so the brightest point is the highlight
    viewport.setKs(1.0f);
    viewport.setM(20.0f);
    const BezierSurface dome = sceneDome(viewport);
    viewport.setSurfaces({dome});

    const Highlight before = brightestPoint(viewport, showViewport(viewport, kMeshImageSize), dome);
    ASSERT_GT(before.brightness, 200) << "there is a visible highlight";
    ASSERT_GT(before.u, 0.15f) << "highlight not at the edge, so it has room to move";
    ASSERT_LT(before.u, 0.85f);

    // 40 px = 0.4 rad around the screen Y axis: the normals turn, the highlight goes to another part of the dome
    drag(viewport, {300, 300}, {340, 300});
    const Highlight after = brightestPoint(viewport, grabImage(viewport), dome);
    EXPECT_GT(after.brightness, 200);
    EXPECT_GT(std::abs(after.u - before.u), 0.1f) << "u: " << before.u << " -> " << after.u << ", v: " << before.v << " -> " << after.v;

    // dragging back brings it back to the same place
    drag(viewport, {340, 300}, {300, 300});
    const Highlight back = brightestPoint(viewport, grabImage(viewport), dome);
    EXPECT_NEAR(back.u, before.u, 0.03f);
    EXPECT_NEAR(back.v, before.v, 0.03f);
}

// ---------- 7: light path drawing, editing and reset ----------

namespace {
/// Viewport with the light path visible and everything else hidden; light red, so its dot isn't white
void showOnlyLightPath(Viewport &viewport) {
    viewport.setShowFill(false);
    viewport.setShowMesh(false);
    viewport.setShowPolygon(false);
    viewport.setShowPoints(false);
    viewport.setShowLightPath(true);
    viewport.setLightColor(QColor(255, 0, 0));
    viewport.setSurfaces({flatSquare()}); // render() draws nothing without surfaces
    showViewport(viewport, kMeshImageSize);
}

/// Screen position of light path point i (the path is camera-fixed: no rotation)
QPointF pathPointOnScreen(const Viewport &viewport, const int i) {
    return sceneToScreen(viewport.lightPath().point(i));
}

/// Scene displacement of a mouse move from `from` to `to` on the plane of the light path
Vec3 sceneDelta(const QPoint &from, const QPoint &to) {
    const Projection proj(kMeshImageSize.width(), kMeshImageSize.height());
    return proj.unproject(static_cast<float>(to.x()), static_cast<float>(to.y()), 0.0f)
           - proj.unproject(static_cast<float>(from.x()), static_cast<float>(from.y()), 0.0f);
}

void expectPathEqual(const BezierCurve &actual, const BezierCurve &expected, const float eps = 1e-5f) {
    ASSERT_EQ(actual.size(), expected.size());
    for (int i = 0; i < actual.size(); ++i)
        EXPECT_VEC3_NEAR(actual.point(i), expected.point(i), eps) << "point " << i;
}

/// Some pixel drawn on the middle part (30%..70%) of the segment a-b: a dotted or dashed line has gaps
bool drawnAlong(const QImage &img, const QPointF &a, const QPointF &b) {
    for (int k = 30; k <= 70; ++k)
        if (lineNear(img, a + (b - a) * (k / 100.0)))
            return true;
    return false;
}
} // namespace

TEST(ViewportLightPathTest, DefaultPathIsCircleAboveScene) {
    const Viewport viewport;
    expectPathEqual(viewport.lightPath(),
                    BezierCurve::circle(Viewport::kDefaultLightPathRadius, Viewport::kDefaultLightZ));
    EXPECT_FLOAT_EQ(viewport.lightZ(), Viewport::kDefaultLightZ);
    // inside the image, above every surface point (|P| <= kSceneRadius)
    EXPECT_LE(Viewport::kDefaultLightPathRadius, Projection::kSceneRadius);
    EXPECT_GT(Viewport::kDefaultLightZ, Projection::kSceneRadius);
}

TEST(ViewportLightPathTest, ControlPolygonAndPointsAreDrawn) {
    Viewport viewport;
    showOnlyLightPath(viewport);
    const QImage img = grabImage(viewport);
    const BezierCurve &path = viewport.lightPath();
    for (int i = 0; i < path.size(); ++i) {
        SCOPED_TRACE(i);
        const QPointF p = pathPointOnScreen(viewport, i);
        if (i % 3 == 0)
            EXPECT_TRUE(dotAt(img, p)) << "anchor is a filled square";
        else
            EXPECT_TRUE(lineNear(img, p + QPointF(3, 0)) || lineNear(img, p - QPointF(3, 0))) << "handle circle";
    }
    // anchor-handle lines of the control polygon lie off the curve, so they show the polygon itself
    for (int s = 0; s < path.segments(); ++s) {
        SCOPED_TRACE(s);
        EXPECT_TRUE(drawnAlong(img, pathPointOnScreen(viewport, 3 * s), pathPointOnScreen(viewport, 3 * s + 1)));
        EXPECT_TRUE(drawnAlong(img, pathPointOnScreen(viewport, 3 * s + 2),
                               pathPointOnScreen(viewport, (3 * s + 3) % path.size())));
    }
}

TEST(ViewportLightPathTest, DragNearPointMovesItByMouseDeltaWithoutRotating) {
    Viewport viewport;
    showOnlyLightPath(viewport);
    const Mat3 rotation = viewport.rotation();
    const BezierCurve before = viewport.lightPath();
    for (const int i : {1, 3}) { // a handle and an anchor
        SCOPED_TRACE(i);
        const BezierCurve start = viewport.lightPath();
        const QPoint from = pathPointOnScreen(viewport, i).toPoint() + QPoint(-3, 2); // near, not exactly on it
        const QPoint to = from + QPoint(-40, 25);
        drag(viewport, from, to);
        const Vec3 expected = start.point(i) + sceneDelta(from, to);
        EXPECT_VEC3_NEAR(viewport.lightPath().point(i), expected, 1e-4f);
        EXPECT_FLOAT_EQ(viewport.lightPath().point(i).z, before.z());
        for (int j = 0; j < start.size(); ++j)
            if (j != i)
                EXPECT_VEC3_EQ(viewport.lightPath().point(j), start.point(j)) << "point " << j << " moved too";
    }
    EXPECT_MAT3_EQ(viewport.rotation(), rotation);
}

TEST(ViewportLightPathTest, GrabbedPointDoesNotJumpToCursor) {
    Viewport viewport;
    showOnlyLightPath(viewport);
    const Vec3 before = viewport.lightPath().point(1);
    const QPoint grab = pathPointOnScreen(viewport, 1).toPoint() + QPoint(-5, 0);
    press(viewport, grab);
    move(viewport, grab);
    release(viewport, grab);
    EXPECT_VEC3_NEAR(viewport.lightPath().point(1), before, 1e-4f);
}

TEST(ViewportLightPathTest, DragAwayFromPointsRotatesAndKeepsPath) {
    Viewport viewport;
    showOnlyLightPath(viewport);
    const Mat3 initial = viewport.rotation();
    const BezierCurve before = viewport.lightPath();
    drag(viewport, {300, 300}, {330, 290}); // screen centre: far from the circle
    EXPECT_MAT3_EQ(viewport.rotation(), applyDrag(initial, 30.0f, -10.0f));
    expectPathEqual(viewport.lightPath(), before);
}

TEST(ViewportLightPathTest, PressJustOutsidePickRadiusRotates) {
    Viewport viewport;
    showOnlyLightPath(viewport);
    const Mat3 initial = viewport.rotation();
    const BezierCurve before = viewport.lightPath();
    // handle 1 is on the right edge: go left, away from the other points
    const QPointF p = pathPointOnScreen(viewport, 1);
    const QPoint from(static_cast<int>(std::floor(p.x() - Viewport::kPickRadius - 2.0f)), static_cast<int>(p.y()));
    drag(viewport, from, from + QPoint(-20, 0));
    EXPECT_MAT3_EQ(viewport.rotation(), applyDrag(initial, -20.0f, 0.0f));
    expectPathEqual(viewport.lightPath(), before);
}

TEST(ViewportLightPathTest, HiddenPathCannotBeGrabbed) {
    Viewport viewport;
    showOnlyLightPath(viewport);
    viewport.setShowLightPath(false);
    const Mat3 initial = viewport.rotation();
    const BezierCurve before = viewport.lightPath();
    const QPoint from = pathPointOnScreen(viewport, 1).toPoint();
    drag(viewport, from, from + QPoint(-30, 0));
    EXPECT_MAT3_EQ(viewport.rotation(), applyDrag(initial, -30.0f, 0.0f));
    expectPathEqual(viewport.lightPath(), before);
}

TEST(ViewportLightPathTest, PointCannotBeDraggedOutOfImage) {
    Viewport viewport;
    showOnlyLightPath(viewport);
    for (const QPoint target : {QPoint(-500, 2000), QPoint(5000, -300)}) {
        const QPoint from = pathPointOnScreen(viewport, 1).toPoint();
        drag(viewport, from, target);
        const QPointF p = pathPointOnScreen(viewport, 1);
        // the grab offset is under 1 px when pressed at the rounded position
        EXPECT_GE(p.x(), -1.0);
        EXPECT_LE(p.x(), kMeshImageSize.width());
        EXPECT_GE(p.y(), -1.0);
        EXPECT_LE(p.y(), kMeshImageSize.height());
    }
}

TEST(ViewportLightPathTest, NearestOfTwoClosePointsIsPicked) {
    Viewport viewport;
    showOnlyLightPath(viewport);
    // put handle 1 five pixels left of anchor 0 (exact position: drag starts at the point itself)
    const QPoint anchor = pathPointOnScreen(viewport, 0).toPoint();
    const QPointF handle = pathPointOnScreen(viewport, 1);
    drag(viewport, handle.toPoint(), handle.toPoint() + (anchor + QPoint(-5, 0) - handle.toPoint()));
    ASSERT_NEAR(pathPointOnScreen(viewport, 1).x(), anchor.x() - 5, 1.0);
    ASSERT_NEAR(pathPointOnScreen(viewport, 1).y(), anchor.y(), 1.0);

    // 1 px right of the handle: both are within reach (anchor 4 px away), the handle is closer
    // (it comes after the anchor in the point order, so "first point in reach" would pick the anchor)
    const QPoint handleNow = pathPointOnScreen(viewport, 1).toPoint();
    BezierCurve before = viewport.lightPath();
    drag(viewport, handleNow + QPoint(1, 0), handleNow + QPoint(1, -20));
    EXPECT_GT(length(viewport.lightPath().point(1) - before.point(1)), 0.1f) << "handle should move";
    EXPECT_VEC3_EQ(viewport.lightPath().point(0), before.point(0));

    // the handle is now 20 px up: 1 px right of the anchor picks the anchor
    before = viewport.lightPath();
    drag(viewport, anchor + QPoint(1, 0), anchor + QPoint(1, 20));
    EXPECT_GT(length(viewport.lightPath().point(0) - before.point(0)), 0.1f) << "anchor should move";
    EXPECT_VEC3_EQ(viewport.lightPath().point(1), before.point(1));
}

TEST(ViewportLightPathTest, DraggingPointMovesLightAndRedraws) {
    // the light is on the curve: moving the point under it moves the light and changes the image
    Viewport viewport;
    showOnlyLightPath(viewport);
    const Vec3 light = viewport.lightPosition(); // t = 0: anchor 0
    const QImage before = grabImage(viewport);
    const QPoint from = pathPointOnScreen(viewport, 0).toPoint();
    drag(viewport, from, from + QPoint(-60, 30));
    EXPECT_GT(length(viewport.lightPosition() - light), 0.5f);
    EXPECT_NE(grabImage(viewport), before);
}

TEST(ViewportLightPathTest, ResetRestoresDefaultShapeKeepingHeight) {
    Viewport viewport;
    showOnlyLightPath(viewport);
    const QImage original = grabImage(viewport);
    const QPoint from = pathPointOnScreen(viewport, 4).toPoint() + QPoint(0, 1);
    drag(viewport, from, from + QPoint(50, 80));
    viewport.setLightZ(12.5f);
    ASSERT_NE(grabImage(viewport), original);

    viewport.resetLightPath();
    expectPathEqual(viewport.lightPath(), BezierCurve::circle(Viewport::kDefaultLightPathRadius, 12.5f));
    EXPECT_FLOAT_EQ(viewport.lightZ(), 12.5f);
    // the path is drawn as in the beginning (height doesn't change the orthographic image of the path)
    EXPECT_EQ(grabImage(viewport), original);
}

TEST(ViewportLightPathTest, ResetDuringDragReleasesThePoint) {
    Viewport viewport;
    showOnlyLightPath(viewport);
    const QPoint from = pathPointOnScreen(viewport, 1).toPoint();
    press(viewport, from);
    viewport.resetLightPath();
    move(viewport, from + QPoint(-40, 40));
    release(viewport, from + QPoint(-40, 40));
    expectPathEqual(viewport.lightPath(), BezierCurve::circle(Viewport::kDefaultLightPathRadius, viewport.lightZ()));
}

// ---------- 7: light animation ----------

TEST(ViewportAnimationTest, StoppedByDefaultAndToggles) {
    Viewport viewport;
    EXPECT_FALSE(viewport.isAnimating());
    viewport.setAnimating(true);
    EXPECT_TRUE(viewport.isAnimating());
    viewport.setAnimating(true); // again: still running
    EXPECT_TRUE(viewport.isAnimating());
    viewport.setAnimating(false);
    EXPECT_FALSE(viewport.isAnimating());
}

TEST(ViewportAnimationTest, StepMovesLightAlongPathWithConstantSpeed) {
    Viewport viewport;
    EXPECT_FLOAT_EQ(viewport.lightT(), 0.0f);
    EXPECT_VEC3_EQ(viewport.lightPosition(), viewport.lightPath().evaluate(0.0f));
    viewport.stepAnimation(0.5f);
    EXPECT_FLOAT_EQ(viewport.lightT(), Viewport::kLightSpeed * 0.5f);
    EXPECT_VEC3_EQ(viewport.lightPosition(), viewport.lightPath().evaluate(Viewport::kLightSpeed * 0.5f));
    // two half steps = one full step: speed doesn't depend on how the time is split into frames
    Viewport other;
    other.stepAnimation(1.0f);
    viewport.stepAnimation(0.5f);
    EXPECT_NEAR(viewport.lightT(), other.lightT(), 1e-6f);
}

TEST(ViewportAnimationTest, FullLapReturnsToStartAndParameterStaysWrapped) {
    Viewport viewport;
    const Vec3 start = viewport.lightPosition();
    const auto segments = static_cast<float>(viewport.lightPath().segments());
    const float lap = segments / Viewport::kLightSpeed; // seconds
    constexpr int kSteps = 400;
    for (int k = 0; k < 3 * kSteps; ++k) { // three laps in small steps
        viewport.stepAnimation(lap / kSteps);
        ASSERT_GE(viewport.lightT(), 0.0f);
        ASSERT_LT(viewport.lightT(), segments);
    }
    EXPECT_VEC3_NEAR(viewport.lightPosition(), start, 1e-2f);
}

TEST(ViewportAnimationTest, StepChangesImage) {
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    viewport.setSurfaces({sceneRect(viewport, {0.0f, 0.0f, 0.0f}, 4.0f, 4.0f)});
    const QImage before = showViewport(viewport, kMeshImageSize);
    viewport.stepAnimation(1.0f);
    EXPECT_NE(grabImage(viewport), before);
}

TEST(ViewportAnimationTest, SetLightZMovesLightAndRedraws) {
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    viewport.setSurfaces({sceneRect(viewport, {0.0f, 0.0f, 0.0f}, 4.0f, 4.0f)});
    const QImage before = showViewport(viewport, kMeshImageSize);
    const Vec3 light = viewport.lightPosition();
    viewport.setLightZ(15.0f);
    EXPECT_FLOAT_EQ(viewport.lightZ(), 15.0f);
    EXPECT_VEC3_EQ(viewport.lightPosition(), (Vec3{light.x, light.y, 15.0f}));
    EXPECT_NE(grabImage(viewport), before);
    // ends of the range are accepted
    viewport.setLightZ(Viewport::kMinLightZ);
    viewport.setLightZ(Viewport::kMaxLightZ);
}

TEST(ViewportAnimationTest, RunningTimerMovesLight) {
    // the only test with real time: just "it moves", no exact values (CI machines are slow)
    Viewport viewport;
    viewport.resize(kMeshImageSize);
    viewport.setAnimating(true);
    QElapsedTimer clock;
    clock.start();
    while (viewport.lightT() == 0.0f && clock.elapsed() < 3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    EXPECT_GT(viewport.lightT(), 0.0f);
    viewport.setAnimating(false);
    const float stopped = viewport.lightT();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    EXPECT_FLOAT_EQ(viewport.lightT(), stopped) << "light moves after stop";
}

// ---------- 7: surface colors ----------

namespace {
/// Three squares in a row (scene coordinates), surface k centred at x = -3, 0, 3 (+ a fourth one below if asked)
std::vector<BezierSurface> squaresInRow(const Viewport &viewport, const bool fourth = false) {
    std::vector<BezierSurface> res;
    for (int k = 0; k < 3; ++k)
        res.push_back(sceneRect(viewport, {-3.0f + 3.0f * static_cast<float>(k), 1.5f, 0.0f}, 1.0f, 1.0f));
    if (fourth)
        res.push_back(sceneRect(viewport, {0.0f, -2.5f, 0.0f}, 1.0f, 1.0f));
    return res;
}

const std::array<Vec3, 4> kSquareCenters = {{{-3.0f, 1.5f, 0.0f}, {0.0f, 1.5f, 0.0f}, {3.0f, 1.5f, 0.0f}, {0.0f, -2.5f, 0.0f}}};

/// Smallest value of the channel over the drawn pixels farther than `gap` from every point in `skip`:
/// the line core, where antialiasing doesn't mix in white (the control point dots are skipped)
int darkestChannel(const QImage &img, int (QColor::*channel)() const, const std::vector<QPointF> &skip = {},
                   const double gap = 0.0) {
    int res = 255;
    for (const QPoint &p : drawnPixels(img)) {
        const bool nearSkipped = std::any_of(skip.begin(), skip.end(), [&](const QPointF &q) {
            return std::hypot(p.x() - q.x(), p.y() - q.y()) <= gap;
        });
        if (!nearSkipped)
            res = std::min(res, (img.pixelColor(p).*channel)());
    }
    return res;
}
} // namespace

TEST(ViewportColorTest, DefaultColorsAndDerivedShades) {
    const Viewport viewport;
    for (int i = 0; i < Viewport::kColorCount; ++i) {
        SCOPED_TRACE(i);
        const QColor fill = viewport.surfaceColor(i);
        EXPECT_EQ(fill, Viewport::kDefaultSurfaceColors[i]);
        // same hue, darker: polygon darker than fill, mesh darker than polygon
        EXPECT_EQ(viewport.polygonColor(i).hsvHue(), fill.hsvHue());
        EXPECT_EQ(viewport.meshColor(i).hsvHue(), fill.hsvHue());
        EXPECT_LT(viewport.polygonColor(i).value(), fill.value());
        EXPECT_LT(viewport.meshColor(i).value(), viewport.polygonColor(i).value());
    }
}

TEST(ViewportColorTest, ShadesFollowChosenColor) {
    Viewport viewport;
    viewport.setSurfaceColor(1, QColor(0, 0, 200));
    EXPECT_EQ(viewport.surfaceColor(1), QColor(0, 0, 200));
    EXPECT_EQ(viewport.polygonColor(1), QColor(0, 0, 200).darker(Viewport::kPolygonDarkness));
    EXPECT_EQ(viewport.meshColor(1), QColor(0, 0, 200).darker(Viewport::kMeshDarkness));
    // white stays visible on the white background: its shades are grays
    viewport.setSurfaceColor(2, Qt::white);
    EXPECT_LT(viewport.polygonColor(2).value(), 255);
    EXPECT_LT(viewport.meshColor(2).value(), viewport.polygonColor(2).value());
}

TEST(ViewportColorTest, SettingColorChangesFillOfOnlyThatSurface) {
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    viewport.setSurfaces(squaresInRow(viewport));
    const QImage before = showViewport(viewport, kMeshImageSize);
    viewport.setSurfaceColor(1, QColor(255, 255, 0));
    const QImage after = grabImage(viewport);
    for (int k = 0; k < 3; ++k) {
        SCOPED_TRACE(k);
        const QPointF p = sceneToScreen(kSquareCenters[k]);
        if (k == 1)
            EXPECT_NE(colorAt(after, p), colorAt(before, p));
        else
            EXPECT_EQ(colorAt(after, p), colorAt(before, p));
    }
    // yellow light-colored fill: red and green equal, no blue
    const QColor c = colorAt(after, sceneToScreen(kSquareCenters[1]));
    EXPECT_EQ(c.red(), c.green());
    EXPECT_EQ(c.blue(), 0);
}

TEST(ViewportColorTest, FourthSurfaceWrapsToFirstColor) {
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    viewport.setSurfaces(squaresInRow(viewport, true));
    const QImage before = showViewport(viewport, kMeshImageSize);
    EXPECT_EQ(surfaceAt(before, sceneToScreen(kSquareCenters[3])), surfaceAt(before, sceneToScreen(kSquareCenters[0])));
    viewport.setSurfaceColor(0, QColor(0, 0, 0));
    const QImage after = grabImage(viewport);
    for (const int k : {0, 3})
        EXPECT_EQ(colorAt(after, sceneToScreen(kSquareCenters[k])), QColor(0, 0, 0)) << "surface " << k;
    for (const int k : {1, 2})
        EXPECT_EQ(colorAt(after, sceneToScreen(kSquareCenters[k])), colorAt(before, sceneToScreen(kSquareCenters[k])));
}

TEST(ViewportColorTest, PolygonAndMeshUseDerivedShades) {
    // one surface: color 0. Pure blue, so every drawn pixel (antialiased with white) stays blue-dominant
    const QColor blue(0, 0, 255);
    for (const bool polygon : {true, false}) {
        SCOPED_TRACE(polygon ? "polygon" : "mesh");
        Viewport viewport;
        polygon ? showOnlyPolygon(viewport) : showOnlyMesh(viewport);
        viewport.setSurfaces({flatSquare()});
        viewport.setSurfaceColor(0, blue);
        const QImage img = showViewport(viewport, kMeshImageSize);
        ASSERT_FALSE(drawnPixels(img).empty());
        for (const QPoint &p : drawnPixels(img)) {
            const QColor c = img.pixelColor(p);
            if (c.red() > 240) continue; // faint antialiased edge, almost white: no hue to check
            ASSERT_GT(c.blue(), c.red()) << p.x() << ", " << p.y();
            ASSERT_NEAR(c.red(), c.green(), 1) << p.x() << ", " << p.y();
        }
        // polygon: the dots are drawn in the polygon shade too, so check the lines alone
        std::vector<QPointF> dots;
        if (polygon)
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    dots.push_back(screenPoint(viewport, controlPointOfFlatSquare(i, j)));
        const QColor expected = polygon ? viewport.polygonColor(0) : viewport.meshColor(0);
        EXPECT_NEAR(darkestChannel(img, &QColor::blue, dots, 5.0), expected.blue(), 8);
    }
}

// ---------- 8: texture as the object color ----------

namespace {
Texture solidTexture(const QColor &c) {
    return {1, 1, {static_cast<std::uint8_t>(c.red()), static_cast<std::uint8_t>(c.green()),
                   static_cast<std::uint8_t>(c.blue())}};
}

constexpr int kStripes = 20;

/// Bytes of stripe k: red grows and blue falls with k, red + blue = 300 for every stripe,
/// so k can be read back from red / (red + blue) whatever the brightness of the lighting is
std::array<std::uint8_t, 3> stripeBytes(const int k) {
    return {static_cast<std::uint8_t>(50 + 10 * k), 0, static_cast<std::uint8_t>(250 - 10 * k)};
}

/// kStripes texels in a row (along u) or in a column (along v)
Texture stripes(const bool alongU) {
    std::vector<std::uint8_t> bytes;
    for (int k = 0; k < kStripes; ++k) {
        const auto b = stripeBytes(k);
        bytes.insert(bytes.end(), b.begin(), b.end());
    }
    return alongU ? Texture(kStripes, 1, bytes) : Texture(1, kStripes, bytes);
}

/// Stripe index seen in a pixel (inverse of stripeBytes), -1 if the pixel is not a lit stripe
float stripeAt(const QImage &img, const QPointF &p) {
    const QColor c = colorAt(img, p);
    if (c.red() + c.blue() < 60 || c.green() != 0) return -1.0f;
    const float ratio = static_cast<float>(c.red()) / static_cast<float>(c.red() + c.blue());
    return (300.0f * ratio - 50.0f) / 10.0f;
}

/// Square lying flat on the screen at the scene centre: (u, v) is linear in the screen position
BezierSurface textureSquare(const Viewport &viewport) {
    return sceneRect(viewport, {0.0f, 0.0f, 0.0f}, 3.0f, 3.0f);
}
} // namespace

TEST(ViewportTextureTest, OffByDefaultWithWhiteTexture) {
    const Viewport viewport;
    EXPECT_FALSE(viewport.useTexture());
    EXPECT_EQ(viewport.texture().width(), 1);
    EXPECT_EQ(viewport.texture().height(), 1);
    const ShadingColor c = viewport.texture().texel(0, 0);
    EXPECT_FLOAT_EQ(c.r, 1.0f);
    EXPECT_FLOAT_EQ(c.g, 1.0f);
    EXPECT_FLOAT_EQ(c.b, 1.0f);
}

TEST(ViewportTextureTest, SettersStoreValuesAndRedraw) {
    // shown widget: only a setter that calls render() changes the grabbed image
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    viewport.setSurfaces({textureSquare(viewport)});
    const QPointF p = sceneToScreen({0.0f, 0.0f, 0.0f});
    const QColor initial = colorAt(showViewport(viewport, kMeshImageSize), p);

    viewport.setTexture(solidTexture(QColor(0, 0, 255)));
    EXPECT_EQ(viewport.texture().texel(0, 0).b, 1.0f);
    EXPECT_EQ(colorAt(grabImage(viewport), p), initial) << "texture set but not used yet";

    viewport.setUseTexture(true);
    EXPECT_TRUE(viewport.useTexture());
    const QColor blue = colorAt(grabImage(viewport), p);
    EXPECT_NE(blue, initial) << "setUseTexture";

    viewport.setTexture(solidTexture(QColor(0, 255, 0)));
    EXPECT_NE(colorAt(grabImage(viewport), p), blue) << "setTexture";
}

TEST(ViewportTextureTest, SolidTextureReplacesSurfaceColor) {
    // diffuse only, white light: the fill keeps only the channels of the texture
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    viewport.setSurfaces({textureSquare(viewport)});
    viewport.setTexture(solidTexture(QColor(255, 0, 0)));
    viewport.setUseTexture(true);
    const QColor c = colorAt(showViewport(viewport, kMeshImageSize), sceneToScreen({0.0f, 0.0f, 0.0f}));
    EXPECT_GT(c.red(), 100);
    EXPECT_EQ(c.green(), 0);
    EXPECT_EQ(c.blue(), 0);
}

TEST(ViewportTextureTest, TextureIsLitLikeSurfaceColor) {
    // the texture is IO in the lighting equation, not a color painted over it
    Viewport viewport;
    showOnlyFill(viewport);
    viewport.setSurfaces({textureSquare(viewport)});
    viewport.setTexture(solidTexture(QColor(255, 255, 255)));
    viewport.setUseTexture(true);
    viewport.setKd(0.0f);
    viewport.setKs(0.0f);
    const QImage img = showViewport(viewport, kMeshImageSize);
    EXPECT_EQ(colorAt(img, sceneToScreen({0.0f, 0.0f, 0.0f})), QColor(Qt::black));
    EXPECT_TRUE(isWhite(img, 0, 0)) << "background stays white";
}

TEST(ViewportTextureTest, UMapsAlongFirstParameterAndVAlongSecond) {
    // u = 0.25 and u = 0.75 fall into different texels of a texture split along u,
    // but into the same texel of a texture split along v (and the other way round)
    const std::array<std::uint8_t, 3> red = {255, 0, 0}, blue = {0, 0, 255};
    const Texture splitU(2, 1, {red[0], red[1], red[2], blue[0], blue[1], blue[2]});
    const Texture splitV(1, 2, {red[0], red[1], red[2], blue[0], blue[1], blue[2]});

    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    const BezierSurface surface = textureSquare(viewport);
    viewport.setSurfaces({surface});
    viewport.setUseTexture(true);
    const auto at = [&](const QImage &img, const float u, const float v) {
        return surfaceAt(img, screenPoint(viewport, surface.evaluate(u, v).P)); // 0 = red, 2 = blue
    };

    viewport.setTexture(splitU);
    QImage img = showViewport(viewport, kMeshImageSize);
    EXPECT_EQ(at(img, 0.25f, 0.5f), 0);
    EXPECT_EQ(at(img, 0.75f, 0.5f), 2);
    EXPECT_EQ(at(img, 0.5f, 0.25f), at(img, 0.5f, 0.75f));

    viewport.setTexture(splitV);
    img = grabImage(viewport);
    EXPECT_EQ(at(img, 0.5f, 0.25f), 0);
    EXPECT_EQ(at(img, 0.5f, 0.75f), 2);
    EXPECT_EQ(at(img, 0.25f, 0.5f), at(img, 0.75f, 0.5f));
}

TEST(ViewportTextureTest, TextureCoordinatesAreInterpolatedInsideTriangles) {
    // 20 stripes, mesh vertices every 0.1 = every 2 stripes: the odd stripes lie strictly inside triangles,
    // so (u, v) taken from a single vertex instead of interpolated would give a neighbouring stripe
    for (const bool alongU : {true, false}) {
        SCOPED_TRACE(alongU ? "along u" : "along v");
        Viewport viewport;
        showOnlyDiffuseFill(viewport);
        const BezierSurface surface = textureSquare(viewport);
        viewport.setSurfaces({surface});
        viewport.setTexture(stripes(alongU));
        viewport.setUseTexture(true);
        const QImage img = showViewport(viewport, kMeshImageSize);

        for (const int k : {1, 3, 7, 11, 15}) {
            const float t = (static_cast<float>(k) + 0.5f) / kStripes; // centre of stripe k
            for (const float other : {0.23f, 0.61f}) {
                const Vec3 P = alongU ? surface.evaluate(t, other).P : surface.evaluate(other, t).P;
                EXPECT_NEAR(stripeAt(img, screenPoint(viewport, P)), static_cast<float>(k), 0.3f)
                    << "stripe " << k << ", other parameter " << other;
            }
        }
    }
}

TEST(ViewportTextureTest, TextureFollowsSurfaceWhenRotated) {
    // the texture is glued to (u, v): after a drag the same surface point shows the same stripe
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    const BezierSurface surface = textureSquare(viewport);
    viewport.setSurfaces({surface});
    viewport.setTexture(stripes(true));
    viewport.setUseTexture(true);
    showViewport(viewport, kMeshImageSize);
    drag(viewport, QPoint(300, 300), QPoint(330, 310));
    const QImage img = grabImage(viewport);

    for (const int k : {3, 9, 15}) {
        const float u = (static_cast<float>(k) + 0.5f) / kStripes;
        EXPECT_NEAR(stripeAt(img, screenPoint(viewport, surface.evaluate(u, 0.5f).P)), static_cast<float>(k), 0.3f)
            << "stripe " << k;
    }
}

TEST(ViewportTextureTest, TextureIsUsedOnEverySurface) {
    // surface colors differ (red, green, blue); with a green texture all three squares are green
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    viewport.setSurfaces(squaresInRow(viewport));
    viewport.setTexture(solidTexture(QColor(0, 255, 0)));
    viewport.setUseTexture(true);
    const QImage img = showViewport(viewport, kMeshImageSize);
    for (int k = 0; k < 3; ++k)
        EXPECT_EQ(surfaceAt(img, sceneToScreen(kSquareCenters[k])), 1) << "surface " << k;
}

TEST(ViewportTextureTest, SwitchingTextureOffRestoresImage) {
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    const QImage before = showViewport(viewport, kMeshImageSize);
    viewport.setTexture(stripes(true));
    viewport.setUseTexture(true);
    ASSERT_NE(grabImage(viewport), before);
    viewport.setUseTexture(false);
    EXPECT_EQ(grabImage(viewport), before);
}

// ---------- 8: normal map ----------

namespace {
/// 1x1 normal map storing direction n (in the surface frame: x along Pu, y along Pv, z along N)
Texture uniformNormalMap(const Vec3 &n) {
    const Vec3 d = normalized(n);
    const auto byte = [](const float c) { return static_cast<std::uint8_t>(std::lround((c + 1.0f) * 127.5f)); };
    return {1, 1, {byte(d.x), byte(d.y), byte(d.z)}};
}

/// 2x1 normal map: left half (u < 0.5) tilted towards +Pu, right half towards -Pu, by 45 degrees
Texture ridgeNormalMap() {
    const Texture left = uniformNormalMap({1.0f, 0.0f, 1.0f});
    const Texture right = uniformNormalMap({-1.0f, 0.0f, 1.0f});
    const auto bytes = [](const Texture &t) {
        const ShadingColor c = t.texel(0, 0);
        return std::array{static_cast<std::uint8_t>(std::lround(c.r * 255.0f)),
                          static_cast<std::uint8_t>(std::lround(c.g * 255.0f)),
                          static_cast<std::uint8_t>(std::lround(c.b * 255.0f))};
    };
    const auto l = bytes(left), r = bytes(right);
    return {2, 1, {l[0], l[1], l[2], r[0], r[1], r[2]}};
}

/// Expected red channel of the diffuse-only fill (showOnlyDiffuseFill) of surface 0 at (u, v):
/// the normal is bent exactly like Viewport does it, from the positions after rotation
float expectedDiffuseRed(const Viewport &viewport, const BezierSurface &surface, const float u, const float v,
                         const bool withMap, const float ioRed = -1.0f) {
    const SurfacePoint sp = surface.evaluate(u, v);
    const Mat3 &R = viewport.rotation();
    Vec3 N = normalized(R * sp.N);
    if (withMap)
        N = perturbNormal(R * sp.Pu, R * sp.Pv, N, normalFromMap(viewport.normalMap().sample(u, v)));
    if (N.z < 0) N = -N; // lit from both sides
    const Vec3 L = normalized(viewport.lightPosition() - R * sp.P);
    const float io = ioRed >= 0.0f ? ioRed : static_cast<float>(viewport.surfaceColor(0).red()) / 255.0f;
    return 255.0f * viewport.kd() * io * std::max(0.0f, dot(N, L));
}

int redAt(const Viewport &viewport, const QImage &img, const BezierSurface &surface, const float u, const float v) {
    return colorAt(img, screenPoint(viewport, surface.evaluate(u, v).P)).red();
}

/// The same square seen from below: rows of control points reversed, so Pu and N point the other way
BezierSurface flipped(const BezierSurface &s) {
    BezierSurface::ControlGrid g;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            g[i][j] = s.controlPoint(3 - i, j);
    return BezierSurface(g);
}

constexpr std::array<std::pair<float, float>, 5> kMapSamples = {{{0.2f, 0.3f}, {0.5f, 0.5f}, {0.8f, 0.2f}, {0.35f, 0.85f}, {0.7f, 0.65f}}};
} // namespace

TEST(ViewportNormalMapTest, OffByDefaultWithFlatMap) {
    const Viewport viewport;
    EXPECT_FALSE(viewport.useNormalMap());
    EXPECT_EQ(viewport.normalMap().width(), 1);
    EXPECT_EQ(viewport.normalMap().height(), 1);
    EXPECT_VEC3_NEAR(colorToNormal(viewport.normalMap().texel(0, 0)), (Vec3{0.0f, 0.0f, 1.0f}), 0.01f);
}

TEST(ViewportNormalMapTest, SettersStoreValuesAndRedraw) {
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    const BezierSurface surface = textureSquare(viewport);
    viewport.setSurfaces({surface});
    const QPointF p = screenPoint(viewport, surface.evaluate(0.3f, 0.6f).P);
    const QColor initial = colorAt(showViewport(viewport, kMeshImageSize), p);

    viewport.setNormalMap(uniformNormalMap({0.6f, 0.0f, 0.8f}));
    EXPECT_GT(colorToNormal(viewport.normalMap().texel(0, 0)).x, 0.5f);
    EXPECT_EQ(colorAt(grabImage(viewport), p), initial) << "map set but not used yet";

    viewport.setUseNormalMap(true);
    EXPECT_TRUE(viewport.useNormalMap());
    const QColor bent = colorAt(grabImage(viewport), p);
    EXPECT_NE(bent, initial) << "setUseNormalMap";

    viewport.setNormalMap(uniformNormalMap({-0.6f, 0.0f, 0.8f}));
    EXPECT_NE(colorAt(grabImage(viewport), p), bent) << "setNormalMap";
}

TEST(ViewportNormalMapTest, FlatMapKeepsImageAlmostUnchanged) {
    // the default map is RGB(128, 128, 255) = [0.004, 0.004, 1]: only rounding-size differences.
    // Exception: single pixels on the silhouette (N.z ~ 0), where the tiny tilt flips the sign of N.z and
    // two-sided lighting turns the normal around (seen: 9 of ~110000 drawn pixels)
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    showOnlyDiffuseFill(viewport);
    const QImage before = showViewport(viewport, kMeshImageSize);
    viewport.setUseNormalMap(true);
    const QImage after = grabImage(viewport);
    const std::vector<QPoint> drawn = drawnPixels(before);
    ASSERT_GT(drawn.size(), 10000u);
    int changed = 0;
    for (const QPoint &p : drawn) {
        const QColor a = before.pixelColor(p), b = after.pixelColor(p);
        if (std::max({std::abs(a.red() - b.red()), std::abs(a.green() - b.green()), std::abs(a.blue() - b.blue())}) > 3)
            ++changed;
    }
    EXPECT_LT(changed, static_cast<int>(drawn.size() / 1000)) << changed << " pixels changed by more than 3";
}

TEST(ViewportNormalMapTest, BumpIsLitOnTheSideFacingTheLight) {
    // normal_circle is a round bump: wherever the light is, the side of the bump facing it is the brightest.
    // Checks the orientation of both axes of the map against the scene (the y axis of normal maps points
    // up the image, v grows down the image), independently of how the normal is computed
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    viewport.setSurfaces({textureSquare(viewport)}); // flat on the screen, centred, half side 3
    TextureLoadResult bump = loadTexture(":/assets/normal_maps/normal_circle.png");
    ASSERT_TRUE(bump.texture.has_value()) << bump.error.toStdString();
    viewport.setNormalMap(std::move(*bump.texture));
    viewport.setUseNormalMap(true);
    showViewport(viewport, kMeshImageSize);

    // four sides of the bump on the screen: right, top, left, bottom (scene y points up the screen)
    const std::array<Vec3, 4> sides = {{{1.8f, 0.0f, 0.0f}, {0.0f, 1.8f, 0.0f}, {-1.8f, 0.0f, 0.0f}, {0.0f, -1.8f, 0.0f}}};
    const float lap = static_cast<float>(viewport.lightPath().segments()) / Viewport::kLightSpeed;
    int checked = 0;
    for (int step = 0; step < 8; ++step) {
        const Vec3 light = viewport.lightPosition();
        const QImage img = grabImage(viewport);
        int brightest = 0, facing = 0;
        for (int k = 0; k < 4; ++k) {
            if (colorAt(img, sceneToScreen(sides[k])).red() > colorAt(img, sceneToScreen(sides[brightest])).red())
                brightest = k;
            if (dot(sides[k], light) > dot(sides[facing], light))
                facing = k;
        }
        // light on a diagonal faces two sides equally: no single answer, skip it
        float second = -1e9f;
        for (int k = 0; k < 4; ++k)
            if (k != facing) second = std::max(second, dot(sides[k], light));
        if (dot(sides[facing], light) - second > 0.5f * dot(sides[facing], light)) {
            ++checked;
            EXPECT_EQ(brightest, facing) << "light at " << light.x << ", " << light.y;
        }
        viewport.stepAnimation(lap / 8.0f);
    }
    EXPECT_EQ(checked, 4) << "the light should pass the four sides of the bump";
}

TEST(ViewportNormalMapTest, BentNormalGivesLambertBrightness) {
    // constant tilted map on a flat square: brightness = cos(N', L) with N' = [Pu, Pv, N] * N_map
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    const BezierSurface surface = textureSquare(viewport);
    viewport.setSurfaces({surface});
    viewport.setNormalMap(uniformNormalMap({0.5f, -0.4f, 0.75f}));
    viewport.setUseNormalMap(true);
    const QImage img = showViewport(viewport, kMeshImageSize);

    float largestChange = 0.0f;
    for (const auto &[u, v] : kMapSamples) {
        const float expected = expectedDiffuseRed(viewport, surface, u, v, true);
        EXPECT_NEAR(static_cast<float>(redAt(viewport, img, surface, u, v)), expected, 2.0f) << "u = " << u << ", v = " << v;
        largestChange = std::max(largestChange, std::abs(expected - expectedDiffuseRed(viewport, surface, u, v, false)));
    }
    EXPECT_GT(largestChange, 20.0f) << "the map must visibly change the shading for this test to mean anything";
}

TEST(ViewportNormalMapTest, ReliefChangesWithLightPosition) {
    // ridge: the half tilted towards the light is brighter; half a lap later the light is on the other side
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    const BezierSurface surface = textureSquare(viewport);
    viewport.setSurfaces({surface});
    viewport.setNormalMap(ridgeNormalMap());
    viewport.setUseNormalMap(true);
    showViewport(viewport, kMeshImageSize);

    // start where the light is far along x, so the two halves differ clearly
    const float lap = static_cast<float>(viewport.lightPath().segments()) / Viewport::kLightSpeed;
    float bestX = 0.0f, bestStep = 0.0f;
    for (int i = 0; i < 16; ++i) {
        const float x = std::abs(viewport.lightPosition().x);
        if (x > bestX) bestX = x, bestStep = static_cast<float>(i) * lap / 16.0f;
        viewport.stepAnimation(lap / 16.0f);
    }
    viewport.stepAnimation(bestStep); // after the full lap above: back at the start, then to the best place

    std::array<bool, 2> leftBrighter{};
    for (int k = 0; k < 2; ++k) {
        SCOPED_TRACE(k == 0 ? "first light position" : "half a lap later");
        const QImage img = grabImage(viewport);
        const int left = redAt(viewport, img, surface, 0.25f, 0.5f);
        const int right = redAt(viewport, img, surface, 0.75f, 0.5f);
        EXPECT_NEAR(static_cast<float>(left), expectedDiffuseRed(viewport, surface, 0.25f, 0.5f, true), 2.0f);
        EXPECT_NEAR(static_cast<float>(right), expectedDiffuseRed(viewport, surface, 0.75f, 0.5f, true), 2.0f);
        EXPECT_GT(std::abs(left - right), 20) << "relief not visible";
        leftBrighter[k] = left > right;
        viewport.stepAnimation(0.5f * lap);
    }
    EXPECT_NE(leftBrighter[0], leftBrighter[1]) << "the lit side of the relief must follow the light";
}

TEST(ViewportNormalMapTest, ReliefStaysOnSurfaceWhenRotated) {
    // the map is in the surface's frame: after a drag the brightness follows the rotated Pu, Pv, N
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    const BezierSurface surface = textureSquare(viewport);
    viewport.setSurfaces({surface});
    viewport.setNormalMap(uniformNormalMap({0.5f, -0.4f, 0.75f}));
    viewport.setUseNormalMap(true);
    showViewport(viewport, kMeshImageSize);
    drag(viewport, QPoint(300, 300), QPoint(320, 285));
    const QImage img = grabImage(viewport);
    for (const auto &[u, v] : kMapSamples)
        EXPECT_NEAR(static_cast<float>(redAt(viewport, img, surface, u, v)), expectedDiffuseRed(viewport, surface, u, v, true), 2.0f)
            << "u = " << u << ", v = " << v;
}

TEST(ViewportNormalMapTest, UnderSideIsLitWithBentNormal) {
    // seen from below N.z < 0: the bent normal is turned to the viewer like the plain one (two-sided lighting)
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    const BezierSurface surface = flipped(textureSquare(viewport));
    viewport.setSurfaces({surface});
    viewport.setNormalMap(uniformNormalMap({0.5f, -0.4f, 0.75f}));
    viewport.setUseNormalMap(true);
    const QImage img = showViewport(viewport, kMeshImageSize);
    ASSERT_LT((viewport.rotation() * surface.evaluate(0.5f, 0.5f).N).z, 0.0f) << "test needs the under side";
    for (const auto &[u, v] : kMapSamples) {
        const float expected = expectedDiffuseRed(viewport, surface, u, v, true);
        EXPECT_GT(expected, 30.0f);
        EXPECT_NEAR(static_cast<float>(redAt(viewport, img, surface, u, v)), expected, 2.0f) << "u = " << u << ", v = " << v;
    }
}

TEST(ViewportNormalMapTest, WorksTogetherWithTexture) {
    // texture sets IO, the map bends N: both at once
    Viewport viewport;
    showOnlyDiffuseFill(viewport);
    const BezierSurface surface = textureSquare(viewport);
    viewport.setSurfaces({surface});
    viewport.setTexture(solidTexture(QColor(255, 0, 0)));
    viewport.setUseTexture(true);
    viewport.setNormalMap(uniformNormalMap({0.5f, -0.4f, 0.75f}));
    viewport.setUseNormalMap(true);
    const QImage img = showViewport(viewport, kMeshImageSize);
    for (const auto &[u, v] : kMapSamples) {
        const QColor c = colorAt(img, screenPoint(viewport, surface.evaluate(u, v).P));
        EXPECT_EQ(c.green(), 0);
        EXPECT_EQ(c.blue(), 0);
        EXPECT_NEAR(static_cast<float>(c.red()), expectedDiffuseRed(viewport, surface, u, v, true, 1.0f), 2.0f)
            << "u = " << u << ", v = " << v;
    }
}

TEST(ViewportNormalMapTest, SwitchingNormalMapOffRestoresImage) {
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    const QImage before = showViewport(viewport, kMeshImageSize);
    viewport.setNormalMap(ridgeNormalMap());
    viewport.setUseNormalMap(true);
    ASSERT_NE(grabImage(viewport), before);
    viewport.setUseNormalMap(false);
    EXPECT_EQ(grabImage(viewport), before);
}

// ---------- 8b: reset rotation ----------

TEST(ViewportMouseTest, ResetRotationRestoresInitialViewAndImage) {
    Viewport viewport;
    viewport.setSurfaces(assetSurfaces());
    EXPECT_MAT3_EQ(viewport.rotation(), Viewport::initialRotation());
    const QImage initial = showViewport(viewport, kMeshImageSize);

    drag(viewport, QPoint(300, 300), QPoint(380, 250));
    ASSERT_NE(grabImage(viewport), initial);
    const BezierCurve path = viewport.lightPath();

    viewport.resetRotation();
    EXPECT_MAT3_EQ(viewport.rotation(), Viewport::initialRotation());
    EXPECT_EQ(grabImage(viewport), initial) << "meshes rotated back and the image redrawn";
    expectPathEqual(viewport.lightPath(), path); // the light path is bound to the camera, not to the scene
}

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(ViewportResolutionDeathTest, RejectsResolutionOutOfRange) {
    GTEST_FLAG_SET(death_test_style, "threadsafe"); // Qt has threads running, don't just fork
    Viewport viewport;
    EXPECT_DEATH(viewport.setResolution(Mesh::kMinResolution - 1), "out of range");
    EXPECT_DEATH(viewport.setResolution(Mesh::kMaxResolution + 1), "out of range");
}

TEST(ViewportDeathTest, RejectsInvalidLightColor) {
    Viewport viewport;
    EXPECT_DEATH(viewport.setLightColor(QColor()), "invalid light color");
}

TEST(ViewportDeathTest, RejectsSurfaceColorIndexOutOfRangeAndInvalidColor) {
    Viewport viewport;
    EXPECT_DEATH(viewport.setSurfaceColor(-1, Qt::red), "surface color index out of range");
    EXPECT_DEATH(viewport.setSurfaceColor(Viewport::kColorCount, Qt::red), "surface color index out of range");
    EXPECT_DEATH(std::ignore = viewport.surfaceColor(Viewport::kColorCount), "surface color index out of range");
    EXPECT_DEATH(viewport.setSurfaceColor(0, QColor()), "invalid surface color");
}

TEST(ViewportDeathTest, RejectsLightZOutOfRange) {
    Viewport viewport;
    EXPECT_DEATH(viewport.setLightZ(Viewport::kMinLightZ - 0.1f), "light z out of range");
    EXPECT_DEATH(viewport.setLightZ(Viewport::kMaxLightZ + 0.1f), "light z out of range");
}
#endif
