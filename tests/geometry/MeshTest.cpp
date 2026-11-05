#include "geometry/BezierSurface.h"
#include "geometry/Mesh.h"
#include "math/TestUtils.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <tuple>
#include <vector>

namespace {
using Grid = BezierSurface::ControlGrid;
using Heights = std::array<std::array<float, 4>, 4>;

/// x, y coordinate of the i-th control point on the regular grid -1, -1/3, 1/3, 1
float gridCoord(const int i) { return -1.0f + 2.0f * static_cast<float>(i) / 3.0f; }

/// Surface z(x, y): regular x, y grid, heights z[i][j]
BezierSurface heightSurface(const Heights &z) {
    Grid g;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            g[i][j] = {.x = gridCoord(i), .y = gridCoord(j), .z = z[i][j]};
    return BezierSurface(g);
}

BezierSurface flatSurface() { return heightSurface({}); }

/// Height field with hills and valleys, similar to the surfaces in assets/
BezierSurface wavySurface() {
    return heightSurface({{
        {0.0f, 0.4f, -0.3f, 0.1f},
        {0.5f, -0.6f, 0.8f, -0.2f},
        {-0.4f, 0.7f, -0.5f, 0.3f},
        {0.2f, -0.1f, 0.6f, 0.0f},
    }});
}

/// A rotation that is not around any coordinate axis
Mat3 someRotation() { return rotationAxisAngle({1.0f, 2.0f, 3.0f}, 0.7f); }

/// Index of vertex (i, j) – i along u, j along v
int vertexIndex(const Mesh &mesh, const int i, const int j) { return i * (mesh.resolution() + 1) + j; }

std::array<Vec3, 3> beforePositions(const Mesh &mesh, const Triangle &t) {
    const auto &vs = mesh.vertices();
    return {vs[static_cast<std::size_t>(t.v1)].before.P, vs[static_cast<std::size_t>(t.v2)].before.P,
            vs[static_cast<std::size_t>(t.v3)].before.P};
}

std::array<Vec3, 3> afterPositions(const Mesh &mesh, const Triangle &t) {
    const auto &vs = mesh.vertices();
    return {vs[static_cast<std::size_t>(t.v1)].after.P, vs[static_cast<std::size_t>(t.v2)].after.P,
            vs[static_cast<std::size_t>(t.v3)].after.P};
}

/// Not normalized face normal from the vertex order v1 -> v2 -> v3
Vec3 faceNormal(const std::array<Vec3, 3> &p) { return cross(p[1] - p[0], p[2] - p[0]); }

/// `after` must be the rotated `before`: point rotated, vectors rotated and normalized
void expectRotatedBy(const Mesh &mesh, const Mat3 &R) {
    for (const auto &v : mesh.vertices()) {
        EXPECT_VEC3_EQ(v.after.P, R * v.before.P);
        EXPECT_VEC3_EQ(v.after.Pu, normalized(R * v.before.Pu));
        EXPECT_VEC3_EQ(v.after.Pv, normalized(R * v.before.Pv));
        EXPECT_VEC3_EQ(v.after.N, normalized(R * v.before.N));
    }
}
} // namespace

// ---------- sizes ----------

class MeshSizeTest : public testing::TestWithParam<int> {};

TEST_P(MeshSizeTest, HasNPlusOneSquaredVerticesAnd2NSquaredTriangles) {
    const int n = GetParam();
    const Mesh mesh(wavySurface(), n);

    EXPECT_EQ(mesh.resolution(), n);
    EXPECT_EQ(mesh.vertices().size(), static_cast<std::size_t>((n + 1) * (n + 1)));
    EXPECT_EQ(mesh.triangles().size(), static_cast<std::size_t>(2 * n * n));
}

