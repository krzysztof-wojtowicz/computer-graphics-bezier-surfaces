#pragma once

#include "geometry/Mesh.h"
#include "math/Mat3.h"
#include "math/Vec3.h"
#include "geometry/BezierSurface.h"
#include "geometry/BezierCurve.h"

#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QMouseEvent>
#include <vector>
#include <array>

#include "renderer/Projection.h"
#include "renderer/ZBuffer.h"
#include "renderer/Rasterizer.h"
#include "renderer/Shading.h"

class Viewport : public QWidget {
public:
    explicit Viewport(QWidget *parent = nullptr);

    // --- GETTERS ---
    [[nodiscard]] const Mat3 &rotation() const { return m_rotation; }
    [[nodiscard]] const std::vector<Mesh> &meshes() const { return m_meshes; }
    [[nodiscard]] int resolution() const { return m_resolution; }
    [[nodiscard]] bool showMesh() const { return m_showMesh; }
    [[nodiscard]] bool showPolygon() const { return m_showPolygon; }
    [[nodiscard]] bool showPoints() const { return m_showPoints; }
    [[nodiscard]] bool showFill() const { return m_showFill; }
    [[nodiscard]] float kd() const { return m_shader.kd(); }
    [[nodiscard]] float ks() const { return m_shader.ks(); }
    [[nodiscard]] float m() const { return m_shader.m(); }
    [[nodiscard]] QColor lightColor() const;
    [[nodiscard]] Vec3 lightPosition() const { return m_shader.lightPosition(); }

    // --- SETTERS ---
    void setResolution(int n);
    void setShowMesh(bool show);
    void setShowPolygon(bool show);
    void setShowPoints(bool show);
    void setShowFill(bool show);
    void setKd(float kd);
    void setKs(float ks);
    void setM(float m);
    void setLightColor(const QColor &color);

    void setSurfaces(const std::vector<BezierSurface> &surfaces);

protected:
    // --- QT EVENTS ---
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    // --- CONST ---
    static constexpr float kPointRadius = 3;
    static constexpr int kColorCount = 3;
    static constexpr QColor kMeshColors[kColorCount] = {
        QColor(100, 0, 0),
        QColor(0, 100, 0),
        QColor(0, 0, 100)
    };
    static constexpr QColor kPolygonColors[kColorCount] = {
        QColor(200, 0, 0),
        QColor(0, 200, 0),
        QColor(0, 0, 200)
    };
    static constexpr QColor kFillColors[kColorCount] = {
        QColor(255, 133, 133),
        QColor(133, 255, 133),
        QColor(133, 133, 255)
    };

    // --- STATES ---
    QImage m_frame;
    std::vector<Mesh> m_meshes; // meshes for all surfaces
    Mat3 m_rotation = rotationX(-1.0f) * rotationZ(0.5f); // current rotation
    QPoint m_lastMousePos; // last mouse position
    ZBuffer m_zbuffer; // sized in resizeEvent, like m_frame
    Shading m_shader; // shader for fill color calculation with light model
    BezierCurve m_lightPath = BezierCurve::circle(5.15f, 8.0f); // light path for animation

    float m_lightT = 0.0f; // timer for light animation
    int m_resolution = Mesh::kDefaultResolution; // mesh resolution
    bool m_showMesh = true;
    bool m_showPolygon = true;
    bool m_showPoints = true;
    bool m_showFill = true;

    // --- RENDERING ---
    void render();
    void applyRotationToMeshes();

    [[nodiscard]] std::array<std::array<Vec3, 4>, 4> surfaceControlPoints(const Mesh &mesh) const;

    // --- DRAWING HELPERS ---
    static void drawVertices(const Projection &proj, const Mesh &mesh, QPainter &painter, int colorIdx);
    static void drawTriangles(const Projection &proj, const Mesh &mesh, QPainter &painter, int colorIdx);
    void drawControlPoints(const Projection &proj, const Mesh &mesh, QPainter &painter, int colorIdx) const;
    void drawPolygon(const Projection &proj, const Mesh &mesh, QPainter &painter, int colorIdx) const;
    void fillTriangles(const Projection &proj, const Mesh &mesh, const Rasterizer &raster, int colorIdx);
    void drawLightPath(const Projection &proj, QPainter &painter) const;
};
