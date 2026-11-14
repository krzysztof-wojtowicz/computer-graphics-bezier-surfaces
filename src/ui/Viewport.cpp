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
    const auto segments = static_cast<float>(m_lightPath.segments());
    // wrap so t stays small: float precision of t + dt drops as t grows
    m_lightT = std::fmod(m_lightT + kLightSpeed * seconds, segments);
    if (m_lightT < 0) m_lightT += segments;

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

QColor Viewport::lightColor() const {
    const auto [r, g, b] = m_shader.il();
    return {Shading::float2int(r), Shading::float2int(g), Shading::float2int(b)};
}

void Viewport::setLightColor(const QColor &color) {
    assert(color.isValid() && "invalid light color!");
    m_shader.setIl({
        .r = Shading::int2float(color.red()),
        .g = Shading::int2float(color.green()),
        .b = Shading::int2float(color.blue())
    });

    render();
    update();
}

/// Works also when the animation is stopped: render() takes the light position from the path
void Viewport::setLightZ(const float z) {
    assert(z >= kMinLightZ && z <= kMaxLightZ && "light z out of range!");
    m_lightPath.setZ(z);

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

void Viewport::setUseTexture(const bool use) {
    m_useTexture = use;

    render();
    update();
}

void Viewport::setTexture(Texture texture) {
    m_texture = std::move(texture);

    render();
    update();
}

void Viewport::setUseNormalMap(const bool use) {
    m_useNormalMap = use;

    render();
    update();
}

void Viewport::setNormalMap(Texture normalMap) {
    m_normalMap = std::move(normalMap);

    render();
    update();
}

void Viewport::resetRotation() {
    m_rotation = initialRotation();
    applyRotationToMeshes();

    render();
    update();
}

void Viewport::resetLightPath() {
    m_lightPath = BezierCurve::circle(kDefaultLightPathRadius, lightZ());
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
        m_dragOffset = m_lightPath.point(m_draggedPoint)
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
        m_lightPath.movePoint(m_draggedPoint, p.x, p.y);
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
    for (int i = 0; i < m_lightPath.size(); ++i) {
        const ScreenPoint sp = proj.project(m_lightPath.point(i));
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

    // update light position
    m_shader.setLightPosition(m_lightPath.evaluate(m_lightT));

    // fill viewport with white color
    m_frame.fill(Qt::white);
    m_zbuffer.clear();

    if (m_meshes.empty()) return;

    // draw vertices and triangles of each mesh
    {
        const int w = m_frame.width();
        const int h = m_frame.height();
        const auto proj = Projection(w, h);

        // fill triangles
        if (m_showFill) {
            const auto raster = Rasterizer(w, h);
            int idx = 0;
            for (const auto &mesh: m_meshes) {
                fillTriangles(proj, mesh, raster, idx);
                idx = ++idx % kColorCount;
            }
        }

        QPainter painter(&m_frame);
        painter.setRenderHint(QPainter::Antialiasing);

        // draw every triangle for every mesh
        if (m_showMesh) {
            int idx = 0;
            for (const auto &mesh: m_meshes) {
                drawTriangles(proj, mesh, painter, idx);
                idx = ++idx % kColorCount;
            }
        }

        // draw bezier polygon for every mesh
        if (m_showPolygon) {
            int idx = 0;
            for (const auto &mesh: m_meshes) {
                drawPolygon(proj, mesh, painter, idx);
                idx = ++idx % kColorCount;
            }
        }

        // draw every mesh vertex for every mesh
        if (m_showPoints) {
            int idx = 0;
            for (const auto &mesh: m_meshes) {
                drawVertices(proj, mesh, painter, idx);
                idx = ++idx % kColorCount;
            }
        }

        // draw every Bezier surface control point
        if (m_showPolygon) {
            int idx = 0;
            for (const auto &mesh: m_meshes) {
                drawControlPoints(proj, mesh, painter, idx);
                idx = ++idx % kColorCount;
            }
        }

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

void Viewport::drawTriangles(const Projection &proj, const Mesh &mesh, QPainter &painter, const int colorIdx) const {
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
                    if (m_useTexture || m_useNormalMap) {
                        const float u = interpolate(w, vertices[v1].u, vertices[v2].u, vertices[v3].u);
                        const float v = interpolate(w, vertices[v1].v, vertices[v2].v, vertices[v3].v);
                        if (m_useTexture)
                            io = m_texture.sample(u, v);
                        if (m_useNormalMap) {
                            const Vec3 Pu = interpolate(w, vertices[v1].after.Pu, vertices[v2].after.Pu, vertices[v3].after.Pu);
                            const Vec3 Pv = interpolate(w, vertices[v1].after.Pv, vertices[v2].after.Pv, vertices[v3].after.Pv);
                            N = perturbNormal(Pu, Pv, N, normalFromMap(m_normalMap.sample(u, v)));
                        }
                    }

                    const auto [red, green, blue] = m_shader.calculateColor(io, N, P);

                    reinterpret_cast<QRgb *>(m_frame.scanLine(y))[x] = qRgb(
                        Shading::float2int(red),
                        Shading::float2int(green),
                        Shading::float2int(blue)
                    );
                }
            }
        );
    }
}

/// Control polygon under the curve, curve, control points, light on top (no Z-buffer: the light is above the scene)
void Viewport::drawLightPath(const Projection &proj, QPainter &painter) const {
    const auto toScreen = [&proj](const Vec3 &p) {
        const ScreenPoint sp = proj.project(p);
        return QPointF(sp.x, sp.y);
    };

    // control polygon: consecutive points, closed (P[3K-1] -> P[0] is the last handle of the last segment)
    const auto &points = m_lightPath.points();
    QPolygonF polygon;
    for (const auto &p: points)
        polygon << toScreen(p);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(kLightPathPolygonColor, 1, Qt::DotLine));
    painter.drawPolygon(polygon);

    // the curve itself as a closed polyline
    QPolygonF curve;
    for (const auto &p: m_lightPath.sample(kLightPathSamples))
        curve << toScreen(p);
    painter.setPen(QPen(kLightPathColor, 1, Qt::DashLine));
    painter.drawPolygon(curve);

    // anchors (on the curve) as squares, handles as circles
    painter.setPen(QPen(kLightPathColor, 1));
    for (int i = 0; i < m_lightPath.size(); ++i) {
        const QPointF c = toScreen(m_lightPath.point(i));
        const bool anchor = i % 3 == 0;
        // anchors filled, handles hollow; the grabbed point is filled too
        painter.setBrush(anchor || i == m_draggedPoint ? kLightPathColor : QColor(Qt::white));
        if (anchor)
            painter.drawRect(QRectF(c.x() - kPointRadius, c.y() - kPointRadius, 2 * kPointRadius, 2 * kPointRadius));
        else
            painter.drawEllipse(c, kPointRadius, kPointRadius);
    }

    // current light position
    painter.setPen(QPen(kLightPathColor, 1));
    painter.setBrush(lightColor());
    painter.drawEllipse(toScreen(lightPosition()), kPointRadius * 3, kPointRadius * 3);
}
