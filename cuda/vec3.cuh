#ifndef CUDA_VEC3_CUH
#define CUDA_VEC3_CUH

#include <cmath>
#include <curand_kernel.h>

// float, not double: A30 FP64 throughput is a fraction of FP32 and ray tracing
// doesn't need the extra precision. This is the main reason the GPU path is its
// own type stack instead of reusing inc/vec3.h.
struct vec3 {
    float e[3];

    __host__ __device__ vec3() : e{0, 0, 0} {}
    __host__ __device__ vec3(float x, float y, float z) : e{x, y, z} {}

    __host__ __device__ float x() const { return e[0]; }
    __host__ __device__ float y() const { return e[1]; }
    __host__ __device__ float z() const { return e[2]; }

    __host__ __device__ vec3 operator-() const { return vec3(-e[0], -e[1], -e[2]); }
    __host__ __device__ float operator[](int i) const { return e[i]; }
    __host__ __device__ float& operator[](int i) { return e[i]; }

    __host__ __device__ vec3& operator+=(const vec3& v) {
        e[0] += v.e[0]; e[1] += v.e[1]; e[2] += v.e[2];
        return *this;
    }
    __host__ __device__ vec3& operator*=(float t) {
        e[0] *= t; e[1] *= t; e[2] *= t;
        return *this;
    }

    __host__ __device__ float length_squared() const {
        return e[0]*e[0] + e[1]*e[1] + e[2]*e[2];
    }
    __host__ __device__ float length() const {
        return std::sqrt(length_squared());
    }
    __host__ __device__ bool near_zero() const {
        const float s = 1e-8f;
        return std::fabs(e[0]) < s && std::fabs(e[1]) < s && std::fabs(e[2]) < s;
    }
};

using point3 = vec3;
using color  = vec3;

__host__ __device__ inline vec3 operator+(const vec3& u, const vec3& v) {
    return vec3(u.e[0]+v.e[0], u.e[1]+v.e[1], u.e[2]+v.e[2]);
}
__host__ __device__ inline vec3 operator-(const vec3& u, const vec3& v) {
    return vec3(u.e[0]-v.e[0], u.e[1]-v.e[1], u.e[2]-v.e[2]);
}
__host__ __device__ inline vec3 operator*(const vec3& u, const vec3& v) {
    return vec3(u.e[0]*v.e[0], u.e[1]*v.e[1], u.e[2]*v.e[2]);
}
__host__ __device__ inline vec3 operator*(float t, const vec3& v) {
    return vec3(t*v.e[0], t*v.e[1], t*v.e[2]);
}
__host__ __device__ inline vec3 operator*(const vec3& v, float t) { return t * v; }
__host__ __device__ inline vec3 operator/(const vec3& v, float t) { return (1.0f/t) * v; }

__host__ __device__ inline float dot(const vec3& u, const vec3& v) {
    return u.e[0]*v.e[0] + u.e[1]*v.e[1] + u.e[2]*v.e[2];
}
__host__ __device__ inline vec3 cross(const vec3& u, const vec3& v) {
    return vec3(u.e[1]*v.e[2] - u.e[2]*v.e[1],
                u.e[2]*v.e[0] - u.e[0]*v.e[2],
                u.e[0]*v.e[1] - u.e[1]*v.e[0]);
}
__host__ __device__ inline vec3 unit_vector(const vec3& v) { return v / v.length(); }

__host__ __device__ inline vec3 reflect(const vec3& v, const vec3& n) {
    return v - 2.0f*dot(v, n)*n;
}
__host__ __device__ inline vec3 refract(const vec3& uv, const vec3& n, float etai_over_etat) {
    float cos_theta = fminf(dot(-uv, n), 1.0f);
    vec3 r_out_perp = etai_over_etat * (uv + cos_theta*n);
    vec3 r_out_parallel = -sqrtf(fabsf(1.0f - r_out_perp.length_squared())) * n;
    return r_out_perp + r_out_parallel;
}

// Device-only RNG helpers (curand). Kept here so material/camera can share them.
__device__ inline float random_float(curandState* st) {
    return curand_uniform(st);  // (0,1]; close enough to the CPU's [0,1)
}
__device__ inline float random_float(curandState* st, float lo, float hi) {
    return lo + (hi - lo) * random_float(st);
}
__device__ inline vec3 random_unit_vector(curandState* st) {
    while (true) {
        vec3 p(random_float(st, -1.0f, 1.0f),
               random_float(st, -1.0f, 1.0f),
               random_float(st, -1.0f, 1.0f));
        float lensq = p.length_squared();
        if (1e-30f < lensq && lensq <= 1.0f)
            return p / sqrtf(lensq);
    }
}
__device__ inline vec3 random_in_unit_disk(curandState* st) {
    while (true) {
        vec3 p(random_float(st, -1.0f, 1.0f), random_float(st, -1.0f, 1.0f), 0.0f);
        if (p.length_squared() < 1.0f)
            return p;
    }
}

#endif
