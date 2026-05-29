#ifndef RT_RENDERWORKER_H
#define RT_RENDERWORKER_H

#include <QThread>
#include <QImage>
#include <QMutex>

#include "renderer.h"
#include "scene.h"

Q_DECLARE_METATYPE(OrbitCamera)

// Owns the Renderer and runs the progressive render loop on its own thread, so
// all CUDA calls stay off the UI thread. The UI pushes camera/scene updates in
// (thread-safe) and receives finished frames + pick results back.
class RenderWorker : public QThread {
    Q_OBJECT
public:
    RenderWorker(int width, int height, SceneDesc initial, QObject* parent = nullptr);

    void setCamera(const OrbitCamera& cam);   // thread-safe; flags a re-render
    void setScene(const SceneDesc& scene);    // thread-safe; re-bakes geometry
    void setInteracting(bool on);             // 1 spp preview vs. refining passes
    void requestPick(float nx, float ny);     // emits objectPicked()
    void setMaxDepth(int depth);
    void setMaxSamples(int samples);
    void setResolution(int width, int height);
    void stop();

signals:
    void frameReady(const QImage& img, int samples);
    void sceneFramed(const OrbitCamera& cam);  // initial auto-framed camera
    void objectPicked(int index);

protected:
    void run() override;

private:
    int w_, h_;

    QMutex      mtx_;
    SceneDesc   pendingScene_;
    bool        sceneDirty_   = true;
    OrbitCamera cam_;
    bool        camDirty_     = true;
    bool        interacting_  = false;
    bool        running_      = true;
    bool        pickPending_  = false;
    float       pickNx_ = 0.0f, pickNy_ = 0.0f;
    int         maxSamples_   = 256;
    int         maxDepth_     = 10;
    bool        resDirty_     = false;
    int         pendingW_, pendingH_;
};

#endif
