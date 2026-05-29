#ifndef CUDA_RAY_CUH
#define CUDA_RAY_CUH

#include "vec3.cuh"

struct ray {
    point3 orig;
    vec3 dir;

    __host__ __device__ ray() {}
    __host__ __device__ ray(const point3& origin, const vec3& direction)
        : orig(origin), dir(direction) {}

    __host__ __device__ const point3& origin() const { return orig; }
    __host__ __device__ const vec3& direction() const { return dir; }
    __host__ __device__ point3 at(float t) const { return orig + t*dir; }
};

#endif
