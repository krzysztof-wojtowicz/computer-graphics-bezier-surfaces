#include "math/TestUtils.h"

#include <tuple>

namespace {
const Vec3 ex{1, 0, 0};
const Vec3 ey{0, 1, 0};
const Vec3 ez{0, 0, 1};

// Generic non-symmetric matrices used in several tests
const Mat3 A = matFromRows({1, 2, 3}, {4, 5, 6}, {7, 8, 10});
const Mat3 B = matFromRows({-1, 0, 2}, {3, 1, -2}, {0.5f, 4, 1});

void expectOrthonormal(const Mat3 &r) {
    EXPECT_MAT3_NEAR(r.transposed() * r, Mat3::identity(), 1e-4f);
    EXPECT_NEAR(det(r), 1.0f, 1e-4f); // proper rotation, no reflection
}
} // namespace

// ---------- compile-time checks (constexpr API) ----------

static_assert(Mat3::identity().m[1][1] == 1.0f && Mat3::identity().m[0][1] == 0.0f);
static_assert((Mat3::identity() * Vec3{1, 2, 3}).z == 3.0f);
static_assert(Mat3::identity().transposed().m[2][2] == 1.0f);

// ---------- construction ----------

TEST(Mat3Test, DefaultConstructedIsZero) {
    const Mat3 a;
    EXPECT_MAT3_EQ(a, matFromRows({}, {}, {}));
}

TEST(Mat3Test, IdentityHasOnesOnDiagonalOnly) {
    const Mat3 i = Mat3::identity();
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            EXPECT_EQ(i.m[r][c], r == c ? 1.0f : 0.0f) << "at m[" << r << "][" << c << "]";
}

TEST(Mat3Test, FromColumnsPlacesVectorsInColumns) {
    const Vec3 c0{1, 2, 3}, c1{4, 5, 6}, c2{7, 8, 9};
    const Mat3 a = Mat3::fromColumns(c0, c1, c2);
    EXPECT_MAT3_EQ(a, matFromRows({1, 4, 7}, {2, 5, 8}, {3, 6, 9}));
}

TEST(Mat3Test, FromColumnsRoundTripsWithColumn) {
    const Vec3 c0{1, -2, 3}, c1{0.5f, 5, -6}, c2{7, 8.25f, 9};
    const Mat3 a = Mat3::fromColumns(c0, c1, c2);
    EXPECT_VEC3_EQ(a.column(0), c0);
    EXPECT_VEC3_EQ(a.column(1), c1);
    EXPECT_VEC3_EQ(a.column(2), c2);
}

TEST(Mat3Test, FromColumnsOfBasisIsIdentity) {
    EXPECT_MAT3_EQ(Mat3::fromColumns(ex, ey, ez), Mat3::identity());
}

// ---------- accessors ----------

TEST(Mat3Test, ColumnReturnsColumn) {
    EXPECT_VEC3_EQ(A.column(0), (Vec3{1, 4, 7}));
    EXPECT_VEC3_EQ(A.column(1), (Vec3{2, 5, 8}));
    EXPECT_VEC3_EQ(A.column(2), (Vec3{3, 6, 10}));
}

TEST(Mat3Test, RowReturnsRow) {
    EXPECT_VEC3_EQ(A.row(0), (Vec3{1, 2, 3}));
    EXPECT_VEC3_EQ(A.row(1), (Vec3{4, 5, 6}));
    EXPECT_VEC3_EQ(A.row(2), (Vec3{7, 8, 10}));
}

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
TEST(Mat3DeathTest, ColumnOutOfRangeAsserts) {
    EXPECT_DEATH(std::ignore = A.column(3), "index out of range");
    EXPECT_DEATH(std::ignore = A.column(-1), "index out of range");
}

TEST(Mat3DeathTest, RowOutOfRangeAsserts) {
    EXPECT_DEATH(std::ignore = A.row(3), "index out of range");
    EXPECT_DEATH(std::ignore = A.row(-1), "index out of range");
}
#endif

// ---------- matrix * vector ----------

TEST(Mat3Test, IdentityTimesVectorIsSameVector) {
    const Vec3 v{1.5f, -2, 3};
    EXPECT_VEC3_EQ(Mat3::identity() * v, v);
}

TEST(Mat3Test, MatrixTimesVectorKnownValue) {
    EXPECT_VEC3_EQ(A * (Vec3{1, -1, 2}), (Vec3{5, 11, 19}));
}

TEST(Mat3Test, MatrixTimesBasisVectorGivesColumn) {
    EXPECT_VEC3_EQ(A * ex, A.column(0));
    EXPECT_VEC3_EQ(A * ey, A.column(1));
    EXPECT_VEC3_EQ(A * ez, A.column(2));
}

TEST(Mat3Test, MatrixTimesVectorIsLinear) {
    const Vec3 u{1, -2, 0.5f}, v{3, 0, -1};
    EXPECT_VEC3_EQ(A * (u + v * 2.0f), A * u + (A * v) * 2.0f);
}

// ---------- matrix * matrix ----------

