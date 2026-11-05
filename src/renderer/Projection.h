#pragma once

#include <algorithm>
#include <cassert>

#include "math/Vec3.h"

struct ScreenPoint {
    float x, y, z;
};

class Projection {
public:
    // default scene radius, for x,y,z from range [-3, 3] -> all points
    // will sit inside the sphere of a radius ~5.196
    static constexpr float kSceneRadius = 5.2f;

    /// Sets the center of the projection and scale (how many pixels one point represents)
    explicit Projection(const int width, const int height, const float sceneRadius = kSceneRadius)
    : _cx(0.5f * static_cast<float>(width)),
    _cy(0.5f * static_cast<float>(height)),
    _scale(0.5f * static_cast<float>(std::min(width, height)) / sceneRadius) {
        assert(width > 0 && height > 0 && sceneRadius > 0 && "arguments must be > 0!");
    }

    /// Projects given point p to screen point
    [[nodiscard]] ScreenPoint project(const Vec3 &p) const {
        return {
            .x = _cx + p.x * _scale,
            .y = _cy - p.y * _scale,
            .z = p.z
        };
    }

private:
    // (_cx, _cy) is the center
    float _cx;
    float _cy;

    // _scale is used to scale original coordinates to the min(width, height)
    // so the surface uses all of the viewport
    float _scale;
};
