// Tests of the side panel in MainWindow (stages 4.7 and 6): the controls start with the Viewport's values
// and changing them changes the Viewport.

#include "ui/MainWindow.h"
#include "ui/Viewport.h"

#include "geometry/BezierCurve.h"
#include "geometry/Mesh.h"
#include "renderer/Projection.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QPushButton>
#include <QColorDialog>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QTimer>
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

// ---------- light color (stage 6) ----------

namespace {
/// Answers the modal QColorDialog opened by `open()` once the dialog's event loop is running
template <class OpenFn>
void answerColorDialog(OpenFn &&open, const QColor &expectedStart, const QColor &color, const bool accept) {
    bool answered = false;
    QTimer::singleShot(0, [&] {
        auto *dialog = qobject_cast<QColorDialog *>(QApplication::activeModalWidget());
        ASSERT_NE(dialog, nullptr);
        EXPECT_EQ(dialog->windowTitle(), "Light color");
        EXPECT_EQ(dialog->currentColor(), expectedStart) << "dialog should start from the current light";
        dialog->setCurrentColor(color);
        accept ? dialog->accept() : dialog->reject();
        answered = true;
    });
    open();
    EXPECT_TRUE(answered) << "no modal dialog was opened";
}
} // namespace

TEST(MainWindowLightingTest, MenuActionSetsLightColor) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    auto *action = window.findChild<QAction *>("lightColorAction");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(action, nullptr);
    EXPECT_EQ(action->text(), "Light color...");

    answerColorDialog([action] { action->trigger(); }, viewport->lightColor(), QColor(255, 128, 0), true);
    EXPECT_EQ(viewport->lightColor(), QColor(255, 128, 0));

    // the second time the dialog starts from the color chosen before, not from white
    answerColorDialog([action] { action->trigger(); }, QColor(255, 128, 0), QColor(0, 64, 255), true);
    EXPECT_EQ(viewport->lightColor(), QColor(0, 64, 255));
}

TEST(MainWindowLightingTest, CancellingDialogKeepsLightColor) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    auto *action = window.findChild<QAction *>("lightColorAction");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(action, nullptr);
    const QColor before = viewport->lightColor();

    answerColorDialog([action] { action->trigger(); }, before, QColor(10, 20, 30), false);
    EXPECT_EQ(viewport->lightColor(), before);
}

namespace {
/// Color of the swatch icon on the light color button
QColor swatchColor(const QPushButton &button) {
    return button.icon().pixmap(16, 16).toImage().pixelColor(8, 8);
}
} // namespace

TEST(MainWindowLightingTest, PanelButtonSetsLightColorAndShowsIt) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    auto *button = window.findChild<QPushButton *>("lightColorButton");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text(), "Light color...");
    EXPECT_EQ(swatchColor(*button), viewport->lightColor()) << "initial swatch";

    answerColorDialog([button] { button->click(); }, viewport->lightColor(), QColor(40, 200, 90), true);
    EXPECT_EQ(viewport->lightColor(), QColor(40, 200, 90));
    EXPECT_EQ(swatchColor(*button), QColor(40, 200, 90));

    answerColorDialog([button] { button->click(); }, QColor(40, 200, 90), QColor(1, 2, 3), false);
    EXPECT_EQ(viewport->lightColor(), QColor(40, 200, 90)) << "cancel keeps the color";
    EXPECT_EQ(swatchColor(*button), QColor(40, 200, 90));
}

TEST(MainWindowLightingTest, MenuAndPanelShareTheLightColor) {
    // a color picked in the menu must also show on the panel button, and the button's dialog starts from it
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    auto *action = window.findChild<QAction *>("lightColorAction");
    auto *button = window.findChild<QPushButton *>("lightColorButton");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(action, nullptr);
    ASSERT_NE(button, nullptr);

    answerColorDialog([action] { action->trigger(); }, viewport->lightColor(), QColor(250, 10, 10), true);
    EXPECT_EQ(swatchColor(*button), QColor(250, 10, 10));
    answerColorDialog([button] { button->click(); }, QColor(250, 10, 10), QColor(250, 10, 10), true);
}

// ---------- light animation and path (stage 7) ----------