TEST_P(MeshSizeTest, IndicesInRangeAndEveryVertexUsed) {
    const Mesh mesh(wavySurface(), GetParam());
    const auto count = static_cast<int>(mesh.vertices().size());
    std::vector<bool> used(mesh.vertices().size(), false);

    for (const auto &t : mesh.triangles())
        for (const int idx : {t.v1, t.v2, t.v3}) {
            ASSERT_GE(idx, 0);
            ASSERT_LT(idx, count);
            used[static_cast<std::size_t>(idx)] = true;
        }

    for (std::size_t k = 0; k < used.size(); ++k)
        EXPECT_TRUE(used[k]) << "vertex " << k << " is not used by any triangle";
}

TEST_P(MeshSizeTest, TrianglesHaveThreeDistinctVertices) {
    const Mesh mesh(wavySurface(), GetParam());
    for (const auto &t : mesh.triangles()) {
        EXPECT_NE(t.v1, t.v2);
        EXPECT_NE(t.v2, t.v3);
        EXPECT_NE(t.v1, t.v3);
    }
}

INSTANTIATE_TEST_SUITE_P(Resolutions, MeshSizeTest,
                         testing::Values(Mesh::kMinResolution, 2, 3, 7, 20, Mesh::kMaxResolution));

// ---------- exact layout ----------

// For n = 1 the only square is a=0, b=2, c=1, d=3 (a=(0,0), b=(1,0), c=(0,1), d=(1,1))
TEST(MeshTest, SingleSquareIsSplitIntoABDAndADC) {
    const Mesh mesh(flatSurface(), 1);
    const auto &ts = mesh.triangles();
    ASSERT_EQ(ts.size(), 2u);

    EXPECT_EQ(std::tie(ts[0].v1, ts[0].v2, ts[0].v3), std::make_tuple(0, 2, 3));
    EXPECT_EQ(std::tie(ts[1].v1, ts[1].v2, ts[1].v3), std::make_tuple(0, 3, 1));
}

TEST(MeshTest, VertexIJLiesAtUVEqualToIOverNJOverN) {
    const BezierSurface surface = wavySurface();
    const Mesh mesh(surface, 4);
    const int n = mesh.resolution();

    for (int i = 0; i <= n; ++i)
        for (int j = 0; j <= n; ++j) {
            const auto &v = mesh.vertices()[static_cast<std::size_t>(vertexIndex(mesh, i, j))];
            const float u = static_cast<float>(i) / static_cast<float>(n);
            const float w = static_cast<float>(j) / static_cast<float>(n);
            EXPECT_FLOAT_EQ(v.u, u);
            EXPECT_FLOAT_EQ(v.v, w);

            const SurfacePoint expected = surface.evaluate(u, w);
            EXPECT_VEC3_EQ(v.before.P, expected.P);
            EXPECT_VEC3_EQ(v.before.Pu, expected.Pu);
            EXPECT_VEC3_EQ(v.before.Pv, expected.Pv);
            EXPECT_VEC3_EQ(v.before.N, expected.N);
        }
}

TEST(MeshTest, ParametersCoverWholeUnitSquareExactly) {
    const Mesh mesh(wavySurface(), 7);
    for (const auto &v : mesh.vertices()) {
        EXPECT_GE(v.u, 0.0f);
        EXPECT_LE(v.u, 1.0f);
        EXPECT_GE(v.v, 0.0f);
        EXPECT_LE(v.v, 1.0f);
    }
    // The last vertex must be exactly (1, 1), not 0.99999 from summing steps
    EXPECT_EQ(mesh.vertices().back().u, 1.0f);
    EXPECT_EQ(mesh.vertices().back().v, 1.0f);
}

TEST(MeshTest, CornerVerticesAreCornerControlPoints) {
    const BezierSurface surface = wavySurface();
    const Mesh mesh(surface, 5);
    const int n = mesh.resolution();
    const auto at = [&](const int i, const int j) {
        return mesh.vertices()[static_cast<std::size_t>(vertexIndex(mesh, i, j))].before.P;
    };

    EXPECT_VEC3_EQ(at(0, 0), surface.controlPoint(0, 0));
    EXPECT_VEC3_EQ(at(0, n), surface.controlPoint(0, 3));
    EXPECT_VEC3_EQ(at(n, 0), surface.controlPoint(3, 0));
    EXPECT_VEC3_EQ(at(n, n), surface.controlPoint(3, 3));
}

