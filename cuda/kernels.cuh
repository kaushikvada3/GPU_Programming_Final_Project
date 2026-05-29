#ifndef CUDA_KERNELS_CUH
#define CUDA_KERNELS_CUH

#include "camera.cuh"
#include "triangle.cuh"

#include <cstdio>
#include <cstdlib>

#define CUDA_CHECK(call)                                                       \
    do {                                                                       \
        cudaError_t err = (call);                                              \
        if (err != cudaSuccess) {                                              \
            std::fprintf(stderr, "CUDA error %s at %s:%d\n",                   \
                         cudaGetErrorString(err), __FILE__, __LINE__);         \
            std::exit(1);                                                      \
        }                                                                      \
    } while (0)

__device__ inline bool hit_world(const triangle* tris, int n, const ray& r,
                                 float t_min, float t_max, hit_record& rec) {
    hit_record temp;
    bool hit = false;
    float closest = t_max;
    for (int i = 0; i < n; i++) {
        if (hit_triangle(tris[i], r, t_min, closest, temp)) {
            hit = true;
            closest = temp.t;
            rec = temp;
        }
    }
    return hit;
}

// Iterative instead of recursive: GPU stacks are tiny, so the RTOW recursion
// becomes a bounce loop that folds attenuation forward. Emissive hits add light
// and end the path; bg_scale dims the sky so a movable light can dominate.
__device__ inline color ray_color(ray r, const triangle* tris, int n_tris,
                                  const material* mats, int max_depth,
                                  float bg_scale, curandState* st) {
    color attenuation(1, 1, 1);
    color result(0, 0, 0);
    for (int depth = 0; depth < max_depth; depth++) {
        hit_record rec;
        if (hit_world(tris, n_tris, r, 0.001f, 3.4e38f, rec)) {
            const material& m = mats[rec.mat_idx];
            result += attenuation * emitted(m);
            ray scattered;
            color atten;
            if (scatter(m, r, rec, atten, scattered, st)) {
                attenuation = attenuation * atten;
                r = scattered;
            } else {
                return result;  // light or fully absorbed
            }
        } else {
            vec3 ud = unit_vector(r.direction());
            float a = 0.5f * (ud.y() + 1.0f);
            color sky = (1.0f - a)*color(1, 1, 1) + a*color(0.5f, 0.7f, 1.0f);
            return result + attenuation * (bg_scale * sky);
        }
    }
    return result;  // ran out of bounces
}

__global__ void render_init(int width, int height, curandState* rng) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i >= width || j >= height)
        return;
    int idx = j * width + i;
    curand_init(1984ull, idx, 0, &rng[idx]);
}

__global__ void render(color* fb, int width, int height, int samples, int max_depth,
                              camera cam, const triangle* tris, int n_tris,
                              const material* mats, float bg_scale, curandState* rng) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i >= width || j >= height)
        return;

    int idx = j * width + i;
    curandState st = rng[idx];

    color pixel(0, 0, 0);
    for (int s = 0; s < samples; s++) {
        ray r = cam.get_ray(i, j, &st);
        pixel += ray_color(r, tris, n_tris, mats, max_depth, bg_scale, &st);
    }
    fb[idx] = pixel / float(samples);
}

// Adds samples_this_pass new samples into an accumulation buffer and persists
// the RNG state so consecutive passes draw fresh randomness. Used by the
// interactive viewer for progressive refinement; host divides accum by the
// running total sample count before display.
__global__ void render_accumulate(color* accum, int width, int height,
                                         int samples_this_pass, int max_depth,
                                         camera cam, const triangle* tris, int n_tris,
                                         const material* mats, float bg_scale,
                                         curandState* rng) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    if (i >= width || j >= height)
        return;

    int idx = j * width + i;
    curandState st = rng[idx];

    color pixel(0, 0, 0);
    for (int s = 0; s < samples_this_pass; s++) {
        ray r = cam.get_ray(i, j, &st);
        pixel += ray_color(r, tris, n_tris, mats, max_depth, bg_scale, &st);
    }
    accum[idx] += pixel;
    rng[idx] = st;
}

static inline int to_byte(float linear) {
    float gamma = linear > 0.0f ? sqrtf(linear) : 0.0f;  // gamma 2, matches CPU path
    if (gamma < 0.0f) gamma = 0.0f;
    if (gamma > 0.999f) gamma = 0.999f;
    return int(256.0f * gamma);
}

#endif
