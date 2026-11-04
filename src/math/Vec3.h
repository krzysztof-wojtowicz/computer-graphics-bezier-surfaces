#pragma once

#include <cmath>

/// Vec3 struct represents vector with 3 dimensions [x, y, z] <br>
/// x,y,z are of float type  <br>
/// Basic operations for vectors are implemented:  <br>
/// -> +, -, *, /, +=  <br>
/// -> dot(), cross(), length(), normalized()
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3 operator+(const Vec3 &o) const { return {.x = x + o.x, .y = y + o.y, .z = z + o.z}; }
    constexpr Vec3 operator-(const Vec3 &o) const { return {.x = x - o.x, .y = y - o.y, .z = z - o.z}; }
    constexpr Vec3 operator-() const { return {.x = -x, .y = -y, .z = -z}; }
    constexpr Vec3 operator*(const float s) const { return {.x = x * s, .y = y * s, .z = z * s}; }
    constexpr Vec3 operator/(const float s) const { return {.x = x / s, .y = y / s, .z = z / s}; }

    constexpr Vec3 &operator+=(const Vec3 &o) {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
};

constexpr Vec3 operator*(float s, const Vec3 &v) { return v * s; }

constexpr float dot(const Vec3 &a, const Vec3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

constexpr Vec3 cross(const Vec3 &a, const Vec3 &b) {
    return {
        .x = a.y * b.z - a.z * b.y,
        .y = a.z * b.x - a.x * b.z,
        .z = a.x * b.y - a.y * b.x
    };
}

inline float length(const Vec3 &v) { return std::sqrt(dot(v, v)); }

inline Vec3 normalized(const Vec3 &v) {
    const float len = length(v);
    return len > 0.0f ? v / len : v;
}