TEST(Mat3Test, MatrixProductKnownValue) {
    EXPECT_MAT3_EQ(A * B, matFromRows({6.5f, 14, 1}, {14, 29, 4}, {22, 48, 8}));
}

TEST(Mat3Test, IdentityIsNeutralForProduct) {
    EXPECT_MAT3_EQ(Mat3::identity() * A, A);
    EXPECT_MAT3_EQ(A * Mat3::identity(), A);
}

TEST(Mat3Test, ProductIsNotCommutative) {
    const Mat3 ab = A * B, ba = B * A;
    bool anyDifferent = false;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            anyDifferent |= std::fabs(ab.m[i][j] - ba.m[i][j]) > kEps;
    EXPECT_TRUE(anyDifferent);
}

TEST(Mat3Test, ProductIsAssociative) {
    const Mat3 c = rotationZ(0.3f);
    EXPECT_MAT3_NEAR((A * B) * c, A * (B * c), 1e-4f);
}

TEST(Mat3Test, ProductComposesTransformations) {
    const Vec3 v{1, -2, 0.5f};
    EXPECT_VEC3_NEAR((A * B) * v, A * (B * v), 1e-4f);
}

// ---------- transposed ----------

TEST(Mat3Test, TransposedSwapsRowsAndColumns) {
    EXPECT_MAT3_EQ(A.transposed(), matFromRows({1, 4, 7}, {2, 5, 8}, {3, 6, 10}));
}

TEST(Mat3Test, TransposedTwiceIsOriginal) {
    EXPECT_MAT3_EQ(A.transposed().transposed(), A);
}

TEST(Mat3Test, TransposeOfProductReversesOrder) {
    EXPECT_MAT3_NEAR((A * B).transposed(), B.transposed() * A.transposed(), 1e-4f);
}

TEST(Mat3Test, TransposedIdentityIsIdentity) {
    EXPECT_MAT3_EQ(Mat3::identity().transposed(), Mat3::identity());
}

// ---------- axis rotations ----------

TEST(Mat3Test, RotationByZeroIsIdentity) {
    EXPECT_MAT3_EQ(rotationX(0.0f), Mat3::identity());
    EXPECT_MAT3_EQ(rotationY(0.0f), Mat3::identity());
    EXPECT_MAT3_EQ(rotationZ(0.0f), Mat3::identity());
    EXPECT_MAT3_EQ(rotationAxisAngle({1, 2, 3}, 0.0f), Mat3::identity());
}

// Right-hand rule: a positive quarter turn around one axis maps the next basis vector onto the one after it
TEST(Mat3Test, RotationXQuarterTurn) {
    const Mat3 r = rotationX(kPi / 2);
    EXPECT_VEC3_EQ(r * ex, ex);
    EXPECT_VEC3_EQ(r * ey, ez);
    EXPECT_VEC3_EQ(r * ez, -ey);
}

TEST(Mat3Test, RotationYQuarterTurn) {
    const Mat3 r = rotationY(kPi / 2);
    EXPECT_VEC3_EQ(r * ey, ey);
    EXPECT_VEC3_EQ(r * ez, ex);
    EXPECT_VEC3_EQ(r * ex, -ez);
}

TEST(Mat3Test, RotationZQuarterTurn) {
    const Mat3 r = rotationZ(kPi / 2);
    EXPECT_VEC3_EQ(r * ez, ez);
    EXPECT_VEC3_EQ(r * ex, ey);
    EXPECT_VEC3_EQ(r * ey, -ex);
}

TEST(Mat3Test, RotationsAddAngles) {
    EXPECT_MAT3_NEAR(rotationX(0.4f) * rotationX(0.7f), rotationX(1.1f), 1e-5f);
    EXPECT_MAT3_NEAR(rotationY(0.4f) * rotationY(0.7f), rotationY(1.1f), 1e-5f);
    EXPECT_MAT3_NEAR(rotationZ(0.4f) * rotationZ(0.7f), rotationZ(1.1f), 1e-5f);
}

TEST(Mat3Test, InverseRotationIsTranspose) {
    EXPECT_MAT3_NEAR(rotationX(-0.8f), rotationX(0.8f).transposed(), 1e-6f);
    EXPECT_MAT3_NEAR(rotationY(-0.8f), rotationY(0.8f).transposed(), 1e-6f);
    EXPECT_MAT3_NEAR(rotationZ(-0.8f), rotationZ(0.8f).transposed(), 1e-6f);
}

// ---------- rotationAxisAngle ----------

TEST(Mat3Test, AxisAngleMatchesAxisRotations) {
    for (const float angle : {-2.0f, -0.3f, 0.5f, kPi / 2, 3.0f}) {
        SCOPED_TRACE(testing::Message() << "angle = " << angle);
        EXPECT_MAT3_EQ(rotationAxisAngle(ex, angle), rotationX(angle));
        EXPECT_MAT3_EQ(rotationAxisAngle(ey, angle), rotationY(angle));
        EXPECT_MAT3_EQ(rotationAxisAngle(ez, angle), rotationZ(angle));
    }
}

