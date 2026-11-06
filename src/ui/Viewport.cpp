#include "ui/Viewport.h"

#include <QPainter>
#include <QColor>
#include <QPointF>
#include <QLineF>
#include <array>

#include "renderer/MouseRotation.h"
#include "renderer/Projection.h"

Viewport::Viewport(QWidget *parent) : QWidget(parent) {
    setMinimumSize(400, 400);
}

/// Generates mesh for each Bezier Surface
void Viewport::setSurfaces(const std::vector<BezierSurface> &surfaces) {
    m_meshes.clear();
    m_meshes.reserve(surfaces.size());

    for (auto &surf: surfaces) {
        // Generate mesh
        m_meshes.emplace_back(surf, Mesh::kDefaultResolution);
    }

    applyRotationToMeshes();
    render();
    update();
}

void Viewport::resizeEvent(QResizeEvent *) {
    m_frame = QImage(size(), QImage::Format_RGB32);
    render();
}

void Viewport::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_lastMousePos = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
    } else {
        QWidget::mousePressEvent(event);
    }
}

void Viewport::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        const auto pos = event->position().toPoint();
        const auto delta = pos - m_lastMousePos;
        m_lastMousePos = pos;
        m_rotation = applyDrag(m_rotation, static_cast<float>(delta.x()), static_cast<float>(delta.y()));
        applyRotationToMeshes();
        render();
        update();
    }
}

void Viewport::mouseReleaseEvent(QMouseEvent *) {
    unsetCursor();
}

void Viewport::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.drawImage(0, 0, m_frame);
}

/// Renders image in the viewport, <br>
/// for now -> draws vertices of each mesh
void Viewport::render() {
    if (m_frame.isNull()) return;

    // fill viewport with white color
    m_frame.fill(Qt::white);

    if (m_meshes.empty()) return;

    const int w = m_frame.width();
    const int h = m_frame.height();

    const auto proj = Projection(w, h);

    // draw vertices and triangles of each mesh
    {
        QPainter painter(&m_frame);
        painter.setRenderHint(QPainter::Antialiasing);

        for (const auto &mesh: m_meshes) {
            const auto &vertices = mesh.vertices();

            // draw every triangle
            painter.setPen(Qt::SolidLine);
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

            // calculate projections of bezier surface control points
            std::array<std::array<ScreenPoint, 4>, 4> controlPointsProj = {};
            auto controlPoints = surfaceControlPoints(mesh);
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    controlPointsProj[i][j] = proj.project(controlPoints[i][j]);

            // draw bezier polygon
            painter.setPen(Qt::DashLine);
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j) {
                    const auto start = controlPointsProj[i][j];
                    if (i + 1 < 4) {
                        const auto end = controlPointsProj[i+1][j];
                        painter.drawLine(QLineF(
                            QPointF(start.x , start.y),
                            QPointF(end.x, end.y)
                        ));
                    }
                    if (j + 1 < 4) {
                        const auto end = controlPointsProj[i][j+1];
                        painter.drawLine(QLineF(
                            QPointF(start.x , start.y),
                            QPointF(end.x, end.y)
                        ));
                    }
                }

            // draw every mesh vertex
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(200, 0, 0));
            for (const auto &v: vertices) {
                const auto sp = proj.project(v.after.P);
                painter.drawEllipse(QPointF(sp.x, sp.y), kPointRadius/2, kPointRadius/2);
            }

            // draw every Bezier surface control point
            painter.setBrush(QColor(0, 0, 200));
            for (const auto &row : controlPointsProj) {
                for (const auto &cp : row) {
                    painter.drawEllipse(QPointF(cp.x, cp.y), kPointRadius,
                        kPointRadius);
                }
            }
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
