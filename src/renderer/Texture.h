#pragma once

#include "math/Vec3.h"
#include "renderer/Shading.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

/// Image sampled by surface parameters (u, v): the object color IO or a normal map. <br>
/// Texels are stored as 8-bit RGB (3 bytes each, row-major, row 0 = top of the image),
/// u selects the column and v the row; texel x covers u in [x/w, (x+1)/w).
class Texture {
public:
    /// 1x1 white: a valid texture before any image is loaded, and IO = 1 doesn't change the lighting
    Texture() = default;

    Texture(const int width, const int height, std::vector<std::uint8_t> rgb)
        : _width(width), _height(height), _rgb(std::move(rgb)) {
        assert(width > 0 && height > 0 && "texture size must be > 0");
        assert(_rgb.size() == 3 * static_cast<std::size_t>(width) * static_cast<std::size_t>(height)
               && "texture needs 3 bytes (R, G, B) per texel");
    }

    [[nodiscard]] int width() const { return _width; }
    [[nodiscard]] int height() const { return _height; }

    /// Returns color of the texel (x, y) in [0, 1]
    [[nodiscard]] ShadingColor texel(const int x, const int y) const {
        assert(x >= 0 && x < _width && y >= 0 && y < _height && "texel out of range");

        const std::size_t idx = 3 * (static_cast<std::size_t>(y) * static_cast<std::size_t>(_width)
                                     + static_cast<std::size_t>(x));
        // a byte is always in [0, 255], so no clamping as in Shading::int2float
        return {
            .r = static_cast<float>(_rgb[idx]) / 255.0f,
            .g = static_cast<float>(_rgb[idx + 1]) / 255.0f,
            .b = static_cast<float>(_rgb[idx + 2]) / 255.0f
        };
    }

    /// Nearest texel; (u, v) outside [0, 1] (barycentric weights on an edge, NaN) is clamped to the border
    [[nodiscard]] ShadingColor sample(const float u, const float v) const {
        return texel(index(u, _width), index(v, _height));
    }

private:
    int _width = 1, _height = 1;
    std::vector<std::uint8_t> _rgb = {255, 255, 255};

    /// Maps t in [0, 1] to a texel index in [0, n - 1]. Clamps t in float before the cast:
    /// casting a huge value (e.g. 1e30) to int is undefined behaviour.
    [[nodiscard]] static int index(const float t, const int n) {
        if (!(t > 0.0f)) return 0; // negatives and NaN (every comparison with NaN is false)

        // truncation, not rounding: rounding would shift the whole image by half a texel
        const int i = static_cast<int>(std::min(t, 1.0f) * static_cast<float>(n));
        return std::min(i, n - 1); // t = 1 (vertices on the surface border) and +inf -> last texel
    }
};

/// Decodes a normal map color: each component [0, 1] -> [-1, 1]. <br>
/// Not normalized here: the result of M * N_texture is, and gray (0.5, 0.5, 0.5) would give a zero vector.
[[nodiscard]] constexpr Vec3 colorToNormal(const ShadingColor c) {
    return Vec3{
        .x = 2.0f * c.r - 1.0f,
        .y = 2.0f * c.g - 1.0f,
        .z = 2.0f * c.b - 1.0f
    };
}
