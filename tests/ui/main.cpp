#include <QApplication>
#include <QtGlobal>

#include <gtest/gtest.h>

// Widgets need a QApplication. The "offscreen" platform renders without a window system,
// so the tests also run in CI (Linux without a display) and don't open windows locally.
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    return RUN_ALL_TESTS();
}
