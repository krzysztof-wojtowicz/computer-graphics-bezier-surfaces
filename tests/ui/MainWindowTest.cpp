// Tests of the side panel in MainWindow (stages 4.7 and 6): the controls start with the Viewport's values
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

#include <cmath>
#include <tuple>

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

/// The label of a lighting slider, e.g. "kd = 0.50" for name "kd"
QLabel *findLabelStartingWith(const MainWindow &window, const QString &prefix) {
    for (QLabel *label : window.findChildren<QLabel *>())
        if (label->text().startsWith(prefix))
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
            .slider = window.findChild<QSlider *>("resolutionSlider"),
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

// ---------- lighting sliders (stage 6) ----------

namespace {
struct LightSlider {
    QSlider *slider;
    QLabel *label;
};

LightSlider findLightSlider(const MainWindow &window, const QString &name) {
    return {.slider = window.findChild<QSlider *>(name + "Slider"),
            .label = findLabelStartingWith(window, name + " = ")};
}
} // namespace

TEST(MainWindowLightingTest, SlidersCoverParameterRanges) {
    const MainWindow window;
    for (const auto &[name, min, max] : {std::tuple{"kd", 0, 100}, std::tuple{"ks", 0, 100}, std::tuple{"m", 1, 100}}) {
        SCOPED_TRACE(name);
        const LightSlider s = findLightSlider(window, name);
        ASSERT_NE(s.slider, nullptr);
        ASSERT_NE(s.label, nullptr);
        EXPECT_EQ(s.slider->orientation(), Qt::Horizontal);
        EXPECT_EQ(s.slider->minimum(), min);
        EXPECT_EQ(s.slider->maximum(), max);
    }
}

TEST(MainWindowLightingTest, SlidersStartWithViewportValues) {
    const MainWindow window;
    const Viewport *viewport = findViewport(window);
    ASSERT_NE(viewport, nullptr);
    const LightSlider kd = findLightSlider(window, "kd"), ks = findLightSlider(window, "ks"),
                      m = findLightSlider(window, "m");
    ASSERT_TRUE(kd.slider && ks.slider && m.slider && kd.label && ks.label && m.label);
    EXPECT_EQ(kd.slider->value(), std::lround(viewport->kd() * 100.0f));
    EXPECT_EQ(ks.slider->value(), std::lround(viewport->ks() * 100.0f));
    EXPECT_EQ(m.slider->value(), std::lround(viewport->m()));
    EXPECT_EQ(kd.label->text(), QString("kd = %1").arg(viewport->kd(), 0, 'f', 2));
    EXPECT_EQ(ks.label->text(), QString("ks = %1").arg(viewport->ks(), 0, 'f', 2));
    EXPECT_EQ(m.label->text(), QString("m = %1").arg(std::lround(viewport->m())));
}

TEST(MainWindowLightingTest, EverySliderChangesOnlyItsOwnParameterAndLabel) {
    const MainWindow window;
    Viewport *viewport = findViewport(window);
    ASSERT_NE(viewport, nullptr);
    const LightSlider kd = findLightSlider(window, "kd"), ks = findLightSlider(window, "ks"),
                      m = findLightSlider(window, "m");
    ASSERT_TRUE(kd.slider && ks.slider && m.slider && kd.label && ks.label && m.label);

    kd.slider->setValue(30);
    ks.slider->setValue(75);
    m.slider->setValue(7);
    EXPECT_FLOAT_EQ(viewport->kd(), 0.30f);
    EXPECT_FLOAT_EQ(viewport->ks(), 0.75f);
    EXPECT_FLOAT_EQ(viewport->m(), 7.0f);
    EXPECT_EQ(kd.label->text(), "kd = 0.30");
    EXPECT_EQ(ks.label->text(), "ks = 0.75");
    EXPECT_EQ(m.label->text(), "m = 7");

    // ends of the ranges are accepted by the Shading asserts
    for (QSlider *slider : {kd.slider, ks.slider, m.slider}) {
        slider->setValue(slider->minimum());
        slider->setValue(slider->maximum());
    }
    EXPECT_FLOAT_EQ(viewport->kd(), 1.0f);
    EXPECT_FLOAT_EQ(viewport->ks(), 1.0f);
    EXPECT_FLOAT_EQ(viewport->m(), 100.0f);
}
