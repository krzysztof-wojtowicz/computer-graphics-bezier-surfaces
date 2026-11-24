#pragma once

#include "geometry/Mesh.h"
#include "math/Mat3.h"
#include "math/Vec3.h"
#include "geometry/BezierSurface.h"
#include "geometry/BezierCurve.h"
#include "geometry/BSplineCurve.h"
#include "geometry/ClosedBezierCurve.h"
#include "geometry/ClosedCurve.h"

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
    /// How the light path is built on its control points; both kinds share the same points
    enum class LightPathType {
        Bezier, // one closed Bezier segment of degree n on the n points, Vn = V0
        BSpline // closed cubic B-spline, one segment per point
    };

    /// Range of the light path height (scene units); the scene fits in |P| <= Projection::kSceneRadius
    static constexpr float kMinLightZ = 0.0f;
    static constexpr float kMaxLightZ = 20.0f;
    /// Default light path: a circle around the screen centre, above the scene
    /// The handles of a 4-segment circle lie at r * sqrt(1 + k^2) ~ 1.14 r from the centre: 4.3 keeps them
    /// (and the light marker) inside the scene circle, so they are visible and can be grabbed
    static constexpr float kDefaultLightPathRadius = 4.3f;
    static constexpr float kDefaultLightZ = 8.0f;
    /// Laps per second (8 s per lap): the paths have different numbers of segments, the speed doesn't depend on it
    static constexpr float kLightSpeed = 0.125f;
    /// Lights on the path: one, or this many spread evenly over the lap (they light the scene from different sides)
    static constexpr int kMaxLightCount = Shading::kMaxLights;
    static constexpr float kPickRadius = 8; // px: a press this close to a light path point drags it, else rotates
    /// Surfaces take their settings in turn: surface k uses group k % kColorCount (color, visibility,
    /// texture, normal map), each group has its own tab in the panel
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
    /// Surfaces are grouped like their colors: switch i shows or hides surfaces i, i + kColorCount, ...
    [[nodiscard]] bool surfaceVisible(int i) const;
    [[nodiscard]] QColor polygonColor(int i) const { return surfaceColor(i).darker(kPolygonDarkness); }
    [[nodiscard]] QColor meshColor(int i) const { return surfaceColor(i).darker(kMeshDarkness); }
    /// Object color IO of group i: false = its surface color, true = its texture
    [[nodiscard]] bool useTexture(int i) const;
    [[nodiscard]] const Texture &texture(int i) const;
    [[nodiscard]] bool useNormalMap(int i) const;
    [[nodiscard]] const Texture &normalMap(int i) const;
    /// From the path, not from the shader: valid also before the first render(). The first light.
    [[nodiscard]] Vec3 lightPosition() const;
    /// Every light: light k is k / lightCount() of a lap ahead of the first one
    [[nodiscard]] std::vector<Vec3> lightPositions() const;
    [[nodiscard]] int lightCount() const { return m_lightCount; }
    /// Part of the lap the light has done, in [0, 1)
    [[nodiscard]] float lightT() const { return m_lightT; }
    [[nodiscard]] bool isAnimating() const { return m_animationTimer.isActive(); }
    [[nodiscard]] float lightZ() const { return lightPath().z(); }
    [[nodiscard]] LightPathType lightPathType() const { return m_lightPathType; }
    /// The path of the current type (the light moves along it, the mouse edits it)
    [[nodiscard]] const ClosedCurve &lightPath() const;
    /// Color of B-spline segment s of count: neighbours differ in hue, so the segments can be told apart
    [[nodiscard]] static QColor lightPathSegmentColor(int s, int count);

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
    void setSurfaceVisible(int i, bool visible);
    /// Object color IO of group i, independent of the other groups
    void setUseTexture(int i, bool use);
    void setTexture(int i, Texture texture);
    /// Normal map of group i, independent of the object color: N = [Pu, Pv, N] * N_map
    void setUseNormalMap(int i, bool use);
    void setNormalMap(int i, Texture normalMap);
    void setLightZ(float z);
    /// Switches the curve built on the control points; the points stay. The light keeps its part of the lap.
    void setLightPathType(LightPathType type);
    /// 1 or kMaxLightCount lights moving together along the path
    void setLightCount(int count);
    /// Restores the default control points; keeps their height (set by the z slider) and the light's t
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
    static constexpr int kLightPathSamples = 32; // polyline points per curve segment...
    static constexpr int kLightPathMinSamples = 128; // ...but at least this many per lap (the degree 6 loop is one segment)
    static constexpr QColor kLightPathColor = QColor(0, 0, 0);
    static constexpr QColor kLightPathPolygonColor = QColor(150, 150, 150);
    static constexpr int kSegmentLineWidth = 3; // px, B-spline segments: thick enough to show their colors
    static constexpr int kSegmentBrightValue = 220; // HSV value of even / odd segments: below 255, seen on white
    static constexpr int kSegmentDarkValue = 130;
    static constexpr int kAnimationIntervalMs = 16; // ~60 fps, real rate depends on render time
    static constexpr float kMaxAnimationStep = 0.1f; // seconds; avoids a jump after the window was blocked
    static inline const Texture kFlatNormalMap = Texture(1, 1, {128, 128, 255}); // N_map ~ [0, 0, 1]

    // --- STATES ---
    QImage m_frame;
    std::vector<Mesh> m_meshes; // meshes for all surfaces
    Mat3 m_rotation = initialRotation(); // current rotation
    QPoint m_lastMousePos; // last mouse position
    int m_draggedPoint = -1; // index of the light path point being dragged, -1 = the mouse rotates the scene
    Vec3 m_dragOffset; // point - mouse in scene units, so the point doesn't jump to the cursor when grabbed
    ZBuffer m_zbuffer; // sized in resizeEvent, like m_frame
    Shading m_shader; // shader for fill color calculation with light model
    // per group, like the colors; Viewport doesn't load files, MainWindow sets the images
    std::array<Texture, kColorCount> m_textures{}; // 1x1 white until MainWindow sets one
    std::array<Texture, kColorCount> m_normalMaps = {kFlatNormalMap, kFlatNormalMap, kFlatNormalMap};
    std::array<QColor, kColorCount> m_surfaceColors = kDefaultSurfaceColors; // fill colors, used in turn
    std::array<bool, kColorCount> m_surfaceVisible = {true, true, true}; // per color group, like the colors
    // Both curves on the same control points (the 12 points of the 4-segment Bezier circle by default):
    // every change of the points goes to both, so switching the type only changes the curve
    ClosedBezierCurve m_bezierPath = ClosedBezierCurve(defaultLightPathPoints(kDefaultLightZ));
    BSplineCurve m_bsplinePath = BSplineCurve(defaultLightPathPoints(kDefaultLightZ));
    // B-spline by default: close to the circle of the earlier versions, tests rely on its shape
    LightPathType m_lightPathType = LightPathType::BSpline;

    float m_lightT = 0.0f; // part of the lap of the first light, in [0, 1)
    int m_lightCount = 1;
    QTimer m_animationTimer; // fires animation steps
    QElapsedTimer m_animationClock; // real time between steps, so speed doesn't depend on frame rate
    int m_resolution = Mesh::kDefaultResolution; // mesh resolution
    bool m_showMesh = true;
    bool m_showPolygon = true;
    bool m_showPoints = true;
    bool m_showFill = true;
    bool m_showLightPath = false; // off by default: tests check the image of the scene alone
    std::array<bool, kColorCount> m_useTexture{}; // off by default: tests see the surface colors
    std::array<bool, kColorCount> m_useNormalMap{};

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
    void drawLightPathShapes(const Projection &proj, QPainter &painter) const;

    // --- LIGHT PATH EDITING ---
    [[nodiscard]] static std::vector<Vec3> defaultLightPathPoints(float z);
    [[nodiscard]] int pickLightPathPoint(const QPointF &pos) const;
};
