// gta_demo.cpp - 验证引擎能做"开放世界开车游戏"(GTA类)：
// 可驾驶的车在体素开放地形上行驶(前进/转向/贴地)，追尾相机跟拍，渲染一帧证明。
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
// 一辆车：车身 + 顶 + 4个轮子
static Mesh makeCar() {
    Mesh m;
    addBox(m, -1.0f,0.35f,-0.55f, 1.0f,0.7f,0.55f);     // 车身
    addBox(m, -0.45f,0.7f,-0.5f, 0.45f,0.95f,0.5f);     // 顶
    // 4 轮子
    addBox(m, -0.85f,0,-0.65f, -0.5f,0.35f,-0.5f);
    addBox(m, 0.5f,0,-0.65f, 0.85f,0.35f,-0.5f);
    addBox(m, -0.85f,0,0.5f, -0.5f,0.35f,0.65f);
    addBox(m, 0.5f,0,0.5f, 0.85f,0.35f,0.65f);
    return m;
}

int main(int argc, char** argv) {
    const char* out = argc>1 ? argv[1] : "/tmp/db_gta.png";
    Framebuffer fb(640,360);

    Scene3 sc; sc.terrain = World(80,40,80); sc.terrain.generate();
    Mesh carMesh = makeCar();
    V3 carPos; float carYaw = 0.6f, speed = 0;

    // 模拟驾驶：加速 + 转向 + 贴地跑 240 帧
    for (int i=0;i<240;i++) {
        float dt = 1.0f/60.0f;
        bool throttle = true, steer = true;
        if (throttle) speed += 14.0f*dt;            // 前进加速
        if (speed > 9.0f) speed = 9.0f;
        if (steer) carYaw += 0.8f*dt;               // 转向
        carPos.x += std::sin(carYaw)*speed*dt;
        carPos.z += std::cos(carYaw)*speed*dt;
        // 贴地：按地形高度
        carPos.y = sc.groundAt((int)carPos.x, (int)carPos.z);
        // 撞到世界边缘就掉头
        if (carPos.x<2||carPos.x>sc.terrain.sx-3||carPos.z<2||carPos.z>sc.terrain.sz-3) carYaw += 3.0f;
    }
    printf("[gta] car at (%.1f,%.1f,%.1f) yaw=%.2f speed=%.1f  (开车模拟240帧)\n",
           carPos.x,carPos.y,carPos.z,carYaw,speed);

    sc.models.clear();
    sc.models.push_back(PlacedModel("car", carMesh, carPos, 1.0f, Color(190,70,50)));

    // 追尾视角：车后上方，看向车
    V3 camPos = carPos - V3(std::sin(carYaw), 0, std::cos(carYaw))*6.0f + V3(0,3.2f,0);
    V3 target = carPos + V3(0,0.8f,0);
    V3 dir = (target - camPos).norm();
    sc.cam.pos = camPos;
    sc.cam.yaw = std::atan2(dir.x, dir.z);
    sc.cam.pitch = std::asin(dir.y);

    TexturePack tp; tp.loadOrGenerate("textures");
    VoxelRenderer vr; MeshRenderer mr;
    sc.render(fb, vr, mr, tp, 26);
    fb.drawText("DBENGINE OPEN-WORLD DRIVING (GTA-like)", 8, 8, 1, Color(255,255,255));

    if (!save_png(fb, out)) { printf("save failed\n"); return 1; }
    printf("[gta] saved %s (追尾视角跟拍行驶中的车)\n", out);
    return 0;
}
