#include "ui/SurfaceLoader.h"

#include <QByteArray>
#include <QFile>

#include <cstddef>
#include <string_view>

ParseResult loadSurfaces(const QString &path) {
    QFile file(path);
    // No QIODevice::Text: the parser handles "\r\n" itself
    if (!file.open(QIODevice::ReadOnly)) {
        ParseResult result;
        result.error = "Cannot open file " + path.toStdString() + ": " + file.errorString().toStdString();
        return result;
    }

    const QByteArray data = file.readAll();
    return parseSurfaces(std::string_view(data.constData(), data.size()));
}
