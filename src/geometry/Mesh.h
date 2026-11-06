#pragma once

#include "geometry/BezierSurface.h"
#include "math/Mat3.h"

#include <vector>
#include <array>

struct Vertex {
    SurfacePoint before;
    SurfacePoint after;
    float u = 0.0f;
    float v = 0.0f;
};

struct Triangle {
    int v1;
    int v2;
    int v3;
};

class Mesh {
public:
    static constexpr int kMinResolution = 1;
    static constexpr int kMaxResolution = 50;
    static constexpr int kDefaultResolution = 10;

    explicit Mesh(BezierSurface surface, int n);

    void applyRotation(const Mat3 &R);

    // Getters
    [[nodiscard]] const std::vector<Vertex> &vertices() const { return _vertices; }
    [[nodiscard]] const std::vector<Triangle> &triangles() const { return _triangles; }
    [[nodiscard]] const BezierSurface &surface() const { return _surface; }
    [[nodiscard]] int resolution() const { return _n; }

    // Setters
    void setResolution(int n);

private:
    int _n = 1;
    BezierSurface _surface;
    std::vector<Vertex> _vertices;
    std::vector<Triangle> _triangles;
    Mat3 _lastRotation = Mat3::identity();

    void triangulate();

    [[nodiscard]] std::array<int, 4> indexes(int i, int j) const;
};
