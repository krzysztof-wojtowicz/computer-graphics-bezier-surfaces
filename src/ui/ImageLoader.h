#pragma once

#include "renderer/Texture.h"

#include <QImage>
#include <QString>

#include <optional>

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
