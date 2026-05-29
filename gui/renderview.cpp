#include "renderview.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVector3D>

#include <cmath>

RenderView::RenderView(QWidget* parent) : QWidget(parent) {
    setMouseTracking(false);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(320, 180);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(20, 20, 24));
    setAutoFillBackground(true);
    setPalette(pal);
}

void RenderView::onFrame(const QImage& img, int samples) {
    frame_ = img;
    samples_ = samples;
    update();
}

void RenderView::onSceneFramed(const OrbitCamera& cam) {
    cam_ = cam;
}

QRect RenderView::imageRect() const {
    if (frame_.isNull())
        return QRect();
    QSize sz = frame_.size().scaled(size(), Qt::KeepAspectRatio);
    return QRect(QPoint((width() - sz.width()) / 2, (height() - sz.height()) / 2), sz);
}

static QVector3D orbitDir(const OrbitCamera& c) {
    return QVector3D(std::cos(c.elevation) * std::sin(c.azimuth),
                     std::sin(c.elevation),
                     std::cos(c.elevation) * std::cos(c.azimuth));
}

RenderView::CamBasis RenderView::camBasis() const {
    CamBasis b;
    QVector3D dir = orbitDir(cam_);
    QVector3D target(cam_.target[0], cam_.target[1], cam_.target[2]);
    b.from = target + cam_.dist * dir;
    b.w = dir.normalized();                                       // toward camera
    b.u = QVector3D::crossProduct(QVector3D(0, 1, 0), b.w).normalized();
    b.v = QVector3D::crossProduct(b.w, b.u);
    b.tanHalf = std::tan(cam_.vfov * 0.5f * float(M_PI) / 180.0f);
    b.aspect = frame_.isNull() ? (16.0f / 9.0f)
                               : float(frame_.width()) / frame_.height();
    return b;
}

bool RenderView::project(const QVector3D& p, QPointF& out) const {
    QRect r = imageRect();
    if (!r.isValid())
        return false;
    CamBasis b = camBasis();
    QVector3D pc = p - b.from;
    float depth = QVector3D::dotProduct(pc, -b.w);
    if (depth <= 1e-3f)
        return false;
    float xc = QVector3D::dotProduct(pc, b.u);
    float yc = QVector3D::dotProduct(pc, b.v);
    float ndcx = xc / (depth * b.tanHalf * b.aspect);
    float ndcy = yc / (depth * b.tanHalf);
    out = QPointF(r.x() + (ndcx * 0.5f + 0.5f) * r.width(),
                  r.y() + (1.0f - (ndcy * 0.5f + 0.5f)) * r.height());
    return true;
}

static QVector3D axisWorld(int k) {
    return QVector3D(k == 0 ? 1.0f : 0.0f, k == 1 ? 1.0f : 0.0f, k == 2 ? 1.0f : 0.0f);
}

static float segDist(const QPointF& p, const QPointF& a, const QPointF& b) {
    QPointF ab = b - a, ap = p - a;
    float len2 = ab.x()*ab.x() + ab.y()*ab.y();
    float t = len2 > 1e-6f ? (ap.x()*ab.x() + ap.y()*ab.y()) / len2 : 0.0f;
    t = qBound(0.0f, t, 1.0f);
    QPointF c = a + t * ab;
    QPointF d = p - c;
    return std::sqrt(d.x()*d.x() + d.y()*d.y());
}

int RenderView::axisUnderCursor(const QPoint& p) const {
    if (!gizmoVisible_)
        return -1;
    QPointF o;
    if (!project(gizmoPos_, o))
        return -1;
    float len = 0.22f * cam_.dist;
    int best = -1;
    float bestd = 9.0f;
    for (int k = 0; k < 3; k++) {
        QPointF tip;
        if (!project(gizmoPos_ + len * axisWorld(k), tip))
            continue;
        float d = segDist(p, o, tip);
        if (d < bestd) { bestd = d; best = k; }
    }
    return best;
}

void RenderView::drawGizmo(QPainter& g) {
    QPointF o;
    if (!gizmoVisible_ || !project(gizmoPos_, o))
        return;
    float len = 0.22f * cam_.dist;
    QColor cols[3] = { QColor(235, 70, 70), QColor(70, 210, 90), QColor(80, 130, 240) };
    for (int k = 0; k < 3; k++) {
        QPointF tip;
        if (!project(gizmoPos_ + len * axisWorld(k), tip))
            continue;
        QColor c = cols[k];
        int w = (k == gizmoAxis_) ? 4 : 2;
        g.setPen(QPen(c, w));
        g.drawLine(o, tip);
        g.setBrush(c);
        g.drawEllipse(tip, 5, 5);
    }
}

