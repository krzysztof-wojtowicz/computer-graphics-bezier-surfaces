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
#include <QTimer>
#include <QElapsedTimer>
#include <vector>
#include <array>

#include "renderer/Projection.h"
#include "renderer/ZBuffer.h"
#include "renderer/Rasterizer.h"
#include "renderer/Shading.h"
#include "renderer/Texture.h"
#include "renderer/NormalMapping.h"

class Viewport : public QWidget {
public:
    /// Range of the light path height (scene units); the scene fits in |P| <= Projection::kSceneRadius
    static constexpr float kMinLightZ = 0.0f;
    static constexpr float kMaxLightZ = 20.0f;
    /// Default light path: a circle around the screen centre, above the scene
    static constexpr float kDefaultLightPathRadius = 5.15f;
    static constexpr float kDefaultLightZ = 8.0f;
    static constexpr float kLightSpeed = 0.5f; // curve segments per second (4 segments = 8 s per lap)
    static constexpr float kPickRadius = 8; // px: a press this close to a light path point drags it, else rotates
    /// Surfaces take colors in turn (surface k gets color k % kColorCount); each color is chosen in the panel
    static constexpr int kColorCount = 3;
    static constexpr std::array<QColor, kColorCount> kDefaultSurfaceColors = {
        QColor(255, 133, 133),
        QColor(133, 255, 133),
        QColor(133, 133, 255)
    };
    /// Lines are drawn in darker shades of the fill color (same hue): visible on the white background
    /// whatever color is chosen, and the surface's fill, polygon and mesh still look like one object
    static constexpr int kPolygonDarkness = 150; // QColor::darker factor: value / 1.5
    static constexpr int kMeshDarkness = 250; // value / 2.5

    explicit Viewport(QWidget *parent = nullptr);

    // --- GETTERS ---
    /// Orientation the program starts with (looking at the surfaces from above, at an angle)
    [[nodiscard]] static Mat3 initialRotation() { return rotationX(-1.0f) * rotationZ(0.5f); }
    [[nodiscard]] const Mat3 &rotation() const { return m_rotation; }
    [[nodiscard]] const std::vector<Mesh> &meshes() const { return m_meshes; }
    [[nodiscard]] int resolution() const { return m_resolution; }
    [[nodiscard]] bool showMesh() const { return m_showMesh; }
    [[nodiscard]] bool showPolygon() const { return m_showPolygon; }
    [[nodiscard]] bool showPoints() const { return m_showPoints; }
    [[nodiscard]] bool showFill() const { return m_showFill; }
    [[nodiscard]] bool showLightPath() const { return m_showLightPath; }
    [[nodiscard]] float kd() const { return m_shader.kd(); }
    [[nodiscard]] float ks() const { return m_shader.ks(); }
    [[nodiscard]] float m() const { return m_shader.m(); }
    [[nodiscard]] QColor lightColor() const;
    /// Fill color i, the polygon and mesh shades are derived from it
    [[nodiscard]] QColor surfaceColor(int i) const;
    [[nodiscard]] QColor polygonColor(int i) const { return surfaceColor(i).darker(kPolygonDarkness); }
    [[nodiscard]] QColor meshColor(int i) const { return surfaceColor(i).darker(kMeshDarkness); }
    [[nodiscard]] bool useTexture() const { return m_useTexture; }
    [[nodiscard]] const Texture &texture() const { return m_texture; }
    [[nodiscard]] bool useNormalMap() const { return m_useNormalMap; }
    [[nodiscard]] const Texture &normalMap() const { return m_normalMap; }
    /// From the path, not from the shader: valid also before the first render()
    [[nodiscard]] Vec3 lightPosition() const { return m_lightPath.evaluate(m_lightT); }
    [[nodiscard]] float lightT() const { return m_lightT; }
    [[nodiscard]] bool isAnimating() const { return m_animationTimer.isActive(); }
    [[nodiscard]] float lightZ() const { return m_lightPath.z(); }
    [[nodiscard]] const BezierCurve &lightPath() const { return m_lightPath; }

