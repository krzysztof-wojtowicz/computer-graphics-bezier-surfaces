// Tests of the side panel in MainWindow (stages 4.7 and 6): the controls start with the Viewport's values
// and changing them changes the Viewport.

#include "ui/MainWindow.h"
#include "ui/Viewport.h"
#include "ui/ImageLoader.h"

#include "geometry/BezierCurve.h"
#include "geometry/Mesh.h"
#include "renderer/Projection.h"

#include <QAbstractButton>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QSet>
#include <QTest>
#include <QAction>
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMessageBox>
#include <QRadioButton>
#include <QStringList>
#include <QTemporaryDir>
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

#include <array>
#include <cstddef>
#include <utility>
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
void answerColorDialog(OpenFn &&open, const QColor &expectedStart, const QColor &color, const bool accept,
                       const QString &title = "Light color") {
    bool answered = false;
    QTimer::singleShot(0, [&] {
        auto *dialog = qobject_cast<QColorDialog *>(QApplication::activeModalWidget());
        ASSERT_NE(dialog, nullptr);
        EXPECT_EQ(dialog->windowTitle(), title);
        EXPECT_EQ(dialog->currentColor(), expectedStart) << "dialog should start from the current color";
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
/// Color of the swatch icon on a color button
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

// ---------- surface colors (stage 7) ----------

namespace {
QPushButton *findSurfaceColorButton(const MainWindow &window, const int i) {
    return window.findChild<QPushButton *>(QString("surfaceColorButton%1").arg(i + 1));
}
} // namespace

TEST(MainWindowColorTest, OneButtonPerColorShowingIt) {
    const MainWindow window;
    const Viewport *viewport = findViewport(window);
    ASSERT_NE(viewport, nullptr);
    for (int i = 0; i < Viewport::kColorCount; ++i) {
        SCOPED_TRACE(i);
        QPushButton *button = findSurfaceColorButton(window, i);
        ASSERT_NE(button, nullptr);
        EXPECT_EQ(button->text(), QString("Color %1...").arg(i + 1));
        EXPECT_EQ(swatchColor(*button), viewport->surfaceColor(i));
    }
    EXPECT_EQ(findSurfaceColorButton(window, Viewport::kColorCount), nullptr) << "exactly kColorCount buttons";
}

TEST(MainWindowColorTest, ButtonSetsOnlyItsOwnColor) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    ASSERT_NE(viewport, nullptr);
    for (int i = 0; i < Viewport::kColorCount; ++i) {
        SCOPED_TRACE(i);
        QPushButton *button = findSurfaceColorButton(window, i);
        ASSERT_NE(button, nullptr);
        std::array<QColor, Viewport::kColorCount> before{};
        for (int j = 0; j < Viewport::kColorCount; ++j)
            before[j] = viewport->surfaceColor(j);

        const QColor chosen(30 + 70 * i, 200 - 50 * i, 90);
        answerColorDialog([button] { button->click(); }, before[i], chosen, true,
                          QString("Surface color %1").arg(i + 1));
        EXPECT_EQ(viewport->surfaceColor(i), chosen);
        EXPECT_EQ(swatchColor(*button), chosen);
        for (int j = 0; j < Viewport::kColorCount; ++j)
            if (j != i)
                EXPECT_EQ(viewport->surfaceColor(j), before[j]) << "color " << j << " changed too";
    }
}

TEST(MainWindowColorTest, CancelKeepsColorAndDialogStartsFromLastChoice) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    QPushButton *button = findSurfaceColorButton(window, 1);
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(button, nullptr);
    answerColorDialog([button] { button->click(); }, viewport->surfaceColor(1), QColor(1, 2, 3), true,
                      "Surface color 2");
    answerColorDialog([button] { button->click(); }, QColor(1, 2, 3), QColor(200, 0, 0), false, "Surface color 2");
    EXPECT_EQ(viewport->surfaceColor(1), QColor(1, 2, 3));
    EXPECT_EQ(swatchColor(*button), QColor(1, 2, 3));
}

// ---------- 8: texture and normal map ----------

namespace {
/// Texture and normal map have the same controls; the tests run once for each
struct ImageControls {
    const char *kind;
    QAbstractButton *enable;   // texture radio / normal map check box
    QComboBox *combo;
    QPushButton *loadButton;
    QAction *loadAction;
    bool (MainWindow::*loadFrom)(const QString &);
    bool (Viewport::*inUse)() const;
    const Texture &(Viewport::*image)() const;
    QStringList builtInNames;
    QStringList builtInPaths;
};

template<std::size_t N>
std::pair<QStringList, QStringList> namesAndPaths(const std::array<BuiltInImage, N> &images) {
    QStringList names, paths;
    for (const auto &image : images) {
        names << QString::fromUtf8(image.name);
        paths << QString::fromUtf8(image.path);
    }
    return {names, paths};
}

std::array<ImageControls, 2> findImageControls(const MainWindow &window) {
    const auto [textureNames, texturePaths] = namesAndPaths(kBuiltInTextures);
    const auto [mapNames, mapPaths] = namesAndPaths(kBuiltInNormalMaps);
    return {{
        {"texture", window.findChild<QRadioButton *>("textureRadio"), window.findChild<QComboBox *>("textureCombo"),
         window.findChild<QPushButton *>("loadTextureButton"), window.findChild<QAction *>("loadTextureAction"),
         &MainWindow::loadTextureFrom, &Viewport::useTexture, &Viewport::texture, textureNames, texturePaths},
        {"normal map", window.findChild<QCheckBox *>("normalMapCheck"), window.findChild<QComboBox *>("normalMapCombo"),
         window.findChild<QPushButton *>("loadNormalMapButton"), window.findChild<QAction *>("loadNormalMapAction"),
         &MainWindow::loadNormalMapFrom, &Viewport::useNormalMap, &Viewport::normalMap, mapNames, mapPaths},
    }};
}

void assertControlsFound(const ImageControls &c) {
    ASSERT_NE(c.enable, nullptr) << c.kind;
    ASSERT_NE(c.combo, nullptr) << c.kind;
    ASSERT_NE(c.loadButton, nullptr) << c.kind;
    ASSERT_NE(c.loadAction, nullptr) << c.kind;
}

/// Same size and every texel equal
bool sameTexture(const Texture &a, const Texture &b) {
    if (a.width() != b.width() || a.height() != b.height()) return false;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) {
            const ShadingColor p = a.texel(x, y), q = b.texel(x, y);
            if (p.r != q.r || p.g != q.g || p.b != q.b) return false;
        }
    return true;
}

Texture expectedImage(const QString &path) {
    TextureLoadResult result = loadTexture(path);
    EXPECT_TRUE(result.texture.has_value()) << result.error.toStdString();
    return result.texture.value_or(Texture());
}

/// Accepts the warning shown by `open`, checks that it mentions `mention`
template<class OpenFn>
void answerWarning(OpenFn &&open, const QString &mention) {
    bool answered = false;
    QTimer::singleShot(0, [&] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        ASSERT_NE(box, nullptr);
        EXPECT_TRUE(box->text().contains(mention)) << box->text().toStdString();
        box->accept();
        answered = true;
    });
    open();
    EXPECT_TRUE(answered) << "no warning was shown";
}

