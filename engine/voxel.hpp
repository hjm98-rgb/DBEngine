// voxel.hpp - 3D 体素世界与软件渲染器（我的世界风格方块世界）
// 完全自写：逐面投影 + 深度缓冲的光栅化，不依赖任何第三方引擎。
#pragma once
#include "framebuffer.hpp"
#include "texture.hpp"
#include <vector>
#include <cmath>

namespace dbg {

struct V3 {
    float x, y, z;
    V3() : x(0), y(0), z(0) {}
    V3(float x, float y, float z) : x(x), y(y), z(z) {}
    V3 operator+(const V3& o) const { return V3(x+o.x, y+o.y, z+o.z); }
    V3 operator-(const V3& o) const { return V3(x-o.x, y-o.y, z-o.z); }
    V3 operator*(float s) const { return V3(x*s, y*s, z*s); }
    float dot(const V3& o) const { return x*o.x + y*o.y + z*o.z; }
    V3 cross(const V3& o) const { return V3(y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x); }
    float len() const { return std::sqrt(x*x+y*y+z*z); }
    V3 norm() const { float l = len(); return l>0 ? V3(x/l,y/l,z/l) : V3(0,0,0); }
};

// 相机：位置 + 水平角(yaw) + 俯仰角(pitch)
struct Camera3 {
    V3 pos;
    float yaw = 0.0f, pitch = 0.0f;
    V3 forward() const {
        return V3(std::cos(pitch)*std::sin(yaw), std::sin(pitch), std::cos(pitch)*std::cos(yaw)).norm();
    }
    V3 right() const { return V3(std::cos(yaw), 0, -std::sin(yaw)).norm(); }
    V3 up() const { return forward().cross(right()).norm(); }
};

// 方块类型、调色板、名称见 texture.hpp（此处共用）

// 方块世界：三维网格
class World {
public:
    int sx, sy, sz;                       // x,y,z 尺寸
    std::vector<uint8_t> blocks;          // sy 为竖直向上
    World() : sx(1), sy(1), sz(1) {}
    World(int sx, int sy, int sz) : sx(sx), sy(sy), sz(sz), blocks((size_t)sx*sy*sz, B_AIR) {}

    inline size_t idx(int x, int y, int z) const { return ((size_t)y*sx + x)*sz + z; }
    inline bool in(int x, int y, int z) const { return x>=0 && x<sx && y>=0 && y<sy && z>=0 && z<sz; }
    inline uint8_t get(int x, int y, int z) const { return in(x,y,z) ? blocks[idx(x,y,z)] : B_AIR; }
    inline void set(int x, int y, int z, uint8_t b) { if (in(x,y,z)) blocks[idx(x,y,z)] = b; }
    inline bool solid(int x, int y, int z) const { return get(x,y,z) != B_AIR; }

    // 生成一个起伏的地形
    void generate() {
        for (int x = 0; x < sx; x++) for (int z = 0; z < sz; z++) {
            float h = std::sin(x*0.13f)*3.0f + std::sin(z*0.17f+1.0f)*3.0f + 4.0f;
            int hh = (int)std::round(h); if (hh < 1) hh = 1; if (hh >= sy-2) hh = sy-3;
            for (int y = 0; y <= hh; y++) {
                uint8_t b = (y == hh) ? B_GRASS : (y > hh-3 ? B_DIRT : B_STONE);
                set(x, y, z, b);
            }
        }
    }
};

// 软件体素渲染器
class VoxelRenderer {
    std::vector<float> zbuf;
    int fw = 0, fh = 0;
    float f = 1.0f;
public:
    int lastFaces = 0;   // 诊断：本次渲染提交的面数
private:

    // 面光强度（按朝向）
    static float faceLight(int axis, int dir) {
        if (axis == 1 && dir > 0) return 1.0f;      // 顶面最亮
        if (axis == 1) return 0.5f;                 // 底面
        if (axis == 2 && dir > 0) return 0.85f;     // +Z 前面
        if (axis == 2) return 0.6f;                // -Z 后面
        return 0.7f;                                // ±X 侧面
    }

