#include "ui/ImageLoader.h"

#include <QImageReader>
#include <QRgb>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

Texture textureFromImage(const QImage &image) {
    assert(!image.isNull() && "image must not be null");

    // one known layout (0xffRRGGBB per pixel) instead of handling every QImage format
    const QImage rgb = image.convertToFormat(QImage::Format_RGB32);
    const int width = rgb.width();
    const int height = rgb.height();

    std::vector<std::uint8_t> bytes;
    bytes.reserve(3 * static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        // through scanLine, not bits() + y * width: QImage doesn't promise rows without padding
        const auto *row = reinterpret_cast<const QRgb *>(rgb.constScanLine(y));
        for (int x = 0; x < width; ++x) {
            bytes.push_back(static_cast<std::uint8_t>(qRed(row[x])));
            bytes.push_back(static_cast<std::uint8_t>(qGreen(row[x])));
            bytes.push_back(static_cast<std::uint8_t>(qBlue(row[x])));
        }
    }

    return {width, height, std::move(bytes)};
}

TextureLoadResult loadTexture(const QString &path) {
    // QImageReader instead of QImage(path): it says why reading failed
    QImageReader reader(path);
    const QImage image = reader.read();
    if (image.isNull())
        return {.texture = std::nullopt, .error = "Cannot load image " + path + ": " + reader.errorString()};

    return {.texture = textureFromImage(image), .error = {}};
}
