#pragma once

#include "geometry/Mesh.h"
#include "math/Mat3.h"
#include "geometry/BezierSurface.h"

#include <QWidget>
#include <QImage>
#include <vector>

class Viewport : public QWidget {
public:
    explicit Viewport(QWidget *parent = nullptr);
    void setSurfaces(std::vector<BezierSurface> &surfaces);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void render();

    QImage m_frame;
    std::vector<Mesh> m_meshes;
    Mat3 m_rotation;
};