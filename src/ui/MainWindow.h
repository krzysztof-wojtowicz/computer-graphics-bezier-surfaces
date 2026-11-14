#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QMainWindow>
#include <QPushButton>
#include <QRadioButton>
#include <QString>

#include <array>
#include <optional>
#include <vector>

#include "Viewport.h"
#include "geometry/BezierSurface.h"
#include "renderer/Texture.h"

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr);

    /// Loads an image file (or a resource ":/...") as the texture and switches the object color to it. <br>
    /// On error shows a message and changes nothing. Public: the file dialog itself can't be driven in tests.
    bool loadTextureFrom(const QString &path);
    /// The same for the normal map: loads it and switches the normal map on
    bool loadNormalMapFrom(const QString &path);

private:
    void loadDefaultSurfaces();
    void loadDefaultImages();
    void chooseTextureFile();
    void chooseNormalMapFile();
    [[nodiscard]] std::optional<Texture> loadImage(const QString &path, const QString &what);
    static void selectComboItem(QComboBox *combo, const QString &path);
    void chooseLightColor();
    void updateLightColorButton();
    void chooseSurfaceColor(int i);
    void updateSurfaceColorButton(int i);

    std::vector<BezierSurface> m_surfaces;
    Viewport *m_viewport = nullptr;
    QPushButton *m_lightColorButton = nullptr;
    std::array<QPushButton *, Viewport::kColorCount> m_surfaceColorButtons{};
    QRadioButton *m_textureRadio = nullptr;
    QComboBox *m_textureCombo = nullptr;
    QCheckBox *m_normalMapCheck = nullptr;
    QComboBox *m_normalMapCombo = nullptr;
};
