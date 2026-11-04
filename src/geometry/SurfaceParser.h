#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "geometry/BezierSurface.h"

struct ParseResult {
    std::vector<BezierSurface> surfaces;
    std::string error; // empty = success, otherwise message with line number

    [[nodiscard]] bool ok() const { return error.empty(); }
};

/// Parses Bézier surfaces from text: blocks of 16 lines "x y z" (P00, P01, ..., P33), one block per surface. <br>
/// No empty lines or comments, only a single trailing newline is allowed. Decimal separator is always '.'.
ParseResult parseSurfaces(std::string_view text);
