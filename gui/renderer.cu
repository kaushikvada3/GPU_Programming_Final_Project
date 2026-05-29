#include "renderer.h"

#include "kernels.cuh"
#include "obj_load.h"

#include <vector>
#include <cmath>

// ---- local-space primitive generators (centered at origin, unit-ish size) ----
namespace {

void addQuad(std::vector<triangle>& out, point3 a, point3 b, point3 c, point3 d) {
    out.push_back(make_triangle(a, b, c, 0));
    out.push_back(make_triangle(a, c, d, 0));
}

void genBox(std::vector<triangle>& out) {
    float h = 0.5f;
    point3 v[8];
    int n = 0;
    for (int x = 0; x < 2; x++)
        for (int y = 0; y < 2; y++)
            for (int z = 0; z < 2; z++)
                v[n++] = point3((x ? h : -h), (y ? h : -h), (z ? h : -h));
    auto g = [&](int ix, int iy, int iz) { return v[ix*4 + iy*2 + iz]; };
    addQuad(out, g(0,0,0), g(0,0,1), g(0,1,1), g(0,1,0));  // -x
    addQuad(out, g(1,0,1), g(1,0,0), g(1,1,0), g(1,1,1));  // +x
    addQuad(out, g(0,0,0), g(1,0,0), g(1,0,1), g(0,0,1));  // -y
    addQuad(out, g(0,1,1), g(1,1,1), g(1,1,0), g(0,1,0));  // +y
    addQuad(out, g(1,0,0), g(0,0,0), g(0,1,0), g(1,1,0));  // -z
    addQuad(out, g(0,0,1), g(1,0,1), g(1,1,1), g(0,1,1));  // +z
}

void genGround(std::vector<triangle>& out) {
    float h = 0.5f;
    addQuad(out, point3(-h, 0, -h), point3(h, 0, -h), point3(h, 0, h), point3(-h, 0, h));
}

void genPrism(std::vector<triangle>& out) {
    // triangular cross-section in x-y, extruded along z; centered at origin
    float hx = 0.5f, hz = 0.5f, y0 = -0.5f, y1 = 0.5f;
    point3 f0(-hx, y0, -hz), f1(hx, y0, -hz), f2(0, y1, -hz);
    point3 b0(-hx, y0,  hz), b1(hx, y0,  hz), b2(0, y1,  hz);
    out.push_back(make_triangle(f0, f2, f1, 0));   // front cap
    out.push_back(make_triangle(b0, b1, b2, 0));   // back cap
    addQuad(out, f0, f1, b1, b0);                  // bottom
    addQuad(out, f1, f2, b2, b1);                  // right slope
    addQuad(out, f2, f0, b0, b2);                  // left slope
}

void genPyramid(std::vector<triangle>& out) {
    float h = 0.5f;
    point3 b[4] = { point3(-h, -h, -h), point3(h, -h, -h),
                    point3(h, -h, h),   point3(-h, -h, h) };
    point3 apex(0, h, 0);
    addQuad(out, b[0], b[3], b[2], b[1]);          // base
    for (int i = 0; i < 4; i++)
        out.push_back(make_triangle(b[i], b[(i+1)%4], apex, 0));
}

void genCylinder(std::vector<triangle>& out, int slices = 32) {
    float r = 0.5f, y0 = -0.5f, y1 = 0.5f;
    point3 capTop(0, y1, 0), capBot(0, y0, 0);
    for (int j = 0; j < slices; j++) {
        float a0 = 2*M_PI * j / slices, a1 = 2*M_PI * (j+1) / slices;
        point3 t0(r*cosf(a0), y1, r*sinf(a0)), t1(r*cosf(a1), y1, r*sinf(a1));
        point3 d0(r*cosf(a0), y0, r*sinf(a0)), d1(r*cosf(a1), y0, r*sinf(a1));
        addQuad(out, d0, d1, t1, t0);              // side
        out.push_back(make_triangle(capTop, t0, t1, 0));
        out.push_back(make_triangle(capBot, d1, d0, 0));
    }
}

void genSphere(std::vector<triangle>& out, int stacks = 24, int slices = 36) {
    float r = 0.5f;
    auto pos = [&](int i, int j) {
        float phi = M_PI * i / stacks, th = 2*M_PI * j / slices;
        return vec3(sinf(phi)*cosf(th), cosf(phi), sinf(phi)*sinf(th));
    };
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            vec3 na = pos(i, j),   nb = pos(i, j+1);
            vec3 nc = pos(i+1, j+1), nd = pos(i+1, j);
            // smooth: vertex normals == unit positions
            out.push_back(make_triangle(r*na, r*nb, r*nc, na, nb, nc, 0));
            out.push_back(make_triangle(r*na, r*nc, r*nd, na, nc, nd, 0));
        }
    }
}

