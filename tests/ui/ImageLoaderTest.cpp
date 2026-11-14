#include "ui/ImageLoader.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRgba64>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>

namespace {
/// Byte of a channel as the texture returns it (exact: byte / 255 in both places)
float channel(const int byte) { return static_cast<float>(byte) / 255.0f; }

void expectTexel(const Texture &t, const int x, const int y, const QColor &c) {
    const ShadingColor s = t.texel(x, y);
    EXPECT_FLOAT_EQ(s.r, channel(c.red())) << "texel " << x << ", " << y;
    EXPECT_FLOAT_EQ(s.g, channel(c.green())) << "texel " << x << ", " << y;
    EXPECT_FLOAT_EQ(s.b, channel(c.blue())) << "texel " << x << ", " << y;
}

/// Color of pixel (x, y) in the test images: unique per pixel, every channel different
QColor pixelColor(const int x, const int y) {
    return {10 * x + 1, 10 * y + 2, 250 - x - 7 * y};
}

QImage gradientImage(const int width, const int height, const QImage::Format format) {
    QImage image(width, height, format);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            image.setPixelColor(x, y, pixelColor(x, y));
    return image;
}

/// Image files in a subdirectory of the assets (textures/, normal_maps/)
QStringList assetImages(const QString &subdirectory) {
    const QDir dir(QStringLiteral(BEZIER_ASSETS_DIR) + "/" + subdirectory);
    QStringList paths;
    for (const QString &name: dir.entryList(QDir::Files, QDir::Name))
        paths << dir.filePath(name);
    return paths;
}
} // namespace

// ---------- QImage -> Texture ----------

TEST(ImageLoaderTest, CopiesPixelsRowByRowKeepingChannels) {
    // 3x2 is not square: a wrong row stride or swapped axes give other texels
    const Texture t = textureFromImage(gradientImage(3, 2, QImage::Format_RGB32));
    ASSERT_EQ(t.width(), 3);
    ASSERT_EQ(t.height(), 2);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 3; ++x)
            expectTexel(t, x, y, pixelColor(x, y));
}

TEST(ImageLoaderTest, TopRowOfImageIsVZero) {
    QImage image(1, 2, QImage::Format_RGB32);
    image.setPixelColor(0, 0, Qt::red);
    image.setPixelColor(0, 1, Qt::blue);
    const Texture t = textureFromImage(image);
    EXPECT_FLOAT_EQ(t.sample(0.5f, 0.0f).r, 1.0f);
    EXPECT_FLOAT_EQ(t.sample(0.5f, 1.0f).b, 1.0f);
}

TEST(ImageLoaderTest, AcceptsCommonFormats) {
    // PNG files load as RGB888, ARGB32 or 16-bit formats, JPG as RGB32, grayscale maps as Grayscale8
    for (const QImage::Format format: {QImage::Format_RGB32, QImage::Format_ARGB32, QImage::Format_RGB888,
                                       QImage::Format_RGBA8888, QImage::Format_RGBX64, QImage::Format_RGBA64}) {
        SCOPED_TRACE(testing::Message() << "QImage::Format " << static_cast<int>(format));
        const Texture t = textureFromImage(gradientImage(4, 3, format));
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 4; ++x)
                expectTexel(t, x, y, pixelColor(x, y));
    }
}

TEST(ImageLoaderTest, ReducesSixteenBitChannelsToEightBits) {
    QImage image(1, 1, QImage::Format_RGBX64);
    image.setPixelColor(0, 0, QColor::fromRgba64(QRgba64::fromRgba64(0x0000, 0x8080, 0xffff, 0xffff)));
    const ShadingColor c = textureFromImage(image).texel(0, 0);
    EXPECT_FLOAT_EQ(c.r, 0.0f);
    EXPECT_FLOAT_EQ(c.g, channel(0x80));
    EXPECT_FLOAT_EQ(c.b, 1.0f);
}

