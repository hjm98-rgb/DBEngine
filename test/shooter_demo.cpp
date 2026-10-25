// shooter_demo.cpp - 验证引擎能做"射击玩法"(FPS向)：射线打中场景里的3D模型=命中消除+得分。
#include "../engine/framebuffer.hpp"
#include "../engine/voxel.hpp"
#include "../engine/texture.hpp"
#include "../engine/mesh.hpp"
#include "../engine/scene3.hpp"
#include <cstdio>
#include <cmath>
using namespace dbg;

int main(int argc, char** argv) {
    const char* base = argc>1 ? argv[1] : "/tmp/db_shoot";
    Framebuffer fb(640,360);

    Scene3 sc; sc.terrain = World(64,40,64); sc.terrain.generate();
    Mods mods; mods.loadFolder("mods");

    // 摆靶子：房子 + 3 棵树
    if (const PlacedModel* h = mods.byName("house"))
        sc.models.push_back(PlacedModel("house", h->mesh, V3(30, sc.groundAt(30,28), 28), 2.0f, Color(205,120,60)));
    for (int i=0;i<3;i++) {
        const PlacedModel* t = mods.byName("tree");
        int x = 18+i*5, z = 12+i*6;
        if (t) sc.models.push_back(PlacedModel("tree", t->mesh, V3(x, sc.groundAt(x,z), z), 1.0f, Color(70,150,60)));
    }

    // 相机放在(10,地面+1.6,10)，瞄准房子中心
    V3 target(30, sc.groundAt(30,28)+1.2f, 28);
    V3 eye(10, sc.groundAt(10,10)+1.6f, 10);
    V3 dir = (target - eye); float len = dir.len(); dir = dir.norm();
    sc.cam.pos = eye;
    sc.cam.yaw = std::atan2(dir.x, dir.z);
    sc.cam.pitch = std::asin(dir.y);

    TexturePack tp; tp.loadOrGenerate("textures");
    VoxelRenderer vr; MeshRenderer mr;

    // 开火前的场景（有房子）
    sc.render(fb, vr, mr, tp, 30);
    fb.drawText("BEFORE SHOT (targets present)", 8, 8, 1, Color(255,255,255));
    std::string p1 = std::string(base)+"_before.png"; save_png(fb, p1.c_str());

    // 开火：从相机沿 forward 射一条射线
    float t=0;
    PlacedModel* hit = hitTestModels(sc.models, sc.cam.pos, sc.cam.forward(), 60.0f, t);
    int score = 0;
    if (hit) {
        std::string hitName = hit->name;
        // 命中：把该模型从场景移除
        for (size_t i=0;i<sc.models.size();i++) if (&sc.models[i]==hit) { sc.models.erase(sc.models.begin()+i); break; }
        score = 1;
        printf("[shoot] HIT target '%s' at dist=%.1f  -> removed, score=1\n", hitName.c_str(), t);
    } else {
        printf("[shoot] MISS (no target hit)\n");
    }

    // 开火后的场景（房子没了，只剩树）
    sc.render(fb, vr, mr, tp, 30);
    char buf[64]; snprintf(buf,sizeof buf,"AFTER SHOT  SCORE=%d", score);
    fb.drawText(buf, 8, 8, 1, Color(255,255,255));
    std::string p2 = std::string(base)+"_after.png"; save_png(fb, p2.c_str());

    printf("[shoot] saved %s / %s  (score=%d)\n", p1.c_str(), p2.c_str(), score);
    return score==1 ? 0 : 1;
}