    struct Vert { float sx, sy, d; float u = 0, v = 0; };

    // 把世界点投影到屏幕
    Vert project(const V3& w, const Camera3& cam, int ww, int hh) {
        V3 d = w - cam.pos;
        V3 R = cam.right(), U = cam.up(), F = cam.forward();
        float cx = d.dot(R), cy = d.dot(U), cz = d.dot(F);
        return Vert{ (float)ww/2 + cx/cz*f, (float)hh/2 - cy/cz*f, cz };
    }

    void fillTri(const Vert& a, const Vert& b, const Vert& c, uint32_t col, Framebuffer& fb) {
        int ww = fb.w, hh = fb.h;
        int minX = (int)std::floor(std::min(a.sx, std::min(b.sx, c.sx)));
        int maxX = (int)std::ceil(std::max(a.sx, std::max(b.sx, c.sx)));
        int minY = (int)std::floor(std::min(a.sy, std::min(b.sy, c.sy)));
        int maxY = (int)std::ceil(std::max(a.sy, std::max(b.sy, c.sy)));
        if (minX < 0) minX = 0; if (maxX >= ww) maxX = ww-1;
        if (minY < 0) minY = 0; if (maxY >= hh) maxY = hh-1;
        float denom = (b.sy-c.sy)*(a.sx-c.sx) + (c.sx-b.sx)*(a.sy-c.sy);
        if (denom == 0.0f) return;
        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                float w1 = ((b.sy-c.sy)*(x-c.sx) + (c.sx-b.sx)*(y-c.sy)) / denom;
                float w2 = ((c.sy-a.sy)*(x-c.sx) + (a.sx-c.sx)*(y-c.sy)) / denom;
                float w3 = 1.0f - w1 - w2;
                if (w1 < 0 || w2 < 0 || w3 < 0) continue;
                float depth = w1*a.d + w2*b.d + w3*c.d;
                size_t i = (size_t)y*ww + x;
                if (depth > 0 && depth < zbuf[i]) { zbuf[i] = depth; fb.px[i] = col; }
            }
        }
    }

    // 画一个面的四条边：按 2 个三角形填充
    void fillQuad(const V3& p0, const V3& p1, const V3& p2, const V3& p3,
                  const Camera3& cam, Framebuffer& fb, uint32_t col) {
        ++lastFaces;
        int ww = fb.w, hh = fb.h;
        Vert a = project(p0,cam,ww,hh), b = project(p1,cam,ww,hh),
             c = project(p2,cam,ww,hh), d = project(p3,cam,ww,hh);
        fillTri(a,b,c,col,fb);
        fillTri(a,c,d,col,fb);
    }

    // 贴图颜色乘光照
    static uint32_t applyLight(uint32_t col, float l) {
        int r = (int)((col & 255) * l); if (r>255) r=255;
        int g = (int)(((col>>8)&255) * l); if (g>255) g=255;
        int b = (int)(((col>>16)&255) * l); if (b>255) b=255;
        return 0xff000000 | ((uint32_t)b<<16) | ((uint32_t)g<<8) | (uint32_t)r;
    }

    // 透视正确 + 贴图三角形填充
    void fillTriTextured(const Vert& a, const Vert& b, const Vert& c, const Texture& tex,
                         float light, Framebuffer& fb) {
        int ww = fb.w, hh = fb.h;
        int minX = (int)std::floor(std::min(a.sx, std::min(b.sx, c.sx)));
        int maxX = (int)std::ceil(std::max(a.sx, std::max(b.sx, c.sx)));
        int minY = (int)std::floor(std::min(a.sy, std::min(b.sy, c.sy)));
        int maxY = (int)std::ceil(std::max(a.sy, std::max(b.sy, c.sy)));
        if (minX < 0) minX = 0; if (maxX >= ww) maxX = ww-1;
        if (minY < 0) minY = 0; if (maxY >= hh) maxY = hh-1;
        float denom = (b.sy-c.sy)*(a.sx-c.sx) + (c.sx-b.sx)*(a.sy-c.sy);
        if (denom == 0.0f) return;
        float ia = 1.0f/a.d, ib = 1.0f/b.d, ic = 1.0f/c.d;
        float ua = a.u*ia, va = a.v*ia, ub = b.u*ib, vb = b.v*ib, uc = c.u*ic, vc = c.v*ic;
        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                float w1 = ((b.sy-c.sy)*(x-c.sx) + (c.sx-b.sx)*(y-c.sy)) / denom;
                float w2 = ((c.sy-a.sy)*(x-c.sx) + (a.sx-c.sx)*(y-c.sy)) / denom;
                float w3 = 1.0f - w1 - w2;
                if (w1 < 0 || w2 < 0 || w3 < 0) continue;
                float inv = w1*ia + w2*ib + w3*ic;
                float d = 1.0f/inv;
                size_t i = (size_t)y*ww + x;
                if (d > 0 && d < zbuf[i]) {
                    float u = (w1*ua + w2*ub + w3*uc) * d;
                    float v = (w1*va + w2*vb + w3*vc) * d;
                    zbuf[i] = d;
                    fb.px[i] = applyLight(tex.sample(u, v), light);
                }
            }
        }
    }

    // 贴图四边形（4 角各带 uv）
    void fillQuadTextured(const V3& p0, const V3& p1, const V3& p2, const V3& p3,
                          const Camera3& cam, const Texture& tex, float light, Framebuffer& fb,
                          float u0,float v0,float u1,float v1,float u2,float v2,float u3,float v3) {
        int ww = fb.w, hh = fb.h;
        Vert a = project(p0,cam,ww,hh), b = project(p1,cam,ww,hh),
             c = project(p2,cam,ww,hh), d = project(p3,cam,ww,hh);
        a.u=u0; a.v=v0; b.u=u1; b.v=v1; c.u=u2; c.v=v2; d.u=u3; d.v=v3;
        fillTriTextured(a,b,c,tex,light,fb);
        fillTriTextured(a,c,d,tex,light,fb);
    }

