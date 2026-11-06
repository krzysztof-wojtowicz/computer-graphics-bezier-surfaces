#pragma once

#include "geometry/Mesh.h"
#include "math/Mat3.h"
#include "math/Vec3.h"
#include "geometry/BezierSurface.h"

#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QMouseEvent>
#include <vector>
#include <array>

#include "renderer/Projection.h"

class Viewport : public QWidget {
public:
    explicit Viewport(QWidget *parent = nullptr);
    void setSurfaces(const std::vector<BezierSurface> &surfaces);
    [[nodiscard]] const Mat3 &rotation() const { return m_rotation; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    static constexpr float kPointRadius = 3;

    void render();

    QImage m_frame;
    std::vector<Mesh> m_meshes;
    Mat3 m_rotation = rotationX(-1.0f) * rotationZ(0.5f);
    QPoint m_lastMousePos;

    void applyRotationToMeshes();

    [[nodiscard]] std::array<std::array<Vec3, 4>, 4> surfaceControlPoints(const Mesh &mesh) const;

    static void drawVertices(const Projection &proj, const Mesh &mesh, QPainter &painter);

    static void drawTriangles(const Projection &proj, const Mesh &mesh, QPainter &painter);
    void drawControlPoints(const Projection &proj, const Mesh &mesh, QPainter &painter) const;
    void drawPolygon(const Projection &proj, const Mesh &mesh, QPainter &painter) const;
};