/// 3x2 PNG in a temporary directory, different from every built-in image
QString saveTestImage(const QTemporaryDir &dir, const QString &name) {
    QImage image(3, 2, QImage::Format_RGB32);
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 3; ++x)
            image.setPixelColor(x, y, QColor(40 * x, 100 + 50 * y, 200));
    const QString path = dir.filePath(name);
    EXPECT_TRUE(image.save(path));
    return path;
}
} // namespace

TEST(MainWindowTextureTest, StartsWithSolidColorsAndDefaultImagesLoaded) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    ASSERT_NE(viewport, nullptr);
    auto *solid = window.findChild<QRadioButton *>("solidColorRadio");
    ASSERT_NE(solid, nullptr);
    EXPECT_TRUE(solid->isChecked());
    for (const ImageControls &c : findImageControls(window)) {
        SCOPED_TRACE(c.kind);
        assertControlsFound(c);
        EXPECT_FALSE(c.enable->isChecked());
        EXPECT_FALSE((viewport->*c.inUse)());
        // the list holds the built-in images, the first (default) one is selected and already in the viewport
        ASSERT_EQ(c.combo->count(), c.builtInNames.size());
        for (int i = 0; i < c.combo->count(); ++i) {
            EXPECT_EQ(c.combo->itemText(i), c.builtInNames[i]);
            EXPECT_EQ(c.combo->itemData(i).toString(), c.builtInPaths[i]);
        }
        EXPECT_EQ(c.combo->currentIndex(), 0);
        EXPECT_TRUE(sameTexture((viewport->*c.image)(), expectedImage(c.builtInPaths[0])));
    }
}

