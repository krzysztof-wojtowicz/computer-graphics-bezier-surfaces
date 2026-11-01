#include "ui/MainWindow.h"
#include "ui/Viewport.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

/// Creates new MainWindow with the central widget with app content:
/// - Viewport
/// - Side panel with controls
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Create central widget for MainWindow with horizontal layout
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);

    // Create new Viewport widget and add it to the layout
    auto *viewport = new Viewport(central);
    layout->addWidget(viewport, 1);

    // Create side panel with fixed width and vertical layout
    auto *panel = new QWidget(central);
    panel->setFixedWidth(280);
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->addWidget(new QLabel("Controls", panel));
    panelLayout->addStretch();
    layout->addWidget(panel);

    // Set central widget, window title and window size
    setCentralWidget(central);
    setWindowTitle("Bezier Surfaces");
    resize(1100, 800);
}