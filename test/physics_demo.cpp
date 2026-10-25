// physics_demo.cpp - 验证引擎有"物理"：一个模型从高处掉下，重力让它落到地形表面(带弹跳)。
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

int main(int argc, char** argv) {
    const char* out = argc>1 ? argv[1] : "/tmp/db_physics.png";
    Framebuffer fb(640,360);
    Scene3 sc; sc.terrain = World(60,40,60); sc.terrain.generate();

    // 两个可动方块：从高处放下
    Mesh box; addBox(box, -0.5f,0,-0.5f, 0.5f,0.5f,0.5f);
    sc.models.push_back(PlacedModel("boxA", box, V3(20, 18.0f, 20), 1.0f, Color(200,60,60)));
    sc.models.push_back(PlacedModel("boxB", box, V3(26, 22.0f, 20), 1.0f, Color(60,120,220)));

    float groundA = sc.groundAt(20,20), groundB = sc.groundAt(26,20);
    printf("[physics] start A=%.1f(ground %.1f) B=%.1f(ground %.1f)\n", sc.models[0].pos.y, groundA, sc.models[1].pos.y, groundB);

    // 物理步进：120 帧重力下落 + 落地
    Physics3 ph; ph.bounce = 0.4f;
    for (int i=0;i<120;i++) ph.step(sc.models, sc.terrain, 1.0f/60.0f);

    printf("[physics] after 120帧: A landed=%.1f B landed=%.1f\n", sc.models[0].pos.y, sc.models[1].pos.y);
    bool ok = sc.models[0].pos.y < 10.0f && sc.models[1].pos.y < 10.0f;

    // 渲染落地后的场景
    sc.cam.pos = V3(23, 12, 26);
    V3 target(23, 4, 20); V3 dir=(target-sc.cam.pos).norm();
    sc.cam.yaw=std::atan2(dir.x,dir.z); sc.cam.pitch=std::asin(dir.y);
    TexturePack tp; tp.loadOrGenerate("textures");
    VoxelRenderer vr; MeshRenderer mr;
    sc.render(fb, vr, mr, tp, 26);
    fb.drawText("DBENGINE PHYSICS (gravity+land+bounce)", 8,8,1,Color(255,255,255));
    if (!save_png(fb, out)) { printf("save failed\n"); return 1; }
    printf("[physics] saved %s (两个方块从高处重力落地)\n", out);
    return ok ? 0 : 1;
}
