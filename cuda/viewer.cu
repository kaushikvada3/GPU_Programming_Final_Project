#include "kernels.cuh"
#include "obj_load.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <vector>
#include <cmath>
#include <cstdint>
#include <cstdlib>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s model.obj\n", argv[0]);
        return 1;
    }

    std::vector<material> materials;
    std::vector<triangle> tris;
    aabb3 bounds = load_obj_scene(argv[1], tris, materials);
    int n_tris = int(tris.size());

    const int   W = 800, H = 450;
    const int   max_depth = 10;
    const int   MAX_SAMPLES = 256;
    const float vfov = 40.0f;
    const float DEG = 3.14159265358979f / 180.0f;

    // Orbit camera: spherical coordinates around the model's centroid. Seeded to
    // the same 3/4 view the offline renderer uses so the first frame matches.
    point3 target = bounds.center();
    float  radius = 0.5f * bounds.diagonal();
    float  base_dist = (radius / std::tan(vfov / 2.0f * DEG)) * 1.3f;
    vec3   d0 = unit_vector(vec3(1.0f, 0.6f, 1.0f));
    float  azimuth   = std::atan2(d0.x(), d0.z());
    float  elevation = std::asin(d0.y());
    float  dist      = base_dist;

    triangle*    d_tris;
    material*    d_mats;
    color*       d_accum;
    curandState* d_rng;
    size_t n_pixels = size_t(W) * H;

    CUDA_CHECK(cudaMalloc(&d_tris, n_tris * sizeof(triangle)));
    CUDA_CHECK(cudaMalloc(&d_mats, materials.size() * sizeof(material)));
    CUDA_CHECK(cudaMalloc(&d_accum, n_pixels * sizeof(color)));
    CUDA_CHECK(cudaMalloc(&d_rng, n_pixels * sizeof(curandState)));
    CUDA_CHECK(cudaMemcpy(d_tris, tris.data(), n_tris * sizeof(triangle), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_mats, materials.data(), materials.size() * sizeof(material), cudaMemcpyHostToDevice));

    dim3 block(8, 8);
    dim3 grid((W + block.x - 1) / block.x, (H + block.y - 1) / block.y);
    render_init<<<grid, block>>>(W, H, d_rng);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());

    std::vector<color> fb(n_pixels);

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        const char* d = std::getenv("DISPLAY");
        std::fprintf(stderr, "viewer: cannot open X display (DISPLAY=%s)\n", d ? d : "(unset)");
        return 1;
    }
    int     screen = DefaultScreen(dpy);
    Visual* visual = DefaultVisual(dpy, screen);
    int     depth  = DefaultDepth(dpy, screen);
    Window  win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, W, H, 0,
                                      BlackPixel(dpy, screen), BlackPixel(dpy, screen));
    XStoreName(dpy, win, "CUDA Ray Tracer  -  drag: orbit   scroll: zoom   q/Esc: quit");
    XSelectInput(dpy, win, ExposureMask | ButtonPressMask | ButtonReleaseMask |
                           PointerMotionMask | KeyPressMask | StructureNotifyMask);
    Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);
    XMapWindow(dpy, win);
    GC gc = DefaultGC(dpy, screen);

    char*   imgbuf = (char*)std::malloc(n_pixels * 4);
    XImage* img = XCreateImage(dpy, visual, depth, ZPixmap, 0, imgbuf, W, H, 32, 0);
    int rsh = __builtin_ctzl(img->red_mask);
    int gsh = __builtin_ctzl(img->green_mask);
    int bsh = __builtin_ctzl(img->blue_mask);
    bool fast_blit = (img->bits_per_pixel == 32 && img->byte_order == LSBFirst);

    bool running = true, dragging = false, dirty = true;
    int  last_x = 0, last_y = 0, total = 0;
    camera cam;

    auto recompute_cam = [&]() {
        vec3 dir(std::cos(elevation) * std::sin(azimuth),
                 std::sin(elevation),
                 std::cos(elevation) * std::cos(azimuth));
        point3 lookfrom = target + dist * dir;
        cam.init(W, H, vfov, lookfrom, target, vec3(0, 1, 0), 0.0f, dist);
    };

    auto handle = [&](XEvent& ev) {
        switch (ev.type) {
        case Expose:
            XPutImage(dpy, win, gc, img, 0, 0, 0, 0, W, H);
            break;
        case ButtonPress:
            if (ev.xbutton.button == Button1) {
                dragging = true;
                last_x = ev.xbutton.x;
                last_y = ev.xbutton.y;
            } else if (ev.xbutton.button == Button4) {       // scroll up -> zoom in
                dist *= 0.9f;
                if (dist < 0.02f * radius) dist = 0.02f * radius;
                dirty = true;
            } else if (ev.xbutton.button == Button5) {       // scroll down -> zoom out
                dist *= 1.1f;
                dirty = true;
            }
            break;
        case ButtonRelease:
            if (ev.xbutton.button == Button1) dragging = false;
            break;
        case MotionNotify:
            if (dragging) {
                azimuth   -= (ev.xmotion.x - last_x) * 0.01f;
                elevation += (ev.xmotion.y - last_y) * 0.01f;
                if (elevation >  1.5f) elevation =  1.5f;
                if (elevation < -1.5f) elevation = -1.5f;
                last_x = ev.xmotion.x;
                last_y = ev.xmotion.y;
                dirty = true;
            }
            break;
        case KeyPress: {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            if (ks == XK_q || ks == XK_Escape) running = false;
            break;
        }
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == wm_delete) running = false;
            break;
        }
    };

    while (running) {
        // Image is fully refined and nothing pending: block until the next event
        // instead of spinning the GPU.
        if (!dirty && total >= MAX_SAMPLES) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            handle(ev);
        }
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            handle(ev);
        }
        if (!running) break;

        if (dirty) {
            CUDA_CHECK(cudaMemset(d_accum, 0, n_pixels * sizeof(color)));
            total = 0;
            recompute_cam();
            dirty = false;
        }

        if (total < MAX_SAMPLES) {
            int spp = dragging ? 1 : 8;   // cheap preview while moving, refine when idle
            render_accumulate<<<grid, block>>>(d_accum, W, H, spp, max_depth,
                                               cam, d_tris, n_tris, d_mats, 1.0f, d_rng);
            CUDA_CHECK(cudaGetLastError());
            CUDA_CHECK(cudaDeviceSynchronize());
            total += spp;

            CUDA_CHECK(cudaMemcpy(fb.data(), d_accum, n_pixels * sizeof(color),
                                  cudaMemcpyDeviceToHost));
            float inv = 1.0f / total;
            if (fast_blit) {
                uint32_t* px = (uint32_t*)imgbuf;
                for (size_t p = 0; p < n_pixels; p++) {
                    unsigned long r = to_byte(fb[p].x() * inv);
                    unsigned long g = to_byte(fb[p].y() * inv);
                    unsigned long b = to_byte(fb[p].z() * inv);
                    px[p] = (r << rsh) | (g << gsh) | (b << bsh);
                }
            } else {
                for (int y = 0; y < H; y++) {
                    for (int x = 0; x < W; x++) {
                        const color& c = fb[size_t(y) * W + x];
                        unsigned long r = to_byte(c.x() * inv);
                        unsigned long g = to_byte(c.y() * inv);
                        unsigned long b = to_byte(c.z() * inv);
                        XPutPixel(img, x, y, (r << rsh) | (g << gsh) | (b << bsh));
                    }
                }
            }
            XPutImage(dpy, win, gc, img, 0, 0, 0, 0, W, H);
            XFlush(dpy);
        }
    }

    XDestroyImage(img);   // frees imgbuf
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    cudaFree(d_tris);
    cudaFree(d_mats);
    cudaFree(d_accum);
    cudaFree(d_rng);
    return 0;
}
