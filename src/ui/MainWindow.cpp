#include "ui/MainWindow.h"
#include "ui/SurfaceLoader.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QSlider>
#include <QCheckBox>
#include <QGroupBox>

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

    // QSlider works on int: kd and ks in [0, 1] are stored as hundredths, m in [1, 100] as is
    auto *lightGroup = new QGroupBox("Lighting", panel);
    auto *lightLayout = new QVBoxLayout(lightGroup);
    auto addSlider = [&](const QString &name, const int min, const int max, const float initial,
                         const float scale, void (Viewport::*setter)(float)) {
        const int decimals = scale == 1.0f ? 0 : 2;
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

    panelLayout->addWidget(meshGroup);
    panelLayout->addWidget(displayGroup);
    panelLayout->addWidget(lightGroup);
    panelLayout->addStretch();
    layout->addWidget(panel);

    // Set central widget, window title and window size
    setCentralWidget(central);
    setWindowTitle("Bezier Surfaces");
    resize(1100, 800);

    loadDefaultSurfaces();
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