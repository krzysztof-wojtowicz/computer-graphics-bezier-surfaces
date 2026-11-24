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
    /// Range and default of the spotlight exponent k in I = IL * cos^k(alpha)
    static constexpr float kMinSpotExponent = 1.0f;
    static constexpr float kMaxSpotExponent = 100.0f;
    static constexpr float kDefaultSpotExponent = 10.0f;
    /// Every spotlight is aimed at this point (the centre of the scene; rotation about it keeps it in place)
    static constexpr Vec3 kSpotTarget{.x = 0.0f, .y = 0.0f, .z = 0.0f};

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

    /// The same color IL for every light
    void setIl(const ShadingColor rgb) {
        assert(validColor(rgb) && "light color out of [0, 1]");
        _il.fill(rgb);
    }

    /// Color IL of each light: colors[i] goes with lightPosition(i); lights without a color in the span keep theirs
    void setLightColors(const std::span<const ShadingColor> colors) {
        assert(!colors.empty() && std::ssize(colors) <= kMaxLights && "1 to kMaxLights light colors");
        for ([[maybe_unused]] const ShadingColor &c : colors)
            assert(validColor(c) && "light color out of [0, 1]");
        std::ranges::copy(colors, _il.begin());
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

    /// Lights as spotlights aimed at kSpotTarget (true) or point lights shining equally everywhere (false)
    void setSpotlights(const bool on) {
        _spotlights = on;
    }

    void setSpotExponent(const float k) {
        assert(k >= kMinSpotExponent && k <= kMaxSpotExponent && "spot exponent out of range");
        _spotExponent = k;
    }

    // --- GETTERS ---
    [[nodiscard]] float kd() const { return _kd; }
    [[nodiscard]] float ks() const { return _ks; }
    [[nodiscard]] float m() const { return _m; }
    [[nodiscard]] ShadingColor il(const int i = 0) const {
        assert(i >= 0 && i < kMaxLights && "light index out of range");
        return _il[static_cast<std::size_t>(i)];
    }
    [[nodiscard]] int lightCount() const { return _lightCount; }
    [[nodiscard]] bool spotlights() const { return _spotlights; }
    [[nodiscard]] float spotExponent() const { return _spotExponent; }
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

    /// Part of IL that reaches P from a spotlight at lightPos (Warn model): cos^k of the angle between the
    /// spotlight axis (towards kSpotTarget) and the direction lightPos -> P; 1 on the axis, 0 at 90 degrees and behind
    [[nodiscard]] float spotFactor(const Vec3 &lightPos, const Vec3 &P) const {
        const Vec3 axis = normalized(kSpotTarget - lightPos);
        const float cosAlpha = dot(axis, normalized(P - lightPos));
        return std::pow(std::max(0.0f, cosAlpha), _spotExponent);
    }

    /// Sum of the Lambert + specular terms of every light (light intensities add up), each scaled by its
    /// spotFactor when the lights are spotlights; the result can exceed 1, float2int clamps it
    [[nodiscard]] ShadingColor calculateColor(const ShadingColor &io, const Vec3 n, const Vec3 P) const {
        Vec3 N = normalized(n);
        if (N.z < 0)
            N = -N; // for the bottom of the surface

        // IL_i * (kd * cos(angle(N, L_i)) + ks * cos^m(angle(V, R_i))) summed over the lights; IO is common
        ShadingColor light{.r = 0.0f, .g = 0.0f, .b = 0.0f};
        for (int i = 0; i < _lightCount; ++i) {
            const Vec3 &lightPos = _lightPos[static_cast<std::size_t>(i)];
            const Vec3 L = normalized(lightPos - P);
            const Vec3 R = dot(N, L) * 2.0f * N - L;
            // IL of a spotlight is weaker off its axis: I(P) = IL * cos^k(alpha)
            const float spot = _spotlights ? spotFactor(lightPos, P) : 1.0f;
            const float intensity = spot * (_kd * std::max(0.0f, dot(N, L))
                                            + _ks * std::pow(std::max(0.0f, dot(_v, R)), _m));
            const ShadingColor &il = _il[static_cast<std::size_t>(i)];
            light.r += il.r * intensity;
            light.g += il.g * intensity;
            light.b += il.b * intensity;
        }

        // calculate each component (R, G, B)
        return {
            .r = io.r * light.r,
            .g = io.g * light.g,
            .b = io.b * light.b
        };
    }

private:
    static constexpr ShadingColor kWhiteLight{.r = 1.0f, .g = 1.0f, .b = 1.0f};

    [[nodiscard]] static constexpr bool validColor(const ShadingColor &c) {
        return c.r >= 0 && c.r <= 1 && c.g >= 0 && c.g <= 1 && c.b >= 0 && c.b <= 1;
    }

    float _kd = 1.0f; // d impact factor from [0,1]
    float _ks = 1.0f; // s impact factor from [0,1]
    // light color of each light, default is white
    std::array<ShadingColor, kMaxLights> _il{kWhiteLight, kWhiteLight, kWhiteLight};
    float _m = 50.0f; // surface character factor from [1,100]
    Vec3 _v = Vec3{.x = 0, .y = 0, .z = 1}; // V vector [0,0,1]
    std::array<Vec3, kMaxLights> _lightPos{Vec3{.x = 10.0f, .y = 0.0f, .z = 1.0f}}; // only the first _lightCount are used
    int _lightCount = 1;
    bool _spotlights = false;
    float _spotExponent = kDefaultSpotExponent; // k in cos^k(alpha), the larger the narrower the beam
};
