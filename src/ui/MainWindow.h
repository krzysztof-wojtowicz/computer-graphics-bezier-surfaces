#pragma once

#include <QMainWindow>

#include <vector>

#include "Viewport.h"
#include "geometry/BezierSurface.h"

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void loadDefaultSurfaces();

    std::vector<BezierSurface> m_surfaces;
    Viewport *m_viewport = nullptr;
};