TEST(MainWindowTextureTest, RadioButtonsSwitchObjectColorSource) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    auto *solid = window.findChild<QRadioButton *>("solidColorRadio");
    auto *texture = window.findChild<QRadioButton *>("textureRadio");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(solid, nullptr);
    ASSERT_NE(texture, nullptr);

    texture->click();
    EXPECT_TRUE(texture->isChecked());
    EXPECT_FALSE(solid->isChecked()) << "the radio buttons must exclude each other";
    EXPECT_TRUE(viewport->useTexture());

    solid->click();
    EXPECT_FALSE(texture->isChecked());
    EXPECT_FALSE(viewport->useTexture());
}

TEST(MainWindowTextureTest, NormalMapCheckBoxIsIndependentOfObjectColor) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    auto *check = window.findChild<QCheckBox *>("normalMapCheck");
    auto *texture = window.findChild<QRadioButton *>("textureRadio");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(check, nullptr);
    ASSERT_NE(texture, nullptr);

    check->click();
    EXPECT_TRUE(viewport->useNormalMap());
    EXPECT_FALSE(viewport->useTexture());
    texture->click();
    EXPECT_TRUE(viewport->useNormalMap()) << "choosing the texture keeps the normal map";
    check->click();
    EXPECT_FALSE(viewport->useNormalMap());
    EXPECT_TRUE(viewport->useTexture()) << "switching the normal map off keeps the texture";
}

TEST(MainWindowTextureTest, ChoosingFromListLoadsImageAndSwitchesItOn) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    ASSERT_NE(viewport, nullptr);
    for (const ImageControls &c : findImageControls(window)) {
        SCOPED_TRACE(c.kind);
        assertControlsFound(c);
        for (int i = c.combo->count() - 1; i >= 0; --i) { // backwards: index 0 is selected already
            c.combo->setCurrentIndex(i);
            EXPECT_TRUE(sameTexture((viewport->*c.image)(), expectedImage(c.builtInPaths[i]))) << "item " << i;
            EXPECT_TRUE(c.enable->isChecked());
            EXPECT_TRUE((viewport->*c.inUse)());
            EXPECT_EQ(c.combo->count(), c.builtInNames.size()) << "a built-in item must not be added again";
        }
    }
}

TEST(MainWindowTextureTest, LoadingFileUsesItAndAddsItToList) {
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    ASSERT_NE(viewport, nullptr);
    for (const ImageControls &c : findImageControls(window)) {
        SCOPED_TRACE(c.kind);
        assertControlsFound(c);
        const QString path = saveTestImage(dir, QString(c.kind).replace(' ', '_') + ".png");
        const int builtIn = static_cast<int>(c.combo->count());

        EXPECT_TRUE((window.*c.loadFrom)(path));
        EXPECT_TRUE(sameTexture((viewport->*c.image)(), expectedImage(path)));
        EXPECT_TRUE(c.enable->isChecked());
        EXPECT_TRUE((viewport->*c.inUse)());
        ASSERT_EQ(c.combo->count(), builtIn + 1);
        EXPECT_EQ(c.combo->currentIndex(), builtIn);
        EXPECT_EQ(c.combo->currentText(), QFileInfo(path).fileName());

        // back to a built-in image and then to the file again from the list
        c.combo->setCurrentIndex(0);
        EXPECT_TRUE(sameTexture((viewport->*c.image)(), expectedImage(c.builtInPaths[0])));
        c.combo->setCurrentIndex(builtIn);
        EXPECT_TRUE(sameTexture((viewport->*c.image)(), expectedImage(path)));

        // loading the same file again selects its item instead of adding another one
        EXPECT_TRUE((window.*c.loadFrom)(path));
        EXPECT_EQ(c.combo->count(), builtIn + 1);
    }
}

