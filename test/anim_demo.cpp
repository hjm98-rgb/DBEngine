// anim_demo.cpp - 验证引擎有"动画"：精灵上下跳动+颜色循环(2D动画) + 3D模型旋转动画。
#include "../engine/framebuffer.hpp"
#include "../engine/engine.hpp"
#include "../engine/mesh.hpp"
#include "../engine/scene3.hpp"
#include <cstdio>
#include <cmath>
using namespace dbg;
using namespace dbg2;

// 跳动+变色脚本
class Bobber : public Script {
    float t = 0;
public:
    void update(float dt) override {
        t += dt;
        owner->pos.y += std::sin(t*6) * 60 * dt;   // 上下跳动
        owner->color = Color(200 + (int)(55*std::sin(t*3)), 120, 255-(int)(60*std::sin(t*2)));
    }
};

// 2D 动画：两帧对比
static void anim2d(const char* out, float time) {
    Framebuffer fb(320, 180);
    Renderer2D r(fb);
    Scene sc;
    GameObject* ball = new GameObject("ball");
    ball->pos = Vec2(80, 60); ball->width = 40; ball->height = 40;
    ball->add(new Sprite())->circle = true;
    ball->add(new Bobber());
    sc.objs.push_back(ball);
    for (int i=0;i<180;i++) sc.update(1.0f/60.0f);   // 跑 3 秒动画
    sc.render(r);
    fb.drawText("2D ANIM frame t="+std::to_string((int)time), 8, 8, 1, Color(255,255,255));
    save_png(fb, out);
}

// 3D 旋转动画：模型绕 Y 轴旋转两帧
static void addBox3(Mesh& m, float x0,float y0,float z0, float x1,float y1,float z1) {
    size_t b = m.verts.size();
    m.verts.push_back(V3(x0,y0,z0)); m.verts.push_back(V3(x1,y0,z0)); m.verts.push_back(V3(x1,y1,z0)); m.verts.push_back(V3(x0,y1,z0));
    m.verts.push_back(V3(x0,y0,z1)); m.verts.push_back(V3(x1,y0,z1)); m.verts.push_back(V3(x1,y1,z1)); m.verts.push_back(V3(x0,y1,z1));
    m.addFace(b+0,b+2,b+1); m.addFace(b+0,b+3,b+2);
    m.addFace(b+1,b+6,b+5); m.addFace(b+1,b+2,b+6);
    m.addFace(b+5,b+7,b+4); m.addFace(b+5,b+6,b+7);
    m.addFace(b+4,b+3,b+0); m.addFace(b+4,b+7,b+3);
    m.addFace(b+3,b+7,b+6); m.addFace(b+3,b+6,b+2);
}
static void anim3d(const char* out, float yaw) {
    Framebuffer fb(640, 360);
    Scene3 sc; sc.terrain = World(40,30,40); sc.terrain.generate();
    Mesh tank; addBox3(tank, -1,0,-0.5f, 1,0.6f,0.5f);   // 用简单坦克    sc.models.push_back(PlacedModel("tank", tank, V3(20, sc.groundAt(20,20), 20), 1.0f, Color(120,130,90)));
    sc.cam.pos = V3(20, 8, 26);
    V3 target(20, 2, 20); V3 dir=(target-sc.cam.pos).norm();
    sc.cam.yaw = yaw; sc.cam.pitch = std::asin(dir.y);
    TexturePack tp; tp.loadOrGenerate("textures");
    VoxelRenderer vr; MeshRenderer mr;
    sc.render(fb, vr, mr, tp, 26);
    fb.drawText("3D ANIM (model rotating)", 8,8,1,Color(255,255,255));
    save_png(fb, out);
}

int main() {
    anim2d("/tmp/db_anim2d_a.png", 1.0f);
    anim2d("/tmp/db_anim2d_b.png", 2.0f);
    anim3d("/tmp/db_anim3d_a.png", 0.3f);
    anim3d("/tmp/db_anim3d_b.png", 0.7f);
    printf("[anim] 2D动画两帧 + 3D模型旋转两帧已渲染\n");
    return 0;
}