public:
    void render(Framebuffer& fb, const Camera3& cam, const World& w, int renderRange, const TexturePack& tp) {
        if (fw != fb.w || fh != fb.h) { fw = fb.w; fh = fb.h; zbuf.assign((size_t)fw*fh, 1e9f); }
        std::fill(zbuf.begin(), zbuf.end(), 1e9f);
        lastFaces = 0;
        float vfov = 70.0f * 3.14159265f / 180.0f;
        f = (float)fb.h / 2.0f / std::tan(vfov/2.0f);
        fb.clear(Color(110, 170, 235)); // 天空

        int cx = (int)std::floor(cam.pos.x), cy = (int)std::floor(cam.pos.y), cz = (int)std::floor(cam.pos.z);
        int x0 = cx - renderRange, x1 = cx + renderRange;
        int y0 = cy - renderRange/2, y1 = cy + renderRange;
        int z0 = cz - renderRange, z1 = cz + renderRange;
        if (x0 < 0) x0 = 0; if (x1 >= w.sx) x1 = w.sx-1;
        if (y0 < 0) y0 = 0; if (y1 >= w.sy) y1 = w.sy-1;
        if (z0 < 0) z0 = 0; if (z1 >= w.sz) z1 = w.sz-1;

        for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++)
        for (int z = z0; z <= z1; z++) {
            uint8_t id = w.get(x,y,z);
            if (id == B_AIR) continue;
            // 距离剔除
            float dx = x + 0.5f - cam.pos.x, dy = y + 0.5f - cam.pos.y, dz = z + 0.5f - cam.pos.z;
            if (dx*dx+dy*dy+dz*dz > (float)(renderRange+2)*(renderRange+2)) continue;
            float c = (float)x+0.5f, cc=(float)y+0.5f, ccc=(float)z+0.5f;
            Color base = blockColor(id);
            // 六个面，仅当相邻为空时绘制（外法线朝外）
            // +X 面 (x+1)
            if (!w.solid(x+1,y,z)) {
                V3 p0(c+0.5f,y,   z), p1(c+0.5f,y,   z+1), p2(c+0.5f,y+1,z+1), p3(c+0.5f,y+1,z);
                const Texture* t = tp.face(id, 0);
                if (t) fillQuadTextured(p0,p1,p2,p3,cam,*t,0.7f,fb, 0,0, 1,0, 1,1, 0,1);
                else fillQuad(p0,p1,p2,p3,cam,fb, Color((int)(base.r*0.7f),(int)(base.g*0.7f),(int)(base.b*0.7f)).abgr());
            }
            // -X 面
            if (!w.solid(x-1,y,z)) {
                V3 p0(c-0.5f,y,   z+1), p1(c-0.5f,y,   z), p2(c-0.5f,y+1,z), p3(c-0.5f,y+1,z+1);
                const Texture* t = tp.face(id, 0);
                if (t) fillQuadTextured(p0,p1,p2,p3,cam,*t,0.7f,fb, 0,0, 1,0, 1,1, 0,1);
                else fillQuad(p0,p1,p2,p3,cam,fb, Color((int)(base.r*0.7f),(int)(base.g*0.7f),(int)(base.b*0.7f)).abgr());
            }
            // +Y 顶面（最亮）
            if (!w.solid(x,y+1,z)) {
                V3 p0(x,   c+1,z), p1(x+1,c+1,z), p2(x+1,c+1,z+1), p3(x,   c+1,z+1);
                const Texture* t = tp.face(id, 1);
                if (t) fillQuadTextured(p0,p1,p2,p3,cam,*t,1.0f,fb, 0,0, 1,0, 1,1, 0,1);
                else fillQuad(p0,p1,p2,p3,cam,fb, base.abgr());
            }
            // -Y 底面
            if (!w.solid(x,y-1,z)) {
                V3 p0(x,   c-1,z+1), p1(x+1,c-1,z+1), p2(x+1,c-1,z), p3(x,   c-1,z);
                const Texture* t = tp.face(id, -1);
                if (t) fillQuadTextured(p0,p1,p2,p3,cam,*t,0.5f,fb, 0,0, 1,0, 1,1, 0,1);
                else fillQuad(p0,p1,p2,p3,cam,fb, Color((int)(base.r*0.5f),(int)(base.g*0.5f),(int)(base.b*0.5f)).abgr());
            }
            // +Z 面
            if (!w.solid(x,y,z+1)) {
                V3 p0(x+1,y,   c+1), p1(x,   y,   c+1), p2(x,   y+1,c+1), p3(x+1,y+1,c+1);
                const Texture* t = tp.face(id, 0);
                if (t) fillQuadTextured(p0,p1,p2,p3,cam,*t,0.85f,fb, 1,0, 0,0, 0,1, 1,1);
                else fillQuad(p0,p1,p2,p3,cam,fb, Color((int)(base.r*0.85f),(int)(base.g*0.85f),(int)(base.b*0.85f)).abgr());
            }
            // -Z 面
            if (!w.solid(x,y,z-1)) {
                V3 p0(x,   y,   c-1), p1(x+1,y,   c-1), p2(x+1,y+1,c-1), p3(x,   y+1,c-1);
                const Texture* t = tp.face(id, 0);
                if (t) fillQuadTextured(p0,p1,p2,p3,cam,*t,0.6f,fb, 0,0, 1,0, 1,1, 0,1);
                else fillQuad(p0,p1,p2,p3,cam,fb, Color((int)(base.r*0.6f),(int)(base.g*0.6f),(int)(base.b*0.6f)).abgr());
            }
        }
    }
};

} // namespace dbg