TEST(MainWindowTextureTest, FailedLoadShowsWarningAndChangesNothing) {
    const QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString notImage = dir.filePath("not_an_image.png");
    {
        QFile file(notImage);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("text");
    }
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    ASSERT_NE(viewport, nullptr);
    for (const ImageControls &c : findImageControls(window)) {
        SCOPED_TRACE(c.kind);
        assertControlsFound(c);
        c.combo->setCurrentIndex(1); // something else than the start, switched on
        const Texture before = (viewport->*c.image)();
        for (const QString &path : {notImage, dir.filePath("missing.png")}) {
            bool loaded = true;
            answerWarning([&] { loaded = (window.*c.loadFrom)(path); }, path);
            EXPECT_FALSE(loaded) << path.toStdString();
            EXPECT_TRUE(sameTexture((viewport->*c.image)(), before));
            EXPECT_EQ(c.combo->count(), c.builtInNames.size());
            EXPECT_EQ(c.combo->currentIndex(), 1);
            EXPECT_TRUE((viewport->*c.inUse)());
        }
    }
}

TEST(MainWindowTextureTest, LoadButtonsAndMenuActionsExist) {
    MainWindow window;
    for (const ImageControls &c : findImageControls(window)) {
        SCOPED_TRACE(c.kind);
        assertControlsFound(c);
        EXPECT_EQ(c.loadButton->text(), QString("Load %1...").arg(c.kind));
        EXPECT_EQ(c.loadAction->text(), QString("Load %1...").arg(c.kind));
    }
}

TEST(MainWindowTextureTest, TextureStaysChosenWhenSwitchingToSolidColorAndBack) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    auto *solid = window.findChild<QRadioButton *>("solidColorRadio");
    auto *texture = window.findChild<QRadioButton *>("textureRadio");
    auto *combo = window.findChild<QComboBox *>("textureCombo");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(solid, nullptr);
    ASSERT_NE(texture, nullptr);
    ASSERT_NE(combo, nullptr);
    combo->setCurrentIndex(1);
    solid->click();
    texture->click();
    EXPECT_EQ(combo->currentIndex(), 1);
    EXPECT_TRUE(sameTexture(viewport->texture(), expectedImage(QString::fromUtf8(kBuiltInTextures[1].path))));
}

// ---------- 8b.1: surface colors from the menu ----------

TEST(MainWindowColorTest, MenuActionsSetOnlyTheirOwnColorAndSwatch) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    ASSERT_NE(viewport, nullptr);
    for (int i = 0; i < Viewport::kColorCount; ++i) {
        SCOPED_TRACE(i);
        auto *action = window.findChild<QAction *>(QString("surfaceColorAction%1").arg(i + 1));
        QPushButton *button = findSurfaceColorButton(window, i);
        ASSERT_NE(action, nullptr);
        ASSERT_NE(button, nullptr);
        EXPECT_EQ(action->text(), QString("Surface color %1...").arg(i + 1));
        std::array<QColor, Viewport::kColorCount> before{};
        for (int j = 0; j < Viewport::kColorCount; ++j)
            before[j] = viewport->surfaceColor(j);

        const QColor chosen(20 + 80 * i, 180 - 60 * i, 40);
        answerColorDialog([action] { action->trigger(); }, before[i], chosen, true, QString("Surface color %1").arg(i + 1));
        EXPECT_EQ(viewport->surfaceColor(i), chosen);
        EXPECT_EQ(swatchColor(*button), chosen) << "the panel button shows the color chosen in the menu";
        for (int j = 0; j < Viewport::kColorCount; ++j)
            if (j != i)
                EXPECT_EQ(viewport->surfaceColor(j), before[j]) << "color " << j << " changed too";
    }
}

TEST(MainWindowColorTest, MenuAndPanelShareSurfaceColorAndCancelKeepsIt) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    auto *action = window.findChild<QAction *>("surfaceColorAction3");
    QPushButton *button = findSurfaceColorButton(window, 2);
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(action, nullptr);
    ASSERT_NE(button, nullptr);
    answerColorDialog([button] { button->click(); }, viewport->surfaceColor(2), QColor(9, 99, 199), true, "Surface color 3");
    // the menu dialog starts from the color chosen with the button; cancelling keeps it
    answerColorDialog([action] { action->trigger(); }, QColor(9, 99, 199), QColor(255, 0, 0), false, "Surface color 3");
    EXPECT_EQ(viewport->surfaceColor(2), QColor(9, 99, 199));
}

