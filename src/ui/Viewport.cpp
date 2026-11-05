#include "ui/Viewport.h"

#include <QPainter>
#include <QColor>
#include <QPointF>
#include <QLineF>
#include <QBrush>

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

            // draw every vertex
            painter.setBrush(QColor(200, 0, 0));
            painter.setPen(Qt::NoPen);
            for (const auto &v: vertices) {
                const auto sp = proj.project(v.after.P);
                painter.drawEllipse(QPointF(sp.x, sp.y), kPointRadius, kPointRadius);
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
