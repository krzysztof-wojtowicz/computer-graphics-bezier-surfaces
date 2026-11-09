#pragma once

#include "renderer/Projection.h"

struct Weights {
    float a, b, c;
};

class Barycentric {
public:
    static constexpr float kMinArea2 = 1e-6f;

    Barycentric(const ScreenPoint &a, const ScreenPoint &b, const ScreenPoint &c) : _a(a), _b(b), _c(c) {
        _area2 = std::abs((_b.x - _a.x) * (_c.y - _a.y) - (_b.y - _a.y) * (_c.x - _a.x));
    }

    /// Checks if area of the triangle is close to 0
    [[nodiscard]] bool degenerate() const { return  _area2 < kMinArea2; }

    /// Returns barycentric weights for a given point (px, py)
    [[nodiscard]] Weights at(const float px, const float py) const {
        const float b = std::abs((px - _a.x) * (_c.y - _a.y) - (_c.x - _a.x) * (py - _a.y)) / _area2;
        const float c = std::abs((_b.x - _a.x) * (py - _a.y) - (px - _a.x) * (_b.y - _a.y)) / _area2;

        return {
            .a = 1 - b - c,
            .b = b,
            .c = c
        };
    }

private:
    ScreenPoint _a, _b, _c;
    float _area2;
};

/// Interpolates parameter F(p) with parameters f_a, f_b and f_c using barycentric weights
[[nodiscard]] inline float interpolate(const Weights &w, const float a, const float b, const float c) {
    return a * w.a + b * w.b + c * w.c;
}