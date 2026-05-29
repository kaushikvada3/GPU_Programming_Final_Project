#ifndef RT_RENDERVIEW_H
#define RT_RENDERVIEW_H

#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QVector3D>

#include "renderer.h"

// Central viewport: shows the latest GPU frame and converts mouse input into
// camera moves (left-drag orbit, middle/right-drag pan, wheel zoom).
class RenderView : public QWidget {
    Q_OBJECT
public:
    explicit RenderView(QWidget* parent = nullptr);

    QSize sizeHint() const override { return QSize(800, 450); }

public slots:
    void onFrame(const QImage& img, int samples);
    void onSceneFramed(const OrbitCamera& cam);
    void setGizmo(float x, float y, float z, bool visible);

signals:
    void cameraChanged(const OrbitCamera& cam);
    void interacting(bool on);
    void pickRequested(float nx, float ny);
    void gizmoTranslated(float dx, float dy, float dz);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    void pan(int dx, int dy);
    QRect imageRect() const;

    // World->screen projection matching the renderer's camera (no OpenGL).
    struct CamBasis { QVector3D from, u, v, w; float tanHalf, aspect; };
    CamBasis camBasis() const;
    bool project(const QVector3D& p, QPointF& out) const;
    int  axisUnderCursor(const QPoint& p) const;   // 0/1/2 or -1
    void drawGizmo(QPainter& g);

    QImage      frame_;
    int         samples_ = 0;
    OrbitCamera cam_;

    bool            dragging_ = false;
    bool            moved_ = false;
    Qt::MouseButton button_ = Qt::NoButton;
    QPoint          press_;
    QPoint          last_;

    QVector3D gizmoPos_;
    bool      gizmoVisible_ = false;
    int       gizmoAxis_ = -1;       // axis being dragged, or -1
};

#endif