TEST(Mat3Test, AxisAngleNormalizesAxis) {
    EXPECT_MAT3_EQ(rotationAxisAngle({0, 0, 5}, 0.7f), rotationZ(0.7f));
    EXPECT_MAT3_EQ(rotationAxisAngle({1, 2, 3}, 0.7f), rotationAxisAngle(normalized({1, 2, 3}), 0.7f));
}

TEST(Mat3Test, AxisAngleLeavesAxisInvariant) {
    const Vec3 axis{1, -2, 3};
    EXPECT_VEC3_NEAR(rotationAxisAngle(axis, 1.234f) * axis, axis, 1e-4f);
}

TEST(Mat3Test, AxisAngleNegativeAxisReversesRotation) {
    const Vec3 axis{1, -2, 3};
    EXPECT_MAT3_EQ(rotationAxisAngle(-axis, 0.9f), rotationAxisAngle(axis, -0.9f));
}

TEST(Mat3Test, AxisAngleThirdTurnAroundDiagonalCyclesAxes) {
    // 120° around (1,1,1) permutes x -> y -> z -> x
    const Mat3 r = rotationAxisAngle({1, 1, 1}, 2 * kPi / 3);
    EXPECT_VEC3_EQ(r * ex, ey);
    EXPECT_VEC3_EQ(r * ey, ez);
    EXPECT_VEC3_EQ(r * ez, ex);
}

// ---------- properties of all rotations, for many angles ----------

class RotationPropertiesTest : public testing::TestWithParam<float> {};

TEST_P(RotationPropertiesTest, AreOrthonormalWithPositiveDeterminant) {
    const float angle = GetParam();
    {
        SCOPED_TRACE("rotationX");
        expectOrthonormal(rotationX(angle));
    }
    {
        SCOPED_TRACE("rotationY");
        expectOrthonormal(rotationY(angle));
    }
    {
        SCOPED_TRACE("rotationZ");
        expectOrthonormal(rotationZ(angle));
    }
    {
        SCOPED_TRACE("rotationAxisAngle");
        expectOrthonormal(rotationAxisAngle({0.3f, -1.2f, 2.5f}, angle));
    }
}

TEST_P(RotationPropertiesTest, PreserveLengthAndAngles) {
    const Mat3 r = rotationAxisAngle({-0.7f, 0.2f, 1.0f}, GetParam());
    const Vec3 u{1, -2, 3}, v{-0.5f, 4, 1};
    EXPECT_NEAR(length(r * u), length(u), 1e-4f);
    EXPECT_NEAR(dot(r * u, r * v), dot(u, v), 1e-4f);
    EXPECT_VEC3_NEAR(cross(r * u, r * v), r * cross(u, v), 1e-4f);
}

INSTANTIATE_TEST_SUITE_P(Angles, RotationPropertiesTest,
                         testing::Values(-kPi, -1.0f, -0.25f, 0.0f, 0.1f, kPi / 4, 1.7f, kPi, 5.5f, 100.0f));

// ---------- orthonormalized ----------

TEST(Mat3Test, OrthonormalizedIdentityIsIdentity) {
    EXPECT_MAT3_EQ(Mat3::identity().orthonormalized(), Mat3::identity());
}

TEST(Mat3Test, OrthonormalizedRotationIsUnchanged) {
    const Mat3 r = rotationAxisAngle({1, 2, 3}, 0.8f);
    EXPECT_MAT3_EQ(r.orthonormalized(), r);
}

TEST(Mat3Test, OrthonormalizedGivesOrthonormalMatrix) {
    expectOrthonormal(A.orthonormalized());
    expectOrthonormal(B.orthonormalized());
}

TEST(Mat3Test, OrthonormalizedKeepsFirstColumnDirection) {
    EXPECT_VEC3_EQ(A.orthonormalized().column(0), normalized(A.column(0)));
}

TEST(Mat3Test, OrthonormalizedSecondColumnLiesInPlaneOfFirstTwo) {
    // Gram-Schmidt: c1' is perpendicular to the plane normal of (c0, c1)
    const Vec3 normal = cross(A.column(0), A.column(1));
    EXPECT_NEAR(dot(A.orthonormalized().column(1), normal), 0.0f, 1e-4f);
}

TEST(Mat3Test, OrthonormalizedScaledRotationRecoversRotation) {
    const Mat3 r = rotationAxisAngle({-1, 0.5f, 2}, 1.3f);
    const Mat3 scaled = Mat3::fromColumns(r.column(0) * 3.0f, r.column(1) * 0.5f, r.column(2) * 7.0f);
    EXPECT_MAT3_NEAR(scaled.orthonormalized(), r, 1e-5f);
}

TEST(Mat3Test, OrthonormalizedRemovesSmallDrift) {
    // Simulates accumulated floating-point error after many incremental rotations
    Mat3 r = Mat3::identity();
    const Mat3 step = rotationAxisAngle({0.3f, 1, -0.2f}, 0.01f);
    for (int i = 0; i < 10000; ++i)
        r = step * r;
    expectOrthonormal(r.orthonormalized());
}
