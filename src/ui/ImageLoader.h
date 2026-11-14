#pragma once

#include "renderer/Texture.h"

#include <QImage>
#include <QString>

#include <array>
#include <optional>

/// Image embedded in the executable (qt_add_resources, BEZIER_ASSET_FILES in CMakeLists.txt)
struct BuiltInImage {
    const char *name; // shown in the list in the side panel
    const char *path; // Qt resource
};

/// Textures for the object color IO; the first one is the default
inline constexpr std::array kBuiltInTextures = {
    BuiltInImage{.name = "Floor", .path = ":/assets/textures/texture_floor.png"},
    BuiltInImage{.name = "Pipes", .path = ":/assets/textures/texture_pipes.png"},
};

/// Normal maps; the first one is the default (pairs with the default texture)
inline constexpr std::array kBuiltInNormalMaps = {
    BuiltInImage{.name = "Floor", .path = ":/assets/normal_maps/normal_floor.png"},
    BuiltInImage{.name = "Bricks", .path = ":/assets/normal_maps/normal_bricks.png"},
    BuiltInImage{.name = "Circle", .path = ":/assets/normal_maps/normal_circle.png"},
    BuiltInImage{.name = "Hemisphere", .path = ":/assets/normal_maps/normal_hemisphere.png"},
    BuiltInImage{.name = "Hex", .path = ":/assets/normal_maps/normal_hex.png"},
    BuiltInImage{.name = "Pipes", .path = ":/assets/normal_maps/normal_pipes.png"},
    BuiltInImage{.name = "Rocks", .path = ":/assets/normal_maps/normal_rocks.png"},
    BuiltInImage{.name = "Waves", .path = ":/assets/normal_maps/normal_waves.png"},
};

/// Result of loading an image: the texture, or an error message for the user
struct TextureLoadResult {
    std::optional<Texture> texture;
    QString error; // empty on success
};

/// Copies the pixels of a (non-null) image into a Texture, so the renderer doesn't depend on Qt. <br>
/// Any format is accepted: alpha is dropped and 16-bit channels are reduced to 8 bits.
Texture textureFromImage(const QImage &image);

/// Reads an image (a regular path or a Qt resource ":/...") as a Texture. <br>
/// Errors (file missing, not an image) are returned in TextureLoadResult::error.
TextureLoadResult loadTexture(const QString &path);
