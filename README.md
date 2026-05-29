[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/N9V_XO6i)
[![Open in Visual Studio Code](https://classroom.github.com/assets/open-in-vscode-2e0aaae1b6195c2367325f4f02e2d04e9abb55f0b24a779b69b11b9e10269abc.svg)](https://classroom.github.com/online_ide?assignment_repo_id=23781550&assignment_repo_type=AssignmentRepo)

# CUDA-Accelerated Ray Tracer with Multi-GPU Rendering

**CS/EE 147 — Spring 2026 Final Project**
**Team:** Chris Antony, Kaushik Vada

A ray tracer that loads a triangle mesh (e.g. an OBJ exported from Fusion 360) and
renders it with diffuse, metal, and dielectric materials. The project layers in two
GPU optimizations on top of a CPU baseline and benchmarks four configurations
end-to-end:

1. **CPU single-threaded** baseline (port of Peter Shirley's *Ray Tracing in One Weekend*)
2. **CUDA naive** — one thread per pixel
3. **CUDA + BVH** — bounding-volume hierarchy for sub-linear ray/scene intersection
4. **Dual-GPU** — framebuffer split across two NVIDIA A30s (stretch goal)

## Repository layout

```
inc/        Header-only CPU ray tracer (vec3, ray, triangle, materials, camera, OBJ loader)
src/        CPU application entry point (main.cc)
cuda/       GPU ray tracer (float POD types + render.cu kernel)
build/      Out-of-source CMake build directory (gitignored)
```

## Building

Requires CMake 3.18+ and a C++17 compiler. The CPU baseline does **not** require CUDA.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Running

Both renderers take an OBJ path and write a PPM to stdout. The camera auto-frames
the mesh, so any export lands on screen regardless of its scale or origin.

```bash
./build/main      model.obj > cpu.ppm   # CPU baseline (config 1)
./build/main_cuda model.obj > gpu.ppm   # CUDA naive   (config 2)
```

View with any PPM-capable viewer, or convert to PNG:

```bash
convert cpu.ppm cpu.png   # ImageMagick
```

Export from Fusion 360 as OBJ. There's no BVH yet, so on a full-detail part the
CPU renderer is very slow and even the naive GPU path scales linearly with
triangle count — use a low/medium mesh refinement on export until Phase 3 lands.

## Cluster environment (UCR)

CUDA 13.2 toolkit is installed at `/usr/local/cuda`. Target GPUs are NVIDIA A30
(compute capability 8.0). `nvcc` is not on the default `PATH`:

```bash
export PATH=/usr/local/cuda/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
```

## Status

- [x] CPU baseline ray tracer + OBJ mesh loading (Week 1)
- [x] CUDA naive port (Week 2)
- [ ] BVH acceleration (Week 3)
- [ ] Dual-GPU rendering (Week 4)
- [ ] Benchmarks + writeup (Week 5)

## References

- Peter Shirley, *Ray Tracing in One Weekend*: https://raytracing.github.io/
- NVIDIA CUDA C++ Programming Guide: https://docs.nvidia.com/cuda/cuda-c-programming-guide/
- Tero Karras, *Thinking Parallel Part II: Tree Traversal on the GPU*: https://developer.nvidia.com/blog/thinking-parallel-part-ii-tree-traversal-gpu/
