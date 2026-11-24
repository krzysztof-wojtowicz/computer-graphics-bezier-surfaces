#include "ui/Viewport.h"

#include <QPainter>
#include <QColor>
#include <QPointF>
#include <QLineF>
#include <QPen>
#include <QPolygonF>
#include <QRectF>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <vector>
#include <utility>

#include "renderer/MouseRotation.h"
#include "renderer/Shading.h"
#include "renderer/Barycentric.h"

Viewport::Viewport(QWidget *parent) : QWidget(parent) {
    setMinimumSize(400, 400);

    m_animationTimer.setInterval(kAnimationIntervalMs);
    connect(&m_animationTimer, &QTimer::timeout, this, &Viewport::onAnimationTick);
}

void Viewport::setAnimating(const bool on) {
    if (on == isAnimating()) return;

    if (on) {
        m_animationClock.start(); // first dt measured from now, not from an old or invalid clock
        m_animationTimer.start();
    } else {
        m_animationTimer.stop();
    }
}

void Viewport::stepAnimation(const float seconds) {
    // wrap so t stays small: float precision of t + dt drops as t grows
    m_lightT = std::fmod(m_lightT + kLightSpeed * seconds, 1.0f);
    if (m_lightT < 0) m_lightT += 1.0f;
    if (!(m_lightT < 1.0f)) m_lightT = 0.0f; // tiny negative t + 1 rounds to exactly 1 in float

    render(); // render() takes the light position from m_lightT
    update();
}

/// QTimer intervals are not exact (render time delays the next tick), so the step uses measured time
void Viewport::onAnimationTick() {
    const float dt = static_cast<float>(m_animationClock.restart()) / 1000.0f;
    stepAnimation(std::min(dt, kMaxAnimationStep));
}

void Viewport::setResolution(const int n) {
    assert(n >= Mesh::kMinResolution && n <= Mesh::kMaxResolution && "n out of range!");
    if (n == m_resolution) return;
    m_resolution = n;

    for (auto &mesh: m_meshes)
        mesh.setResolution(n); // current rotation is kept by _lastRotation

    render();
    update();
}

void Viewport::setShowMesh(const bool show) {
    m_showMesh = show;

    render();
    update();
}

void Viewport::setShowPolygon(const bool show) {
    m_showPolygon = show;

    render();
    update();
}

void Viewport::setShowPoints(const bool show) {
    m_showPoints = show;

    render();
    update();
}

void Viewport::setShowFill(const bool show) {
    m_showFill = show;

    render();
    update();
}

void Viewport::setShowLightPath(const bool show) {
    m_showLightPath = show;

    render();
    update();
}

void Viewport::setKd(const float kd) {
    m_shader.setKd(kd);

    render();
    update();
}

void Viewport::setKs(const float ks) {
    m_shader.setKs(ks);

    render();
    update();
}

void Viewport::setM(const float m) {
    m_shader.setM(m);

    render();
    update();
}

namespace {
ShadingColor toShadingColor(const QColor &color) {
    return {
        .r = Shading::int2float(color.red()),
        .g = Shading::int2float(color.green()),
        .b = Shading::int2float(color.blue())
    };
}
} // namespace

std::vector<QColor> Viewport::lightColors() const {
    if (!spotlights()) return {m_lightColor};
    return {kSpotlightColors.begin(), kSpotlightColors.begin() + m_lightCount};
}

/// Kept also while the spotlights are on: it comes back with the single light
void Viewport::setLightColor(const QColor &color) {
    assert(color.isValid() && "invalid light color!");
    m_lightColor = color.toRgb();

    render();
    update();
}

/// Works also when the animation is stopped: render() takes the light position from the path
void Viewport::setLightZ(const float z) {
    assert(z >= kMinLightZ && z <= kMaxLightZ && "light z out of range!");
    // both paths: the slider sets the height of the light whichever path it follows
    m_bezierPath.setZ(z);
    m_bsplinePath.setZ(z);

    render();
    update();
}

QColor Viewport::surfaceColor(const int i) const {
    assert(i >= 0 && i < kColorCount && "surface color index out of range!");
    return m_surfaceColors[i];
}