// ---------- triangle orientation ----------

// On the plane z = 0 the face normal of every triangle must point to +z, like N = Pu x Pv
TEST(MeshTest, FlatSurfaceTrianglesFaceUpAndAreNotDegenerate) {
    const Mesh mesh(flatSurface(), 6);
    const float cell = 2.0f / 6.0f; // the plane spans [-1, 1]^2
    const float expectedArea = cell * cell / 2.0f;

    for (const auto &t : mesh.triangles()) {
        const Vec3 fn = faceNormal(beforePositions(mesh, t));
        EXPECT_GT(fn.z, 0.0f);
        EXPECT_NEAR(length(fn) / 2.0f, expectedArea, kEps);
    }
}

// On a curved surface the face normal should agree with the surface normals at its vertices
TEST(MeshTest, CurvedSurfaceTrianglesAgreeWithVertexNormals) {
    const Mesh mesh(wavySurface(), 20);
    const auto &vs = mesh.vertices();

    for (const auto &t : mesh.triangles()) {
        const Vec3 fn = faceNormal(beforePositions(mesh, t));
        for (const int idx : {t.v1, t.v2, t.v3})
            EXPECT_GT(dot(fn, vs[static_cast<std::size_t>(idx)].before.N), 0.0f);
    }
}

// ---------- rotation ----------

TEST(MeshTest, NewMeshIsNotRotated) {
    const Mesh mesh(wavySurface(), 5);
    expectRotatedBy(mesh, Mat3::identity());
}

TEST(MeshTest, IdentityRotationKeepsPositions) {
    Mesh mesh(wavySurface(), 5);
    mesh.applyRotation(Mat3::identity());
    for (const auto &v : mesh.vertices())
        EXPECT_VEC3_EQ(v.after.P, v.before.P);
}

TEST(MeshTest, RotationRotatesPointsAndVectors) {
    Mesh mesh(wavySurface(), 5);
    const Mat3 R = someRotation();
    mesh.applyRotation(R);
    expectRotatedBy(mesh, R);
}

TEST(MeshTest, RotatedVectorsHaveUnitLength) {
    Mesh mesh(wavySurface(), 5);
    mesh.applyRotation(someRotation());
    for (const auto &v : mesh.vertices()) {
        EXPECT_NEAR(length(v.after.Pu), 1.0f, kEps);
        EXPECT_NEAR(length(v.after.Pv), 1.0f, kEps);
        EXPECT_NEAR(length(v.after.N), 1.0f, kEps);
    }
}

TEST(MeshTest, RotationDoesNotChangeOriginalData) {
    const BezierSurface surface = wavySurface();
    Mesh mesh(surface, 5);
    const std::vector<Vertex> original = mesh.vertices();

    mesh.applyRotation(someRotation());

    ASSERT_EQ(mesh.vertices().size(), original.size());
    for (std::size_t k = 0; k < original.size(); ++k) {
        const auto &a = mesh.vertices()[k];
        const auto &b = original[k];
        EXPECT_VEC3_EQ(a.before.P, b.before.P);
        EXPECT_VEC3_EQ(a.before.Pu, b.before.Pu);
        EXPECT_VEC3_EQ(a.before.Pv, b.before.Pv);
        EXPECT_VEC3_EQ(a.before.N, b.before.N);
        EXPECT_EQ(a.u, b.u);
        EXPECT_EQ(a.v, b.v);
    }
}

