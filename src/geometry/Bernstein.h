#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

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

/// Returns B0..Bn of the Bezier curve of degree n for parameter t; the degree is known only at run time
/// (the closed light curve has as many points as the user's control polygon)
inline std::vector<float> bernstein(const int n, const float t) {
    assert(n >= 0 && "degree must be non-negative");
    const float t1 = 1 - t;

    std::vector<float> res(static_cast<std::size_t>(n) + 1);
    float binomial = 1; // C(n, i), updated in place: C(n, i + 1) = C(n, i) * (n - i) / (i + 1)
    for (int i = 0; i <= n; ++i) {
        float value = binomial;
        for (int k = 0; k < i; ++k) value *= t;
        for (int k = 0; k < n - i; ++k) value *= t1;
        res[static_cast<std::size_t>(i)] = value;
        binomial = binomial * static_cast<float>(n - i) / static_cast<float>(i + 1);
    }
    return res;
}
