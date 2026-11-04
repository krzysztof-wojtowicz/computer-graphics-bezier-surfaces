#include "geometry/SurfaceParser.h"
#include "math/TestUtils.h"

#include <array>
#include <cmath>
#include <fstream>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::array kCoords{-3, -1, 1, 3};

/// 16 lines of one surface on the regular grid. z = zBase + 10·i + j, so every point is recognizable.
std::vector<std::string> surfaceLines(const int zBase = 0) {
    std::vector<std::string> lines;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            lines.push_back(std::to_string(kCoords[i]) + " " + std::to_string(kCoords[j]) + " " +
                            std::to_string(zBase + 10 * i + j));
    return lines;
}

std::string join(const std::vector<std::string> &lines, const std::string &eol = "\n") {
    std::string text;
    for (const auto &line : lines)
        text += line + eol;
    return text;
}

/// Valid surface with line `index` (0-based) replaced by `replacement`
std::string withLine(const std::size_t index, const std::string &replacement) {
    auto lines = surfaceLines();
    lines[index] = replacement;
    return join(lines);
}

testing::AssertionResult failsAtLine(const std::string &text, const int line) {
    const ParseResult r = parseSurfaces(text);
    if (r.ok())
        return testing::AssertionFailure() << "parsing succeeded, expected error at line " << line;
    if (!r.surfaces.empty())
        return testing::AssertionFailure() << "error reported, but " << r.surfaces.size() << " surfaces returned";
    if (r.error.find("Line " + std::to_string(line) + ":") == std::string::npos)
        return testing::AssertionFailure() << "error does not point to line " << line << ": " << r.error;
    return testing::AssertionSuccess();
}

testing::AssertionResult failsWith(const std::string &text, const std::string &fragment) {
    const ParseResult r = parseSurfaces(text);
    if (r.ok())
        return testing::AssertionFailure() << "parsing succeeded, expected error containing \"" << fragment << "\"";
    if (!r.surfaces.empty())
        return testing::AssertionFailure() << "error reported, but " << r.surfaces.size() << " surfaces returned";
    if (r.error.find(fragment) == std::string::npos)
        return testing::AssertionFailure() << "error \"" << r.error << "\" does not contain \"" << fragment << "\"";
    return testing::AssertionSuccess();
}

/// Sets the global C++ and C locale for the duration of a test, restores the previous one afterwards
class ScopedGlobalLocale {
public:
    explicit ScopedGlobalLocale(const std::locale &loc) : _previous(std::locale::global(loc)) {}
    ~ScopedGlobalLocale() { std::locale::global(_previous); }
    ScopedGlobalLocale(const ScopedGlobalLocale &) = delete;
    ScopedGlobalLocale &operator=(const ScopedGlobalLocale &) = delete;

private:
    std::locale _previous;
};
} // namespace

// ---------- valid input ----------

TEST(SurfaceParserTest, ParsesSingleSurface) {
    const ParseResult r = parseSurfaces(join(surfaceLines()));
    ASSERT_TRUE(r.ok()) << r.error;
    ASSERT_EQ(r.surfaces.size(), 1u);
}

TEST(SurfaceParserTest, LinesMapToControlPointsRowByRow) {
    // line 4·i + j (0-based) is P[i][j]
    const ParseResult r = parseSurfaces(join(surfaceLines()));
    ASSERT_TRUE(r.ok()) << r.error;
    const BezierSurface &s = r.surfaces[0];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            EXPECT_VEC3_EQ(s.controlPoint(i, j),
                           (Vec3{static_cast<float>(kCoords[i]), static_cast<float>(kCoords[j]),
                                 static_cast<float>(10 * i + j)}))
                << "i = " << i << ", j = " << j;
}

TEST(SurfaceParserTest, ParsesSeveralSurfacesInOrder) {
    auto lines = surfaceLines(0);
    for (const int base : {100, 200})
        for (const auto &line : surfaceLines(base))
            lines.push_back(line);

    const ParseResult r = parseSurfaces(join(lines));
    ASSERT_TRUE(r.ok()) << r.error;
    ASSERT_EQ(r.surfaces.size(), 3u);
    EXPECT_FLOAT_EQ(r.surfaces[0].controlPoint(0, 0).z, 0.0f);
    EXPECT_FLOAT_EQ(r.surfaces[1].controlPoint(0, 0).z, 100.0f);
    EXPECT_FLOAT_EQ(r.surfaces[2].controlPoint(3, 3).z, 233.0f);
}

