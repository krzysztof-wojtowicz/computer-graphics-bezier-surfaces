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

    panelLayout->addWidget(meshGroup);
    panelLayout->addWidget(displayGroup);
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