// big3d_demo.cpp - 验证引擎能做"非我的世界"的大型3D：纯3D模型场景(坦克+楼房+地面)
// 全程没有体素方块，全用 .obj 网格模型，证明引擎不锁死MC风格。
#include "../engine/framebuffer.hpp"
#include "../engine/voxel.hpp"
#include "../engine/texture.hpp"
#include "../engine/mesh.hpp"
#include "../engine/scene3.hpp"
#include <cstdio>
#include <cmath>
using namespace dbg;

static void addBox(Mesh& m, float x0,float y0,float z0, float x1,float y1,float z1) {
    size_t b = m.verts.size();
    m.verts.push_back(V3(x0,y0,z0)); m.verts.push_back(V3(x1,y0,z0)); m.verts.push_back(V3(x1,y1,z0)); m.verts.push_back(V3(x0,y1,z0));
    m.verts.push_back(V3(x0,y0,z1)); m.verts.push_back(V3(x1,y0,z1)); m.verts.push_back(V3(x1,y1,z1)); m.verts.push_back(V3(x0,y1,z1));
    m.addFace(b+0,b+2,b+1); m.addFace(b+0,b+3,b+2);
    m.addFace(b+1,b+6,b+5); m.addFace(b+1,b+2,b+6);
    m.addFace(b+5,b+7,b+4); m.addFace(b+5,b+6,b+7);
    m.addFace(b+4,b+3,b+0); m.addFace(b+4,b+7,b+3);
    m.addFace(b+3,b+7,b+6); m.addFace(b+3,b+6,b+2);
}
// 低多边形坦克：车体+炮塔+炮管
static Mesh makeTank() {
    Mesh m;
    addBox(m, -1.6f,0,  -0.9f,  1.6f,0.7f, 0.9f);       // 车体
    addBox(m, -0.9f,0.7f,-0.7f,  0.9f,1.1f,0.7f);      // 炮塔
    addBox(m, 0.4f, 0.8f,-0.22f, 2.6f,0.95f,0.22f);    // 炮管
    addBox(m, -1.6f, -0.25f, -1.1f, 1.6f, 0.0f, 1.1f); // 履带底座
    return m;
}
// 楼房：几层箱体摞起来
static Mesh makeBuilding(float w, float h) {
    Mesh m;
    addBox(m, 0,0,0, w, h, w*0.8f);
    addBox(m, w*0.25f, h, w*0.2f, w*0.75f, h+0.4f, w*0.6f); // 顶楼
    return m;
}

int main(int argc, char** argv) {
    const char* out = argc>1 ? argv[1] : "/tmp/db_big3d.png";
    Framebuffer fb(640, 360);

    // 纯模型场景：地面 + 坦克 + 两栋楼，不用体素
    std::vector<PlacedModel> models;
    models.push_back(PlacedModel("ground", makeBuilding(24.0f, 0.2f), V3(-2, 0, 0), 1.0f, Color(90,120,70)));
    models.push_back(PlacedModel("tank", makeTank(), V3(0, 0.2f, 0), 1.2f, Color(120,130,90)));
    models.push_back(PlacedModel("tower", makeBuilding(2.4f,6.0f), V3(6, 0.2f, 2), 1.0f, Color(150,90,70)));
    models.push_back(PlacedModel("tower2", makeBuilding(2.0f,4.5f), V3(-5, 0.2f, 3), 1.0f, Color(120,110,150)));

    Camera3 cam; cam.pos = V3(0, 4.2f, 9.0f);
    V3 target(-1, 1.2f, 0); V3 dir = (target - cam.pos).norm();
    cam.yaw = std::atan2(dir.x, dir.z);
    cam.pitch = std::asin(dir.y);

    // 画天空
    Framebuffer skyfb = fb;
    Scene3::drawSky(fb, Color(30,60,130), Color(170,200,240), V3(fb.w*0.8f, fb.h*0.15f, 0), 20.0f);

    MeshRenderer mr;
    std::vector<float> zbuf((size_t)fb.w*fb.h, 1e30f);
    mr.useZBuf(zbuf);
    V3 sun(0.5f, 0.8f, 0.3f);
    for (auto& m : models) mr.render(fb, m.mesh, cam, m.pos, m.scale, m.tint, sun);

    fb.drawText("DBENGINE BIG 3D (tank+buildings, NO voxel)", 8, 8, 1, Color(255,255,255));
    if (!save_png(fb, out)) { printf("save failed\n"); return 1; }
    printf("[big3d] saved %s (纯3D模型场景: 地面+坦克+2栋楼, 无体素方块)\n", out);
    return 0;
}
