// Tests of the side panel in MainWindow (stage 4.7): the controls start with the Viewport's values
// and changing them changes the Viewport.

#include "ui/MainWindow.h"
#include "ui/Viewport.h"

#include "geometry/Mesh.h"

#include <QCheckBox>
#include <QLabel>
#include <QList>
#include <QSlider>
#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

namespace {
/// Viewport has no Q_OBJECT, so findChild<Viewport *> can't tell it from other widgets: use dynamic_cast
Viewport *findViewport(const MainWindow &window) {
    for (QWidget *w : window.findChildren<QWidget *>())
        if (auto *viewport = dynamic_cast<Viewport *>(w))
            return viewport;
    return nullptr;
}

QCheckBox *findCheckBox(const MainWindow &window, const QString &text) {
    for (QCheckBox *box : window.findChildren<QCheckBox *>())
        if (box->text() == text)
            return box;
    return nullptr;
}

/// The label showing the resolution: the only one whose text contains "n ="
QLabel *findResolutionLabel(const MainWindow &window) {
    for (QLabel *label : window.findChildren<QLabel *>())
        if (label->text().contains("n ="))
            return label;
    return nullptr;
}

struct Panel {
    Viewport *viewport;
    QSlider *slider;
    QLabel *label;
    QCheckBox *polygon;
    QCheckBox *mesh;
    QCheckBox *points;
    QCheckBox *fill;
};

Panel findPanel(const MainWindow &window) {
    return {.viewport = findViewport(window),
            .slider = window.findChild<QSlider *>(),
            .label = findResolutionLabel(window),
            .polygon = findCheckBox(window, "Bezier polygon"),
            .mesh = findCheckBox(window, "Triangle mesh"),
            .points = findCheckBox(window, "Mesh points"),
            .fill = findCheckBox(window, "Triangle fill")};
}

void assertPanelFound(const Panel &p) {
    ASSERT_NE(p.viewport, nullptr);
    ASSERT_NE(p.slider, nullptr);
    ASSERT_NE(p.label, nullptr);
    ASSERT_NE(p.polygon, nullptr);
    ASSERT_NE(p.mesh, nullptr);
    ASSERT_NE(p.points, nullptr);
    ASSERT_NE(p.fill, nullptr);
}
} // namespace

TEST(MainWindowPanelTest, LoadsDefaultSurfaces) {
    const MainWindow window;
    const Viewport *viewport = findViewport(window);
    ASSERT_NE(viewport, nullptr);
    EXPECT_FALSE(viewport->meshes().empty());
}

TEST(MainWindowPanelTest, SliderCoversWholeResolutionRange) {
    const MainWindow window;
    const Panel p = findPanel(window);
    ASSERT_NO_FATAL_FAILURE(assertPanelFound(p));
    EXPECT_EQ(p.slider->orientation(), Qt::Horizontal);
    EXPECT_EQ(p.slider->minimum(), Mesh::kMinResolution);
    EXPECT_EQ(p.slider->maximum(), Mesh::kMaxResolution);
}

TEST(MainWindowPanelTest, ControlsStartWithViewportValues) {
    const MainWindow window;
    const Panel p = findPanel(window);
    ASSERT_NO_FATAL_FAILURE(assertPanelFound(p));
    EXPECT_EQ(p.slider->value(), p.viewport->resolution());
    EXPECT_TRUE(p.label->text().contains(QString::number(p.viewport->resolution())));
    EXPECT_EQ(p.polygon->isChecked(), p.viewport->showPolygon());
    EXPECT_EQ(p.mesh->isChecked(), p.viewport->showMesh());
    EXPECT_EQ(p.points->isChecked(), p.viewport->showPoints());
    EXPECT_EQ(p.fill->isChecked(), p.viewport->showFill());
}

TEST(MainWindowPanelTest, SliderChangesResolutionAndLabel) {
    const MainWindow window;
    const Panel p = findPanel(window);
    ASSERT_NO_FATAL_FAILURE(assertPanelFound(p));
    for (const int n : {Mesh::kMinResolution, 23, Mesh::kMaxResolution}) {
        p.slider->setValue(n);
        EXPECT_EQ(p.viewport->resolution(), n);
        for (const Mesh &mesh : p.viewport->meshes())
            EXPECT_EQ(mesh.resolution(), n);
        EXPECT_EQ(p.label->text(), QString("Resolution n = %1").arg(n));
    }
}

TEST(MainWindowPanelTest, EveryCheckBoxTogglesItsOwnLayer) {
    const MainWindow window;
    const Panel p = findPanel(window);
    ASSERT_NO_FATAL_FAILURE(assertPanelFound(p));

    p.polygon->setChecked(false);
    EXPECT_FALSE(p.viewport->showPolygon());
    EXPECT_TRUE(p.viewport->showMesh());
    EXPECT_TRUE(p.viewport->showPoints());

    p.mesh->setChecked(false);
    EXPECT_FALSE(p.viewport->showMesh());
    EXPECT_TRUE(p.viewport->showPoints());

    p.points->setChecked(false);
    EXPECT_FALSE(p.viewport->showPoints());
    EXPECT_TRUE(p.viewport->showFill());

    p.fill->setChecked(false);
    EXPECT_FALSE(p.viewport->showFill());

    p.polygon->setChecked(true);
    p.mesh->setChecked(true);
    p.points->setChecked(true);
    p.fill->setChecked(true);
    EXPECT_TRUE(p.viewport->showPolygon());
    EXPECT_TRUE(p.viewport->showMesh());
    EXPECT_TRUE(p.viewport->showPoints());
    EXPECT_TRUE(p.viewport->showFill());
}
