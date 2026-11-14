#pragma once

#include "math/Mat3.h"
#include "math/Vec3.h"

/// Normal from a normal map: N = M * N_texture, M = [Pu, Pv, N] (columns, each normalized), as in the task. <br>
/// The map stores the normal in the surface's own frame (x along Pu, y along Pv, z along N), so M carries it
/// to the scene. Pv is not orthogonalized: the task defines M like this and on our patches Pu, Pv are nearly
/// perpendicular. Ntex doesn't have to be unit (colorToNormal isn't), the result is.
[[nodiscard]] inline Vec3 perturbNormal(const Vec3 &Pu, const Vec3 &Pv, const Vec3 &N, const Vec3 &Ntex) {
    const Mat3 M = Mat3::fromColumns(normalized(Pu), normalized(Pv), normalized(N));
    return normalized(M * Ntex);
}
