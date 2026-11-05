#include "ui/Viewport.h"

#include <QPainter>

Viewport::Viewport(QWidget *parent) : QWidget(parent) {
    setMinimumSize(400, 400);
}

/// Generates mesh for each Bezier Surface
void Viewport::setSurfaces(std::vector<BezierSurface> &surfaces) {
    m_meshes.clear();
    m_meshes.reserve(surfaces.size());

    for (auto &surf : surfaces) {
        // Generate mesh
        m_meshes.emplace_back(surf, Mesh::kDefaultResolution);
        // Apply rotation
        m_meshes.back().applyRotation(m_rotation);
    }

    render();
    update();
}

void Viewport::resizeEvent(QResizeEvent *) {
    m_frame = QImage(size(), QImage::Format_RGB32);
    render();
}

void Viewport::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.drawImage(0, 0, m_frame);
}

/// Renders image in the viewport
void Viewport::render() {
    if (m_frame.isNull()) return;

    const int w = m_frame.width();
    const int h = m_frame.height();

    for (int row = 0; row < h; ++row) {
        auto *line = reinterpret_cast<QRgb *>(m_frame.scanLine(row));

        // Draw white pixel
        for (int col = 0; col < w; ++col) {
            line[col] = qRgb(255, 255, 255);
        }
    }
}