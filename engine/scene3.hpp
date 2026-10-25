// scene3.hpp - DBEngine 3D 场景系统：天空 + 体素地形 + 可摆放的3D模型 + 玩家(重力/落地)
// + 模组加载器：mods/ 文件夹里丢 .obj 模型，运行时自动载入进场景。
#pragma once
#include "framebuffer.hpp"
#include "voxel.hpp"
#include "texture.hpp"
#include "mesh.hpp"
#include <string>
#include <vector>
#include <dirent.h>
#include <cstring>

namespace dbg {

// 场景里一个被摆放的 3D 模型（一个"对象/实体"）
struct PlacedModel {
    std::string name;
    Mesh mesh;
    V3 pos;
    float scale = 1.0f;
    Color tint = Color(200, 200, 200);
    PlacedModel() {}
    PlacedModel(const std::string& n, const Mesh& m, V3 p, float s, Color t)
        : name(n), mesh(m), pos(p), scale(s), tint(t) {}
};

// 模组加载器：扫描一个文件夹，把里面所有 .obj 作为可摆放模型载入
struct Mods {
    std::vector<PlacedModel> models;
    bool loadFolder(const std::string& dir) {
        DIR* d = opendir(dir.c_str());
        if (!d) return false;
        struct dirent* e;
        while ((e = readdir(d))) {
            std::string n = e->d_name;
            if (n.size() > 4 && n.substr(n.size()-4) == ".obj") {
                Mesh m;
                std::string path = dir + "/" + n;
                if (loadOBJ(path, m)) models.push_back(PlacedModel(n.substr(0, n.size()-4), m, V3(0,0,0), 1.0f, Color(200,200,200)));
            }
        }
        closedir(d);
        return !models.empty();
    }
    const PlacedModel* byName(const std::string& n) const {
        for (auto& m : models) if (m.name == n) return &m;
        return nullptr;
    }
};

// 3D 场景：统一天空 + 地形 + 模型 + 玩家
class Scene3 {
public:
    World terrain;                        // 体素地形
    std::vector<PlacedModel> models;      // 摆放的 3D 模型
    Camera3 cam;                          // 相机/玩家视角
    V3 playerPos, playerVel;              // 玩家位置/速度（重力）
    bool hasPlayer = true;
    std::vector<float> zbuf;              // 全场景共享深度缓冲

    // 地形在地表 (x,z) 处的地面高度（第一个实心方块的顶面）
    float groundAt(int x, int z) const {
        for (int y = terrain.sy-1; y >= 0; y--)
            if (terrain.solid(x,y,z)) return (float)y + 1.0f;
        return 0.0f;
    }

    // 玩家物理：重力 + 落到地形表面
    void physics(float dt) {
        if (!hasPlayer) return;
        playerVel.y -= 25.0f * dt;                 // 重力
        playerPos = playerPos + playerVel * dt;
        float g = groundAt((int)playerPos.x, (int)playerPos.z);
        if (playerPos.y < g + 0.2f) { playerPos.y = g + 0.2f; playerVel.y = 0; }
    }

    // 画天空渐变 + 太阳
    static void drawSky(Framebuffer& fb, Color top, Color horizon, V3 sunScreen, float sunR) {
        int w = fb.w, h = fb.h;
        for (int y = 0; y < h; y++) {
            float t = (float)y / (float)h;
            Color c((int)(top.r+(horizon.r-top.r)*t),
                    (int)(top.g+(horizon.g-top.g)*t),
                    (int)(top.b+(horizon.b-top.b)*t));
            uint32_t v = c.abgr();
            size_t row = (size_t)y*w;
            for (int x = 0; x < w; x++) fb.px[row+x] = v;
        }
        // 太阳
        int sx=(int)sunScreen.x, sy=(int)sunScreen.y;
        for (int y = sy-(int)sunR; y <= sy+(int)sunR; y++)
            for (int x = sx-(int)sunR; x <= sx+(int)sunR; x++) {
                if (x<0||y<0||x>=w||y>=h) continue;
                if ((float)((x-sx)*(x-sx)+(y-sy)*(y-sy)) <= sunR*sunR) fb.px[(size_t)y*w+x] = Color(255,235,160).abgr();
            }
    }

    void render(Framebuffer& fb, VoxelRenderer& vr, MeshRenderer& mr, const TexturePack& tp, int renderRange) {
        if (zbuf.size() != (size_t)fb.w*fb.h) zbuf.assign((size_t)fb.w*fb.h, 1e9f);
        // 天空渐变（天顶深蓝 → 地平线浅蓝）+ 太阳
        drawSky(fb, Color(40,80,160), Color(150,190,240), V3(fb.w*0.75f, fb.h*0.18f, 0), 18.0f);
        // 地形（复用外部深度缓冲，与模型互相遮挡）
        vr.renderInto(fb, cam, terrain, renderRange, tp, zbuf);
        // 摆放的 3D 模型
        mr.useZBuf(zbuf);
        V3 sun(0.4f, 0.85f, 0.4f);
        for (auto& m : models) mr.render(fb, m.mesh, cam, m.pos, m.scale, m.tint, sun);
    }
};

// 模型的 AABB 世界包围盒（用于射线命中检测）
inline void modelBounds(const PlacedModel& m, V3& bmin, V3& bmax) {
    if (m.mesh.verts.empty()) { bmin=bmax=m.pos; return; }
    bmin = m.mesh.verts[0]; bmax = m.mesh.verts[0];
    for (auto& v : m.mesh.verts) {
        bmin.x = std::min(bmin.x, v.x); bmin.y = std::min(bmin.y, v.y); bmin.z = std::min(bmin.z, v.z);
        bmax.x = std::max(bmax.x, v.x); bmax.y = std::max(bmax.y, v.y); bmax.z = std::max(bmax.z, v.z);
    }
    bmin = bmin * m.scale + m.pos;
    bmax = bmax * m.scale + m.pos;
}

// 射线 vs AABB（slab 法）。返回是否命中，t 为最近距离。
inline bool rayAABB(const V3& o, const V3& d, const V3& bmin, const V3& bmax, float& t) {
    float tmin = -1e9f, tmax = 1e9f;
    for (int a = 0; a < 3; a++) {
        float ori = (a==0)?o.x:(a==1?o.y:o.z), dir = (a==0)?d.x:(a==1?d.y:d.z);
        float lo = (a==0)?bmin.x:(a==1?bmin.y:bmin.z), hi = (a==0)?bmax.x:(a==1?bmax.y:bmax.z);
        if (std::fabs(dir) < 1e-8f) { if (ori < lo || ori > hi) return false; }
        else { float t1 = (lo-ori)/dir, t2 = (hi-ori)/dir; if (t1>t2) std::swap(t1,t2); tmin=std::max(tmin,t1); tmax=std::min(tmax,t2); if (tmin>tmax) return false; }
    }
    t = tmin > 0 ? tmin : tmax;
    return t >= 0 && t < 1e9f;
}

// 射线打中最近的模型（返回命中项，最近距离写 t）
inline PlacedModel* hitTestModels(std::vector<PlacedModel>& models, const V3& o, const V3& d, float maxD, float& t) {
    PlacedModel* best = nullptr; t = maxD;
    for (auto& m : models) {
        V3 lo, hi; modelBounds(m, lo, hi);
        float tt;
        if (rayAABB(o, d, lo, hi, tt) && tt < t) { t = tt; best = &m; }
    }
    return best;
}

} // namespace dbg
