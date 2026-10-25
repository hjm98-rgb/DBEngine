// mesh.hpp - DBEngine 3D 网格模型系统：载入真实 3D 模型(.obj)、透视投影渲染。
// 这才是"真 3D 不只是像素方块"的部分：任何 .obj 模型都能显示。
#pragma once
#include "framebuffer.hpp"
#include "voxel.hpp"   // 复用 V3 / Camera3
#include <vector>
#include <string>
#include <cstdio>
#include <algorithm>

namespace dbg {

// 网格：顶点 + 三角形面
struct Mesh {
    std::vector<V3> verts;
    std::vector<int> faces;               // 每 3 个 int 一个三角形
    size_t nFaces() const { return faces.size() / 3; }
    void addFace(int a, int b, int c) { faces.push_back(a); faces.push_back(b); faces.push_back(c); }
};

// 从 Wavefront .obj 载入模型（支持 v 顶点、f 三角形面）
inline bool loadOBJ(const std::string& path, Mesh& m) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    char line[1024];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            float x, y, z;
            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) == 3) m.verts.push_back(V3(x, y, z));
        } else if (line[0] == 'f') {
            int a, b, c;
            if (sscanf(line + 2, "%d %d %d", &a, &b, &c) == 3) m.addFace(a - 1, b - 1, c - 1);
        }
    }
    fclose(f);
    return !m.faces.empty();
}

// 3D 网格渲染器：透视 + 背面剔除 + 简单光照(Lambert) + 深度缓冲
class MeshRenderer {
    std::vector<float> zbuf;
    std::vector<float>* zActive = nullptr;   // 场景共享深度缓冲时指向它
    int fw = 0, fh = 0;
    float f = 150.0f;   // 焦距：放大远处模型（体素版是 f=1 靠满屏方块）
public:
    int lastFaces = 0;
    void useZBuf(std::vector<float>& z) { zActive = &z; }

    void render(Framebuffer& fb, const Mesh& m, const Camera3& cam,
                V3 obj, float scale, Color base, V3 lightDir = V3(0.4f, 0.8f, 0.4f)) {
        int ww = fb.w, hh = fb.h;
        std::vector<float>& Z = zActive ? *zActive : zbuf;
        if (zActive) { if ((size_t)Z.size() < (size_t)ww*hh) Z.assign((size_t)ww*hh, 1e30f); }
        else if ((int)zbuf.size() != ww*hh) { zbuf.assign((size_t)ww*hh, 1e30f); fw = ww; fh = hh; }
        std::fill(Z.begin(), Z.end(), 1e30f);
        lastFaces = 0;

        V3 R = cam.right(), U = cam.up(), F = cam.forward();
        lightDir = lightDir.norm();
        V3 viewDir = F;

        for (size_t i = 0; i + 2 < m.faces.size(); i += 3) {
            V3 wA = obj + m.verts[m.faces[i]] * scale;
            V3 wB = obj + m.verts[m.faces[i + 1]] * scale;
            V3 wC = obj + m.verts[m.faces[i + 2]] * scale;

            V3 n = (wB - wA).cross(wC - wA).norm();
            if (n.dot(wA - cam.pos) >= 0) continue;   // 背面剔除
            lastFaces++;

            // 顶点投影
            auto proj = [&](const V3& w) -> float { return (float)ww / 2 + (w - cam.pos).dot(R) / (w - cam.pos).dot(F) * f; };
            auto projy = [&](const V3& w) -> float { return (float)hh / 2 - (w - cam.pos).dot(U) / (w - cam.pos).dot(F) * f; };
            auto dep = [&](const V3& w) -> float { return (w - cam.pos).dot(F); };

            float ax = proj(wA), ay = projy(wA), da = dep(wA);
            float bx = proj(wB), by = projy(wB), db = dep(wB);
            float cx = proj(wC), cy = projy(wC), dc = dep(wC);

            // 背面剔除后给正面一个光照（面向光源的三角更亮）
            float lum = 0.35f + 0.65f * std::max(0.0f, n.dot(lightDir));
            uint32_t col = Color((uint8_t)(base.r * lum), (uint8_t)(base.g * lum), (uint8_t)(base.b * lum)).abgr();

            // 光栅化 + 深度测试
            int minX = (int)std::floor(std::min(ax, std::min(bx, cx))), maxX = (int)std::ceil(std::max(ax, std::max(bx, cx)));
            int minY = (int)std::floor(std::min(ay, std::min(by, cy))), maxY = (int)std::ceil(std::max(ay, std::max(by, cy)));
            minX = std::max(0, minX); maxX = std::min(ww - 1, maxX);
            minY = std::max(0, minY); maxY = std::min(hh - 1, maxY);

            for (int y = minY; y <= maxY; y++) {
                for (int x = minX; x <= maxX; x++) {
                    // 重心坐标
                    float det = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
                    if (det == 0) continue;
                    float px = (float)x + 0.5f, py = (float)y + 0.5f;
                    // 面积坐标：一致叉乘顺序，保证 w0+w1+w2=det
                    float w0 = ((bx - px) * (cy - py) - (by - py) * (cx - px)) / det; // cross(B-P, C-P)
                    float w1 = ((cx - px) * (ay - py) - (cy - py) * (ax - px)) / det; // cross(C-P, A-P)
                    float w2 = ((ax - px) * (by - py) - (ay - py) * (bx - px)) / det; // cross(A-P, B-P)
                    if (w0 < -0.01f || w1 < -0.01f || w2 < -0.01f) continue;
                    float d = w0 * da + w1 * db + w2 * dc;
                    if (d <= 0.1f) continue;
                    int idx = y * ww + x;
                    std::vector<float>& Z = zActive ? *zActive : zbuf;
                    if (d < Z[idx]) { Z[idx] = d; fb.setPixel(x, y, col); }
                }
            }
        }
    }

