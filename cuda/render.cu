#include "kernels.cuh"
#include "obj_load.h"

#include <vector>
#include <chrono>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s model.obj > out.ppm\n", argv[0]);
        return 1;
    }

    std::vector<material> materials = { material::lambertian(color(0.7f, 0.7f, 0.7f)) };
    std::vector<triangle> tris;
    aabb3 bounds = load_obj_scene(argv[1], tris, materials);
    int n_tris = int(tris.size());

    const int image_width = 800;
    const int image_height = int(image_width / (16.0f / 9.0f));
    const int samples = 20;
    const int max_depth = 10;

    // same auto-framing as the CPU main.cc
    point3 target = bounds.center();
    float radius = 0.5f * bounds.diagonal();
    float vfov = 40.0f;
    float dist = (radius / std::tan(vfov / 2.0f * 3.14159265358979f / 180.0f)) * 1.3f;
    vec3 dir = unit_vector(vec3(1.0f, 0.6f, 1.0f));

    camera cam;
    cam.init(image_width, image_height, vfov,
             target + dir * dist, target, vec3(0, 1, 0), 0.0f, dist);

    triangle* d_tris;
    material* d_mats;
    color* d_fb;
    curandState* d_rng;
    size_t n_pixels = size_t(image_width) * image_height;

    CUDA_CHECK(cudaMalloc(&d_tris, n_tris * sizeof(triangle)));
    CUDA_CHECK(cudaMalloc(&d_mats, materials.size() * sizeof(material)));
    CUDA_CHECK(cudaMalloc(&d_fb, n_pixels * sizeof(color)));
    CUDA_CHECK(cudaMalloc(&d_rng, n_pixels * sizeof(curandState)));
    CUDA_CHECK(cudaMemcpy(d_tris, tris.data(), n_tris * sizeof(triangle), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_mats, materials.data(), materials.size() * sizeof(material), cudaMemcpyHostToDevice));

    dim3 block(8, 8);
    dim3 grid((image_width + block.x - 1) / block.x,
              (image_height + block.y - 1) / block.y);

    auto t0 = std::chrono::high_resolution_clock::now();
    render_init<<<grid, block>>>(image_width, image_height, d_rng);
    CUDA_CHECK(cudaGetLastError());
    render<<<grid, block>>>(d_fb, image_width, image_height, samples, max_depth,
                            cam, d_tris, n_tris, d_mats, 1.0f, d_rng);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::fprintf(stderr, "render: %d tris, %dx%d, %d spp -> %.1f ms\n",
                 n_tris, image_width, image_height, samples, ms);

    std::vector<color> fb(n_pixels);
    CUDA_CHECK(cudaMemcpy(fb.data(), d_fb, n_pixels * sizeof(color), cudaMemcpyDeviceToHost));

    std::printf("P3\n%d %d\n255\n", image_width, image_height);
    for (size_t p = 0; p < n_pixels; p++) {
        std::printf("%d %d %d\n", to_byte(fb[p].x()), to_byte(fb[p].y()), to_byte(fb[p].z()));
    }

    cudaFree(d_tris);
    cudaFree(d_mats);
    cudaFree(d_fb);
    cudaFree(d_rng);
    return 0;
}
