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
#include <array>
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
#endif