void Viewport::setSurfaceColor(const int i, const QColor &color) {
    assert(i >= 0 && i < kColorCount && "surface color index out of range!");
    assert(color.isValid() && "invalid surface color!");
    m_surfaceColors[i] = color;

    render();
    update();
}

bool Viewport::useTexture(const int i) const {
    assert(i >= 0 && i < kColorCount && "surface group index out of range!");
    return m_useTexture[i];
}

const Texture &Viewport::texture(const int i) const {
    assert(i >= 0 && i < kColorCount && "surface group index out of range!");
    return m_textures[i];
}

bool Viewport::useNormalMap(const int i) const {
    assert(i >= 0 && i < kColorCount && "surface group index out of range!");
    return m_useNormalMap[i];
}

const Texture &Viewport::normalMap(const int i) const {
    assert(i >= 0 && i < kColorCount && "surface group index out of range!");
    return m_normalMaps[i];
}

void Viewport::setUseTexture(const int i, const bool use) {
    assert(i >= 0 && i < kColorCount && "surface group index out of range!");
    m_useTexture[i] = use;

    render();
    update();
}

void Viewport::setTexture(const int i, Texture texture) {
    assert(i >= 0 && i < kColorCount && "surface group index out of range!");
    m_textures[i] = std::move(texture);

    render();
    update();
}

void Viewport::setUseNormalMap(const int i, const bool use) {
    assert(i >= 0 && i < kColorCount && "surface group index out of range!");
    m_useNormalMap[i] = use;

    render();
    update();
}

void Viewport::setNormalMap(const int i, Texture normalMap) {
    assert(i >= 0 && i < kColorCount && "surface group index out of range!");
    m_normalMaps[i] = std::move(normalMap);

    render();
    update();
}

void Viewport::resetRotation() {
    m_rotation = initialRotation();
    applyRotationToMeshes();

    render();
    update();
}

bool Viewport::surfaceVisible(const int i) const {
    assert(i >= 0 && i < kColorCount && "surface group index out of range!");
    return m_surfaceVisible[i];
}

void Viewport::setSurfaceVisible(const int i, const bool visible) {
    assert(i >= 0 && i < kColorCount && "surface group index out of range!");
    m_surfaceVisible[i] = visible;

    render();
    update();
}

void Viewport::setLightPathType(const LightPathType type) {
    if (type == m_lightPathType) return;
    m_lightPathType = type;
    m_draggedPoint = -1; // a drag started on the old curve ends; the points are the same, so nothing else changes

    render();
    update();
}

const ClosedCurve &Viewport::lightPath() const {
    if (m_lightPathType == LightPathType::Bezier) return m_bezierPath;
    return m_bsplinePath;
}

std::vector<Vec3> Viewport::defaultLightPathPoints(const float z) {
    return BezierCurve::circle(kDefaultLightPathRadius, z).points();
}

/// Neighbouring segments must contrast, so the joins are visible: consecutive segments jump around the hue circle
/// by a stride coprime with count and close to count / 2 (each hue still used once; for 12 segments 150 degrees,
/// also between the last and the first), and they alternate between a brighter and a darker shade
QColor Viewport::lightPathSegmentColor(const int s, const int count) {
    assert(count >= 1 && s >= 0 && s < count && "segment index out of range!");
    int stride = 1;
    for (int k = count / 2; k > 1; --k)
        if (std::gcd(k, count) == 1) {
            stride = k;
            break;
        }
    const int hue = 360 * (s * stride % count) / count;
    return QColor::fromHsv(hue, 255, s % 2 == 0 ? kSegmentBrightValue : kSegmentDarkValue);
}

/// From the path, not from the shader: valid also before the first render()
Vec3 Viewport::lightPosition() const {
    const ClosedCurve &path = lightPath();
    return path.evaluate(m_lightT * static_cast<float>(path.segments()));
}

