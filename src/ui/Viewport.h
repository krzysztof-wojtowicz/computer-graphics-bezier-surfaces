#pragma once

#include <QWidget>
#include <QImage>

class Viewport : public QWidget {
public:
    explicit Viewport(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void render();

    QImage m_frame;
};