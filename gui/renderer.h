#ifndef RT_RENDERER_H
#define RT_RENDERER_H

#include <cstdint>

#include "scene.h"

// Camera expressed as an orbit around a target point; the renderer turns this
// into the actual view frustum. Kept as plain floats so the Qt layer can pass
// it through signals without dragging in any CUDA types.
struct OrbitCamera {
    float target[3] = {0.0f, 0.0f, 0.0f};
    float azimuth   = 0.0f;   // radians
    float elevation = 0.0f;   // radians
    float dist      = 10.0f;
    float vfov      = 40.0f;  // degrees
};

// Plain-C++ facade over the CUDA path tracer. Every CUDA detail lives in the
// .cu implementation behind a pimpl, so nothing that includes this header needs
// nvcc. Construct and drive it from a single thread (the render thread) — CUDA
// context affinity.
class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void loadSceneOBJ(const char* path);

    // Bake a scene description into device geometry and upload it.
    void setScene(const SceneDesc& scene);

    // Nearest object under a normalized viewport coordinate (nx, ny in [0,1],
    // origin top-left), using the current camera. Returns the object index or -1.
    int pick(float nx, float ny) const;

    // Centroid + bounding-sphere radius of the loaded scene, for auto-framing.
    void sceneBounds(float center[3], float& radius) const;

    void setCamera(const OrbitCamera& cam);
    void resetAccumulation();

    // Reallocate framebuffers for a new output resolution (keeps the scene).
    void resize(int width, int height);

    // Accumulate `spp` more samples into the running average; returns the new
    // total sample count.
    int accumulate(int spp, int max_depth);

    // Gamma-mapped RGB888, width*height*3 bytes. Valid until the next call to
    // accumulate().
    const uint8_t* rgb() const;
    int width() const;
    int height() const;

private:
    struct Impl;
    Impl* impl_;
};

#endif