/// Spread evenly in the lap parameter, the same one the light moves by: the lights keep their distances
/// along the curve while they move, on both path types
std::vector<Vec3> Viewport::lightPositions() const {
    const ClosedCurve &path = lightPath();
    std::vector<Vec3> positions;
    positions.reserve(static_cast<std::size_t>(m_lightCount));
    for (int k = 0; k < m_lightCount; ++k) {
        float t = m_lightT + static_cast<float>(k) / static_cast<float>(m_lightCount);
        if (t >= 1.0f) t -= 1.0f; // both terms are in [0, 1)
        positions.push_back(path.evaluate(t * static_cast<float>(path.segments())));
    }
    return positions;
}

void Viewport::setLightCount(const int count) {
    assert((count == 1 || count == kMaxLightCount) && "1 or kMaxLightCount lights!");
    if (count == m_lightCount) return;
    m_lightCount = count;

    render();
    update();
}

void Viewport::setSpotExponent(const float k) {
    assert(k >= Shading::kMinSpotExponent && k <= Shading::kMaxSpotExponent && "spot exponent out of range!");
    m_shader.setSpotExponent(k);

    render();
    update();
}

void Viewport::resetLightPath() {
    const auto points = defaultLightPathPoints(lightZ());
    m_bezierPath = ClosedBezierCurve(points);
    m_bsplinePath = BSplineCurve(points);
    m_draggedPoint = -1; // a reset during a drag must not keep moving a point of the old curve

    render();
    update();
}


/// Generates mesh for each Bezier Surface
void Viewport::setSurfaces(const std::vector<BezierSurface> &surfaces) {
    m_meshes.clear();
    m_meshes.reserve(surfaces.size());

    for (auto &surf: surfaces) {
        // Generate mesh
        m_meshes.emplace_back(surf, m_resolution);
    }

    applyRotationToMeshes();
    render();
    update();
}

void Viewport::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.drawImage(0, 0, m_frame);
}

void Viewport::resizeEvent(QResizeEvent *) {
    m_frame = QImage(size(), QImage::Format_RGB32);
    if (!m_frame.isNull()) // a collapsed widget has size 0, render() returns early then
        m_zbuffer.resize(m_frame.width(), m_frame.height());

    render();
}

/// A press near a visible light path point grabs the point, anywhere else it starts rotating the scene
void Viewport::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_lastMousePos = event->position().toPoint();
    m_draggedPoint = pickLightPathPoint(event->position());
    if (m_draggedPoint >= 0) {
        const Projection proj(m_frame.width(), m_frame.height());
        const auto pos = event->position();
        m_dragOffset = lightPath().point(m_draggedPoint)
                       - proj.unproject(static_cast<float>(pos.x()), static_cast<float>(pos.y()), lightZ());
        setCursor(Qt::SizeAllCursor);
    } else {
        setCursor(Qt::ClosedHandCursor);
    }
}

void Viewport::mouseMoveEvent(QMouseEvent *event) {
    if (!(event->buttons() & Qt::LeftButton)) return;

    if (m_draggedPoint >= 0) {
        // the light path is camera-fixed (not rotated), so the screen maps straight to its plane z = const;
        // the cursor is kept inside the widget so a point can't be dragged out of sight
        const Projection proj(m_frame.width(), m_frame.height());
        const float x = std::clamp(static_cast<float>(event->position().x()), 0.0f,
                                   static_cast<float>(m_frame.width() - 1));
        const float y = std::clamp(static_cast<float>(event->position().y()), 0.0f,
                                   static_cast<float>(m_frame.height() - 1));
        const Vec3 p = proj.unproject(x, y, lightZ()) + m_dragOffset;
        // the points are shared: both curves get the move
        m_bezierPath.movePoint(m_draggedPoint, p.x, p.y);
        m_bsplinePath.movePoint(m_draggedPoint, p.x, p.y);
    } else {
        const auto pos = event->position().toPoint();
        const auto delta = pos - m_lastMousePos;
        m_lastMousePos = pos;
        m_rotation = applyDrag(m_rotation, static_cast<float>(delta.x()), static_cast<float>(delta.y()));
        applyRotationToMeshes();
    }
    render();
    update();
}

void Viewport::mouseReleaseEvent(QMouseEvent *) {
    m_draggedPoint = -1;
    unsetCursor();
}

