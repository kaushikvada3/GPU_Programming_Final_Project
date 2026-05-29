#ifndef CUDA_OBJ_LOAD_H
#define CUDA_OBJ_LOAD_H

#include "triangle.cuh"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <limits>
#include <map>

#include "material.cuh"

struct aabb3 {
    point3 min, max;
    point3 center() const { return 0.5f * (min + max); }
    float diagonal() const { return (max - min).length(); }
};

inline void parse_face_vertex(const std::string& tok, int vcount, int ncount,
                              int& vi, int& ni) {
    size_t first = tok.find('/');
    vi = std::stoi(first == std::string::npos ? tok : tok.substr(0, first));
    vi = (vi < 0) ? vcount + vi : vi - 1;

    ni = -1;
    if (first == std::string::npos)
        return;
    size_t second = tok.find('/', first + 1);
    if (second == std::string::npos)
        return;
    std::string ns = tok.substr(second + 1);
    if (ns.empty())
        return;
    ni = std::stoi(ns);
    ni = (ni < 0) ? ncount + ni : ni - 1;
}

inline aabb3 load_obj(const std::string& path, std::vector<triangle>& tris, int mat_idx) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "obj_load: could not open '" << path << "'\n";
        std::exit(1);
    }

    const float inf = std::numeric_limits<float>::infinity();
    std::vector<point3> verts;
    std::vector<vec3> normals;
    aabb3 bounds{ point3(inf, inf, inf), point3(-inf, -inf, -inf) };

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            verts.emplace_back(x, y, z);
            for (int k = 0; k < 3; k++) {
                bounds.min[k] = fminf(bounds.min[k], verts.back()[k]);
                bounds.max[k] = fmaxf(bounds.max[k], verts.back()[k]);
            }
        } else if (tag == "vn") {
            float x, y, z;
            ss >> x >> y >> z;
            normals.emplace_back(x, y, z);
        } else if (tag == "f") {
            std::vector<int> vi, ni;
            std::string tok;
            while (ss >> tok) {
                int a, b;
                parse_face_vertex(tok, int(verts.size()), int(normals.size()), a, b);
                vi.push_back(a);
                ni.push_back(b);
            }
            for (size_t k = 1; k + 1 < vi.size(); k++) {
                bool smooth = !normals.empty()
                           && ni[0] >= 0 && ni[k] >= 0 && ni[k + 1] >= 0;
                if (smooth) {
                    tris.push_back(make_triangle(
                        verts[vi[0]], verts[vi[k]], verts[vi[k + 1]],
                        normals[ni[0]], normals[ni[k]], normals[ni[k + 1]], mat_idx));
                } else {
                    tris.push_back(make_triangle(
                        verts[vi[0]], verts[vi[k]], verts[vi[k + 1]], mat_idx));
                }
            }
        }
    }

    std::clog << "obj_load: " << verts.size() << " verts, "
              << tris.size() << " triangles\n";
    return bounds;
}

// Built-in named palette so a multi-shape OBJ can vary its material per group via
// `usemtl <name>` lines without shipping a separate .mtl file. Unknown names fall
// back to neutral grey.
inline material material_from_name(const std::string& name) {
    if (name == "red")    return material::lambertian(color(0.80f, 0.20f, 0.20f));
    if (name == "green")  return material::lambertian(color(0.20f, 0.70f, 0.30f));
    if (name == "blue")   return material::lambertian(color(0.25f, 0.45f, 0.85f));
    if (name == "yellow") return material::lambertian(color(0.90f, 0.80f, 0.20f));
    if (name == "purple") return material::lambertian(color(0.60f, 0.30f, 0.80f));
    if (name == "grey" || name == "gray")
                          return material::lambertian(color(0.65f, 0.65f, 0.65f));
    if (name == "metal")  return material::make_metal(color(0.85f, 0.85f, 0.88f), 0.05f);
    if (name == "gold")   return material::make_metal(color(0.90f, 0.75f, 0.30f), 0.12f);
    if (name == "glass")  return material::dielectric(1.5f);
    return material::lambertian(color(0.73f, 0.73f, 0.73f));
}

// Like load_obj but also honors `usemtl <name>` groups, appending a material per
// distinct name (resolved through material_from_name) and tagging each triangle
// with its group's index. Materials starts with a default grey at index 0, so an
// OBJ with no usemtl renders identically to the single-material loader.
inline aabb3 load_obj_scene(const std::string& path, std::vector<triangle>& tris,
                            std::vector<material>& materials) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "obj_load: could not open '" << path << "'\n";
        std::exit(1);
    }

    if (materials.empty())
        materials.push_back(material::lambertian(color(0.73f, 0.73f, 0.73f)));

    const float inf = std::numeric_limits<float>::infinity();
    std::vector<point3> verts;
    std::vector<vec3> normals;
    std::map<std::string, int> mat_index;
    int cur_mat = 0;
    aabb3 bounds{ point3(inf, inf, inf), point3(-inf, -inf, -inf) };

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            verts.emplace_back(x, y, z);
            for (int k = 0; k < 3; k++) {
                bounds.min[k] = fminf(bounds.min[k], verts.back()[k]);
                bounds.max[k] = fmaxf(bounds.max[k], verts.back()[k]);
            }
        } else if (tag == "vn") {
            float x, y, z;
            ss >> x >> y >> z;
            normals.emplace_back(x, y, z);
        } else if (tag == "usemtl") {
            std::string name;
            ss >> name;
            auto it = mat_index.find(name);
            if (it == mat_index.end()) {
                cur_mat = int(materials.size());
                materials.push_back(material_from_name(name));
                mat_index[name] = cur_mat;
            } else {
                cur_mat = it->second;
            }
        } else if (tag == "f") {
            std::vector<int> vi, ni;
            std::string tok;
            while (ss >> tok) {
                int a, b;
                parse_face_vertex(tok, int(verts.size()), int(normals.size()), a, b);
                vi.push_back(a);
                ni.push_back(b);
            }
            for (size_t k = 1; k + 1 < vi.size(); k++) {
                bool smooth = !normals.empty()
                           && ni[0] >= 0 && ni[k] >= 0 && ni[k + 1] >= 0;
                if (smooth) {
                    tris.push_back(make_triangle(
                        verts[vi[0]], verts[vi[k]], verts[vi[k + 1]],
                        normals[ni[0]], normals[ni[k]], normals[ni[k + 1]], cur_mat));
                } else {
                    tris.push_back(make_triangle(
                        verts[vi[0]], verts[vi[k]], verts[vi[k + 1]], cur_mat));
                }
            }
        }
    }

    std::clog << "obj_load: " << verts.size() << " verts, " << tris.size()
              << " triangles, " << materials.size() << " materials\n";
    return bounds;
}

#endif