TEST(SurfaceParserTest, TrailingNewlineIsOptional) {
    std::string text = join(surfaceLines());
    text.pop_back(); // remove the final '\n'
    const ParseResult r = parseSurfaces(text);
    ASSERT_TRUE(r.ok()) << r.error;
    EXPECT_EQ(r.surfaces.size(), 1u);
}

TEST(SurfaceParserTest, AcceptsWindowsLineEndings) {
    auto lines = surfaceLines(0);
    for (const auto &line : surfaceLines(100))
        lines.push_back(line);

    const ParseResult r = parseSurfaces(join(lines, "\r\n"));
    ASSERT_TRUE(r.ok()) << r.error;
    ASSERT_EQ(r.surfaces.size(), 2u);
    EXPECT_FLOAT_EQ(r.surfaces[1].controlPoint(3, 3).z, 133.0f); // last number not broken by '\r'
}

TEST(SurfaceParserTest, AcceptsExtraWhitespaceBetweenNumbers) {
    const ParseResult r = parseSurfaces(withLine(5, "  -1\t-1    7.5  "));
    ASSERT_TRUE(r.ok()) << r.error;
    EXPECT_VEC3_EQ(r.surfaces[0].controlPoint(1, 1), (Vec3{-1, -1, 7.5f}));
}

TEST(SurfaceParserTest, AcceptsDecimalAndScientificNotation) {
    const ParseResult r = parseSurfaces(withLine(0, "-2.75 .5 1e-1"));
    ASSERT_TRUE(r.ok()) << r.error;
    EXPECT_VEC3_EQ(r.surfaces[0].controlPoint(0, 0), (Vec3{-2.75f, 0.5f, 0.1f}));
}

TEST(SurfaceParserTest, UsesDotAsDecimalSeparatorRegardlessOfGlobalLocale) {
    // Qt sets the system locale at startup; with a Polish locale strtod/std::stod would expect ','
    // Locale names differ between systems (macOS/Linux vs MSVC); skip if none is installed
    std::optional<std::locale> polish;
    for (const char *name : {"pl_PL.UTF-8", "pl_PL.utf8", "pl_PL", "pl-PL", "Polish_Poland.1250"}) {
        try {
            polish = std::locale(name);
            break;
        } catch (const std::runtime_error &) {}
    }
    if (!polish)
        GTEST_SKIP() << "no Polish locale available on this system";
    const ScopedGlobalLocale scoped(*polish);

    const ParseResult ok = parseSurfaces(withLine(0, "-3 -3 2.5"));
    ASSERT_TRUE(ok.ok()) << ok.error;
    EXPECT_FLOAT_EQ(ok.surfaces[0].controlPoint(0, 0).z, 2.5f);
    EXPECT_TRUE(failsAtLine(withLine(0, "-3 -3 2,5"), 1));
}

// ---------- wrong number of lines ----------

TEST(SurfaceParserTest, RejectsEmptyText) {
    EXPECT_TRUE(failsWith("", "empty"));
}

TEST(SurfaceParserTest, RejectsOnlyNewline) {
    EXPECT_FALSE(parseSurfaces("\n").ok());
}

TEST(SurfaceParserTest, RejectsLineCountNotMultipleOf16) {
    auto lines = surfaceLines();
    lines.pop_back();
    EXPECT_TRUE(failsWith(join(lines), "15 lines")) << "15 lines";

    lines = surfaceLines();
    lines.push_back("0 0 0");
    EXPECT_TRUE(failsWith(join(lines), "17 lines")) << "17 lines";

    lines = surfaceLines();
    for (const auto &line : surfaceLines())
        lines.push_back(line);
    lines.pop_back();
    EXPECT_TRUE(failsWith(join(lines), "31 lines")) << "31 lines";
}

TEST(SurfaceParserTest, RejectsMoreThanOneTrailingNewline) {
    EXPECT_TRUE(failsWith(join(surfaceLines()) + "\n", "17 lines"));
}

// ---------- invalid lines ----------

TEST(SurfaceParserTest, RejectsEmptyLine) {
    auto lines = surfaceLines(0);
    for (const auto &line : surfaceLines(100))
        lines.push_back(line);
    lines[22] = ""; // in the second surface: the complete first one must not be returned either
    EXPECT_TRUE(failsAtLine(join(lines), 23));
    EXPECT_TRUE(failsWith(join(lines), "empty line"));
}

TEST(SurfaceParserTest, RejectsWhitespaceOnlyLine) {
    EXPECT_TRUE(failsAtLine(withLine(3, "   \t"), 4));
}

