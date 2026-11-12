#pragma once

#include <QMainWindow>
#include <QPushButton>

#include <array>
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
    void chooseSurfaceColor(int i);
    void updateSurfaceColorButton(int i);

    std::vector<BezierSurface> m_surfaces;
    Viewport *m_viewport = nullptr;
    QPushButton *m_lightColorButton = nullptr;
    std::array<QPushButton *, Viewport::kColorCount> m_surfaceColorButtons{};
};
