#pragma once

#include "math/Mat3.h"
#include "math/Vec3.h"
#include "renderer/Shading.h"
#include "renderer/Texture.h"

/// Normal from a normal map: N = M * N_texture, M = [Pu, Pv, N] (columns, each normalized), as in the task. <br>
/// The map stores the normal in the surface's own frame (x along Pu, y along Pv, z along N), so M carries it
/// to the scene. Pv is not orthogonalized: the task defines M like this and on our patches Pu, Pv are nearly
/// perpendicular. Ntex doesn't have to be unit (colorToNormal isn't), the result is.
[[nodiscard]] inline Vec3 perturbNormal(const Vec3 &Pu, const Vec3 &Pv, const Vec3 &N, const Vec3 &Ntex) {
    const Mat3 M = Mat3::fromColumns(normalized(Pu), normalized(Pv), normalized(N));
    return normalized(M * Ntex);
}

/// Vector stored in a normal map texel, in the surface frame used by perturbNormal (x along Pu, y along Pv). <br>
/// Normal maps keep +y towards the top of the image (OpenGL convention: in normal_circle, a bump, green is high
/// at the top), while v grows down the image (row 0 = v = 0, so Pv points down the image): y is negated.
[[nodiscard]] constexpr Vec3 normalFromMap(const ShadingColor c) {
    const Vec3 n = colorToNormal(c);
    return {.x = n.x, .y = -n.y, .z = n.z};
}