/// Index of the light path control point closest to pos within kPickRadius, -1 if none.
/// Hidden points can't be grabbed, so with the path switched off the mouse always rotates.
int Viewport::pickLightPathPoint(const QPointF &pos) const {
    if (!m_showLightPath || m_frame.isNull()) return -1;

    const Projection proj(m_frame.width(), m_frame.height());
    int best = -1;
    float bestDist2 = kPickRadius * kPickRadius;
    const ClosedCurve &path = lightPath();
    for (int i = 0; i < path.size(); ++i) {
        const ScreenPoint sp = proj.project(path.point(i));
        const float dx = sp.x - static_cast<float>(pos.x());
        const float dy = sp.y - static_cast<float>(pos.y());
        const float dist2 = dx * dx + dy * dy;
        if (dist2 <= bestDist2) {
            bestDist2 = dist2;
            best = i;
        }
    }
    return best;
}

/// Renders image in the viewport, <br>
/// for now -> draws vertices of each mesh
void Viewport::render() {
    if (m_frame.isNull()) return;

    // update light positions
    const std::vector<Vec3> lights = lightPositions();
    m_shader.setLightPositions(lights);
    m_shader.setSpotlights(spotlights());
    std::vector<ShadingColor> colors;
    for (const QColor &c: lightColors())
        colors.push_back(toShadingColor(c));
    m_shader.setLightColors(colors);

    // fill viewport with white color
    m_frame.fill(Qt::white);
    m_zbuffer.clear();

    if (m_meshes.empty()) return;

    // draw vertices and triangles of each mesh
    {
        const int w = m_frame.width();
        const int h = m_frame.height();
        const auto proj = Projection(w, h);

        // every layer of every visible surface; surface k takes color (and visibility switch) k % kColorCount.
        // A hidden surface is skipped entirely, also in the Z-buffer, so it doesn't hide what is behind it
        const auto forEachVisible = [this](auto &&draw) {
            for (int k = 0; k < static_cast<int>(m_meshes.size()); ++k) {
                const int idx = k % kColorCount;
                if (m_surfaceVisible[idx])
                    draw(m_meshes[k], idx);
            }
        };

        // fill triangles
        if (m_showFill) {
            const auto raster = Rasterizer(w, h);
            forEachVisible([&](const Mesh &mesh, const int idx) { fillTriangles(proj, mesh, raster, idx); });
        }

        QPainter painter(&m_frame);
        painter.setRenderHint(QPainter::Antialiasing);

        // draw every triangle for every mesh
        if (m_showMesh)
            forEachVisible([&](const Mesh &mesh, const int idx) { drawTriangles(proj, mesh, painter, idx); });

        // draw bezier polygon for every mesh
        if (m_showPolygon)
            forEachVisible([&](const Mesh &mesh, const int idx) { drawPolygon(proj, mesh, painter, idx); });

        // draw every mesh vertex for every mesh
        if (m_showPoints)
            forEachVisible([&](const Mesh &mesh, const int idx) { drawVertices(proj, mesh, painter, idx); });

        // draw every Bezier surface control point
        if (m_showPolygon)
            forEachVisible([&](const Mesh &mesh, const int idx) { drawControlPoints(proj, mesh, painter, idx); });

        // draw light path and current light position
        if (m_showLightPath) {
            drawLightPath(proj, painter);
        }
    } // destructor destroys painter and ends drawing on m_frame
}

/// Applies m_rotation to every mesh in m_meshes
void Viewport::applyRotationToMeshes() {
    for (auto &mesh: m_meshes) {
        mesh.applyRotation(m_rotation);
    }
}

/// Calculates control points of the bezier surface for a given mesh
std::array<std::array<Vec3, 4>, 4> Viewport::surfaceControlPoints(const Mesh &mesh) const {
    std::array<std::array<Vec3, 4>, 4> controlPoints = {};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            controlPoints[i][j] = m_rotation * mesh.surface().controlPoint(i, j);
        }
    }

    return controlPoints;
}

