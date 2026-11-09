#pragma once

#include <cassert>
#include <vector>

/// Stores the ZBuffer for frame of size width x height
class ZBuffer {
public:
    ZBuffer(const int width, const int height) : _width(width), _height(height) {
        _depth.resize(_width * _height);
    }

    /// Resizes the ZBuffer and resets its content
    void resize(const int width, const int height) {
        assert(width > 0 && height > 0);
        _width = width;
        _height = height;

        _depth.resize(_width * _height);
        clear();
    }

    /// Resets ZBuffer to -inf everywhere
    void clear() {
        std::ranges::fill(_depth, std::numeric_limits<float>::lowest());
    }

    /// Checks if value z is bigger than current value at (x, y),
    /// if it is => sets it
    [[nodiscard]] bool testAndSet(const int x, const int y, const float z) {
        assert(x > 0 && x < _width && y > 0 && y < _height);

        if (const int idx = indexAt(x, y); z > _depth[idx]) {
            _depth[idx] = z;
            return true;
        }

        return false;
    }

    /// Returns ZBuffer value for (x, y)
    [[nodiscard]] float at(const int x, const int y) const {
        assert(x > 0 && x < _width && y > 0 && y < _height);

        return _depth[indexAt(x, y)];
    }

    [[nodiscard]] int width() const { return _width; }
    [[nodiscard]] int height() const { return _height; }

private:
    int _width = 0, _height = 0;
    std::pmr::vector<float> _depth;

    [[nodiscard]] int indexAt(const int x, const int y) const { return y * _width + x; }
};
