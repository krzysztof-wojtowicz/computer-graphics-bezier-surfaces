#pragma once

#include <array>

/// Returns B0, B1, B2, B3 of the Bezier curve of 3 degree for parameter t
constexpr std::array<float, 4> bernstein3(const float t) {
    const float t1 = 1 - t;

    return {
        {
            t1 * t1 * t1,
            3 * t * t1 * t1,
            3 * t * t * t1,
            t * t * t
        }};
}

/// Returns B0', B1', B2', B3' of the Bezier curve of 3 degree for parameter t
constexpr std::array<float, 4> bernstein3Deriv(const float t) {
    const float t1 = 1 - t;

    return {
        {
            -3 * t1 * t1,
            3 * t1 * t1 - 6 * t * t1,
            6 * t * t1 - 3 * t * t,
            3 * t * t
        }};
}