TEST(ImageLoaderTest, GrayscaleGivesEqualChannels) {
    QImage image(2, 1, QImage::Format_Grayscale8);
    image.setPixelColor(0, 0, QColor(40, 40, 40));
    image.setPixelColor(1, 0, QColor(200, 200, 200));
    const Texture t = textureFromImage(image);
    expectTexel(t, 0, 0, QColor(40, 40, 40));
    expectTexel(t, 1, 0, QColor(200, 200, 200));
}

TEST(ImageLoaderTest, OpaqueAlphaDoesNotChangeColor) {
    QImage image(1, 1, QImage::Format_ARGB32);
    image.setPixelColor(0, 0, QColor(10, 20, 30, 255));
    expectTexel(textureFromImage(image), 0, 0, QColor(10, 20, 30));
}

// ---------- files ----------

TEST(ImageLoaderTest, LoadsSavedPng) {
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("gradient.png");
    ASSERT_TRUE(gradientImage(5, 4, QImage::Format_RGB32).save(path));

    const TextureLoadResult result = loadTexture(path);
    ASSERT_TRUE(result.texture.has_value()) << result.error.toStdString();
    EXPECT_TRUE(result.error.isEmpty());
    ASSERT_EQ(result.texture->width(), 5);
    ASSERT_EQ(result.texture->height(), 4);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 5; ++x)
            expectTexel(*result.texture, x, y, pixelColor(x, y));
}

TEST(ImageLoaderTest, MissingFileGivesErrorWithPath) {
    const QString path = QStringLiteral(BEZIER_ASSETS_DIR) + "/no_such_image.png";
    const TextureLoadResult result = loadTexture(path);
    EXPECT_FALSE(result.texture.has_value());
    EXPECT_TRUE(result.error.contains(path)) << result.error.toStdString();
}

TEST(ImageLoaderTest, NonImageFileGivesError) {
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("not_an_image.png");
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("this is text, not a PNG");
    file.close();

    const TextureLoadResult result = loadTexture(path);
    EXPECT_FALSE(result.texture.has_value());
    EXPECT_FALSE(result.error.isEmpty());
}

TEST(ImageLoaderTest, SurfacesFileIsNotAnImage) {
    const TextureLoadResult result = loadTexture(QStringLiteral(BEZIER_ASSETS_DIR) + "/surfaces.txt");
    EXPECT_FALSE(result.texture.has_value());
    EXPECT_FALSE(result.error.isEmpty());
}

// ---------- the project's images ----------

TEST(ImageLoaderTest, EveryAssetImageLoads) {
    // catches an image added to assets that Qt can't read (e.g. a format without its plugin in the package)
    const QStringList paths = assetImages("textures") + assetImages("normal_maps");
    EXPECT_GE(assetImages("textures").size(), 2);
    EXPECT_GE(assetImages("normal_maps").size(), 8);
    for (const QString &path: paths) {
        const TextureLoadResult result = loadTexture(path);
        ASSERT_TRUE(result.texture.has_value()) << result.error.toStdString();
        const QImage image(path);
        EXPECT_EQ(result.texture->width(), image.width()) << path.toStdString();
        EXPECT_EQ(result.texture->height(), image.height()) << path.toStdString();
    }
}

TEST(ImageLoaderTest, NormalMapsPointOutOfSurfaceOnAverage) {
    // sanity of the data: Blue >= 128 on average (Nz > 0) and no overall tilt.
    // Not checked per texel: normal_bricks and normal_hex have some texels with Blue < 128.
    for (const QString &path: assetImages("normal_maps")) {
        const TextureLoadResult result = loadTexture(path);
        ASSERT_TRUE(result.texture.has_value()) << result.error.toStdString();
        const Texture &map = *result.texture;

        double sx = 0, sy = 0, sz = 0;
        for (int y = 0; y < map.height(); ++y)
            for (int x = 0; x < map.width(); ++x) {
                const Vec3 n = colorToNormal(map.texel(x, y));
                sx += n.x;
                sy += n.y;
                sz += n.z;
            }
        const double count = static_cast<double>(map.width()) * map.height();
        EXPECT_GT(sz / count, 0.5) << path.toStdString();
        EXPECT_LT(std::abs(sx / count), 0.1) << path.toStdString();
        EXPECT_LT(std::abs(sy / count), 0.1) << path.toStdString();
    }
}

