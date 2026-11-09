#pragma once

#include <cassert>

#include "math/Vec3.h"
#include "geometry/BezierSurface.h"

struct ShadingColor {
    float r, g, b;
};

class Shading {
public:
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
        assert(m >= 0 && m <= 100);
        _m = m;
    }
    void setL(const Vec3 v) {
        _l = v;
        normalized(_l);
    }

    // --- METHODS ---
    static int scaleColor(const float color) { return static_cast<int>(color * 255); }

    [[nodiscard]] ShadingColor calculateColor(const ShadingColor &io, const SurfacePoint &p) const {
        // calculate cos(angle(N, L)) and cos^m(angle(V,R))
        const Vec3 N = normalized(p.N);
        const Vec3 R = dot(N, _l) * 2.0f * N - _l;
        const float cosNL = _kd * std::max(0.0f, std::cos(angle(N, _l)));
        const float cosVR = _ks * std::pow(std::max(0.0f, std::cos(angle(_v, R))), _m);

        // calculate each component (R, G, B)
        return {
            .r = _il.r * io.r * cosNL + _il.r * io.r * cosVR,
            .g = _il.g * io.g * cosNL + _il.g * io.g * cosVR,
            .b = _il.b * io.b * cosNL + _il.b * io.b * cosVR
        };
    }

private:
    float _kd = 1.0f; // d impact factor from [0,1]
    float _ks = 1.0f; // s impact factor from [0,1]
    ShadingColor _il = ShadingColor{.r = 1, .g = 1, .b = 1}; // light color, default is white
    float _m = 50.0f; // surface character factor from [0,1]
    Vec3 _v = Vec3{.x = 0, .y = 0, .z = 1}; // V vector [0,0,1]
    Vec3 _l = Vec3{.x = 1, .y = 0, .z = 0}; // light direction
};