// ---------- 8b.2: keyboard shortcuts ----------

namespace {
/// Every shortcut of the program: action object name -> key sequence ("Ctrl" = Cmd on macOS).
/// A function, not a global: QKeySequence::FullScreen asks the platform theme, which needs the QApplication
std::vector<std::pair<QString, QKeySequence>> shortcuts() {
    return {
    {"quitAction", QKeySequence(Qt::CTRL | Qt::Key_Q)},
    {"resetRotationAction", QKeySequence(Qt::Key_R)},
    {"fullScreenAction", QKeySequence(QKeySequence::FullScreen)},
    {"textureAction", QKeySequence(Qt::Key_T)},
    {"normalMapAction", QKeySequence(Qt::Key_N)},
    {"loadTextureAction", QKeySequence(Qt::CTRL | Qt::Key_T)},
    {"loadNormalMapAction", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N)},
    {"surfaceColorAction1", QKeySequence(Qt::CTRL | Qt::Key_1)},
    {"surfaceColorAction2", QKeySequence(Qt::CTRL | Qt::Key_2)},
    {"surfaceColorAction3", QKeySequence(Qt::CTRL | Qt::Key_3)},
    {"animationAction", QKeySequence(Qt::Key_Space)},
    {"lightColorAction", QKeySequence(Qt::CTRL | Qt::Key_L)},
    {"resetLightPathAction", QKeySequence(Qt::CTRL | Qt::Key_R)},
    };
}

/// Rotates the scene with a mouse drag in the middle of the viewport
void dragScene(Viewport *viewport) {
    const QPoint from = viewport->rect().center(), to = from + QPoint(50, 30);
    QMouseEvent press(QEvent::MouseButtonPress, from, viewport->mapToGlobal(from), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &press);
    QMouseEvent move(QEvent::MouseMove, to, viewport->mapToGlobal(to), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, to, viewport->mapToGlobal(to), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &release);
}

/// Shows the window and makes it active: window shortcuts only work in the active window
bool showActive(MainWindow &window) {
    window.show();
    window.activateWindow();
    return QTest::qWaitForWindowActive(&window);
}
} // namespace

TEST(MainWindowShortcutTest, EveryActionHasItsShortcut) {
    MainWindow window;
    for (const auto &[name, keys] : shortcuts()) {
        auto *action = window.findChild<QAction *>(name);
        ASSERT_NE(action, nullptr) << name.toStdString();
        EXPECT_EQ(action->shortcut(), keys) << name.toStdString();
    }
}

TEST(MainWindowShortcutTest, ShortcutsAreUniqueAndEveryMenuActionHasOne) {
    // a shortcut used twice is ambiguous: Qt then triggers neither action
    MainWindow window;
    QSet<QString> seen;
    for (QAction *action : window.findChildren<QAction *>()) {
        if (action->isSeparator() || action->menu() || action->objectName().isEmpty()) continue;
        const QString keys = action->shortcut().toString();
        EXPECT_FALSE(keys.isEmpty()) << action->objectName().toStdString() << " has no shortcut";
        EXPECT_FALSE(seen.contains(keys)) << keys.toStdString() << " used twice";
        seen.insert(keys);
    }
    EXPECT_EQ(seen.size(), static_cast<qsizetype>(shortcuts().size())) << "shortcuts() must list every action";
}

TEST(MainWindowShortcutTest, AnimationActionAndButtonStayInSync) {
    MainWindow window;
    Viewport *viewport = findViewport(window);
    auto *action = window.findChild<QAction *>("animationAction");
    auto *button = window.findChild<QPushButton *>("animationButton");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(action, nullptr);
    ASSERT_NE(button, nullptr);
    ASSERT_TRUE(action->isChecked());

    action->trigger();
    EXPECT_FALSE(viewport->isAnimating());
    EXPECT_FALSE(button->isChecked());
    EXPECT_EQ(button->text(), "Start animation");

    button->click();
    EXPECT_TRUE(viewport->isAnimating());
    EXPECT_TRUE(action->isChecked());
}

