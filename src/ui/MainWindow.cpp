#include "ui/MainWindow.h"
#include "ui/SurfaceLoader.h"
#include "ui/ImageLoader.h"

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
#include <QVariant>

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
    panel->setFixedWidth(280);
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

    auto *displayGroup = new QGroupBox("Display", panel);
    auto *displayLayout = new QVBoxLayout(displayGroup);
    auto addToggle = [&](const QString &text, bool initial, void (Viewport::*setter)(bool)) {
        auto *box = new QCheckBox(text, displayGroup);
        box->setChecked(initial);
        displayLayout->addWidget(box);
        connect(box, &QCheckBox::toggled, m_viewport, setter);
    };
    addToggle("Bezier polygon", m_viewport->showPolygon(), &Viewport::setShowPolygon);
    addToggle("Triangle mesh",  m_viewport->showMesh(),    &Viewport::setShowMesh);
    addToggle("Mesh points",    m_viewport->showPoints(),  &Viewport::setShowPoints);
    addToggle("Triangle fill", m_viewport->showFill(), &Viewport::setShowFill);
    m_viewport->setShowLightPath(true); // hidden in a bare Viewport, shown in the program from the start
    addToggle("Light path", m_viewport->showLightPath(), &Viewport::setShowLightPath);

    // one button per surface color (surfaces take them in turn), each shows its color as an icon
    auto *colorGroup = new QGroupBox("Surface colors", panel);
    auto *colorLayout = new QVBoxLayout(colorGroup);
    for (int i = 0; i < Viewport::kColorCount; ++i) {
        auto *button = new QPushButton(QString("Color %1...").arg(i + 1), colorGroup);
        button->setObjectName(QString("surfaceColorButton%1").arg(i + 1));
        colorLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i] { chooseSurfaceColor(i); });
        m_surfaceColorButtons[i] = button;
        updateSurfaceColorButton(i);
    }

    // object color IO (surface colors or a texture) and the normal map, independent of each other
    auto *surfaceGroup = new QGroupBox("Surface", panel);
    auto *surfaceLayout = new QVBoxLayout(surfaceGroup);
    auto *solidColorRadio = new QRadioButton("Solid color", surfaceGroup);
    solidColorRadio->setObjectName("solidColorRadio");
    m_textureRadio = new QRadioButton("Texture", surfaceGroup);
    m_textureRadio->setObjectName("textureRadio"); // same parent: the two radio buttons exclude each other
    (m_viewport->useTexture() ? m_textureRadio : solidColorRadio)->setChecked(true);
    connect(m_textureRadio, &QRadioButton::toggled, m_viewport, &Viewport::setUseTexture);

    m_normalMapCheck = new QCheckBox("Normal map", surfaceGroup);
    m_normalMapCheck->setObjectName("normalMapCheck");
    m_normalMapCheck->setChecked(m_viewport->useNormalMap());
    connect(m_normalMapCheck, &QCheckBox::toggled, m_viewport, &Viewport::setUseNormalMap);

    // built-in images in a list (item data = resource path), files loaded by the user are added to it
    const auto addImageCombo = [&](const QString &name, const auto &images) {
        auto *combo = new QComboBox(surfaceGroup);
        combo->setObjectName(name);
        for (const BuiltInImage &image : images)
            combo->addItem(QString::fromUtf8(image.name), QString::fromUtf8(image.path));
        return combo;
    };
    m_textureCombo = addImageCombo("textureCombo", kBuiltInTextures);
    m_normalMapCombo = addImageCombo("normalMapCombo", kBuiltInNormalMaps);
    auto *loadTextureButton = new QPushButton("Load texture...", surfaceGroup);
    loadTextureButton->setObjectName("loadTextureButton");
    auto *loadNormalMapButton = new QPushButton("Load normal map...", surfaceGroup);
    loadNormalMapButton->setObjectName("loadNormalMapButton");

    surfaceLayout->addWidget(solidColorRadio);
    surfaceLayout->addWidget(m_textureRadio);
    surfaceLayout->addWidget(m_textureCombo);
    surfaceLayout->addWidget(loadTextureButton);
    surfaceLayout->addWidget(m_normalMapCheck);
    surfaceLayout->addWidget(m_normalMapCombo);
    surfaceLayout->addWidget(loadNormalMapButton);

    // connected after filling the lists: adding the first item already emits currentIndexChanged
    connect(m_textureCombo, &QComboBox::currentIndexChanged, this, [this](const int i) {
        loadTextureFrom(m_textureCombo->itemData(i).toString());
    });
    connect(m_normalMapCombo, &QComboBox::currentIndexChanged, this, [this](const int i) {
        loadNormalMapFrom(m_normalMapCombo->itemData(i).toString());
    });
    connect(loadTextureButton, &QPushButton::clicked, this, &MainWindow::chooseTextureFile);
    connect(loadNormalMapButton, &QPushButton::clicked, this, &MainWindow::chooseNormalMapFile);

    // QSlider works on int: kd and ks in [0, 1] are stored as hundredths, light z as tenths, m in [1, 100] as is
    auto *lightGroup = new QGroupBox("Lighting", panel);
    auto *lightLayout = new QVBoxLayout(lightGroup);
    auto addSlider = [&](const QString &name, const int min, const int max, const float initial,
                         const float scale, void (Viewport::*setter)(float)) {
        const int decimals = scale >= 1.0f ? 0 : scale >= 0.1f ? 1 : 2;
        auto *label = new QLabel(QString("%1 = %2").arg(name).arg(initial, 0, 'f', decimals), lightGroup);
        auto *slider = new QSlider(Qt::Horizontal, lightGroup);
        slider->setObjectName(name + "Slider");
        slider->setRange(min, max);
        slider->setValue(static_cast<int>(std::lround(initial / scale)));
        lightLayout->addWidget(label);
        lightLayout->addWidget(slider);

        connect(slider, &QSlider::valueChanged, m_viewport, [this, setter, scale](const int value) {
            (m_viewport->*setter)(static_cast<float>(value) * scale);
        });
        connect(slider, &QSlider::valueChanged, label, [label, name, scale, decimals](const int value) {
            label->setText(QString("%1 = %2").arg(name).arg(static_cast<float>(value) * scale, 0, 'f', decimals));
        });
    };
    addSlider("kd", 0, 100, m_viewport->kd(), 0.01f, &Viewport::setKd);
    addSlider("ks", 0, 100, m_viewport->ks(), 0.01f, &Viewport::setKs);
    addSlider("m", 1, 100, m_viewport->m(), 1.0f, &Viewport::setM);
    constexpr float kLightZStep = 0.1f;
    addSlider("z", static_cast<int>(std::lround(Viewport::kMinLightZ / kLightZStep)),
              static_cast<int>(std::lround(Viewport::kMaxLightZ / kLightZStep)),
              m_viewport->lightZ(), kLightZStep, &Viewport::setLightZ);

    // the button shows the current light color as an icon; the same dialog is also in the menu
    m_lightColorButton = new QPushButton("Light color...", lightGroup);
    m_lightColorButton->setObjectName("lightColorButton");
    lightLayout->addWidget(m_lightColorButton);
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
    lightLayout->addWidget(animationButton);
    connect(animationButton, &QPushButton::toggled, m_viewport, &Viewport::setAnimating);
    connect(animationButton, &QPushButton::toggled, animationButton, updateAnimationText);

    panelLayout->addWidget(meshGroup);
    panelLayout->addWidget(displayGroup);
    panelLayout->addWidget(colorGroup);
    panelLayout->addWidget(surfaceGroup);
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

    auto *surfaceMenu = menuBar()->addMenu("Surface");
    auto *textureAction = addAction(surfaceMenu, "Texture", "textureAction", QKeySequence(Qt::Key_T));
    textureAction->setCheckable(true);
    textureAction->setChecked(m_textureRadio->isChecked());
    connect(textureAction, &QAction::toggled, this, [this, solidColorRadio](const bool on) {
        (on ? m_textureRadio : solidColorRadio)->setChecked(true);
    });
    connect(m_textureRadio, &QRadioButton::toggled, textureAction, &QAction::setChecked);
    auto *normalMapAction = addAction(surfaceMenu, "Normal map", "normalMapAction", QKeySequence(Qt::Key_N));
    normalMapAction->setCheckable(true);
    normalMapAction->setChecked(m_normalMapCheck->isChecked());
    connect(normalMapAction, &QAction::toggled, m_normalMapCheck, &QCheckBox::setChecked);
    connect(m_normalMapCheck, &QCheckBox::toggled, normalMapAction, &QAction::setChecked);
    surfaceMenu->addSeparator();
    auto *loadTextureAction = addAction(surfaceMenu, "Load texture...", "loadTextureAction",
                                        QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(loadTextureAction, &QAction::triggered, this, &MainWindow::chooseTextureFile);
    // not Ctrl+M: Cmd+M minimizes the window on macOS
    auto *loadNormalMapAction = addAction(surfaceMenu, "Load normal map...", "loadNormalMapAction",
                                          QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    connect(loadNormalMapAction, &QAction::triggered, this, &MainWindow::chooseNormalMapFile);
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
    auto *resetLightPathAction = addAction(lightingMenu, "Reset light path", "resetLightPathAction",
                                           QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(resetLightPathAction, &QAction::triggered, m_viewport, &Viewport::resetLightPath);

    // Set central widget, window title and window size
    setCentralWidget(central);
    setWindowTitle("Bezier Surfaces");
    resize(1100, 800);

    loadDefaultSurfaces();
    loadDefaultImages();
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
    QPixmap swatch(16, 16);
    swatch.fill(m_viewport->surfaceColor(i));
    m_surfaceColorButtons[i]->setIcon(QIcon(swatch));
}

/// The first built-in images are set in the viewport, but not switched on: the program starts with solid colors
void MainWindow::loadDefaultImages() {
    if (auto texture = loadImage(QString::fromUtf8(kBuiltInTextures.front().path), "texture"))
        m_viewport->setTexture(std::move(*texture));
    if (auto normalMap = loadImage(QString::fromUtf8(kBuiltInNormalMaps.front().path), "normal map"))
        m_viewport->setNormalMap(std::move(*normalMap));
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

bool MainWindow::loadTextureFrom(const QString &path) {
    auto texture = loadImage(path, "texture");
    if (!texture) return false;

    m_viewport->setTexture(std::move(*texture));
    selectComboItem(m_textureCombo, path);
    m_textureRadio->setChecked(true); // a chosen texture is meant to be seen; toggled() switches the viewport
    statusBar()->showMessage(QString("Texture: %1").arg(m_textureCombo->currentText()));
    return true;
}

bool MainWindow::loadNormalMapFrom(const QString &path) {
    auto normalMap = loadImage(path, "normal map");
    if (!normalMap) return false;

    m_viewport->setNormalMap(std::move(*normalMap));
    selectComboItem(m_normalMapCombo, path);
    m_normalMapCheck->setChecked(true);
    statusBar()->showMessage(QString("Normal map: %1").arg(m_normalMapCombo->currentText()));
    return true;
}

namespace {
const QString kImageFilter = QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp);;All files (*)");
} // namespace

void MainWindow::chooseTextureFile() {
    const QString path = QFileDialog::getOpenFileName(this, "Load texture", QString(), kImageFilter);
    if (path.isEmpty()) return; // dialog cancelled
    loadTextureFrom(path);
}

void MainWindow::chooseNormalMapFile() {
    const QString path = QFileDialog::getOpenFileName(this, "Load normal map", QString(), kImageFilter);
    if (path.isEmpty()) return;
    loadNormalMapFrom(path);
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && m_fullScreenAction)
        m_fullScreenAction->setChecked(isFullScreen()); // e.g. the green button on macOS
}
