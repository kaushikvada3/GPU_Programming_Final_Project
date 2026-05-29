#ifndef RT_SCENE_H
#define RT_SCENE_H

#include <string>
#include <vector>

// Plain-C++ scene description shared between the Qt layer and the renderer. No
// CUDA types here on purpose: the GUI edits this freely, and renderer.cu turns
// it into device geometry. (cuda/vec3.cuh pulls in curand, so anything the Qt
// side includes must stay CUDA-free.)

enum class ShapeType { Ground, Box, Sphere, Prism, Pyramid, Cylinder };
enum class MaterialType { Diffuse, Metal, Glass, Emissive };

struct MaterialDesc {
    MaterialType type = MaterialType::Diffuse;
    float color[3]    = {0.73f, 0.73f, 0.73f};
    float fuzz        = 0.05f;   // metal
    float ior         = 1.5f;    // glass
    float emission    = 4.0f;    // emissive multiplier
};

struct SceneObject {
    std::string  name;
    ShapeType    type     = ShapeType::Box;
    float        position[3] = {0.0f, 0.0f, 0.0f};
    float        rotation[3] = {0.0f, 0.0f, 0.0f};  // euler XYZ, degrees
    float        scale[3]    = {1.0f, 1.0f, 1.0f};
    MaterialDesc material;
};

// A movable area light, realized as an emissive sphere in the path tracer.
struct Light {
    float position[3] = {3.0f, 9.0f, 4.0f};
    float color[3]    = {1.0f, 0.95f, 0.85f};
    float intensity   = 6.0f;
    float radius      = 1.2f;
    bool  enabled     = true;
};

struct SceneDesc {
    std::vector<SceneObject> objects;
    Light light;
    float background = 0.4f;   // sky dimming (1 = full sky, 0 = black)
};

const char* shapeName(ShapeType t);

// A reasonable starting object for a freshly-added shape of the given type.
SceneObject makeObject(ShapeType type);

// The default multi-shape demo scene (box / prism / pyramid / glass sphere on a
// ground plane) — mirrors models/gen_scene.py.
SceneDesc defaultScene();

#endif