TEST(MainWindowShortcutTest, TextureActionAndRadioButtonsStayInSync) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    auto *action = window.findChild<QAction *>("textureAction");
    auto *solid = window.findChild<QRadioButton *>("solidColorRadio");
    auto *texture = window.findChild<QRadioButton *>("textureRadio");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(action, nullptr);
    ASSERT_NE(solid, nullptr);
    ASSERT_NE(texture, nullptr);
    EXPECT_FALSE(action->isChecked());

    action->trigger();
    EXPECT_TRUE(viewport->useTexture());
    EXPECT_TRUE(texture->isChecked());
    action->trigger();
    EXPECT_FALSE(viewport->useTexture());
    EXPECT_TRUE(solid->isChecked());

    texture->click();
    EXPECT_TRUE(action->isChecked());
    solid->click();
    EXPECT_FALSE(action->isChecked());
    // choosing a texture from the list switches it on: the menu shows that too
    window.findChild<QComboBox *>("textureCombo")->setCurrentIndex(1);
    EXPECT_TRUE(action->isChecked());
}

TEST(MainWindowShortcutTest, NormalMapActionAndCheckBoxStayInSync) {
    MainWindow window;
    const Viewport *viewport = findViewport(window);
    auto *action = window.findChild<QAction *>("normalMapAction");
    auto *check = window.findChild<QCheckBox *>("normalMapCheck");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(action, nullptr);
    ASSERT_NE(check, nullptr);
    EXPECT_FALSE(action->isChecked());

    action->trigger();
    EXPECT_TRUE(viewport->useNormalMap());
    EXPECT_TRUE(check->isChecked());
    check->click();
    EXPECT_FALSE(viewport->useNormalMap());
    EXPECT_FALSE(action->isChecked());
}

TEST(MainWindowShortcutTest, ResetRotationActionRestoresInitialView) {
    MainWindow window;
    Viewport *viewport = findViewport(window);
    auto *action = window.findChild<QAction *>("resetRotationAction");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(action, nullptr);
    window.show();
    QCoreApplication::processEvents();
    const QPoint from = viewport->rect().center();
    QMouseEvent press(QEvent::MouseButtonPress, from, viewport->mapToGlobal(from), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &press);
    const QPoint to = from + QPoint(60, -40);
    QMouseEvent move(QEvent::MouseMove, to, viewport->mapToGlobal(to), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, to, viewport->mapToGlobal(to), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &release);
    ASSERT_NE(viewport->rotation().m, Viewport::initialRotation().m);

    action->trigger();
    EXPECT_EQ(viewport->rotation().m, Viewport::initialRotation().m);
}

TEST(MainWindowShortcutTest, QuitActionClosesWindow) {
    MainWindow window;
    window.show();
    QCoreApplication::processEvents();
    auto *action = window.findChild<QAction *>("quitAction");
    ASSERT_NE(action, nullptr);
    action->trigger();
    EXPECT_FALSE(window.isVisible());
}