void Viewport::drawVertices(const Projection &proj, const Mesh &mesh, QPainter &painter, const int colorIdx) const {
    painter.setPen(Qt::NoPen);
    painter.setBrush(meshColor(colorIdx));

    for (const auto &vertices = mesh.vertices(); const auto &v: vertices) {
        const auto sp = proj.project(v.after.P);
        painter.drawEllipse(
            QPointF(sp.x, sp.y),
            kPointRadius / 2,
            kPointRadius / 2);
    }
}

/// Without antialiasing: at n = 50 the mesh has ~15k edges and smoothing them cost more than filling the triangles
void Viewport::drawTriangles(const Projection &proj, const Mesh &mesh, QPainter &painter, const int colorIdx) const {
    painter.save(); // the antialiasing hint is restored for the layers drawn after the mesh
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(meshColor(colorIdx), 1, Qt::SolidLine));
    const auto &vertices = mesh.vertices();
    for (const auto &[v1, v2, v3]: mesh.triangles()) {
        // a, b, c vertices of a triangle
        const auto a = proj.project(vertices[v1].after.P);
        const auto b = proj.project(vertices[v2].after.P);
        const auto c = proj.project(vertices[v3].after.P);

        // draw all three lines: ab, ac, bc
        QLineF lines[3];
        lines[0] = QLineF(
            QPointF(a.x, a.y),
            QPointF(b.x, b.y));
        lines[1] = QLineF(
            QPointF(a.x, a.y),
            QPointF(c.x, c.y));
        lines[2] = QLineF(
            QPointF(b.x, b.y),
            QPointF(c.x, c.y));
        painter.drawLines(lines, 3);
    }
    painter.restore();
}

void Viewport::drawControlPoints(const Projection &proj, const Mesh &mesh, QPainter &painter,
                                 const int colorIdx) const {
    painter.setPen(Qt::NoPen);
    painter.setBrush(polygonColor(colorIdx));

    // calculate control points projections
    std::array<std::array<ScreenPoint, 4>, 4> controlPointsProj = {};
    const auto controlPoints = surfaceControlPoints(mesh);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            controlPointsProj[i][j] = proj.project(controlPoints[i][j]);

    for (const auto &row: controlPointsProj) {
        for (const auto &cp: row) {
            painter.drawEllipse(QPointF(cp.x, cp.y), kPointRadius,
                                kPointRadius);
        }
    }
}

void Viewport::drawPolygon(const Projection &proj, const Mesh &mesh, QPainter &painter, const int colorIdx) const {
    painter.setPen(QPen(polygonColor(colorIdx), 1, Qt::DashLine));

    // calculate control points projections
    std::array<std::array<ScreenPoint, 4>, 4> controlPointsProj = {};
    const auto controlPoints = surfaceControlPoints(mesh);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            controlPointsProj[i][j] = proj.project(controlPoints[i][j]);

    QLineF lines[24];

    int idx = 0;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            const auto start = controlPointsProj[i][j];
            if (i + 1 < 4) {
                const auto end = controlPointsProj[i + 1][j];
                lines[idx++] = QLineF(
                    QPointF(start.x, start.y),
                    QPointF(end.x, end.y)
                );
            }
            if (j + 1 < 4) {
                const auto end = controlPointsProj[i][j + 1];
                lines[idx++] = QLineF(
                    QPointF(start.x, start.y),
                    QPointF(end.x, end.y)
                );
            }
        }

    painter.drawLines(lines, 24);
}

