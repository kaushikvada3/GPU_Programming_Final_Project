#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "hittable.h"

// Single triangle, Moller-Trumbore intersection. Carries optional per-vertex
// normals so smooth-shaded OBJ meshes look right; falls back to the flat face
// normal when the file has no vn data.
class triangle : public hittable {
  public:
    triangle(const point3& a, const point3& b, const point3& c, shared_ptr<material> mat)
      : v0(a), v1(b), v2(c), mat(mat), smooth(false) {
        face_normal = unit_vector(cross(v1 - v0, v2 - v0));
    }

    triangle(const point3& a, const point3& b, const point3& c,
             const vec3& na, const vec3& nb, const vec3& nc, shared_ptr<material> mat)
      : v0(a), v1(b), v2(c), n0(na), n1(nb), n2(nc), mat(mat), smooth(true) {
        face_normal = unit_vector(cross(v1 - v0, v2 - v0));
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        vec3 e1 = v1 - v0;
        vec3 e2 = v2 - v0;
        vec3 pvec = cross(r.direction(), e2);
        double det = dot(e1, pvec);

        // det ~ 0 means the ray is parallel to the triangle's plane.
        if (std::fabs(det) < 1e-12)
            return false;

        double inv_det = 1.0 / det;
        vec3 tvec = r.origin() - v0;

        double u = dot(tvec, pvec) * inv_det;
        if (u < 0.0 || u > 1.0)
            return false;

        vec3 qvec = cross(tvec, e1);
        double w = dot(r.direction(), qvec) * inv_det;
        if (w < 0.0 || u + w > 1.0)
            return false;

        double t = dot(e2, qvec) * inv_det;
        if (!ray_t.surrounds(t))
            return false;

        rec.t = t;
        rec.p = r.at(t);

        vec3 outward = smooth
            ? unit_vector((1.0 - u - w)*n0 + u*n1 + w*n2)
            : face_normal;
        rec.set_face_normal(r, outward);
        rec.mat = mat;

        return true;
    }

  private:
    point3 v0, v1, v2;
    vec3 n0, n1, n2;
    vec3 face_normal;
    shared_ptr<material> mat;
    bool smooth;
};

#endif
