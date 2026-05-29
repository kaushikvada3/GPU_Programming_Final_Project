#include "rtweekend.h"
#include "camera.h"
#include "hittable_list.h"
#include "material.h"
#include "obj_loader.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " model.obj > out.ppm\n";
        return 1;
    }

    hittable_list world;
    auto mat = make_shared<lambertian>(color(0.7, 0.7, 0.7));
    aabb3 bounds = load_obj(argv[1], world, mat);

    // Auto-frame the camera around the mesh so any Fusion export lands on screen
    // regardless of its scale/origin. 3/4 view from above.
    point3 target = bounds.center();
    double radius = 0.5 * bounds.diagonal();

    double vfov = 40.0;
    double dist = (radius / std::tan(degrees_to_radians(vfov / 2))) * 1.3;
    vec3 dir = unit_vector(vec3(1.0, 0.6, 1.0));

    camera cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.image_width       = 800;
    cam.samples_per_pixel = 20;
    cam.max_depth         = 10;

    cam.vfov     = vfov;
    cam.lookfrom = target + dir * dist;
    cam.lookat   = target;
    cam.vup      = vec3(0, 1, 0);

    cam.defocus_angle = 0;
    cam.focus_dist    = dist;

    cam.render(world);
}