void Viewport::fillTriangles(const Projection &proj, const Mesh &mesh, const Rasterizer &raster, const int colorIdx) {
    const QRgb color = surfaceColor(colorIdx).rgb();
    const ShadingColor sColor = {
        .r = Shading::int2float(qRed(color)),
        .g = Shading::int2float(qGreen(color)),
        .b = Shading::int2float(qBlue(color))
    };
    const auto &vertices = mesh.vertices();
    const bool useTexture = m_useTexture[colorIdx], useNormalMap = m_useNormalMap[colorIdx];
    const Texture &texture = m_textures[colorIdx], &normalMap = m_normalMaps[colorIdx];
    // pixel access through one pointer: QImage::scanLine() per pixel checks for detaching every time
    auto *const frameBits = reinterpret_cast<QRgb *>(m_frame.bits());
    const qsizetype frameStride = m_frame.bytesPerLine() / static_cast<qsizetype>(sizeof(QRgb));

    for (const auto &[v1, v2, v3]: mesh.triangles()) {
        const ScreenPoint a = proj.project(vertices[v1].after.P);
        const ScreenPoint b = proj.project(vertices[v2].after.P);
        const ScreenPoint c = proj.project(vertices[v3].after.P);
        const Barycentric bary(a, b, c);
        if (bary.degenerate()) continue;

        raster.fillTriangle(
            a, b, c,
            [&](const int x, const int y) {
                const Weights w = bary.at(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
                const float z = interpolate(w, a.z, b.z, c.z);

                if (m_zbuffer.testAndSet(x, y, z)) {
                    // interpolate N
                    Vec3 N = interpolate(w, vertices[v1].after.N, vertices[v2].after.N, vertices[v3].after.N);
                    const auto P = interpolate(w, vertices[v1].after.P, vertices[v2].after.P, vertices[v3].after.P);

                    // object color: the surface color, or the texture at (u, v) of this pixel;
                    // normal: from the surface, or bent by the normal map at (u, v)
                    ShadingColor io = sColor;
                    if (useTexture || useNormalMap) {
                        const float u = interpolate(w, vertices[v1].u, vertices[v2].u, vertices[v3].u);
                        const float v = interpolate(w, vertices[v1].v, vertices[v2].v, vertices[v3].v);
                        if (useTexture)
                            io = texture.sample(u, v);
                        if (useNormalMap) {
                            const Vec3 Pu = interpolate(w, vertices[v1].after.Pu, vertices[v2].after.Pu, vertices[v3].after.Pu);
                            const Vec3 Pv = interpolate(w, vertices[v1].after.Pv, vertices[v2].after.Pv, vertices[v3].after.Pv);
                            N = perturbNormal(Pu, Pv, N, normalFromMap(normalMap.sample(u, v)));
                        }
                    }

                    const auto [red, green, blue] = m_shader.calculateColor(io, N, P);

                    frameBits[y * frameStride + x] = qRgb(
                        Shading::float2int(red),
                        Shading::float2int(green),
                        Shading::float2int(blue)
                    );
                }
            }
        );
    }
}

/// Control polygon under the curve, curve, control points, light on top.
/// The whole path lies in the plane z = lightZ(), so one depth serves every pixel: QPainter draws it into an overlay
/// (keeping antialiasing and dashes) and the Z-buffer erases the hidden pixels before the overlay goes on the frame
void Viewport::drawLightPath(const Projection &proj, QPainter &painter) const {
    QImage overlay(m_frame.size(), QImage::Format_ARGB32_Premultiplied);
    overlay.fill(Qt::transparent);
    {
        QPainter overlayPainter(&overlay);
        overlayPainter.setRenderHint(QPainter::Antialiasing);
        drawLightPathShapes(proj, overlayPainter);
    } // destructor ends drawing on overlay

    // larger z is closer to the observer (+z); an empty pixel holds lowest(), so the path stays visible there
    const float z = lightZ();
    for (int y = 0; y < overlay.height(); ++y) {
        auto *row = reinterpret_cast<QRgb *>(overlay.scanLine(y));
        for (int x = 0; x < overlay.width(); ++x)
            if (row[x] != 0 && m_zbuffer.at(x, y) >= z)
                row[x] = 0; // fully transparent in a premultiplied format
    }

    painter.drawImage(0, 0, overlay);
}

void Viewport::drawLightPathShapes(const Projection &proj, QPainter &painter) const {
    const auto toScreen = [&proj](const Vec3 &p) {
        const ScreenPoint sp = proj.project(p);
        return QPointF(sp.x, sp.y);
    };

    // control polygon: consecutive points, closed (Bezier: V5 -> V6 = V0; B-spline: the last segments use P[0])
    const ClosedCurve &path = lightPath();
    const auto &points = path.points();
    QPolygonF polygon;
    for (const auto &p: points)
        polygon << toScreen(p);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(kLightPathPolygonColor, 1, Qt::DotLine));
    painter.drawPolygon(polygon);

    // the curve itself: a closed polyline; B-spline segments each in its own color
    const int perSegment = std::max(kLightPathSamples, kLightPathMinSamples / path.segments());
    const auto samples = path.sample(perSegment);
    if (m_lightPathType == LightPathType::BSpline) {
        const int count = path.segments();
        for (int s = 0; s < count; ++s) {
            QPolygonF segment;
            for (int k = 0; k <= perSegment; ++k) // up to the first sample of the next segment: no gaps
                segment << toScreen(samples[static_cast<std::size_t>((s * perSegment + k) % std::ssize(samples))]);
            painter.setPen(QPen(lightPathSegmentColor(s, count), kSegmentLineWidth));
            painter.drawPolyline(segment);
        }
    } else {
        QPolygonF curve;
        for (const auto &p: samples)
            curve << toScreen(p);
        painter.setPen(QPen(kLightPathColor, 1, Qt::DashLine));
        painter.drawPolygon(curve);
    }

    // points on the curve as squares, points that only pull it as circles
    painter.setPen(QPen(kLightPathColor, 1));
    for (int i = 0; i < path.size(); ++i) {
        const QPointF c = toScreen(path.point(i));
        const bool anchor = path.passesThrough(i);
        // anchors filled, handles hollow; the grabbed point is filled too
        painter.setBrush(anchor || i == m_draggedPoint ? kLightPathColor : QColor(Qt::white));
        if (anchor)
            painter.drawRect(QRectF(c.x() - kPointRadius, c.y() - kPointRadius, 2 * kPointRadius, 2 * kPointRadius));
        else
            painter.drawEllipse(c, kPointRadius, kPointRadius);
    }

    // current light positions, each in its color; spotlights with an arrow along their beam
    const std::vector<Vec3> lights = lightPositions();
    const std::vector<QColor> colors = lightColors();
    for (std::size_t k = 0; k < lights.size(); ++k) {
        if (spotlights())
            drawSpotArrow(proj, painter, lights[k], colors[k]);
        painter.setPen(QPen(kLightPathColor, 1));
        painter.setBrush(colors[k]);
        painter.drawEllipse(toScreen(lights[k]), kPointRadius * 3, kPointRadius * 3);
    }
}

