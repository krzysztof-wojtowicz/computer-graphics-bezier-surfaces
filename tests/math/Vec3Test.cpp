#include "math/TestUtils.h"

// ---------- compile-time checks (constexpr API) ----------

static_assert(Vec3{}.x == 0.0f && Vec3{}.y == 0.0f && Vec3{}.z == 0.0f);
static_assert((Vec3{1, 2, 3} + Vec3{4, 5, 6}).z == 9.0f);
static_assert(dot(Vec3{1, 2, 3}, Vec3{4, 5, 6}) == 32.0f);
static_assert(cross(Vec3{1, 0, 0}, Vec3{0, 1, 0}).z == 1.0f);

// ---------- construction ----------

TEST(Vec3Test, DefaultConstructedIsZero) {
    constexpr Vec3 v;
    EXPECT_EQ(v.x, 0.0f);
    EXPECT_EQ(v.y, 0.0f);
    EXPECT_EQ(v.z, 0.0f);
}

TEST(Vec3Test, DesignatedInitializerSetsComponents) {
    constexpr Vec3 v{.x = 1.5f, .y = -2.0f, .z = 3.25f};
    EXPECT_EQ(v.x, 1.5f);
    EXPECT_EQ(v.y, -2.0f);
    EXPECT_EQ(v.z, 3.25f);
}

// ---------- arithmetic operators ----------

TEST(Vec3Test, Addition) {
    EXPECT_VEC3_EQ((Vec3{1, 2, 3} + Vec3{4, -5, 6}), (Vec3{5, -3, 9}));
}

TEST(Vec3Test, AdditionIsCommutative) {
    const Vec3 a{1.5f, -2, 3}, b{-4, 5.25f, 0.5f};
    EXPECT_VEC3_EQ(a + b, b + a);
}

TEST(Vec3Test, Subtraction) {
    EXPECT_VEC3_EQ((Vec3{1, 2, 3} - Vec3{4, -5, 6}), (Vec3{-3, 7, -3}));
}

TEST(Vec3Test, SubtractingItselfGivesZero) {
    const Vec3 a{1.5f, -2, 3};
    EXPECT_VEC3_EQ(a - a, Vec3{});
}

TEST(Vec3Test, UnaryMinusNegatesAllComponents) {
    EXPECT_VEC3_EQ(-(Vec3{1, -2, 3}), (Vec3{-1, 2, -3}));
}

TEST(Vec3Test, MultiplicationByScalar) {
    EXPECT_VEC3_EQ((Vec3{1, -2, 3} * 2.0f), (Vec3{2, -4, 6}));
}

TEST(Vec3Test, ScalarMultiplicationIsCommutative) {
    const Vec3 v{1, -2, 3};
    EXPECT_VEC3_EQ(2.5f * v, v * 2.5f);
}

TEST(Vec3Test, MultiplicationByZeroGivesZero) {
    EXPECT_VEC3_EQ((Vec3{1, -2, 3} * 0.0f), Vec3{});
}

TEST(Vec3Test, DivisionByScalar) {
    EXPECT_VEC3_EQ((Vec3{2, -4, 6} / 2.0f), (Vec3{1, -2, 3}));
}

TEST(Vec3Test, DivisionIsInverseOfMultiplication) {
    const Vec3 v{1.3f, -2.7f, 3.1f};
    EXPECT_VEC3_EQ((v * 3.7f) / 3.7f, v);
}

TEST(Vec3Test, PlusEqualsModifiesInPlace) {
    Vec3 v{1, 2, 3};
    v += Vec3{4, 5, 6};
    EXPECT_VEC3_EQ(v, (Vec3{5, 7, 9}));
}

TEST(Vec3Test, PlusEqualsReturnsReferenceToSelf) {
    Vec3 v{1, 2, 3};
    Vec3 &ref = (v += Vec3{1, 1, 1});
    EXPECT_EQ(&ref, &v);

    (v += Vec3{1, 0, 0}) += Vec3{0, 1, 0}; // chaining
    EXPECT_VEC3_EQ(v, (Vec3{3, 4, 4}));
}

TEST(Vec3Test, PlusEqualsWithItself) {
    Vec3 v{1, 2, 3};
    v += v;
    EXPECT_VEC3_EQ(v, (Vec3{2, 4, 6}));
}

// ---------- dot ----------

TEST(Vec3Test, DotProduct) {
    EXPECT_FLOAT_EQ(dot(Vec3{1, 2, 3}, Vec3{4, -5, 6}), 12.0f);
}

