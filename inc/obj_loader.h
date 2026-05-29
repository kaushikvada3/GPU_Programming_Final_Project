#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "triangle.h"
#include "hittable_list.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct aabb3 {
    point3 min, max;

    point3 center() const { return 0.5 * (min + max); }
    double diagonal() const { return (max - min).length(); }
};

// Parses one face vertex token: "v", "v/vt", "v//vn", or "v/vt/vn".
// Returns 0-based indices; ni == -1 means no normal on this vertex.
// OBJ allows negative (relative-to-end) indices, hence the counts.
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

inline aabb3 load_obj(const std::string& path, hittable_list& world,
                      shared_ptr<material> mat) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "obj_loader: could not open '" << path << "'\n";
        std::exit(1);
    }

    std::vector<point3> verts;
    std::vector<vec3> normals;
    aabb3 bounds{ point3(infinity, infinity, infinity),
                  point3(-infinity, -infinity, -infinity) };

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "v") {
            double x, y, z;
            ss >> x >> y >> z;
            verts.emplace_back(x, y, z);
            for (int k = 0; k < 3; k++) {
                bounds.min[k] = std::fmin(bounds.min[k], verts.back()[k]);
                bounds.max[k] = std::fmax(bounds.max[k], verts.back()[k]);
            }
        } else if (tag == "vn") {
            double x, y, z;
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

            // fan triangulation: quads and n-gons become n-2 triangles
            for (size_t k = 1; k + 1 < vi.size(); k++) {
                bool smooth = !normals.empty()
                           && ni[0] >= 0 && ni[k] >= 0 && ni[k + 1] >= 0;
                if (smooth) {
                    world.add(make_shared<triangle>(
                        verts[vi[0]], verts[vi[k]], verts[vi[k + 1]],
                        normals[ni[0]], normals[ni[k]], normals[ni[k + 1]], mat));
                } else {
                    world.add(make_shared<triangle>(
                        verts[vi[0]], verts[vi[k]], verts[vi[k + 1]], mat));
                }
            }
        }
    }

    std::clog << "obj_loader: " << verts.size() << " verts, "
              << world.objects.size() << " triangles\n";
    return bounds;
}

#endif
