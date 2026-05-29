#ifndef CUDA_MATERIAL_CUH
#define CUDA_MATERIAL_CUH

#include "ray.cuh"

struct hit_record {
    point3 p;
    vec3 normal;
    float t;
    bool front_face;
    int mat_idx;

    __device__ void set_face_normal(const ray& r, const vec3& outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0.0f;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

enum mat_type { MAT_LAMBERTIAN, MAT_METAL, MAT_DIELECTRIC, MAT_EMISSIVE };

// One flat struct for every material kind instead of virtual dispatch — a
// triangle just stores an index into the material array and we switch on type.
struct material {
    int type;
    color albedo;
    float fuzz;  // metal only
    float ir;    // dielectric refraction index
    color emission{0.0f, 0.0f, 0.0f};  // emissive only (light)

    static material lambertian(const color& a) {
        return material{MAT_LAMBERTIAN, a, 0.0f, 1.0f};
    }
    static material make_metal(const color& a, float f) {
        return material{MAT_METAL, a, f < 1.0f ? f : 1.0f, 1.0f};
    }
    static material dielectric(float ir) {
        return material{MAT_DIELECTRIC, color(1,1,1), 0.0f, ir};
    }
    static material make_emissive(const color& e) {
        material m{MAT_EMISSIVE, color(0,0,0), 0.0f, 1.0f};
        m.emission = e;
        return m;
    }
};

__device__ inline color emitted(const material& m) {
    return m.type == MAT_EMISSIVE ? m.emission : color(0, 0, 0);
}

__device__ inline float schlick(float cosine, float ir) {
    float r0 = (1.0f - ir) / (1.0f + ir);
    r0 = r0*r0;
    return r0 + (1.0f - r0)*powf(1.0f - cosine, 5.0f);
}

__device__ inline bool scatter(const material& m, const ray& r_in, const hit_record& rec,
                               color& attenuation, ray& scattered, curandState* st) {
    switch (m.type) {
        case MAT_EMISSIVE:
            return false;  // a light emits but does not scatter
        case MAT_LAMBERTIAN: {
            vec3 dir = rec.normal + random_unit_vector(st);
            if (dir.near_zero())
                dir = rec.normal;
            scattered = ray(rec.p, dir);
            attenuation = m.albedo;
            return true;
        }
        case MAT_METAL: {
            vec3 reflected = unit_vector(reflect(r_in.direction(), rec.normal))
                           + m.fuzz * random_unit_vector(st);
            scattered = ray(rec.p, reflected);
            attenuation = m.albedo;
            return dot(scattered.direction(), rec.normal) > 0.0f;
        }
        case MAT_DIELECTRIC: {
            attenuation = color(1, 1, 1);
            float ri = rec.front_face ? (1.0f / m.ir) : m.ir;
            vec3 unit_dir = unit_vector(r_in.direction());
            float cos_theta = fminf(dot(-unit_dir, rec.normal), 1.0f);
            float sin_theta = sqrtf(1.0f - cos_theta*cos_theta);

            vec3 dir;
            if (ri * sin_theta > 1.0f || schlick(cos_theta, ri) > random_float(st))
                dir = reflect(unit_dir, rec.normal);
            else
                dir = refract(unit_dir, rec.normal, ri);
            scattered = ray(rec.p, dir);
            return true;
        }
    }
    return false;
}

#endif
