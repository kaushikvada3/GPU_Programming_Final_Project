#include "scene.h"

const char* shapeName(ShapeType t) {
    switch (t) {
        case ShapeType::Ground:   return "Ground";
        case ShapeType::Box:      return "Box";
        case ShapeType::Sphere:   return "Sphere";
        case ShapeType::Prism:    return "Prism";
        case ShapeType::Pyramid:  return "Pyramid";
        case ShapeType::Cylinder: return "Cylinder";
    }
    return "Object";
}

SceneObject makeObject(ShapeType type) {
    SceneObject o;
    o.type = type;
    o.name = shapeName(type);
    o.scale[0] = o.scale[1] = o.scale[2] = 2.0f;
    o.position[1] = 1.0f;  // rest roughly on the ground
    o.material.type = MaterialType::Diffuse;
    o.material.color[0] = 0.7f; o.material.color[1] = 0.7f; o.material.color[2] = 0.7f;
    return o;
}

static SceneObject obj(const char* name, ShapeType t, MaterialType mt,
                       float px, float py, float pz,
                       float sx, float sy, float sz,
                       float r, float g, float b) {
    SceneObject o;
    o.name = name;
    o.type = t;
    o.position[0] = px; o.position[1] = py; o.position[2] = pz;
    o.scale[0] = sx; o.scale[1] = sy; o.scale[2] = sz;
    o.material.type = mt;
    o.material.color[0] = r; o.material.color[1] = g; o.material.color[2] = b;
    return o;
}

SceneDesc defaultScene() {
    SceneDesc s;
    s.objects.push_back(obj("Ground",  ShapeType::Ground,  MaterialType::Diffuse,
                            0, 0, 0,   18, 1, 12,   0.65f, 0.65f, 0.65f));
    s.objects.push_back(obj("Box",     ShapeType::Box,     MaterialType::Diffuse,
                            -4.5f, 1.1f, 0,  2.2f, 2.2f, 2.2f,  0.80f, 0.20f, 0.20f));
    s.objects.push_back(obj("Prism",   ShapeType::Prism,   MaterialType::Diffuse,
                            -1.5f, 1.3f, 0,  2.4f, 2.6f, 2.2f,  0.20f, 0.70f, 0.30f));
    s.objects.push_back(obj("Pyramid", ShapeType::Pyramid, MaterialType::Metal,
                            1.5f, 1.4f, 0,   2.4f, 2.8f, 2.4f,  0.90f, 0.75f, 0.30f));
    s.objects.push_back(obj("Sphere",  ShapeType::Sphere,  MaterialType::Glass,
                            4.8f, 1.2f, 0,   2.4f, 2.4f, 2.4f,  1.0f, 1.0f, 1.0f));
    return s;
}
