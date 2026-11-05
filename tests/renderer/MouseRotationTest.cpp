#include "renderer/MouseRotation.h"
#include "math/TestUtils.h"

#include <array>
#include <cmath>

namespace {
constexpr Vec3 kFront{0.0f, 0.0f, 1.0f}; // point closest to the viewer (V = [0, 0, 1])
constexpr Vec3 kAxisX{1.0f, 0.0f, 0.0f};
constexpr Vec3 kAxisY{0.0f, 1.0f, 0.0f};

/// A few rotations that are far from the identity, including upside down scenes
std::array<Mat3, 5> sampleRotations() {
    return {
        Mat3::identity(),
        rotationX(kPi),
        rotationY(kPi),
        rotationX(-1.0f) * rotationZ(0.5f),
        rotationAxisAngle({1.0f, -2.0f, 0.5f}, 2.3f),
    };
}

void expectRotation(const Mat3 &r, const float eps) {
    const Vec3 c0 = r.column(0), c1 = r.column(1), c2 = r.column(2);
    EXPECT_NEAR(length(c0), 1.0f, eps);
    EXPECT_NEAR(length(c1), 1.0f, eps);
    EXPECT_NEAR(length(c2), 1.0f, eps);
    EXPECT_NEAR(dot(c0, c1), 0.0f, eps);
    EXPECT_NEAR(dot(c0, c2), 0.0f, eps);
    EXPECT_NEAR(dot(c1, c2), 0.0f, eps);
    EXPECT_NEAR(det(r), 1.0f, eps);
}
} // namespace

// ---------- dragRotation: no drag, angle ----------

TEST(MouseRotationTest, NoDragGivesIdentity) {
    EXPECT_MAT3_EQ(dragRotation(0.0f, 0.0f), Mat3::identity());
}

TEST(MouseRotationTest, HorizontalDragIsRotationAroundScreenY) {
    // 100 px * 0.01 rad/px = 1 rad: radiansPerPixel is used exactly once
    EXPECT_MAT3_EQ(dragRotation(100.0f, 0.0f), rotationY(1.0f));
}

TEST(MouseRotationTest, VerticalDragIsRotationAroundScreenX) {
    EXPECT_MAT3_EQ(dragRotation(0.0f, 100.0f), rotationX(1.0f));
}

TEST(MouseRotationTest, RadiansPerPixelScalesTheAngle) {
    EXPECT_MAT3_EQ(dragRotation(10.0f, 0.0f, 0.05f), rotationY(0.5f));
    EXPECT_MAT3_EQ(dragRotation(0.0f, -10.0f, 0.05f), rotationX(-0.5f));
}

TEST(MouseRotationTest, DefaultSensitivityIsOneHundredthRadianPerPixel) {
    EXPECT_FLOAT_EQ(kRadiansPerPixel, 0.01f);
}

// ---------- directions: the front of the scene follows the mouse ----------

TEST(MouseRotationTest, DragRightMovesFrontRight) {
    EXPECT_GT((dragRotation(10.0f, 0.0f) * kFront).x, 0.0f);
}

TEST(MouseRotationTest, DragLeftMovesFrontLeft) {
    EXPECT_LT((dragRotation(-10.0f, 0.0f) * kFront).x, 0.0f);
}

TEST(MouseRotationTest, DragDownMovesFrontDown) {
    // Qt: dy > 0 is down on the screen; in the scene down is -y (y_screen = cy - y * s)
    EXPECT_LT((dragRotation(0.0f, 10.0f) * kFront).y, 0.0f);
}

TEST(MouseRotationTest, DragUpMovesFrontUp) {
    EXPECT_GT((dragRotation(0.0f, -10.0f) * kFront).y, 0.0f);
}

TEST(MouseRotationTest, HorizontalDragKeepsScreenYAxis) {
    EXPECT_VEC3_EQ(dragRotation(37.0f, 0.0f) * kAxisY, kAxisY);
}

TEST(MouseRotationTest, VerticalDragKeepsScreenXAxis) {
    EXPECT_VEC3_EQ(dragRotation(0.0f, 37.0f) * kAxisX, kAxisX);
}

TEST(MouseRotationTest, DragRotationIsARotation) {
    expectRotation(dragRotation(23.0f, -71.0f), kEps);
}