/// Arrow from the rim of the light marker along the screen projection of the spotlight axis (towards
/// Shading::kSpotTarget). Its length is kSpotArrowLength times the horizontal part of the unit axis, so a beam
/// pointing almost straight down (into the screen) gets a short arrow, and none when exactly down.
void Viewport::drawSpotArrow(const Projection &proj, QPainter &painter, const Vec3 &light, const QColor &color) const {
    const Vec3 axis = normalized(Shading::kSpotTarget - light);
    const float horizontal = std::hypot(axis.x, axis.y);
    const float length = kSpotArrowLength * horizontal;
    if (length < 2.0f) return;

    // screen direction of the axis: orthographic projection keeps x and flips y
    const ScreenPoint from = proj.project(light), to = proj.project(light + axis);
    const QPointF dir = QPointF(to.x - from.x, to.y - from.y) / std::hypot(to.x - from.x, to.y - from.y);
    const QPointF centre(from.x, from.y);
    const QPointF start = centre + dir * (kPointRadius * 3);
    const QPointF tip = start + dir * static_cast<qreal>(length);

    // arrowhead: two strokes back from the tip, 25 degrees off the shaft
    constexpr qreal kHead = 8, kHeadAngle = 0.436; // px, radians
    const auto rotated = [&dir](const qreal angle) {
        return QPointF(dir.x() * std::cos(angle) - dir.y() * std::sin(angle),
                       dir.x() * std::sin(angle) + dir.y() * std::cos(angle));
    };
    painter.setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(start, tip);
    painter.drawLine(tip, tip - rotated(kHeadAngle) * kHead);
    painter.drawLine(tip, tip - rotated(-kHeadAngle) * kHead);
}