// ---------- built-in images (Qt resources) ----------

namespace {
template<std::size_t N>
QStringList resourcePaths(const std::array<BuiltInImage, N> &images) {
    QStringList paths;
    for (const auto &image: images)
        paths << QString::fromUtf8(image.path);
    return paths;
}

template<std::size_t N>
QStringList names(const std::array<BuiltInImage, N> &images) {
    QStringList list;
    for (const auto &image: images)
        list << QString::fromUtf8(image.name);
    return list;
}
} // namespace

TEST(BuiltInImagesTest, NamesAreNonEmptyAndUnique) {
    for (const QStringList &list: {names(kBuiltInTextures), names(kBuiltInNormalMaps)}) {
        for (const QString &name: list)
            EXPECT_FALSE(name.isEmpty());
        EXPECT_EQ(QSet<QString>(list.begin(), list.end()).size(), list.size()) << list.join(", ").toStdString();
    }
}

TEST(BuiltInImagesTest, EveryBuiltInImageLoadsFromResourcesLikeTheFileOnDisk) {
    // a path missing from BEZIER_ASSET_FILES in CMakeLists.txt is not in the resources and fails here
    for (const QString &path: resourcePaths(kBuiltInTextures) + resourcePaths(kBuiltInNormalMaps)) {
        SCOPED_TRACE(path.toStdString());
        ASSERT_TRUE(path.startsWith(":/assets/"));
        const TextureLoadResult result = loadTexture(path);
        ASSERT_TRUE(result.texture.has_value()) << result.error.toStdString();

        // ":/assets/x" is src/assets/x: the same image, not a stale or different one
        const QImage onDisk(QStringLiteral(BEZIER_ASSETS_DIR) + path.mid(QStringLiteral(":/assets").size()));
        ASSERT_FALSE(onDisk.isNull());
        EXPECT_EQ(QImage(path).convertToFormat(QImage::Format_RGB32), onDisk.convertToFormat(QImage::Format_RGB32));
    }
}

TEST(BuiltInImagesTest, EveryAssetImageIsOnTheBuiltInList) {
    // an image added to src/assets but forgotten in kBuiltInTextures / kBuiltInNormalMaps
    const QStringList textures = resourcePaths(kBuiltInTextures);
    for (const QString &path: assetImages("textures"))
        EXPECT_TRUE(textures.contains(":/assets/textures/" + QFileInfo(path).fileName())) << path.toStdString();

    const QStringList normalMaps = resourcePaths(kBuiltInNormalMaps);
    for (const QString &path: assetImages("normal_maps"))
        EXPECT_TRUE(normalMaps.contains(":/assets/normal_maps/" + QFileInfo(path).fileName())) << path.toStdString();
}

TEST(BuiltInImagesTest, ListsHaveNoTexturesAmongNormalMapsAndViceVersa) {
    for (const QString &path: resourcePaths(kBuiltInTextures))
        EXPECT_TRUE(path.startsWith(":/assets/textures/")) << path.toStdString();
    for (const QString &path: resourcePaths(kBuiltInNormalMaps))
        EXPECT_TRUE(path.startsWith(":/assets/normal_maps/")) << path.toStdString();
}

TEST(BuiltInImagesTest, DefaultTextureAndNormalMapArePair) {
    // the first entries are the defaults: the floor texture with its own normal map
    EXPECT_STREQ(kBuiltInTextures.front().path, ":/assets/textures/texture_floor.png");
    EXPECT_STREQ(kBuiltInNormalMaps.front().path, ":/assets/normal_maps/normal_floor.png");
}
