#ifndef CUDA_CAMERA_CUH
#define CUDA_CAMERA_CUH

#include "ray.cuh"

// Precomputed on the host, passed to the kernel by value. get_ray runs per
// sample on device. Mirrors inc/camera.h's geometry.
struct camera {
    point3 center;
    point3 pixel00_loc;
    vec3 pixel_delta_u;
    vec3 pixel_delta_v;
    vec3 defocus_disk_u;
    vec3 defocus_disk_v;
    float defocus_angle;

    void init(int image_width, int image_height, float vfov,
              const point3& lookfrom, const point3& lookat, const vec3& vup,
              float defocus_angle_deg, float focus_dist) {
        defocus_angle = defocus_angle_deg;
        center = lookfrom;

        float theta = vfov * 3.14159265358979f / 180.0f;
        float h = std::tan(theta / 2.0f);
        float viewport_height = 2.0f * h * focus_dist;
        float viewport_width = viewport_height * (float(image_width) / image_height);

        vec3 w = unit_vector(lookfrom - lookat);
        vec3 u = unit_vector(cross(vup, w));
        vec3 v = cross(w, u);

        vec3 viewport_u = viewport_width * u;
        vec3 viewport_v = viewport_height * -v;

        pixel_delta_u = viewport_u / float(image_width);
        pixel_delta_v = viewport_v / float(image_height);

        point3 viewport_upper_left = center - (focus_dist * w)
                                   - viewport_u / 2.0f - viewport_v / 2.0f;
        pixel00_loc = viewport_upper_left + 0.5f * (pixel_delta_u + pixel_delta_v);

        float defocus_radius = focus_dist * std::tan(defocus_angle_deg / 2.0f * 3.14159265358979f / 180.0f);
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    __device__ ray get_ray(int i, int j, curandState* st) const {
        float ox = random_float(st) - 0.5f;
        float oy = random_float(st) - 0.5f;
        point3 pixel_sample = pixel00_loc
                            + ((i + ox) * pixel_delta_u)
                            + ((j + oy) * pixel_delta_v);

        point3 origin = center;
        if (defocus_angle > 0.0f) {
            vec3 p = random_in_unit_disk(st);
            origin = center + p[0]*defocus_disk_u + p[1]*defocus_disk_v;
        }
        return ray(origin, pixel_sample - origin);
    }
};

#endif