TEST(MainWindowShortcutTest, KeysWorkWhateverControlHasFocus) {
    // real key presses through the shortcut map. The focused control must not eat the key:
    // a focused button would "click" on Space (toggling the animation twice = no change), a list would jump on T
    MainWindow window;
    ASSERT_TRUE(showActive(window));
    Viewport *viewport = findViewport(window);
    auto *animationButton = window.findChild<QPushButton *>("animationButton");
    auto *combo = window.findChild<QComboBox *>("textureCombo");
    auto *slider = window.findChild<QSlider *>("kdSlider");
    ASSERT_NE(viewport, nullptr);
    ASSERT_NE(animationButton, nullptr);
    ASSERT_NE(combo, nullptr);
    ASSERT_NE(slider, nullptr);

    for (QWidget *focus : std::initializer_list<QWidget *>{viewport, animationButton, combo, slider}) {
        SCOPED_TRACE(focus->objectName().isEmpty() ? "viewport" : focus->objectName().toStdString());
        focus->setFocus();
        QCoreApplication::processEvents();
        const int comboIndex = combo->currentIndex();
        const int sliderValue = slider->value();

        const bool animating = viewport->isAnimating();
        QTest::keyClick(focus, Qt::Key_Space);
        EXPECT_NE(viewport->isAnimating(), animating) << "Space";

        const bool texture = viewport->useTexture();
        QTest::keyClick(focus, Qt::Key_T);
        EXPECT_NE(viewport->useTexture(), texture) << "T";

        const bool normalMap = viewport->useNormalMap();
        QTest::keyClick(focus, Qt::Key_N);
        EXPECT_NE(viewport->useNormalMap(), normalMap) << "N";

        dragScene(viewport);
        ASSERT_NE(viewport->rotation().m, Viewport::initialRotation().m);
        focus->setFocus(); // the drag may have moved the focus to the viewport
        QTest::keyClick(focus, Qt::Key_R);
        EXPECT_EQ(viewport->rotation().m, Viewport::initialRotation().m) << "R";

        EXPECT_EQ(combo->currentIndex(), comboIndex) << "the list must not react to the letters";
        EXPECT_EQ(slider->value(), sliderValue);
    }
}

TEST(MainWindowShortcutTest, SceneMenuIsNotCalledView) {
    // macOS inserts its own items into a menu titled "View", which broke the shortcuts of that menu
    MainWindow window;
    QStringList titles;
    for (QAction *action : window.menuBar()->actions())
        titles << action->text();
    EXPECT_FALSE(titles.contains("View")) << titles.join(", ").toStdString();
    EXPECT_TRUE(titles.contains("Scene"));
}

TEST(MainWindowShortcutTest, FullScreenHasPlatformKeysAndF11) {
    MainWindow window;
    auto *action = window.findChild<QAction *>("fullScreenAction");
    ASSERT_NE(action, nullptr);
    const QList<QKeySequence> keys = action->shortcuts();
    for (const QKeySequence &platformKey : QKeySequence::keyBindings(QKeySequence::FullScreen))
        EXPECT_TRUE(keys.contains(platformKey)) << platformKey.toString().toStdString();
    EXPECT_TRUE(keys.contains(QKeySequence(Qt::Key_F11)));
}

TEST(MainWindowShortcutTest, FullScreenActionTogglesWindowAndFollowsIt) {
    MainWindow window;
    window.show();
    QCoreApplication::processEvents();
    auto *action = window.findChild<QAction *>("fullScreenAction");
    ASSERT_NE(action, nullptr);
    EXPECT_FALSE(action->isChecked());

    action->trigger();
    EXPECT_TRUE(window.isFullScreen());
    EXPECT_TRUE(action->isChecked());
    action->trigger();
    EXPECT_FALSE(window.isFullScreen());

    // left full screen by other means (e.g. the green button on macOS): the menu check follows
    window.showFullScreen();
    QCoreApplication::processEvents();
    EXPECT_TRUE(action->isChecked());
    window.showNormal();
    QCoreApplication::processEvents();
    EXPECT_FALSE(action->isChecked());
}

TEST(MainWindowShortcutTest, ResetRotationAndFullScreenKeysWork) {
    MainWindow window;
    ASSERT_TRUE(showActive(window));
    Viewport *viewport = findViewport(window);
    ASSERT_NE(viewport, nullptr);
    viewport->setFocus();

    const QPoint from = viewport->rect().center(), to = from + QPoint(50, 30);
    QMouseEvent press(QEvent::MouseButtonPress, from, viewport->mapToGlobal(from), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &press);
    QMouseEvent move(QEvent::MouseMove, to, viewport->mapToGlobal(to), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, to, viewport->mapToGlobal(to), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(viewport, &release);
    ASSERT_NE(viewport->rotation().m, Viewport::initialRotation().m);

    QTest::keyClick(viewport, Qt::Key_R);
    EXPECT_EQ(viewport->rotation().m, Viewport::initialRotation().m) << "R";

    QTest::keyClick(viewport, Qt::Key_F11);
    EXPECT_TRUE(window.isFullScreen()) << "F11";
    QTest::keyClick(viewport, Qt::Key_F11);
    EXPECT_FALSE(window.isFullScreen()) << "F11 again";
}
