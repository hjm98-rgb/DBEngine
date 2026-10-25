// scene_demo.cpp - 验证 DBEngine 3D 场景系统：天空+体素地形+摆放的3D模型+玩家(重力)
// 模型来自 mods/ 文件夹（模组加载），证明引擎能载入玩家自己放的 .obj 模型。
#include "../engine/framebuffer.hpp"
#include "../engine/voxel.hpp"
#include "../engine/texture.hpp"
#include "../engine/mesh.hpp"
#include "../engine/scene3.hpp"
#include <cstdio>
using namespace dbg;

// 程序生成一个简单盒子网格（用于造树）
static void addBox(Mesh& m, float x0,float y0,float z0, float x1,float y1,float z1) {
    size_t b = m.verts.size();
    m.verts.push_back(V3(x0,y0,z0)); m.verts.push_back(V3(x1,y0,z0)); m.verts.push_back(V3(x1,y1,z0)); m.verts.push_back(V3(x0,y1,z0));
    m.verts.push_back(V3(x0,y0,z1)); m.verts.push_back(V3(x1,y0,z1)); m.verts.push_back(V3(x1,y1,z1)); m.verts.push_back(V3(x0,y1,z1));
    // 四面 + 顶面（底面不画）
    m.addFace(b+0,b+2,b+1); m.addFace(b+0,b+3,b+2);   // 前
    m.addFace(b+1,b+6,b+5); m.addFace(b+1,b+2,b+6);   // 右
    m.addFace(b+5,b+7,b+4); m.addFace(b+5,b+6,b+7);   // 后
    m.addFace(b+4,b+3,b+0); m.addFace(b+4,b+7,b+3);   // 左
    m.addFace(b+3,b+7,b+6); m.addFace(b+3,b+6,b+2);   // 顶
}
static Mesh makeTree() {
    Mesh m;
    addBox(m, -0.15f,0,0,   0.45f,1,0.6f);   // 树干
    addBox(m, -0.7f,1,-0.7f, 1.0f,2.3,1.0f);  // 树冠
    return m;
}

int main(int argc, char** argv) {
    const char* out = argc>1 ? argv[1] : "/tmp/db_scene.png";
    Framebuffer fb(640,360);

    Scene3 sc;
    sc.terrain = World(64,40,64);
    sc.terrain.generate();

    // 模组加载：mods/ 文件夹里的 .obj
    Mods mods;
    bool modsOk = mods.loadFolder("mods");
    printf("[scene] mods loaded=%s (models=%zu)\n", modsOk?"yes":"no", mods.models.size());

    // 摆房子（用模组里的 house.obj）
    if (const PlacedModel* h = mods.byName("house"))
        sc.models.push_back(PlacedModel("house", h->mesh, V3(30, sc.groundAt(30,28), 28), 3.0f, Color(205,120,60)));

    // 摆树（用模组里的 tree.obj + 程序树）
    if (const PlacedModel* t = mods.byName("tree"))
        sc.models.push_back(PlacedModel("tree", t->mesh, V3(20, sc.groundAt(20,22), 22), 1.0f, Color(70,150,60)));
    for (int i=0;i<3;i++) { int x=14+i*7, z=18+i*9;
        sc.models.push_back(PlacedModel("tree", makeTree(), V3(x, sc.groundAt(x,z), z), 1.0f, Color(70,150,60)));
    }

    // 玩家
    sc.hasPlayer = true;
    sc.playerPos = V3(8, sc.groundAt(8,8)+1.5f, 8);
    for (int i=0;i<30;i++) sc.physics(1.0f/60.0f);   // 重力落地验证
    sc.cam.pos = sc.playerPos;
    sc.cam.yaw = 0.72f; sc.cam.pitch = -0.10f;        // 朝房子方向看

    TexturePack tp; tp.loadOrGenerate("textures");
    VoxelRenderer vr; MeshRenderer mr;
    sc.render(fb, vr, mr, tp, 26);
    fb.drawText("DBENGINE SCENE3: sky+terrain+models+mods+player", 8, 8, 1, Color(255,255,255));

    if (!save_png(fb, out)) { printf("save failed\n"); return 1; }
    printf("saved %s (player landed at y=%.1f)\n", out, sc.playerPos.y);
    return 0;
}