void genShape(ShapeType t, std::vector<triangle>& out) {
    switch (t) {
        case ShapeType::Ground:   genGround(out); break;
        case ShapeType::Box:      genBox(out); break;
        case ShapeType::Sphere:   genSphere(out); break;
        case ShapeType::Prism:    genPrism(out); break;
        case ShapeType::Pyramid:  genPyramid(out); break;
        case ShapeType::Cylinder: genCylinder(out); break;
    }
}

material toMaterial(const MaterialDesc& m) {
    color c(m.color[0], m.color[1], m.color[2]);
    switch (m.type) {
        case MaterialType::Diffuse:  return material::lambertian(c);
        case MaterialType::Metal:    return material::make_metal(c, m.fuzz);
        case MaterialType::Glass:    return material::dielectric(m.ior);
        case MaterialType::Emissive: return material::lambertian(c);  // until M4
    }
    return material::lambertian(c);
}

// Euler XYZ (degrees) -> rotate a vector.
vec3 rotateEuler(const vec3& p, const float deg[3]) {
    float rx = deg[0]*M_PI/180.0f, ry = deg[1]*M_PI/180.0f, rz = deg[2]*M_PI/180.0f;
    float cx=cosf(rx), sx=sinf(rx), cy=cosf(ry), sy=sinf(ry), cz=cosf(rz), sz=sinf(rz);
    // Rz * Ry * Rx
    vec3 a(p.x(), cx*p.y() - sx*p.z(), sx*p.y() + cx*p.z());
    vec3 b(cy*a.x() + sy*a.z(), a.y(), -sy*a.x() + cy*a.z());
    return vec3(cz*b.x() - sz*b.y(), sz*b.x() + cz*b.y(), b.z());
}

bool hostHit(const triangle& t, const vec3& o, const vec3& d, float& tout) {
    vec3 e1 = t.v1 - t.v0, e2 = t.v2 - t.v0;
    vec3 pv = cross(d, e2);
    float det = dot(e1, pv);
    if (fabsf(det) < 1e-12f) return false;
    float inv = 1.0f / det;
    vec3 tv = o - t.v0;
    float u = dot(tv, pv) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    vec3 qv = cross(tv, e1);
    float w = dot(d, qv) * inv;
    if (w < 0.0f || u + w > 1.0f) return false;
    float tt = dot(e2, qv) * inv;
    if (tt <= 1e-3f) return false;
    tout = tt;
    return true;
}

}  // namespace

struct Renderer::Impl {
    int W, H;
    size_t n_pixels;

    std::vector<material> materials;
    std::vector<triangle> tris;
    std::vector<int>      tri_obj;   // triangle index -> source object index
    aabb3 bounds{};
    bool have_scene = false;

    triangle*    d_tris  = nullptr;
    material*    d_mats  = nullptr;
    color*       d_accum = nullptr;
    curandState* d_rng   = nullptr;
    int n_tris = 0;

    camera cam;
    float bg_scale = 1.0f;
    int total_samples = 0;

    std::vector<color>   fb;
    std::vector<uint8_t> rgb;

    dim3 block{8, 8, 1};
    dim3 grid{1, 1, 1};

