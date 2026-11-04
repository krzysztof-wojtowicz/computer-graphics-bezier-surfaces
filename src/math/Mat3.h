#pragma once

#include "math/Vec3.h"

#include <array>
#include <cassert>
#include <cmath>

/// Mat3 struct represents a 3x3 matrix with float values <br>
/// basic matrix operations are implemented: <br>
/// -> identity(), fromColumns(), column(), row(), * (matrix and vector) <br>
/// -> transposed(), orthonormalized() <br>
/// -> rotations
struct Mat3 {
    std::array<std::array<float, 3>, 3> m{}; // m[row][column]

    /// Returns identity matrix
    static constexpr Mat3 identity() {
        Mat3 res;
        res.m[0][0] = res.m[1][1] = res.m[2][2] = 1.0f;
        return res;
    }

    /// Returns matrix made from 3 columns c0, c1 and c2
    static constexpr Mat3 fromColumns(const Vec3 &c0, const Vec3 &c1, const Vec3 &c2) {
        Mat3 res;
        res.m = {
            {
                {c0.x, c1.x, c2.x},
                {c0.y, c1.y, c2.y},
                {c0.z, c1.z, c2.z}
            }
        };
        return res;
    }

    /// Returns i column of the matrix, 0 <= i < 3
    [[nodiscard]] constexpr Vec3 column(const int i) const {
        assert(i >= 0 && i < 3 && "Mat3::column: index out of range");
        return {.x = m[0][i], .y = m[1][i], .z = m[2][i]};
    }

    /// Returns i row of the matrix, 0 <= i < 3
    [[nodiscard]] constexpr Vec3 row(const int i) const {
        assert(i >= 0 && i < 3 && "Mat3::row: index out of range");
        return {.x = m[i][0], .y = m[i][1], .z = m[i][2]};
    }

    constexpr Vec3 operator*(const Vec3 &v) const {
        return {
            .x = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            .y = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            .z = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        };
    }

    constexpr Mat3 operator*(const Mat3 &o) const {
        Mat3 res;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    res.m[i][j] += m[i][k] * o.m[k][j];
        return res;
    }

    /// Returns transposed matrix
    [[nodiscard]] constexpr Mat3 transposed() const {
        Mat3 res;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                res.m[i][j] = m[j][i];
        return res;
    }

    /// Returns orthonormalized matrix (columns with length 1, perpendicular to each other)
    [[nodiscard]] Mat3 orthonormalized() const {
        const Vec3 c0 = normalized(column(0));
        const Vec3 c1 = normalized(column(1) - dot(column(1), c0) * c0);
        const Vec3 c2 = cross(c0, c1);
        return fromColumns(c0, c1, c2);
    }
};

inline Mat3 rotationX(const float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat3 res;
    res.m = {
        {
            {1, 0, 0},
            {0, c, -s},
            {0, s, c}
        }
    };
    return res;
}

inline Mat3 rotationY(const float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat3 res;
    res.m = {
        {
            {c, 0, s},
            {0, 1, 0},
            {-s, 0, c}
        }
    };
    return res;
}

inline Mat3 rotationZ(const float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat3 res;
    res.m = {
        {
            {c, -s, 0},
            {s, c, 0},
            {0, 0, 1}
        }
    };
    return res;
}

/// Returns the rotation matrix from Rodrigues formula <br>
/// https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula#Matrix_notation
inline Mat3 rotationAxisAngle(const Vec3 &axis, const float angle) {
    const auto [x, y, z] = normalized(axis);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float t = 1.0f - c;
    Mat3 r;
    r.m = {
        {
            {t * x * x + c, t * x * y - s * z, t * x * z + s * y},
            {t * x * y + s * z, t * y * y + c, t * y * z - s * x},
            {t * x * z - s * y, t * y * z + s * x, t * z * z + c}
        }
    };
    return r;
}
