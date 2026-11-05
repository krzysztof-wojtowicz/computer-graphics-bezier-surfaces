#include "geometry/Mesh.h"

#include <cassert>

Mesh::Mesh(const BezierSurface surface, const int n) : _n(n), _surface(surface) {
    assert(n >= kMinResolution && n <= kMaxResolution && "n out of range!");

    triangulate();
}

/// Triangulates the Bezier surface for n <br>
/// Calculates (n+1)^2 triangle vertices for 2n^2 triangles <br>
/// Divides the surface in n^2 squares, each creating 2 triangles
void Mesh::triangulate() {
    // Calculate vertices
    _vertices.clear();
    _vertices.reserve((_n + 1) * (_n + 1));

    for (int i = 0; i <= _n; ++i) {
        const float u = i / static_cast<float>(_n);

        for (int j = 0; j <= _n; ++j) {
            const float v = j / static_cast<float>(_n);
            const auto sp = _surface.evaluate(u, v);

            _vertices.emplace_back(Vertex{
                .before = sp,
                .after = {},
                .u = u,
                .v = v
            });
        }
    }

    // Calculate triangles, divide each square:
    // |a   c|
    //   .\.
    // |b   d|
    // tri1 = a-b-d, tri2 = a-d-c
    _triangles.clear();
    _triangles.reserve(2 * _n * _n);

    for (int i = 0; i < _n; ++i) {
        for (int j = 0; j < _n; ++j) {
            auto idx = indexes(i,j);
            _triangles.emplace_back(Triangle{
                .v1 = idx[0],   // a
                .v2 = idx[1],   // b
                .v3 = idx[3]    // d
            });

            _triangles.emplace_back(Triangle{
                .v1 = idx[0],   // a
                .v2 = idx[3],   // d
                .v3 = idx[2]    // c
            });
        }
    }

    applyRotation(_lastRotation);
}

/// Returns indexes of the vertices of one square of the mesh
std::array<int, 4> Mesh::indexes(const int i, const int j) const {
    std::array<int, 4> indexes = {};

    // |a   c|
    //   .\.
    // |b   d|
    indexes[0] = i * (_n + 1) + j;      // a = (i, j)
    indexes[1] = indexes[0] + _n + 1;   // b = (i+1, j)
    indexes[2] = indexes[0] + 1;        // c = (i, j+1)
    indexes[3] = indexes[1] + 1;        // d = (i+1, j+1)

    return indexes;
}

/// Applies rotation defined by R matrix for all of the vertices
void Mesh::applyRotation(const Mat3 &R) {
    _lastRotation = R;
    for (auto &v: _vertices) {
        v.after.P = R * v.before.P;
        v.after.Pu = normalized(R * v.before.Pu);
        v.after.Pv = normalized(R * v.before.Pv);
        v.after.N = normalized(R * v.before.N);
    }
}

/// Changes the mesh resolution and triangulates new mesh
void Mesh::setResolution(const int n) {
    assert(n >= kMinResolution && n <= kMaxResolution && "n out of range!");
    _n = n;

    triangulate();
}