// ---------- applyDrag: composing with the current rotation ----------

TEST(MouseRotationTest, ApplyDragWithoutMovementKeepsRotation) {
    for (const Mat3 &r : sampleRotations())
        EXPECT_MAT3_EQ(applyDrag(r, 0.0f, 0.0f), r);
}

TEST(MouseRotationTest, ApplyDragFromIdentityEqualsDragRotation) {
    EXPECT_MAT3_EQ(applyDrag(Mat3::identity(), 12.0f, -5.0f), dragRotation(12.0f, -5.0f));
}

TEST(MouseRotationTest, ApplyDragMultipliesIncrementFromTheLeft) {
    for (const Mat3 &r : sampleRotations())
        EXPECT_MAT3_EQ(applyDrag(r, 12.0f, -5.0f), dragRotation(12.0f, -5.0f) * r);
}

TEST(MouseRotationTest, DragRightMovesFrontRightWhateverTheCurrentRotation) {
    // The object point currently closest to the viewer must move right on the screen,
    // also when the scene is upside down. Multiplying from the right (R * dR) fails for rotationX(pi).
    for (const Mat3 &r : sampleRotations()) {
        const Vec3 frontInObject = r.transposed() * kFront;
        EXPECT_GT((applyDrag(r, 10.0f, 0.0f) * frontInObject).x, 0.0f);
        EXPECT_LT((applyDrag(r, -10.0f, 0.0f) * frontInObject).x, 0.0f);
    }
}

TEST(MouseRotationTest, DragDownMovesFrontDownWhateverTheCurrentRotation) {
    for (const Mat3 &r : sampleRotations()) {
        const Vec3 frontInObject = r.transposed() * kFront;
        EXPECT_LT((applyDrag(r, 0.0f, 10.0f) * frontInObject).y, 0.0f);
        EXPECT_GT((applyDrag(r, 0.0f, -10.0f) * frontInObject).y, 0.0f);
    }
}

TEST(MouseRotationTest, HorizontalDragBackAndForthRestoresRotation) {
    for (const Mat3 &r : sampleRotations())
        EXPECT_MAT3_EQ(applyDrag(applyDrag(r, 40.0f, 0.0f), -40.0f, 0.0f), r);
}

TEST(MouseRotationTest, VerticalDragBackAndForthRestoresRotation) {
    for (const Mat3 &r : sampleRotations())
        EXPECT_MAT3_EQ(applyDrag(applyDrag(r, 0.0f, 40.0f), 0.0f, -40.0f), r);
}

TEST(MouseRotationTest, ApplyDragPreservesDistances) {
    const Mat3 r = applyDrag(rotationX(-1.0f) * rotationZ(0.5f), 17.0f, 29.0f);
    const Vec3 p{3.0f, -3.0f, 2.5f}, q{-1.0f, 1.0f, -2.0f};
    EXPECT_NEAR(length(r * p - r * q), length(p - q), kEps);
}

TEST(MouseRotationTest, ApplyDragResultIsARotation) {
    for (const Mat3 &r : sampleRotations())
        expectRotation(applyDrag(r, -33.0f, 58.0f), kEps);
}

// ---------- 4.5: orthonormalization keeps the matrix a rotation ----------

TEST(MouseRotationTest, ManyDragsKeepMatrixOrthonormal) {
    // Small, varying movements like real mouse events; without orthonormalization float errors would accumulate
    Mat3 r = rotationX(-1.0f) * rotationZ(0.5f);
    for (int i = 0; i < 100'000; ++i) {
        const float dx = static_cast<float>(i % 7) - 3.0f;
        const float dy = static_cast<float>(i % 5) - 2.0f;
        r = applyDrag(r, dx, dy);
    }
    expectRotation(r, kEps);
}

TEST(MouseRotationTest, ApplyDragRepairsSlightlyDistortedMatrix) {
    // A rotation with float noise added: one applyDrag brings it back to an exact rotation
    Mat3 distorted = rotationAxisAngle({1.0f, 1.0f, 0.0f}, 0.7f);
    distorted.m[0][0] *= 1.001f;
    distorted.m[1][2] += 0.001f;
    expectRotation(applyDrag(distorted, 1.0f, 1.0f), kEps);
}
