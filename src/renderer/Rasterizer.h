#pragma once

#include "renderer/Projection.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <numeric>
#include <span>
#include <vector>

/// Scan-line polygon filling with sorted vertices (no edge table).
///
/// Conventions:
///  - pixel (x, y) is the square [x, x+1) x [y, y+1), sampled at its center (x + 0.5, y + 0.5),
///  - a pixel is filled when its center lies in [yTop, yBottom) and [xLeft, xRight) –
///    half-open on the bottom and right, so two polygons sharing an edge never fill
///    the same pixel twice and never leave a gap between them,
///  - the callback only ever gets x in [0, width) and y in [0, height).
class Rasterizer {
public:
    Rasterizer(const int width, const int height) : _width(width), _height(height) {
        assert(width > 0 && height > 0 && "image size must be > 0");
    }

    /// Fills a polygon given by its vertices in order (the last one connects to the first).
    /// Works for any orientation, also for non-convex polygons (even-odd rule).
    /// pixel(int x, int y) is called once for every filled pixel.
    template <class PixelFn>
    void fillPolygon(std::span<const ScreenPoint> points, PixelFn &&pixel) const {
        const int n = static_cast<int>(points.size());
        if (n < 3) return;
        for ([[maybe_unused]] const auto &p : points) {
            assert(std::isfinite(p.x) && std::isfinite(p.y) && "polygon vertex must be finite");
        }

        // Sort indices, not points: the polygon order is needed to find each vertex's neighbors.
        std::vector<int> order(static_cast<std::size_t>(n));
        std::iota(order.begin(), order.end(), 0);
        std::ranges::sort(order, [&](const int a, const int b) { return points[a].y < points[b].y; });

        const int yStart = std::max(0, static_cast<int>(std::ceil(points[order.front()].y - 0.5f)));
        const int yEnd = std::min(_height, static_cast<int>(std::ceil(points[order.back()].y - 0.5f)));

        std::vector<ActiveEdge> aet;
        std::vector<float> xs;
        int k = 0;

        for (int y = yStart; y < yEnd; ++y) {
            const float yc = static_cast<float>(y) + 0.5f;

            // Every vertex the scan line has reached: an edge going down from it starts here,
            // an edge coming from above ends here. Horizontal edges are never active.
            while (k < n && points[order[k]].y <= yc) {
                const int i = order[k++];
                for (const int j : {(i + n - 1) % n, (i + 1) % n}) {
                    if (points[j].y > points[i].y) {
                        aet.push_back(makeEdge(points[i], points[j], i, j));
                    } else if (points[j].y < points[i].y) {
                        std::erase_if(aet, [&](const ActiveEdge &e) { return e.top == j && e.bottom == i; });
                    }
                }
            }

            // Intersections of the scan line with active edges, filled in pairs.
            xs.clear();
            for (const auto &e : aet) {
                xs.push_back(e.x + (yc - e.y) * e.dxdy);
            }
            std::ranges::sort(xs);
            assert(xs.size() % 2 == 0 && "a closed polygon crosses a scan line an even number of times");

            for (std::size_t m = 0; m + 1 < xs.size(); m += 2) {
                const int xStart = std::max(0, static_cast<int>(std::ceil(xs[m] - 0.5f)));
                const int xEnd = std::min(_width, static_cast<int>(std::ceil(xs[m + 1] - 0.5f)));
                for (int x = xStart; x < xEnd; ++x) {
                    pixel(x, y);
                }
            }
        }
    }

    template <class PixelFn>
    void fillTriangle(const ScreenPoint &a, const ScreenPoint &b, const ScreenPoint &c, PixelFn &&pixel) const {
        const std::array<ScreenPoint, 3> points{a, b, c};
        fillPolygon(points, pixel);
    }

private:
    /// Edge from its upper end (smaller y) down. x at a scan line is always computed
    /// from the upper end, so neighbouring polygons get bit-identical x on a shared edge.
    struct ActiveEdge {
        int top, bottom; // vertex indices, to find the edge again when removing it
        float x, y;      // upper end
        float dxdy;      // inverse slope
    };

    [[nodiscard]] static ActiveEdge makeEdge(const ScreenPoint &upper, const ScreenPoint &lower,
                                             const int top, const int bottom) {
        return {
            .top = top,
            .bottom = bottom,
            .x = upper.x,
            .y = upper.y,
            .dxdy = (lower.x - upper.x) / (lower.y - upper.y)
        };
    }

    int _width, _height;
};
