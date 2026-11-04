#pragma once

#include <QString>

#include "geometry/SurfaceParser.h"

/// Path of the default surfaces embedded in the executable (qt_add_resources in CMakeLists.txt)
inline const auto kDefaultSurfacesPath = QStringLiteral(":/assets/surfaces.txt");

/// Reads a text file (a regular path or a Qt resource ":/...") and parses Bezier surfaces from it. <br>
/// Errors (file can't be opened, invalid content) are returned in ParseResult::error.
ParseResult loadSurfaces(const QString &path);