    Impl(int w, int h) : W(w), H(h), n_pixels(size_t(w) * h) {
        fb.resize(n_pixels);
        rgb.resize(n_pixels * 3);
        grid = dim3((W + block.x - 1) / block.x, (H + block.y - 1) / block.y, 1);
        CUDA_CHECK(cudaMalloc(&d_accum, n_pixels * sizeof(color)));
        CUDA_CHECK(cudaMalloc(&d_rng, n_pixels * sizeof(curandState)));
        render_init<<<grid, block>>>(W, H, d_rng);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    ~Impl() {
        cudaFree(d_tris);
        cudaFree(d_mats);
        cudaFree(d_accum);
        cudaFree(d_rng);
    }

    void uploadScene() {
        if (d_tris) { cudaFree(d_tris); d_tris = nullptr; }
        if (d_mats) { cudaFree(d_mats); d_mats = nullptr; }
        n_tris = int(tris.size());
        CUDA_CHECK(cudaMalloc(&d_tris, n_tris * sizeof(triangle)));
        CUDA_CHECK(cudaMalloc(&d_mats, materials.size() * sizeof(material)));
        CUDA_CHECK(cudaMemcpy(d_tris, tris.data(), n_tris * sizeof(triangle),
                              cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemcpy(d_mats, materials.data(), materials.size() * sizeof(material),
                              cudaMemcpyHostToDevice));
        have_scene = n_tris > 0;
    }
};

Renderer::Renderer(int width, int height) : impl_(new Impl(width, height)) {}
Renderer::~Renderer() { delete impl_; }

void Renderer::loadSceneOBJ(const char* path) {
    impl_->tris.clear();
    impl_->materials.clear();
    impl_->bounds = load_obj_scene(path, impl_->tris, impl_->materials);
    impl_->uploadScene();
    impl_->total_samples = 0;
}

void Renderer::setScene(const SceneDesc& scene) {
    auto& tris = impl_->tris;
    auto& mats = impl_->materials;
    auto& tobj = impl_->tri_obj;
    tris.clear();
    mats.clear();
    tobj.clear();

    const float inf = std::numeric_limits<float>::infinity();
    impl_->bounds = aabb3{ point3(inf, inf, inf), point3(-inf, -inf, -inf) };

    for (size_t oi = 0; oi < scene.objects.size(); oi++) {
        const SceneObject& ob = scene.objects[oi];
        int mat_idx = int(mats.size());
        mats.push_back(toMaterial(ob.material));

        std::vector<triangle> local;
        genShape(ob.type, local);

        vec3 scl(ob.scale[0], ob.scale[1], ob.scale[2]);
        vec3 pos(ob.position[0], ob.position[1], ob.position[2]);
        auto xform = [&](const vec3& p) {
            return rotateEuler(vec3(p.x()*scl.x(), p.y()*scl.y(), p.z()*scl.z()),
                               ob.rotation) + pos;
        };

        for (triangle t : local) {
            t.v0 = xform(t.v0);
            t.v1 = xform(t.v1);
            t.v2 = xform(t.v2);
            t.face_normal = unit_vector(cross(t.v1 - t.v0, t.v2 - t.v0));
            if (t.smooth) {
                t.n0 = unit_vector(rotateEuler(t.n0, ob.rotation));
                t.n1 = unit_vector(rotateEuler(t.n1, ob.rotation));
                t.n2 = unit_vector(rotateEuler(t.n2, ob.rotation));
            } else {
                t.n0 = t.n1 = t.n2 = t.face_normal;
            }
            t.mat_idx = mat_idx;
            tris.push_back(t);
            tobj.push_back(int(oi));

            for (const point3* v : { &t.v0, &t.v1, &t.v2 })
                for (int k = 0; k < 3; k++) {
                    impl_->bounds.min[k] = fminf(impl_->bounds.min[k], (*v)[k]);
                    impl_->bounds.max[k] = fmaxf(impl_->bounds.max[k], (*v)[k]);
                }
        }
    }

    // Append the movable light as an emissive sphere (not a selectable object,
    // so its triangles map to object id -1).
    impl_->bg_scale = scene.background;
    if (scene.light.enabled) {
        int light_mat = int(mats.size());
        color e(scene.light.color[0] * scene.light.intensity,
                scene.light.color[1] * scene.light.intensity,
                scene.light.color[2] * scene.light.intensity);
        mats.push_back(material::make_emissive(e));

        std::vector<triangle> local;
        genSphere(local);
        float r = scene.light.radius;
        vec3 pos(scene.light.position[0], scene.light.position[1], scene.light.position[2]);
        for (triangle t : local) {
            t.v0 = 2.0f * r * t.v0 + pos;   // genSphere radius is 0.5 -> diameter*r
            t.v1 = 2.0f * r * t.v1 + pos;
            t.v2 = 2.0f * r * t.v2 + pos;
            t.face_normal = unit_vector(cross(t.v1 - t.v0, t.v2 - t.v0));
            t.mat_idx = light_mat;
            tris.push_back(t);
            tobj.push_back(-1);
        }
    }

    impl_->uploadScene();
    impl_->total_samples = 0;
}

int Renderer::pick(float nx, float ny) const {
    if (!impl_->have_scene) return -1;
    const camera& c = impl_->cam;
    float i = nx * impl_->W;
    float j = ny * impl_->H;
    point3 origin = c.center;
    vec3 dir = c.pixel00_loc + i * c.pixel_delta_u + j * c.pixel_delta_v - origin;

    float best = std::numeric_limits<float>::infinity();
    int   hit_obj = -1;
    for (size_t k = 0; k < impl_->tris.size(); k++) {
        float t;
        if (hostHit(impl_->tris[k], origin, dir, t) && t < best) {
            best = t;
            hit_obj = impl_->tri_obj[k];
        }
    }
    return hit_obj;
}

void Renderer::sceneBounds(float center[3], float& radius) const {
    point3 c = impl_->bounds.center();
    center[0] = c.x();
    center[1] = c.y();
    center[2] = c.z();
    radius = 0.5f * impl_->bounds.diagonal();
}

void Renderer::setCamera(const OrbitCamera& c) {
    point3 target(c.target[0], c.target[1], c.target[2]);
    vec3 dir(cosf(c.elevation) * sinf(c.azimuth),
             sinf(c.elevation),
             cosf(c.elevation) * cosf(c.azimuth));
    point3 lookfrom = target + c.dist * dir;
    impl_->cam.init(impl_->W, impl_->H, c.vfov, lookfrom, target, vec3(0, 1, 0),
                    0.0f, c.dist);
}

void Renderer::resetAccumulation() {
    CUDA_CHECK(cudaMemset(impl_->d_accum, 0, impl_->n_pixels * sizeof(color)));
    impl_->total_samples = 0;
}

void Renderer::resize(int width, int height) {
    Impl& I = *impl_;
    I.W = width;
    I.H = height;
    I.n_pixels = size_t(width) * height;
    I.fb.assign(I.n_pixels, color());
    I.rgb.assign(I.n_pixels * 3, 0);
    cudaFree(I.d_accum);
    cudaFree(I.d_rng);
    CUDA_CHECK(cudaMalloc(&I.d_accum, I.n_pixels * sizeof(color)));
    CUDA_CHECK(cudaMalloc(&I.d_rng, I.n_pixels * sizeof(curandState)));
    I.grid = dim3((width + I.block.x - 1) / I.block.x,
                  (height + I.block.y - 1) / I.block.y, 1);
    render_init<<<I.grid, I.block>>>(width, height, I.d_rng);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
    I.total_samples = 0;
}

int Renderer::accumulate(int spp, int max_depth) {
    if (!impl_->have_scene)
        return impl_->total_samples;

    render_accumulate<<<impl_->grid, impl_->block>>>(
        impl_->d_accum, impl_->W, impl_->H, spp, max_depth, impl_->cam,
        impl_->d_tris, impl_->n_tris, impl_->d_mats, impl_->bg_scale, impl_->d_rng);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaDeviceSynchronize());
    impl_->total_samples += spp;

    CUDA_CHECK(cudaMemcpy(impl_->fb.data(), impl_->d_accum,
                          impl_->n_pixels * sizeof(color), cudaMemcpyDeviceToHost));

    float inv = 1.0f / impl_->total_samples;
    uint8_t* o = impl_->rgb.data();
    for (size_t p = 0; p < impl_->n_pixels; p++) {
        o[p * 3 + 0] = uint8_t(to_byte(impl_->fb[p].x() * inv));
        o[p * 3 + 1] = uint8_t(to_byte(impl_->fb[p].y() * inv));
        o[p * 3 + 2] = uint8_t(to_byte(impl_->fb[p].z() * inv));
    }
    return impl_->total_samples;
}

const uint8_t* Renderer::rgb() const { return impl_->rgb.data(); }
int Renderer::width() const { return impl_->W; }
int Renderer::height() const { return impl_->H; }
