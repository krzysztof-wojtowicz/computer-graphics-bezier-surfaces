#pragma once

#include "math/Mat3.h"
#include "math/Vec3.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <ostream>

inline constexpr float kEps = 1e-5f;
inline constexpr float kPi = std::numbers::pi_v<float>;

// gtest uses these to print values in failure messages
inline void PrintTo(const Vec3 &v, std::ostream *os) {
    *os << "[" << v.x << ", " << v.y << ", " << v.z << "]";
}

inline void PrintTo(const Mat3 &a, std::ostream *os) {
    *os << "\n";
    for (const auto &r : a.m)
        *os << "  [" << r[0] << ", " << r[1] << ", " << r[2] << "]\n";
}

/// Builds a matrix from rows (m[row][column]) – independent of Mat3::fromColumns
constexpr Mat3 matFromRows(const Vec3 &r0, const Vec3 &r1, const Vec3 &r2) {
    Mat3 res;
    res.m = {{{r0.x, r0.y, r0.z}, {r1.x, r1.y, r1.z}, {r2.x, r2.y, r2.z}}};
    return res;
}

constexpr float det(const Mat3 &a) {
    const auto &m = a.m;
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
         - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
         + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

inline testing::AssertionResult vecNear(const char *aExpr, const char *bExpr, const char *, const Vec3 &a,
                                        const Vec3 &b, const float eps) {
    if (std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps && std::fabs(a.z - b.z) <= eps)
        return testing::AssertionSuccess();
    return testing::AssertionFailure() << aExpr << " = " << testing::PrintToString(a) << "\n"
                                       << bExpr << " = " << testing::PrintToString(b) << "\n"
                                       << "differ by more than " << eps;
}

inline testing::AssertionResult matNear(const char *aExpr, const char *bExpr, const char *, const Mat3 &a,
                                        const Mat3 &b, const float eps) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (std::fabs(a.m[i][j] - b.m[i][j]) > eps)
                return testing::AssertionFailure() << aExpr << " =" << testing::PrintToString(a)
                                                   << bExpr << " =" << testing::PrintToString(b)
                                                   << "differ at m[" << i << "][" << j << "] by more than " << eps;
    return testing::AssertionSuccess();
}

#define EXPECT_VEC3_NEAR(a, b, eps) EXPECT_PRED_FORMAT3(vecNear, a, b, eps)
#define EXPECT_VEC3_EQ(a, b) EXPECT_PRED_FORMAT3(vecNear, a, b, kEps)
#define EXPECT_MAT3_NEAR(a, b, eps) EXPECT_PRED_FORMAT3(matNear, a, b, eps)
#define EXPECT_MAT3_EQ(a, b) EXPECT_PRED_FORMAT3(matNear, a, b, kEps)
