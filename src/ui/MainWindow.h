#pragma once

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QMainWindow>
#include <QPushButton>
#include <QRadioButton>
#include <QSize>
#include <QString>
#include <QTabWidget>

#include <array>
#include <optional>
#include <vector>

#include "Viewport.h"
#include "ui/HelpWindow.h"
#include "geometry/BezierSurface.h"
#include "renderer/Texture.h"

class MainWindow : public QMainWindow {
public:
    static constexpr int kPanelWidth = 340;
    /// Size of the color swatch on the surface color buttons (the buttons have no text)
    static constexpr QSize kSwatchSize{48, 16};
    /// Gap (px) between the rows of a surface tab: small, so the tabs stay compact
    static constexpr int kRowSpacing = 2;

    explicit MainWindow(QWidget *parent = nullptr);

    /// Loads an image file (or a resource ":/...") as the texture of surface group i and switches its object
    /// color to it. On error shows a message and changes nothing. Public: the file dialog can't be driven in tests.
    bool loadTextureFrom(int i, const QString &path);
    /// The same for the normal map of group i: loads it and switches it on
    bool loadNormalMapFrom(int i, const QString &path);
    /// Surface group whose tab is open: the menu items of "Surface" (texture, normal map, loading) act on it
    [[nodiscard]] int currentSurface() const { return m_surfaceTabs->currentIndex(); }

    /// Opens the documentation (or brings it to the front); one window, created on first use
    void showHelp();
    [[nodiscard]] HelpWindow *helpWindow() const { return m_helpWindow; }

protected:
    /// Keeps the "Full screen" action checked in sync when the window leaves full screen by other means
    void changeEvent(QEvent *event) override;

private:
    /// Controls of one surface group, in its tab of the side panel
    struct SurfaceControls {
        QCheckBox *visible = nullptr;
        QRadioButton *solidColor = nullptr;
        QPushButton *color = nullptr;
        QRadioButton *texture = nullptr;
        QComboBox *textureCombo = nullptr;
        QCheckBox *normalMap = nullptr;
        QComboBox *normalMapCombo = nullptr;
    };

    [[nodiscard]] QWidget *createSurfaceTab(int i);
    void loadDefaultSurfaces();
    void loadDefaultImages();
    void chooseTextureFile(int i);
    void chooseNormalMapFile(int i);
    [[nodiscard]] std::optional<Texture> loadImage(const QString &path, const QString &what);
    static void selectComboItem(QComboBox *combo, const QString &path);
    void chooseLightColor();
    void updateLightColorButton();
    void chooseSurfaceColor(int i);
    void updateSurfaceColorButton(int i);

    std::vector<BezierSurface> m_surfaces;
    Viewport *m_viewport = nullptr;
    QPushButton *m_lightColorButton = nullptr;
    QTabWidget *m_surfaceTabs = nullptr;
    std::array<SurfaceControls, Viewport::kColorCount> m_surfaceControls{};
    QAction *m_fullScreenAction = nullptr;
    HelpWindow *m_helpWindow = nullptr;
};
