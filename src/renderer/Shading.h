#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <span>

#include "math/Vec3.h"

struct ShadingColor {
    float r, g, b;
};

class Shading {
public:
    /// Point lights summed in calculateColor; a fixed array, so the per-pixel loop never allocates
    static constexpr int kMaxLights = 3;

    Shading() = default;

    // --- SETTERS ---
    void setKd(const float value) {
        assert(value >= 0 && value <= 1);
        _kd = value;
    }

    void setKs(const float value) {
        assert(value >= 0 && value <= 1);
        _ks = value;
    }

    void setIl(const ShadingColor rgb) {
        assert(rgb.r >= 0 && rgb.r <= 1 && rgb.g >= 0 && rgb.g <= 1 && rgb.b >= 0 && rgb.b <= 1);
        _il = rgb;
    }

    void setM(const float m) {
        assert(m >= 1 && m <= 100);
        _m = m;
    }

    /// One light at v (the others switched off)
    void setLightPosition(const Vec3 v) {
        _lightPos[0] = v;
        _lightCount = 1;
    }

    /// Lights at the given positions, all with the same color IL
    void setLightPositions(const std::span<const Vec3> positions) {
        assert(!positions.empty() && std::ssize(positions) <= kMaxLights && "1 to kMaxLights lights");
        std::ranges::copy(positions, _lightPos.begin());
        _lightCount = static_cast<int>(positions.size());
    }

    // --- GETTERS ---
    [[nodiscard]] float kd() const { return _kd; }
    [[nodiscard]] float ks() const { return _ks; }
    [[nodiscard]] float m() const { return _m; }
    [[nodiscard]] ShadingColor il() const { return _il; }
    [[nodiscard]] int lightCount() const { return _lightCount; }
    [[nodiscard]] Vec3 lightPosition(const int i = 0) const {
        assert(i >= 0 && i < _lightCount && "light index out of range");
        return _lightPos[static_cast<std::size_t>(i)];
    }

    // --- METHODS ---
    /// Converts float values [0,1] to int values [0,255];
    static int float2int(const float color) {
        return static_cast<int>(std::lround(std::max(0.0f, std::min(color * 255.0f, 255.0f))));
    }

    /// Converts int values [0,255] to float values [0,1]
    static float int2float(const int color) {
        return std::min(std::max(static_cast<float>(color) / 255.0f, 0.0f), 1.0f);
    }

    /// Sum of the Lambert + specular terms of every light (light intensities add up);
    /// the result can exceed 1, float2int clamps it
    [[nodiscard]] ShadingColor calculateColor(const ShadingColor &io, const Vec3 n, const Vec3 P) const {
        Vec3 N = normalized(n);
        if (N.z < 0)
            N = -N; // for the bottom of the surface

        // cos(angle(N, L)) and cos^m(angle(V, R)) summed over the lights; IL and IO are the same for all
        float intensity = 0.0f;
        for (int i = 0; i < _lightCount; ++i) {
            const Vec3 L = normalized(_lightPos[static_cast<std::size_t>(i)] - P);
            const Vec3 R = dot(N, L) * 2.0f * N - L;
            intensity += _kd * std::max(0.0f, dot(N, L))
                         + _ks * std::pow(std::max(0.0f, dot(_v, R)), _m);
        }

        // calculate each component (R, G, B)
        return {
            .r = _il.r * io.r * intensity,
            .g = _il.g * io.g * intensity,
            .b = _il.b * io.b * intensity
        };
    }

private:
    float _kd = 1.0f; // d impact factor from [0,1]
    float _ks = 1.0f; // s impact factor from [0,1]
    ShadingColor _il = ShadingColor{.r = 1, .g = 1, .b = 1}; // light color, default is white
    float _m = 50.0f; // surface character factor from [1,100]
    Vec3 _v = Vec3{.x = 0, .y = 0, .z = 1}; // V vector [0,0,1]
    std::array<Vec3, kMaxLights> _lightPos{Vec3{.x = 10.0f, .y = 0.0f, .z = 1.0f}}; // only the first _lightCount are used
    int _lightCount = 1;
};