// Each call starts from `before`, so applying the same R twice is the same as applying it once
TEST(MeshTest, RotationsDoNotAccumulate) {
    Mesh mesh(wavySurface(), 5);
    const Mat3 R = someRotation();
    mesh.applyRotation(R);
    mesh.applyRotation(R);
    expectRotatedBy(mesh, R);

    mesh.applyRotation(Mat3::identity());
    expectRotatedBy(mesh, Mat3::identity());
}

TEST(MeshTest, RotationPreservesDistancesBetweenVertices) {
    Mesh mesh(wavySurface(), 4);
    mesh.applyRotation(someRotation());
    const auto &vs = mesh.vertices();

    for (std::size_t a = 0; a < vs.size(); ++a)
        for (std::size_t b = a + 1; b < vs.size(); ++b)
            EXPECT_NEAR(length(vs[a].after.P - vs[b].after.P), length(vs[a].before.P - vs[b].before.P), kEps);
}

// The triangle winding must stay consistent with the normals after rotation too
TEST(MeshTest, RotatedTrianglesAgreeWithRotatedNormals) {
    Mesh mesh(wavySurface(), 20);
    mesh.applyRotation(someRotation());
    const auto &vs = mesh.vertices();

    for (const auto &t : mesh.triangles()) {
        const Vec3 fn = faceNormal(afterPositions(mesh, t));
        for (const int idx : {t.v1, t.v2, t.v3})
            EXPECT_GT(dot(fn, vs[static_cast<std::size_t>(idx)].after.N), 0.0f);
    }
}

// ---------- changing resolution ----------

TEST(MeshTest, SetResolutionRebuildsMesh) {
    Mesh mesh(wavySurface(), 3);
    mesh.setResolution(10);

    EXPECT_EQ(mesh.resolution(), 10);
    EXPECT_EQ(mesh.vertices().size(), 121u);
    EXPECT_EQ(mesh.triangles().size(), 200u);

    mesh.setResolution(2);
    EXPECT_EQ(mesh.resolution(), 2);
    EXPECT_EQ(mesh.vertices().size(), 9u);
    EXPECT_EQ(mesh.triangles().size(), 8u);
}

TEST(MeshTest, SetResolutionGivesSameMeshAsConstructor) {
    Mesh changed(wavySurface(), 3);
    changed.setResolution(6);
    const Mesh fresh(wavySurface(), 6);

    ASSERT_EQ(changed.vertices().size(), fresh.vertices().size());
    for (std::size_t k = 0; k < fresh.vertices().size(); ++k)
        EXPECT_VEC3_EQ(changed.vertices()[k].before.P, fresh.vertices()[k].before.P);

    ASSERT_EQ(changed.triangles().size(), fresh.triangles().size());
    for (std::size_t k = 0; k < fresh.triangles().size(); ++k) {
        const auto &a = changed.triangles()[k];
        const auto &b = fresh.triangles()[k];
        EXPECT_EQ(std::tie(a.v1, a.v2, a.v3), std::tie(b.v1, b.v2, b.v3));
    }
}

// After changing n the new vertices must be rotated with the last rotation, not reset
TEST(MeshTest, SetResolutionKeepsCurrentRotation) {
    Mesh mesh(wavySurface(), 3);
    const Mat3 R = someRotation();
    mesh.applyRotation(R);

    mesh.setResolution(8);
    expectRotatedBy(mesh, R);
}

// ---------- invalid resolution ----------

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(MeshDeathTest, ConstructorRejectsResolutionOutOfRange) {
    EXPECT_DEATH(Mesh(flatSurface(), Mesh::kMinResolution - 1), "n out of range");
    EXPECT_DEATH(Mesh(flatSurface(), Mesh::kMaxResolution + 1), "n out of range");
}

TEST(MeshDeathTest, SetResolutionRejectsResolutionOutOfRange) {
    Mesh mesh(flatSurface(), 2);
    EXPECT_DEATH(mesh.setResolution(Mesh::kMinResolution - 1), "n out of range");
    EXPECT_DEATH(mesh.setResolution(Mesh::kMaxResolution + 1), "n out of range");
}
#endif