TEST(MainWindowLightPathTest, LightPathCheckBoxStartsCheckedAndToggles) {
    // a bare Viewport hides the path, the program shows it from the start
    const MainWindow window;
    Viewport *viewport = findViewport(window);
    QCheckBox *box = findCheckBox(window, "Light path");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(box, nullptr);
    EXPECT_TRUE(box->isChecked());
    EXPECT_TRUE(viewport->showLightPath());
    box->setChecked(false);
    EXPECT_FALSE(viewport->showLightPath());
    box->setChecked(true);
    EXPECT_TRUE(viewport->showLightPath());
}

TEST(MainWindowLightPathTest, AnimationRunsFromStartAndButtonToggles) {
    const MainWindow window;
    Viewport *viewport = findViewport(window);
    auto *button = window.findChild<QPushButton *>("animationButton");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(button, nullptr);
    EXPECT_TRUE(button->isCheckable());
    EXPECT_TRUE(viewport->isAnimating());
    EXPECT_TRUE(button->isChecked());
    EXPECT_EQ(button->text(), "Stop animation");

    button->click();
    EXPECT_FALSE(viewport->isAnimating());
    EXPECT_FALSE(button->isChecked());
    EXPECT_EQ(button->text(), "Start animation");

    button->click();
    EXPECT_TRUE(viewport->isAnimating());
    EXPECT_EQ(button->text(), "Stop animation");
}

TEST(MainWindowLightPathTest, ZSliderCoversRangeAndSetsLightZAndLabel) {
    const MainWindow window;
    Viewport *viewport = findViewport(window);
    const LightSlider z = findLightSlider(window, "z");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(z.slider, nullptr);
    ASSERT_NE(z.label, nullptr);
    EXPECT_EQ(z.slider->minimum(), std::lround(Viewport::kMinLightZ * 10.0f));
    EXPECT_EQ(z.slider->maximum(), std::lround(Viewport::kMaxLightZ * 10.0f));
    EXPECT_EQ(z.slider->value(), std::lround(viewport->lightZ() * 10.0f));
    EXPECT_EQ(z.label->text(), QString("z = %1").arg(viewport->lightZ(), 0, 'f', 1));

    z.slider->setValue(125);
    EXPECT_FLOAT_EQ(viewport->lightZ(), 12.5f);
    EXPECT_EQ(z.label->text(), "z = 12.5");
    // the other lighting parameters stay
    const float kd = viewport->kd(), ks = viewport->ks(), m = viewport->m();
    z.slider->setValue(z.slider->minimum());
    z.slider->setValue(z.slider->maximum());
    EXPECT_FLOAT_EQ(viewport->lightZ(), Viewport::kMaxLightZ);
    EXPECT_FLOAT_EQ(viewport->kd(), kd);
    EXPECT_FLOAT_EQ(viewport->ks(), ks);
    EXPECT_FLOAT_EQ(viewport->m(), m);
}

namespace {
void sendMouseTo(Viewport &viewport, const QEvent::Type type, const QPoint &pos, const Qt::MouseButton button,
                 const Qt::MouseButtons buttons) {
    const QPointF local(pos);
    QMouseEvent event(type, local, viewport.mapToGlobal(local), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&viewport, &event);
}
} // namespace

TEST(MainWindowLightPathTest, MenuActionResetsDraggedPath) {
    MainWindow window;
    Viewport *viewport = findViewport(window);
    auto *action = window.findChild<QAction *>("resetLightPathAction");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(action, nullptr);
    EXPECT_EQ(action->text(), "Reset light path");

    // drag handle 1 with the mouse, as the user would
    viewport->resize(600, 600);
    viewport->grab(); // creates the frame, so points can be picked
    const Projection proj(viewport->width(), viewport->height());
    const ScreenPoint sp = proj.project(viewport->lightPath().point(1));
    const QPoint from(static_cast<int>(sp.x), static_cast<int>(sp.y));
    const QPoint to = from + QPoint(-80, 60);
    sendMouseTo(*viewport, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    sendMouseTo(*viewport, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
    sendMouseTo(*viewport, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
    const BezierCurve circle = BezierCurve::circle(Viewport::kDefaultLightPathRadius, viewport->lightZ());
    ASSERT_GT(length(viewport->lightPath().point(1) - circle.point(1)), 0.5f) << "drag didn't move the point";

    action->trigger();
    for (int i = 0; i < circle.size(); ++i) {
        const Vec3 d = viewport->lightPath().point(i) - circle.point(i);
        EXPECT_LT(length(d), 1e-5f) << "point " << i;
    }
}
