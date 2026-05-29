#ifndef CUDA_TRIANGLE_CUH
#define CUDA_TRIANGLE_CUH

#include "material.cuh"

struct triangle {
    point3 v0, v1, v2;
    vec3 n0, n1, n2;
    vec3 face_normal;
    int mat_idx;
    bool smooth;
};

// host-side helpers so the loader stays readable
inline triangle make_triangle(const point3& a, const point3& b, const point3& c, int mat) {
    triangle t;
    t.v0 = a; t.v1 = b; t.v2 = c;
    t.face_normal = unit_vector(cross(b - a, c - a));
    t.n0 = t.n1 = t.n2 = t.face_normal;
    t.mat_idx = mat;
    t.smooth = false;
    return t;
}
inline triangle make_triangle(const point3& a, const point3& b, const point3& c,
                              const vec3& na, const vec3& nb, const vec3& nc, int mat) {
    triangle t;
    t.v0 = a; t.v1 = b; t.v2 = c;
    t.n0 = na; t.n1 = nb; t.n2 = nc;
    t.face_normal = unit_vector(cross(b - a, c - a));
    t.mat_idx = mat;
    t.smooth = true;
    return t;
}

__device__ inline bool hit_triangle(const triangle& tri, const ray& r,
                                    float t_min, float t_max, hit_record& rec) {
    vec3 e1 = tri.v1 - tri.v0;
    vec3 e2 = tri.v2 - tri.v0;
    vec3 pvec = cross(r.direction(), e2);
    float det = dot(e1, pvec);
    if (fabsf(det) < 1e-12f)
        return false;

    float inv_det = 1.0f / det;
    vec3 tvec = r.origin() - tri.v0;

    float u = dot(tvec, pvec) * inv_det;
    if (u < 0.0f || u > 1.0f)
        return false;

    vec3 qvec = cross(tvec, e1);
    float w = dot(r.direction(), qvec) * inv_det;
    if (w < 0.0f || u + w > 1.0f)
        return false;

    float t = dot(e2, qvec) * inv_det;
    if (t <= t_min || t >= t_max)
        return false;

    rec.t = t;
    rec.p = r.at(t);
    vec3 outward = tri.smooth
        ? unit_vector((1.0f - u - w)*tri.n0 + u*tri.n1 + w*tri.n2)
        : tri.face_normal;
    rec.set_face_normal(r, outward);
    rec.mat_idx = tri.mat_idx;
    return true;
}

#endif
