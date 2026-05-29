#include "renderworker.h"

#include <cmath>

RenderWorker::RenderWorker(int width, int height, SceneDesc initial, QObject* parent)
    : QThread(parent), w_(width), h_(height), pendingScene_(std::move(initial)),
      pendingW_(width), pendingH_(height) {}

void RenderWorker::setCamera(const OrbitCamera& cam) {
    QMutexLocker lk(&mtx_);
    cam_ = cam;
    camDirty_ = true;
}

void RenderWorker::setScene(const SceneDesc& scene) {
    QMutexLocker lk(&mtx_);
    pendingScene_ = scene;
    sceneDirty_ = true;
}

void RenderWorker::setInteracting(bool on) {
    QMutexLocker lk(&mtx_);
    interacting_ = on;
}

void RenderWorker::requestPick(float nx, float ny) {
    QMutexLocker lk(&mtx_);
    pickNx_ = nx;
    pickNy_ = ny;
    pickPending_ = true;
}

void RenderWorker::setMaxDepth(int depth) {
    QMutexLocker lk(&mtx_);
    maxDepth_ = depth;
    camDirty_ = true;   // restart accumulation so the change shows immediately
}

void RenderWorker::setMaxSamples(int samples) {
    QMutexLocker lk(&mtx_);
    maxSamples_ = samples;
}

void RenderWorker::setResolution(int width, int height) {
    QMutexLocker lk(&mtx_);
    pendingW_ = width;
    pendingH_ = height;
    resDirty_ = true;
}

void RenderWorker::stop() {
    {
        QMutexLocker lk(&mtx_);
        running_ = false;
    }
    wait();
}

void RenderWorker::run() {
    Renderer r(w_, h_);

    // Initial scene + auto-frame (3/4 view from above).
    {
        SceneDesc initial;
        {
            QMutexLocker lk(&mtx_);
            initial = pendingScene_;
            sceneDirty_ = false;
        }
        r.setScene(initial);
    }
    float center[3], radius;
    r.sceneBounds(center, radius);
    OrbitCamera cam;
    cam.target[0] = center[0];
    cam.target[1] = center[1];
    cam.target[2] = center[2];
    cam.vfov = 40.0f;
    cam.dist = (radius / std::tan(cam.vfov * 0.5f * float(M_PI) / 180.0f)) * 1.3f;
    {
        float dx = 1.0f, dy = 0.6f, dz = 1.0f;
        float l = std::sqrt(dx*dx + dy*dy + dz*dz);
        cam.azimuth   = std::atan2(dx / l, dz / l);
        cam.elevation = std::asin(dy / l);
    }
    {
        QMutexLocker lk(&mtx_);
        cam_ = cam;
        camDirty_ = true;
    }
    emit sceneFramed(cam);

    int total = 0;
    while (true) {
        OrbitCamera localCam;
        SceneDesc   localScene;
        bool sceneDirty, camDirty, interacting, running, doPick, resDirty;
        float pnx, pny;
        int maxSamples, maxDepth, rw, rh;
        {
            QMutexLocker lk(&mtx_);
            running     = running_;
            localCam    = cam_;
            camDirty    = camDirty_;
            sceneDirty  = sceneDirty_;
            if (sceneDirty) localScene = pendingScene_;
            interacting = interacting_;
            doPick      = pickPending_;
            pnx = pickNx_; pny = pickNy_;
            maxSamples  = maxSamples_;
            maxDepth    = maxDepth_;
            resDirty    = resDirty_;
            rw = pendingW_; rh = pendingH_;
            camDirty_   = false;
            sceneDirty_ = false;
            pickPending_ = false;
            resDirty_   = false;
        }
        if (!running)
            break;

        if (doPick)
            emit objectPicked(r.pick(pnx, pny));

        bool reset = false;
        if (resDirty)   { r.resize(rw, rh); camDirty = true; reset = true; }
        if (sceneDirty) { r.setScene(localScene); reset = true; }
        if (camDirty)   { r.setCamera(localCam);  reset = true; }
        if (reset) { r.resetAccumulation(); total = 0; }

        if (total < maxSamples) {
            int spp = interacting ? 1 : 8;
            total = r.accumulate(spp, maxDepth);
            QImage img(r.rgb(), r.width(), r.height(), r.width() * 3,
                       QImage::Format_RGB888);
            emit frameReady(img.copy(), total);
        } else {
            msleep(10);  // converged: idle without spinning the GPU
        }
    }
}