TEST(SurfaceParserTest, RejectsTooFewNumbers) {
    EXPECT_TRUE(failsAtLine(withLine(5, "-1 -1"), 6));
    EXPECT_TRUE(failsAtLine(withLine(5, "-1"), 6));
}

TEST(SurfaceParserTest, RejectsTooManyNumbers) {
    EXPECT_TRUE(failsAtLine(withLine(5, "-1 -1 2 7"), 6));
}

TEST(SurfaceParserTest, RejectsDecimalComma) {
    EXPECT_TRUE(failsAtLine(withLine(5, "-1 -1 2,4"), 6));
    EXPECT_TRUE(failsAtLine(withLine(5, "-1,5 -1 2"), 6));
}

TEST(SurfaceParserTest, RejectsText) {
    EXPECT_TRUE(failsAtLine(withLine(5, "-1 -1 abc"), 6));
    EXPECT_TRUE(failsAtLine(withLine(5, "x y z"), 6));
}

TEST(SurfaceParserTest, RejectsGarbageAfterNumber) {
    EXPECT_TRUE(failsAtLine(withLine(5, "-1 -1 2abc"), 6));
    EXPECT_TRUE(failsAtLine(withLine(5, "-1 -1 2 # comment"), 6));
}

TEST(SurfaceParserTest, RejectsNonFiniteNumbers) {
    EXPECT_TRUE(failsAtLine(withLine(5, "-1 -1 1e999"), 6));
}

TEST(SurfaceParserTest, ErrorQuotesOffendingLine) {
    EXPECT_TRUE(failsWith(withLine(5, "-1 -1 2,4"), "\"-1 -1 2,4\""));
}

TEST(SurfaceParserTest, ErrorInLaterSurfaceReportsAbsoluteLineAndReturnsNothing) {
    auto lines = surfaceLines(0);
    for (const auto &line : surfaceLines(100))
        lines.push_back(line);
    lines[19] = "oops";
    EXPECT_TRUE(failsAtLine(join(lines), 20)); // also checks that the valid first surface is not returned
}

// ---------- the real data file shipped with the program ----------

class AssetSurfacesTest : public testing::Test {
protected:
    void SetUp() override {
        std::ifstream file(BEZIER_ASSETS_DIR "/surfaces.txt", std::ios::binary);
        ASSERT_TRUE(file) << "cannot open " << BEZIER_ASSETS_DIR "/surfaces.txt";
        std::stringstream buffer;
        buffer << file.rdbuf();
        result = parseSurfaces(buffer.str());
        ASSERT_TRUE(result.ok()) << result.error;
    }

    ParseResult result;
};

TEST_F(AssetSurfacesTest, ContainsTwoSurfaces) {
    EXPECT_EQ(result.surfaces.size(), 2u);
}

TEST_F(AssetSurfacesTest, ControlPointsLieOnRegularGrid) {
    for (const BezierSurface &s : result.surfaces)
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                const Vec3 &p = s.controlPoint(i, j);
                EXPECT_FLOAT_EQ(p.x, static_cast<float>(kCoords[i])) << "i = " << i << ", j = " << j;
                EXPECT_FLOAT_EQ(p.y, static_cast<float>(kCoords[j])) << "i = " << i << ", j = " << j;
                EXPECT_LE(std::fabs(p.z), 3.0f) << "i = " << i << ", j = " << j;
            }
}

TEST_F(AssetSurfacesTest, NormalsPointUp) {
    for (const BezierSurface &s : result.surfaces)
        for (int a = 0; a <= 10; ++a)
            for (int b = 0; b <= 10; ++b) {
                const float u = static_cast<float>(a) / 10.0f, v = static_cast<float>(b) / 10.0f;
                EXPECT_GT(s.evaluate(u, v).N.z, 0.0f) << "u = " << u << ", v = " << v;
            }
}

TEST_F(AssetSurfacesTest, SurfacesIntersect) {
    // Needed to see the Z-buffer working in stage 5: the dome is above the saddle in the middle, below it at a corner
    ASSERT_EQ(result.surfaces.size(), 2u);
    const BezierSurface &dome = result.surfaces[0], &saddle = result.surfaces[1];
    EXPECT_GT(dome.evaluate(0.5f, 0.5f).P.z, saddle.evaluate(0.5f, 0.5f).P.z);
    EXPECT_LT(dome.evaluate(1.0f, 1.0f).P.z, saddle.evaluate(1.0f, 1.0f).P.z);
}
