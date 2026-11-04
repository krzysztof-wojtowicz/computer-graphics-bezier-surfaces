#include "geometry/SurfaceParser.h"

#include <cmath>
#include <cstddef>
#include <locale>
#include <sstream>

namespace {
constexpr std::size_t kPointsPerSurface = 16;

/// Splits text into lines. A trailing '\r' is removed from every line (Windows line endings).
/// A single newline at the end of the text does not produce an extra empty line.
std::vector<std::string_view> splitLines(std::string_view text) {
    std::vector<std::string_view> lines;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        if (!line.empty() && line.back() == '\r') // Windows line endings: \r\n
            line.remove_suffix(1);
        lines.push_back(line);

        if (end == std::string_view::npos)
            break;
        text.remove_prefix(end + 1);
    }
    return lines;
}

/// Reads exactly three finite numbers "x y z" from a line. Returns false if anything else is there.
bool parsePoint(const std::string_view line, Vec3 &out) {
    // create input string stream from line, set its locale, so '.' is always accepted as decimal separator
    std::istringstream ss{std::string(line)};
    ss.imbue(std::locale::classic());

    // parse 3 numbers from stream
    if (!(ss >> out.x >> out.y >> out.z))
        return false; // fewer than 3 numbers or text instead of a number

    // remove all whitespaces
    ss >> std::ws;
    if (!ss.eof())
        return false; // something left after the third number, e.g. a 4th number or "1 2 3abc"

    // check if numbers are finite
    return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z);
}

/// Creates string with line error
std::string lineError(const std::size_t index, const std::string_view line, const std::string_view reason) {
    return "Line " + std::to_string(index + 1) + ": " + std::string(reason) + ", current line: \"" + std::string(line) +
           "\"";
}
} // namespace

ParseResult parseSurfaces(const std::string_view text) {
    ParseResult result;
    const std::vector<std::string_view> lines = splitLines(text);

    // check if file is empty
    if (lines.empty()) {
        result.error = "File is empty";
        return result;
    }

    // check if numbers of lines is multiple of 16
    if (lines.size() % kPointsPerSurface != 0) {
        result.error = "File has " + std::to_string(lines.size()) +
                       " lines, expected multiple of 16 (16 points per surface)";
        return result;
    }

    BezierSurface::ControlGrid grid;
    for (std::size_t k = 0; k < lines.size(); ++k) {
        Vec3 point;

        // check if empty line
        if (lines[k].empty()) {
            result.error = lineError(k, lines[k], "empty line");
            result.surfaces.clear();
            return result;
        }

        // parse points and check result
        if (!parsePoint(lines[k], point)) {
            result.error = lineError(k, lines[k], "expected 3 numbers in a format: \"x y z\"");
            result.surfaces.clear();
            return result;
        }

        // line k is control point P[i][j] of surface k / 16
        const std::size_t index = k % kPointsPerSurface;
        grid[index / 4][index % 4] = point;
        if (index == kPointsPerSurface - 1)
            result.surfaces.emplace_back(grid);
    }
    return result;
}
