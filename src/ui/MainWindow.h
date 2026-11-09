#pragma once

#include <QMainWindow>
#include <QPushButton>

#include <vector>

#include "Viewport.h"
#include "geometry/BezierSurface.h"

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void loadDefaultSurfaces();
    void chooseLightColor();
    void updateLightColorButton();

    std::vector<BezierSurface> m_surfaces;
    Viewport *m_viewport = nullptr;
    QPushButton *m_lightColorButton = nullptr;
};
