#include "ui/MainWindow.h"
#include "ui/SurfaceLoader.h"
#include "ui/ImageLoader.h"

#include <QGridLayout>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QSlider>
#include <QCheckBox>
#include <QGroupBox>
#include <QColorDialog>
#include <QMenuBar>
#include <QEvent>
#include <QList>
#include <QMenu>
#include <QKeySequence>
#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QTabWidget>
#include <QVariant>

#include <cassert>
#include <cmath>
#include <utility>

/// Creates new MainWindow with the central widget with app content:
/// - Viewport
/// - Side panel with controls
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Create central widget for MainWindow with horizontal layout
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);

    // Create new Viewport widget and add it to the layout
    m_viewport = new Viewport(central);
    layout->addWidget(m_viewport, 1);
    m_viewport->setAnimating(true); // the light moves from the start, the button in the panel stops it

    // Create side panel with fixed width and vertical layout
    auto *panel = new QWidget(central);
    panel->setFixedWidth(kPanelWidth);
    auto *panelLayout = new QVBoxLayout(panel);

    auto *meshGroup = new QGroupBox("Triangulation", panel);
    auto *meshLayout = new QVBoxLayout(meshGroup);
    auto *resolutionLabel = new QLabel(meshGroup);
    auto *resolutionSlider = new QSlider(Qt::Horizontal, meshGroup);
    resolutionSlider->setObjectName("resolutionSlider");
    resolutionSlider->setRange(Mesh::kMinResolution, Mesh::kMaxResolution);
    resolutionSlider->setValue(m_viewport->resolution());
    resolutionLabel->setText(QString("Resolution n = %1").arg(m_viewport->resolution()));
    meshLayout->addWidget(resolutionLabel);
    meshLayout->addWidget(resolutionSlider);

    connect(resolutionSlider, &QSlider::valueChanged, m_viewport, &Viewport::setResolution);
    connect(resolutionSlider, &QSlider::valueChanged, resolutionLabel, [resolutionLabel](int n) {
        resolutionLabel->setText(QString("Resolution n = %1").arg(n));
    });

    // two columns of check boxes, filled row by row
    auto *displayGroup = new QGroupBox("Display", panel);
    auto *displayLayout = new QGridLayout(displayGroup);
    int toggleCount = 0;
    auto addToggle = [&](const QString &text, bool initial, void (Viewport::*setter)(bool)) {
        auto *box = new QCheckBox(text, displayGroup);
        box->setChecked(initial);
        displayLayout->addWidget(box, toggleCount / 2, toggleCount % 2);
        ++toggleCount;
        connect(box, &QCheckBox::toggled, m_viewport, setter);
    };
    addToggle("Bezier polygon", m_viewport->showPolygon(), &Viewport::setShowPolygon);
    addToggle("Triangle mesh",  m_viewport->showMesh(),    &Viewport::setShowMesh);
    addToggle("Mesh points",    m_viewport->showPoints(),  &Viewport::setShowPoints);
    addToggle("Triangle fill", m_viewport->showFill(), &Viewport::setShowFill);
    m_viewport->setShowLightPath(true); // hidden in a bare Viewport, shown in the program from the start
    addToggle("Light path", m_viewport->showLightPath(), &Viewport::setShowLightPath);

    // one tab per surface group (surface k is in group k % kColorCount) with everything about its look
    m_surfaceTabs = new QTabWidget(panel);
    m_surfaceTabs->setObjectName("surfaceTabs");
    m_surfaceTabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum); // as tall as a tab, no empty space
    for (int i = 0; i < Viewport::kColorCount; ++i)
        m_surfaceTabs->addTab(createSurfaceTab(i), QString("Surface %1").arg(i + 1));

    // QSlider works on int: kd and ks in [0, 1] are stored as hundredths, light z as tenths, m in [1, 100] as is
    auto *lightGroup = new QGroupBox("Lighting", panel);
    auto *lightLayout = new QVBoxLayout(lightGroup);

    // how the light path is built; both kinds are edited with the mouse, each keeps its own points
    auto *pathTypeRow = new QWidget(lightGroup); // own parent: the two radios exclude only each other
    auto *pathTypeLayout = new QHBoxLayout(pathTypeRow);
    pathTypeLayout->setContentsMargins(0, 0, 0, 0);
    pathTypeLayout->addWidget(new QLabel("Light path:", pathTypeRow));
    auto *bezierPathRadio = new QRadioButton("Bezier", pathTypeRow);
    bezierPathRadio->setObjectName("bezierPathRadio");
    auto *bsplinePathRadio = new QRadioButton("B-spline", pathTypeRow);
    bsplinePathRadio->setObjectName("bsplinePathRadio");
    pathTypeLayout->addWidget(bezierPathRadio);
    pathTypeLayout->addWidget(bsplinePathRadio);
    pathTypeLayout->addStretch();
    lightLayout->addWidget(pathTypeRow);
    (m_viewport->lightPathType() == Viewport::LightPathType::Bezier ? bezierPathRadio : bsplinePathRadio)
        ->setChecked(true);
    connect(bezierPathRadio, &QRadioButton::toggled, m_viewport, [this](const bool bezier) {
        m_viewport->setLightPathType(bezier ? Viewport::LightPathType::Bezier : Viewport::LightPathType::BSpline);
    });

    // one light, or kMaxLightCount lights spread evenly along the same path
    auto *lightCountRow = new QWidget(lightGroup); // own parent: the two radios exclude only each other
    auto *lightCountLayout = new QHBoxLayout(lightCountRow);
    lightCountLayout->setContentsMargins(0, 0, 0, 0);
    lightCountLayout->addWidget(new QLabel("Lights:", lightCountRow));
    auto *oneLightRadio = new QRadioButton("1 light", lightCountRow);
    oneLightRadio->setObjectName("oneLightRadio");
    auto *manyLightsRadio = new QRadioButton(QString("%1 lights").arg(Viewport::kMaxLightCount), lightCountRow);
    manyLightsRadio->setObjectName("manyLightsRadio");
    lightCountLayout->addWidget(oneLightRadio);
    lightCountLayout->addWidget(manyLightsRadio);
    lightCountLayout->addStretch();
    lightLayout->addWidget(lightCountRow);
    (m_viewport->lightCount() == 1 ? oneLightRadio : manyLightsRadio)->setChecked(true);
    connect(manyLightsRadio, &QRadioButton::toggled, m_viewport, [this](const bool many) {
        m_viewport->setLightCount(many ? Viewport::kMaxLightCount : 1);
    });
    auto addSlider = [&](const QString &name, const int min, const int max, const float initial,
                         const float scale, void (Viewport::*setter)(float), QBoxLayout *into) {
        const int decimals = scale >= 1.0f ? 0 : scale >= 0.1f ? 1 : 2;
        auto *label = new QLabel(QString("%1 = %2").arg(name).arg(initial, 0, 'f', decimals), lightGroup);
        auto *slider = new QSlider(Qt::Horizontal, lightGroup);
        slider->setObjectName(name + "Slider");
        slider->setRange(min, max);
        slider->setValue(static_cast<int>(std::lround(initial / scale)));
        into->addWidget(label);
        into->addWidget(slider);

        connect(slider, &QSlider::valueChanged, m_viewport, [this, setter, scale](const int value) {
            (m_viewport->*setter)(static_cast<float>(value) * scale);
        });
        connect(slider, &QSlider::valueChanged, label, [label, name, scale, decimals](const int value) {
            label->setText(QString("%1 = %2").arg(name).arg(static_cast<float>(value) * scale, 0, 'f', decimals));
        });
    };
    addSlider("kd", 0, 100, m_viewport->kd(), 0.01f, &Viewport::setKd, lightLayout);
    addSlider("ks", 0, 100, m_viewport->ks(), 0.01f, &Viewport::setKs, lightLayout);
    addSlider("m", 1, 100, m_viewport->m(), 1.0f, &Viewport::setM, lightLayout);

    // z and spot share a row, half each: one more full-width slider wouldn't fit in the default window
    auto *zSpotRow = new QHBoxLayout();
    auto *zColumn = new QVBoxLayout();
    auto *spotColumn = new QVBoxLayout();
    zSpotRow->addLayout(zColumn, 1);
    zSpotRow->addLayout(spotColumn, 1);
    lightLayout->addLayout(zSpotRow);
    constexpr float kLightZStep = 0.1f;
    addSlider("z", static_cast<int>(std::lround(Viewport::kMinLightZ / kLightZStep)),
              static_cast<int>(std::lround(Viewport::kMaxLightZ / kLightZStep)),
              m_viewport->lightZ(), kLightZStep, &Viewport::setLightZ, zColumn);
    // spotlight beam: I = IL * cos^k(alpha); only the 3 lights are spotlights, so the slider works only for them
    addSlider("spot", static_cast<int>(Shading::kMinSpotExponent), static_cast<int>(Shading::kMaxSpotExponent),
              m_viewport->spotExponent(), 1.0f, &Viewport::setSpotExponent, spotColumn);
    auto *spotSlider = lightGroup->findChild<QSlider *>("spotSlider");
    spotSlider->setEnabled(m_viewport->spotlights());
    connect(manyLightsRadio, &QRadioButton::toggled, spotSlider, &QSlider::setEnabled);

    // the button shows the current light color as an icon; the same dialog is also in the menu
    m_lightColorButton = new QPushButton("Light color...", lightGroup);
    m_lightColorButton->setObjectName("lightColorButton");
    connect(m_lightColorButton, &QPushButton::clicked, this, &MainWindow::chooseLightColor);
    updateLightColorButton();

    // checked = animation running; the text says what a click will do
    auto *animationButton = new QPushButton(lightGroup);
    animationButton->setObjectName("animationButton");
    animationButton->setCheckable(true);
    const auto updateAnimationText = [animationButton](const bool running) {
        animationButton->setText(running ? "Stop animation" : "Start animation");
    };
    animationButton->setChecked(m_viewport->isAnimating());
    updateAnimationText(m_viewport->isAnimating());
    // the two buttons share a row, half of it each
    auto *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(m_lightColorButton, 1);
    buttonRow->addWidget(animationButton, 1);
    lightLayout->addLayout(buttonRow);
    connect(animationButton, &QPushButton::toggled, m_viewport, &Viewport::setAnimating);
    connect(animationButton, &QPushButton::toggled, animationButton, updateAnimationText);

    panelLayout->addWidget(meshGroup);
    panelLayout->addWidget(displayGroup);
    panelLayout->addWidget(m_surfaceTabs);
    panelLayout->addWidget(lightGroup);
    panelLayout->addStretch();
    layout->addWidget(panel);

    // ---------- menus ----------
    // Every action has a keyboard shortcut (the documentation lists them). "Ctrl" is Cmd on macOS.
    // Checkable actions mirror a control in the panel both ways; setChecked() with the same value emits nothing,
    // so the two connections don't loop.
    const auto addAction = [](QMenu *menu, const QString &text, const QString &name, const QKeySequence &shortcut) {
        auto *action = menu->addAction(text);
        action->setObjectName(name);
        action->setShortcut(shortcut);
        return action;
    };

    auto *fileMenu = menuBar()->addMenu("File");
    auto *quitAction = addAction(fileMenu, "Quit", "quitAction", QKeySequence(Qt::CTRL | Qt::Key_Q));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    // not "View": macOS adds its own "Enter Full Screen" items to a menu with that title, and Qt maps the native
    // menu items by position, so the shortcuts of that menu stopped working (Reset rotation, full screen)
    auto *sceneMenu = menuBar()->addMenu("Scene");
    // a single key like T, N and Space (Cmd+0 was easy to confuse with Cmd+O)
    auto *resetRotationAction = addAction(sceneMenu, "Reset rotation", "resetRotationAction", QKeySequence(Qt::Key_R));
    connect(resetRotationAction, &QAction::triggered, m_viewport, &Viewport::resetRotation);
    // the platform's own full screen keys (Ctrl+Cmd+F on macOS, F11 on Windows) and F11 everywhere
    m_fullScreenAction = addAction(sceneMenu, "Full screen", "fullScreenAction", QKeySequence(QKeySequence::FullScreen));
    QList<QKeySequence> fullScreenKeys = QKeySequence::keyBindings(QKeySequence::FullScreen);
    if (!fullScreenKeys.contains(QKeySequence(Qt::Key_F11)))
        fullScreenKeys << QKeySequence(Qt::Key_F11);
    m_fullScreenAction->setShortcuts(fullScreenKeys);
    m_fullScreenAction->setCheckable(true);
    connect(m_fullScreenAction, &QAction::toggled, this, [this](const bool on) {
        if (on == isFullScreen()) return; // the window changed by itself, changeEvent() only updates the check
        on ? showFullScreen() : showNormal();
    });

    // texture, normal map and loading act on the surface of the open tab; the checks show that surface's state
    auto *surfaceMenu = menuBar()->addMenu("Surface");
    auto *textureAction = addAction(surfaceMenu, "Texture", "textureAction", QKeySequence(Qt::Key_T));
    textureAction->setCheckable(true);
    auto *normalMapAction = addAction(surfaceMenu, "Normal map", "normalMapAction", QKeySequence(Qt::Key_N));
    normalMapAction->setCheckable(true);
    // the actions only follow the open tab: a change on a hidden tab doesn't touch them
    const auto showTabState = [this, textureAction, normalMapAction](const int i) {
        textureAction->setChecked(m_surfaceControls[i].texture->isChecked());
        normalMapAction->setChecked(m_surfaceControls[i].normalMap->isChecked());
    };
    showTabState(currentSurface());
    connect(m_surfaceTabs, &QTabWidget::currentChanged, this, showTabState);
    connect(textureAction, &QAction::toggled, this, [this](const bool on) {
        const SurfaceControls &c = m_surfaceControls[currentSurface()];
        (on ? c.texture : c.solidColor)->setChecked(true);
    });
    connect(normalMapAction, &QAction::toggled, this, [this](const bool on) {
        m_surfaceControls[currentSurface()].normalMap->setChecked(on);
    });
    for (int i = 0; i < Viewport::kColorCount; ++i) {
        connect(m_surfaceControls[i].texture, &QRadioButton::toggled, textureAction, [this, i, textureAction](const bool on) {
            if (i == currentSurface()) textureAction->setChecked(on);
        });
        connect(m_surfaceControls[i].normalMap, &QCheckBox::toggled, normalMapAction, [this, i, normalMapAction](const bool on) {
            if (i == currentSurface()) normalMapAction->setChecked(on);
        });
    }
    surfaceMenu->addSeparator();
    auto *loadTextureAction = addAction(surfaceMenu, "Load texture...", "loadTextureAction",
                                        QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(loadTextureAction, &QAction::triggered, this, [this] { chooseTextureFile(currentSurface()); });
    // not Ctrl+M: Cmd+M minimizes the window on macOS
    auto *loadNormalMapAction = addAction(surfaceMenu, "Load normal map...", "loadNormalMapAction",
                                          QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(loadNormalMapAction, &QAction::triggered, this, [this] { chooseNormalMapFile(currentSurface()); });
    surfaceMenu->addSeparator();
    // the solid color is "chosen from the menu" (task); the buttons in the panel open the same dialog
    for (int i = 0; i < Viewport::kColorCount; ++i) {
        auto *colorAction = addAction(surfaceMenu, QString("Surface color %1...").arg(i + 1),
                                      QString("surfaceColorAction%1").arg(i + 1),
                                      QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_1 + i)));
        connect(colorAction, &QAction::triggered, this, [this, i] { chooseSurfaceColor(i); });
    }

    auto *lightingMenu = menuBar()->addMenu("Lighting");
    auto *animationAction = addAction(lightingMenu, "Animate light", "animationAction", QKeySequence(Qt::Key_Space));
    animationAction->setCheckable(true);
    animationAction->setChecked(animationButton->isChecked());
    connect(animationAction, &QAction::toggled, animationButton, &QPushButton::setChecked);
    connect(animationButton, &QPushButton::toggled, animationAction, &QAction::setChecked);
    auto *lightColorAction = addAction(lightingMenu, "Light color...", "lightColorAction",
                                       QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(lightColorAction, &QAction::triggered, this, &MainWindow::chooseLightColor);
    // the spotlights have fixed colors: the chosen color is only for the single light
    lightColorAction->setEnabled(!manyLightsRadio->isChecked());
    m_lightColorButton->setEnabled(!manyLightsRadio->isChecked());
    connect(manyLightsRadio, &QRadioButton::toggled, lightColorAction, [lightColorAction, this](const bool many) {
        lightColorAction->setEnabled(!many);
        m_lightColorButton->setEnabled(!many);
    });
    auto *resetLightPathAction = addAction(lightingMenu, "Reset light path", "resetLightPathAction",
                                           QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(resetLightPathAction, &QAction::triggered, m_viewport, &Viewport::resetLightPath);

    // F1, and the platform's own help key (Cmd+? on macOS) if it is another one
    auto *helpMenu = menuBar()->addMenu("Help");
    auto *helpAction = addAction(helpMenu, "Documentation", "helpAction", QKeySequence(Qt::Key_F1));
    QList<QKeySequence> helpKeys = {QKeySequence(Qt::Key_F1)};
    for (const QKeySequence &key : QKeySequence::keyBindings(QKeySequence::HelpContents))
        if (!helpKeys.contains(key))
            helpKeys << key;
    helpAction->setShortcuts(helpKeys);
    connect(helpAction, &QAction::triggered, this, &MainWindow::showHelp);

    // Set central widget, window title and window size
    setCentralWidget(central);
    setWindowTitle("Bezier Surfaces");
    resize(1300, 800);

    loadDefaultSurfaces();
    loadDefaultImages();
}

/// Tab of surface group i, two columns: what is chosen on the left, its details on the right
/// - [x] Visible
/// - (o) Solid color | [color swatch]
/// - ( ) Texture     | [list] [Load...]
/// - [x] Normal map  | [list] [Load...]
QWidget *MainWindow::createSurfaceTab(const int i) {
    auto *tab = new QWidget();
    auto *grid = new QGridLayout(tab);
    grid->setVerticalSpacing(kRowSpacing);
    grid->setColumnStretch(1, 1);
    const QString n = QString::number(i + 1);
    SurfaceControls &c = m_surfaceControls[i];

    c.visible = new QCheckBox("Visible", tab);
    c.visible->setObjectName("surfaceVisibleCheck" + n);
    c.visible->setToolTip(QString("Show surface %1 (and every %2th surface after it)").arg(n).arg(Viewport::kColorCount));
    c.visible->setChecked(m_viewport->surfaceVisible(i));
    connect(c.visible, &QCheckBox::toggled, m_viewport, [this, i](const bool on) { m_viewport->setSurfaceVisible(i, on); });
    grid->addWidget(c.visible, 0, 0, 1, 2);

    // object color IO: the two radio buttons have the same parent (the tab), so they exclude each other
    c.solidColor = new QRadioButton("Solid color", tab);
    c.solidColor->setObjectName("solidColorRadio" + n);
    c.color = new QPushButton(tab);
    c.color->setObjectName("surfaceColorButton" + n);
    c.color->setToolTip(QString("Surface color %1...").arg(n));
    c.color->setAccessibleName(QString("Surface color %1").arg(n));
    c.color->setIconSize(kSwatchSize);
    connect(c.color, &QPushButton::clicked, this, [this, i] { chooseSurfaceColor(i); });
    grid->addWidget(c.solidColor, 1, 0);
    grid->addWidget(c.color, 1, 1);
    updateSurfaceColorButton(i);

    c.texture = new QRadioButton("Texture", tab);
    c.texture->setObjectName("textureRadio" + n);
    (m_viewport->useTexture(i) ? c.texture : c.solidColor)->setChecked(true);
    connect(c.texture, &QRadioButton::toggled, m_viewport, [this, i](const bool on) { m_viewport->setUseTexture(i, on); });

    c.normalMap = new QCheckBox("Normal map", tab);
    c.normalMap->setObjectName("normalMapCheck" + n);
    c.normalMap->setChecked(m_viewport->useNormalMap(i));
    connect(c.normalMap, &QCheckBox::toggled, m_viewport, [this, i](const bool on) { m_viewport->setUseNormalMap(i, on); });

    // built-in images in a list (item data = resource path), files loaded by the user are added to it
    // object names: e.g. "textureCombo2" and "loadTextureButton2" for name "Texture" on tab 2
    const auto addImageRow = [&](const int row, QWidget *enable, const QString &what, const QString &name,
                                 const auto &images) {
        auto *combo = new QComboBox(tab);
        combo->setObjectName(name.left(1).toLower() + name.mid(1) + "Combo" + n);
        for (const BuiltInImage &image : images)
            combo->addItem(QString::fromUtf8(image.name), QString::fromUtf8(image.path));
        auto *load = new QPushButton("Load...", tab);
        load->setObjectName("load" + name + "Button" + n);
        load->setToolTip(QString("Load %1 from a file").arg(what));

        auto *details = new QHBoxLayout();
        details->addWidget(combo, 2);
        details->addWidget(load, 1);
        grid->addWidget(enable, row, 0);
        grid->addLayout(details, row, 1);
        return std::pair{combo, load};
    };
    const auto [textureCombo, loadTexture] = addImageRow(2, c.texture, "texture", "Texture", kBuiltInTextures);
    const auto [normalMapCombo, loadNormalMap] = addImageRow(3, c.normalMap, "normal map", "NormalMap", kBuiltInNormalMaps);
    c.textureCombo = textureCombo;
    c.normalMapCombo = normalMapCombo;

    // connected after filling the lists: adding the first item already emits currentIndexChanged
    connect(c.textureCombo, &QComboBox::currentIndexChanged, this, [this, i](const int item) {
        loadTextureFrom(i, m_surfaceControls[i].textureCombo->itemData(item).toString());
    });
    connect(c.normalMapCombo, &QComboBox::currentIndexChanged, this, [this, i](const int item) {
        loadNormalMapFrom(i, m_surfaceControls[i].normalMapCombo->itemData(item).toString());
    });
    connect(loadTexture, &QPushButton::clicked, this, [this, i] { chooseTextureFile(i); });
    connect(loadNormalMap, &QPushButton::clicked, this, [this, i] { chooseNormalMapFile(i); });

    return tab;
}

/// Loads the surfaces embedded in the executable. On error shows a message and continues with no surfaces.
void MainWindow::loadDefaultSurfaces() {
    ParseResult result = loadSurfaces(kDefaultSurfacesPath);

    if (!result.ok()) {
        const QString message = QString::fromStdString(result.error);
        statusBar()->showMessage("Failed to load surfaces");
        // Show the message after the event loop starts, so it appears on top of the already visible window
        QTimer::singleShot(0, this, [this, message] {
            QMessageBox::critical(this, "Failed to load surfaces", message);
        });
        return;
    }

    m_surfaces = std::move(result.surfaces);
    statusBar()->showMessage(QString("Loaded %1 surfaces").arg(m_surfaces.size()));

    m_viewport->setSurfaces(m_surfaces);
}

/// Cancelling the dialog returns an invalid color: the light stays as it was then
void MainWindow::chooseLightColor() {
    const QColor color = QColorDialog::getColor(m_viewport->lightColor(), this, "Light color");
    if (!color.isValid()) return;

    m_viewport->setLightColor(color);
    updateLightColorButton();
}

void MainWindow::updateLightColorButton() {
    QPixmap swatch(16, 16);
    swatch.fill(m_viewport->lightColor());
    m_lightColorButton->setIcon(QIcon(swatch));
}

void MainWindow::chooseSurfaceColor(const int i) {
    const QColor color = QColorDialog::getColor(m_viewport->surfaceColor(i), this,
                                                QString("Surface color %1").arg(i + 1));
    if (!color.isValid()) return; // dialog cancelled

    m_viewport->setSurfaceColor(i, color);
    updateSurfaceColorButton(i);
}

void MainWindow::updateSurfaceColorButton(const int i) {
    QPixmap swatch(kSwatchSize);
    swatch.fill(m_viewport->surfaceColor(i));
    m_surfaceControls[i].color->setIcon(QIcon(swatch));
}

/// The first built-in images are set for every surface, but not switched on: the program starts with solid colors.
/// Each file is read once and copied
void MainWindow::loadDefaultImages() {
    if (const auto texture = loadImage(QString::fromUtf8(kBuiltInTextures.front().path), "texture"))
        for (int i = 0; i < Viewport::kColorCount; ++i)
            m_viewport->setTexture(i, *texture);
    if (const auto normalMap = loadImage(QString::fromUtf8(kBuiltInNormalMaps.front().path), "normal map"))
        for (int i = 0; i < Viewport::kColorCount; ++i)
            m_viewport->setNormalMap(i, *normalMap);
}

std::optional<Texture> MainWindow::loadImage(const QString &path, const QString &what) {
    TextureLoadResult result = loadTexture(path);
    if (!result.texture) {
        statusBar()->showMessage(QString("Failed to load %1").arg(what));
        QMessageBox::warning(this, QString("Failed to load %1").arg(what), result.error);
        return std::nullopt;
    }
    return std::move(result.texture);
}

/// Shows which image is in use: the list item with this path, added (named after the file) if it isn't there yet
void MainWindow::selectComboItem(QComboBox *combo, const QString &path) {
    int i = combo->findData(path);
    if (i < 0) {
        combo->addItem(QFileInfo(path).fileName(), path);
        i = combo->count() - 1;
    }
    const QSignalBlocker blocker(combo); // the image is already loaded, don't load it again
    combo->setCurrentIndex(i);
}

bool MainWindow::loadTextureFrom(const int i, const QString &path) {
    assert(i >= 0 && i < Viewport::kColorCount && "surface group index out of range!");
    auto texture = loadImage(path, "texture");
    if (!texture) return false;

    const SurfaceControls &c = m_surfaceControls[i];
    m_viewport->setTexture(i, std::move(*texture));
    selectComboItem(c.textureCombo, path);
    c.texture->setChecked(true); // a chosen texture is meant to be seen; toggled() switches the viewport
    statusBar()->showMessage(QString("Surface %1 texture: %2").arg(i + 1).arg(c.textureCombo->currentText()));
    return true;
}

bool MainWindow::loadNormalMapFrom(const int i, const QString &path) {
    assert(i >= 0 && i < Viewport::kColorCount && "surface group index out of range!");
    auto normalMap = loadImage(path, "normal map");
    if (!normalMap) return false;

    const SurfaceControls &c = m_surfaceControls[i];
    m_viewport->setNormalMap(i, std::move(*normalMap));
    selectComboItem(c.normalMapCombo, path);
    c.normalMap->setChecked(true);
    statusBar()->showMessage(QString("Surface %1 normal map: %2").arg(i + 1).arg(c.normalMapCombo->currentText()));
    return true;
}

namespace {
const QString kImageFilter = QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp);;All files (*)");
} // namespace

void MainWindow::chooseTextureFile(const int i) {
    const QString path = QFileDialog::getOpenFileName(this, QString("Load texture of surface %1").arg(i + 1),
                                                      QString(), kImageFilter);
    if (path.isEmpty()) return; // dialog cancelled
    loadTextureFrom(i, path);
}

void MainWindow::chooseNormalMapFile(const int i) {
    const QString path = QFileDialog::getOpenFileName(this, QString("Load normal map of surface %1").arg(i + 1),
                                                      QString(), kImageFilter);
    if (path.isEmpty()) return;
    loadNormalMapFrom(i, path);
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && m_fullScreenAction)
        m_fullScreenAction->setChecked(isFullScreen()); // e.g. the green button on macOS
}

void MainWindow::showHelp() {
    if (!m_helpWindow)
        m_helpWindow = new HelpWindow(this); // owned by the main window, kept for the next F1
    m_helpWindow->show();
    m_helpWindow->raise();
    m_helpWindow->activateWindow();
}