    // 带贴图的网格渲染：平面UV把 Texture 贴到模型上 + 光照（这才是"真模型"的关键）
    void renderTextured(Framebuffer& fb, const Mesh& m, const Camera3& cam,
                        V3 obj, float scale, const Texture& tex, float tscale = 1.0f,
                        V3 lightDir = V3(0.4f, 0.8f, 0.4f)) {
        int ww = fb.w, hh = fb.h;
        std::vector<float>& Z = zActive ? *zActive : zbuf;
        if (zActive) { if ((size_t)Z.size() < (size_t)ww*hh) Z.assign((size_t)ww*hh, 1e30f); }
        else if ((int)zbuf.size() != ww*hh) { zbuf.assign((size_t)ww*hh, 1e30f); fw=ww; fh=hh; }
        std::fill(Z.begin(), Z.end(), 1e30f);
        lastFaces = 0;

        // 模型包围盒 → 平面UV范围
        V3 lo, hi; if (m.verts.empty()) return;
        lo = hi = m.verts[0];
        for (auto& v : m.verts) { lo.x=std::min(lo.x,v.x);lo.y=std::min(lo.y,v.y);lo.z=std::min(lo.z,v.z);
                                  hi.x=std::max(hi.x,v.x);hi.y=std::max(hi.y,v.y);hi.z=std::max(hi.z,v.z); }
        float dw = (hi.x-lo.x)>0.01f?(hi.x-lo.x):1, dd = (hi.z-lo.z)>0.01f?(hi.z-lo.z):1;

        V3 R = cam.right(), U = cam.up(), F = cam.forward();
        lightDir = lightDir.norm();

        for (size_t i = 0; i + 2 < m.faces.size(); i += 3) {
            V3 wA = obj + m.verts[m.faces[i]] * scale;
            V3 wB = obj + m.verts[m.faces[i + 1]] * scale;
            V3 wC = obj + m.verts[m.faces[i + 2]] * scale;
            V3 n = (wB - wA).cross(wC - wA).norm();
            if (n.dot(wA - cam.pos) >= 0) continue;
            lastFaces++;
            float lum = 0.35f + 0.65f * std::max(0.0f, n.dot(lightDir));

            auto proj = [&](const V3& w) -> float { return (float)ww/2 + (w-cam.pos).dot(R)/(w-cam.pos).dot(F)*f; };
            auto projy = [&](const V3& w) -> float { return (float)hh/2 - (w-cam.pos).dot(U)/(w-cam.pos).dot(F)*f; };
            auto dep  = [&](const V3& w) -> float { return (w-cam.pos).dot(F); };
            float ax=proj(wA), ay=projy(wA), da=dep(wA);
            float bx=proj(wB), by=projy(wB), db=dep(wB);
            float cx=proj(wC), cy=projy(wC), dc=dep(wC);

            // 每顶点 UV（平面投影 + 平铺）
            auto uv = [&](const V3& w) -> V3 {
                return V3((w.x-lo.x*scale-obj.x)/dw, (w.z-lo.z*scale-obj.z)/dd, 0);
            };
            V3 uva=uv(wA), uvb=uv(wB), uvc=uv(wC);

            int minX=std::max(0,(int)std::floor(std::min(ax,std::min(bx,cx)))), maxX=std::min(ww-1,(int)std::ceil(std::max(ax,std::max(bx,cx))));
            int minY=std::max(0,(int)std::floor(std::min(ay,std::min(by,cy)))), maxY=std::min(hh-1,(int)std::ceil(std::max(ay,std::max(by,cy))));
            for (int y=minY; y<=maxY; y++) for (int x=minX; x<=maxX; x++) {
                float det=(bx-ax)*(cy-ay)-(by-ay)*(cx-ax); if (det==0) continue;
                float pxf=(float)x+0.5f, pyf=(float)y+0.5f;
                float w0=((bx-pxf)*(cy-pyf)-(by-pyf)*(cx-pxf))/det;
                float w1=((cx-pxf)*(ay-pyf)-(cy-pyf)*(ax-pxf))/det;
                float w2=((ax-pxf)*(by-pyf)-(ay-pyf)*(bx-pxf))/det;
                if (w0<-0.01f||w1<-0.01f||w2<-0.01f) continue;
                float d=w0*da+w1*db+w2*dc; if (d<=0.1f) continue;
                float u=w0*uva.x+w1*uvb.x+w2*uvc.x;
                float v=w0*uva.y+w1*uvb.y+w2*uvc.y;
                uint32_t tcol = tex.sample(u*tscale, v*tscale);
                // 乘光照
                int r=(int)((tcol&255)*lum); if(r>255)r=255;
                int g=(int)(((tcol>>8)&255)*lum); if(g>255)g=255;
                int b=(int)(((tcol>>16)&255)*lum); if(b>255)b=255;
                uint32_t col = 0xff000000|((uint32_t)b<<16)|((uint32_t)g<<8)|(uint32_t)r;
                int idx=y*ww+x;
                if (d < Z[idx]) { Z[idx]=d; fb.setPixel(x,y,col); }
            }
        }
    }
};

} // namespace dbg