void RenderView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 20, 24));
    if (frame_.isNull())
        return;

    p.drawImage(imageRect(), frame_);
    p.setRenderHint(QPainter::Antialiasing, true);
    drawGizmo(p);

    p.setPen(QColor(220, 220, 220));
    p.drawText(8, 18, QString("%1 spp").arg(samples_));
}

void RenderView::setGizmo(float x, float y, float z, bool visible) {
    gizmoPos_ = QVector3D(x, y, z);
    gizmoVisible_ = visible;
    update();
}

void RenderView::mousePressEvent(QMouseEvent* e) {
    dragging_ = true;
    moved_ = false;
    button_ = e->button();
    press_ = e->pos();
    last_ = e->pos();

    // Grab a gizmo axis if the cursor is on one (left button only).
    gizmoAxis_ = (e->button() == Qt::LeftButton) ? axisUnderCursor(e->pos()) : -1;
    if (gizmoAxis_ >= 0)
        update();
    emit interacting(true);
}

void RenderView::mouseMoveEvent(QMouseEvent* e) {
    if (!dragging_)
        return;
    int dx = e->x() - last_.x();
    int dy = e->y() - last_.y();
    last_ = e->pos();
    if ((e->pos() - press_).manhattanLength() > 3)
        moved_ = true;

    if (gizmoAxis_ >= 0) {
        // Translate along the picked world axis by mapping the cursor motion onto
        // the axis' screen projection.
        QVector3D ax = axisWorld(gizmoAxis_);
        QPointF o, tip;
        if (project(gizmoPos_, o) && project(gizmoPos_ + ax, tip)) {
            QPointF sdir = tip - o;
            float plen = std::sqrt(sdir.x()*sdir.x() + sdir.y()*sdir.y());
            if (plen > 1e-3f) {
                QPointF sdh = sdir / plen;       // screen px per 1 world unit
                float along = dx * sdh.x() + dy * sdh.y();   // px along axis
                float world = along / plen;
                emit gizmoTranslated(ax.x() * world, ax.y() * world, ax.z() * world);
            }
        }
        return;
    }

    if (button_ == Qt::LeftButton) {
        cam_.azimuth   -= dx * 0.01f;
        cam_.elevation += dy * 0.01f;
        if (cam_.elevation >  1.5f) cam_.elevation =  1.5f;
        if (cam_.elevation < -1.5f) cam_.elevation = -1.5f;
    } else {
        pan(dx, dy);
    }
    emit cameraChanged(cam_);
}

void RenderView::mouseReleaseEvent(QMouseEvent* e) {
    dragging_ = false;
    emit interacting(false);
    bool wasGizmo = gizmoAxis_ >= 0;
    gizmoAxis_ = -1;
    if (wasGizmo) {
        update();
        button_ = Qt::NoButton;
        return;
    }
    // A click without a drag selects the object under the cursor.
    if (!moved_ && button_ == Qt::LeftButton) {
        QRect r = imageRect();
        if (r.isValid() && r.contains(e->pos())) {
            float nx = float(e->x() - r.x()) / r.width();
            float ny = float(e->y() - r.y()) / r.height();
            emit pickRequested(nx, ny);
        }
    }
    button_ = Qt::NoButton;
}

void RenderView::wheelEvent(QWheelEvent* e) {
    float steps = e->angleDelta().y() / 120.0f;
    cam_.dist *= std::pow(0.9f, steps);
    if (cam_.dist < 1e-3f) cam_.dist = 1e-3f;
    emit cameraChanged(cam_);
}

void RenderView::pan(int dx, int dy) {
    // Move the orbit target in the camera's screen plane.
    QVector3D forward(std::cos(cam_.elevation) * std::sin(cam_.azimuth),
                      std::sin(cam_.elevation),
                      std::cos(cam_.elevation) * std::cos(cam_.azimuth));
    forward = -forward;  // target - lookfrom
    QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 1, 0)).normalized();
    QVector3D up = QVector3D::crossProduct(right, forward).normalized();

    float scale = 0.0015f * cam_.dist;
    QVector3D delta = (-dx * right + dy * up) * scale;
    cam_.target[0] += delta.x();
    cam_.target[1] += delta.y();
    cam_.target[2] += delta.z();
}