    // --- SETTERS ---
    void setResolution(int n);
    void setShowMesh(bool show);
    void setShowPolygon(bool show);
    void setShowPoints(bool show);
    void setShowFill(bool show);
    void setShowLightPath(bool show);
    void setKd(float kd);
    void setKs(float ks);
    void setM(float m);
    void setLightColor(const QColor &color);
    void setSurfaceColor(int i, const QColor &color);
    /// Object color IO: false = surface colors, true = the texture (on every surface)
    void setUseTexture(bool use);
    void setTexture(Texture texture);
    /// Normal map, independent of the object color: N = [Pu, Pv, N] * N_map
    void setUseNormalMap(bool use);
    void setNormalMap(Texture normalMap);
    void setLightZ(float z);
    /// Restores the default shape of the light path; keeps its height (set by the z slider) and the light's t
    void resetLightPath();
    /// Back to initialRotation(); the light path is bound to the camera, so it doesn't change
    void resetRotation();

    void setSurfaces(const std::vector<BezierSurface> &surfaces);

    // --- ANIMATION ---
    void setAnimating(bool on);
    /// Moves the light along its path by the given time
    void stepAnimation(float seconds);

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
    static constexpr int kLightPathSamples = 32; // polyline points per curve segment
    static constexpr QColor kLightPathColor = QColor(0, 0, 0);
    static constexpr QColor kLightPathPolygonColor = QColor(150, 150, 150);
    static constexpr int kAnimationIntervalMs = 16; // ~60 fps, real rate depends on render time
    static constexpr float kMaxAnimationStep = 0.1f; // seconds; avoids a jump after the window was blocked

    // --- STATES ---
    QImage m_frame;
    std::vector<Mesh> m_meshes; // meshes for all surfaces
    Mat3 m_rotation = initialRotation(); // current rotation
    QPoint m_lastMousePos; // last mouse position
    int m_draggedPoint = -1; // index of the light path point being dragged, -1 = the mouse rotates the scene
    Vec3 m_dragOffset; // point - mouse in scene units, so the point doesn't jump to the cursor when grabbed
    ZBuffer m_zbuffer; // sized in resizeEvent, like m_frame
    Shading m_shader; // shader for fill color calculation with light model
    Texture m_texture; // 1x1 white until MainWindow sets one; Viewport doesn't load files
    Texture m_normalMap = Texture(1, 1, {128, 128, 255}); // flat (N_map ~ [0, 0, 1]) until MainWindow sets one
    std::array<QColor, kColorCount> m_surfaceColors = kDefaultSurfaceColors; // fill colors, used in turn
    BezierCurve m_lightPath = BezierCurve::circle(kDefaultLightPathRadius, kDefaultLightZ); // light path for animation

    float m_lightT = 0.0f; // light position parameter on m_lightPath, kept in [0, segments)
    QTimer m_animationTimer; // fires animation steps
    QElapsedTimer m_animationClock; // real time between steps, so speed doesn't depend on frame rate
    int m_resolution = Mesh::kDefaultResolution; // mesh resolution
    bool m_showMesh = true;
    bool m_showPolygon = true;
    bool m_showPoints = true;
    bool m_showFill = true;
    bool m_showLightPath = false; // off by default: tests check the image of the scene alone
    bool m_useTexture = false; // off by default: tests see the surface colors
    bool m_useNormalMap = false;

    // --- RENDERING ---
    void render();
    void applyRotationToMeshes();
    void onAnimationTick();

    [[nodiscard]] std::array<std::array<Vec3, 4>, 4> surfaceControlPoints(const Mesh &mesh) const;

    // --- DRAWING HELPERS ---
    void drawVertices(const Projection &proj, const Mesh &mesh, QPainter &painter, int colorIdx) const;
    void drawTriangles(const Projection &proj, const Mesh &mesh, QPainter &painter, int colorIdx) const;
    void drawControlPoints(const Projection &proj, const Mesh &mesh, QPainter &painter, int colorIdx) const;
    void drawPolygon(const Projection &proj, const Mesh &mesh, QPainter &painter, int colorIdx) const;
    void fillTriangles(const Projection &proj, const Mesh &mesh, const Rasterizer &raster, int colorIdx);
    void drawLightPath(const Projection &proj, QPainter &painter) const;

    // --- LIGHT PATH EDITING ---
    [[nodiscard]] int pickLightPathPoint(const QPointF &pos) const;
};