TEST(Vec3Test, DotIsCommutative) {
    const Vec3 a{1.5f, -2, 3}, b{-4, 5.25f, 0.5f};
    EXPECT_FLOAT_EQ(dot(a, b), dot(b, a));
}

TEST(Vec3Test, DotOfOrthogonalVectorsIsZero) {
    EXPECT_FLOAT_EQ(dot(Vec3{1, 0, 0}, Vec3{0, 1, 0}), 0.0f);
    EXPECT_FLOAT_EQ(dot(Vec3{1, 1, 0}, Vec3{-1, 1, 5}), 0.0f);
}

TEST(Vec3Test, DotWithItselfIsSquaredLength) {
    const Vec3 v{2, -3, 6};
    EXPECT_FLOAT_EQ(dot(v, v), 49.0f);
}

// ---------- cross ----------

TEST(Vec3Test, CrossOfBasisVectorsIsRightHanded) {
    const Vec3 ex{1, 0, 0}, ey{0, 1, 0}, ez{0, 0, 1};
    EXPECT_VEC3_EQ(cross(ex, ey), ez);
    EXPECT_VEC3_EQ(cross(ey, ez), ex);
    EXPECT_VEC3_EQ(cross(ez, ex), ey);
}

TEST(Vec3Test, CrossProductKnownValue) {
    EXPECT_VEC3_EQ(cross(Vec3{1, 2, 3}, Vec3{4, 5, 6}), (Vec3{-3, 6, -3}));
}

TEST(Vec3Test, CrossIsAnticommutative) {
    const Vec3 a{1.5f, -2, 3}, b{-4, 5.25f, 0.5f};
    EXPECT_VEC3_EQ(cross(a, b), -cross(b, a));
}

TEST(Vec3Test, CrossIsPerpendicularToBothArguments) {
    const Vec3 a{1.5f, -2, 3}, b{-4, 5.25f, 0.5f};
    const Vec3 c = cross(a, b);
    EXPECT_NEAR(dot(c, a), 0.0f, kEps);
    EXPECT_NEAR(dot(c, b), 0.0f, kEps);
}

TEST(Vec3Test, CrossOfParallelVectorsIsZero) {
    const Vec3 a{1, -2, 3};
    EXPECT_VEC3_EQ(cross(a, a), Vec3{});
    EXPECT_VEC3_EQ(cross(a, a * -2.5f), Vec3{});
}

// ---------- length ----------

TEST(Vec3Test, LengthKnownValues) {
    EXPECT_FLOAT_EQ(length(Vec3{3, 4, 0}), 5.0f);
    EXPECT_FLOAT_EQ(length(Vec3{2, -3, 6}), 7.0f);
    EXPECT_FLOAT_EQ(length(Vec3{0, 0, -1}), 1.0f);
}

TEST(Vec3Test, LengthOfZeroVectorIsZero) {
    EXPECT_FLOAT_EQ(length(Vec3{}), 0.0f);
}

TEST(Vec3Test, LengthScalesWithAbsoluteValueOfScalar) {
    const Vec3 v{1, -2, 3};
    EXPECT_FLOAT_EQ(length(v * -3.0f), 3.0f * length(v));
}

// ---------- normalized ----------

TEST(Vec3Test, NormalizedHasUnitLength) {
    EXPECT_NEAR(length(normalized(Vec3{1, -2, 3})), 1.0f, kEps);
    EXPECT_NEAR(length(normalized(Vec3{1e-3f, 0, 0})), 1.0f, kEps);
    EXPECT_NEAR(length(normalized(Vec3{1e3f, 1e3f, -1e3f})), 1.0f, kEps);
}

TEST(Vec3Test, NormalizedKeepsDirection) {
    EXPECT_VEC3_EQ(normalized(Vec3{3, 4, 0}), (Vec3{0.6f, 0.8f, 0}));
    EXPECT_VEC3_EQ(normalized(Vec3{0, 0, -5}), (Vec3{0, 0, -1}));
}

TEST(Vec3Test, NormalizedUnitVectorIsUnchanged) {
    const Vec3 v{0, 1, 0};
    EXPECT_VEC3_EQ(normalized(v), v);
}

TEST(Vec3Test, NormalizedZeroVectorStaysZero) {
    const Vec3 n = normalized(Vec3{});
    EXPECT_FALSE(std::isnan(n.x) || std::isnan(n.y) || std::isnan(n.z));
    EXPECT_VEC3_EQ(n, Vec3{});
}
