// textured_demo.cpp - 验证引擎能做"带贴图的3D模型"(真模型关键)：模型贴上材质贴图(UV)渲染。
// 坦克贴迷彩、房子贴木头、地面贴草地 —— 证明不只是纯色低多边形，能做出"真"的模型。
#include "../engine/framebuffer.hpp"
#include "../engine/voxel.hpp"
#include "../engine/texture.hpp"
#include "../engine/mesh.hpp"
#include "../engine/scene3.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
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
static Mesh makeTank() {
    Mesh m;
    addBox(m, -1.6f,0,-0.9f, 1.6f,0.7f,0.9f);
    addBox(m, -0.9f,0.7f,-0.7f, 0.9f,1.1f,0.7f);
    addBox(m, 0.4f,0.8f,-0.22f, 2.6f,0.95f,0.22f);
    addBox(m, -1.6f,-0.25f,-1.1f, 1.6f,0.0f,1.1f);
    return m;
}
// 生成一块军事迷彩贴图（深绿/土褐斑块）
static Texture makeCamo() {
    Texture t(64,64);
    for (int y=0;y<64;y++) for (int x=0;x<64;x++) {
        int c = hashNoise(x/6, y/6, 5) % 100;
        Color col = c<45?Color(70,95,55) : c<80?Color(110,120,60) : Color(90,80,55);
        // 微抖动
        col = jitter(col, hashNoise(x,y,7), 0, 8);
        t.px[(size_t)y*64+x] = col.abgr();
    }
    return t;
}

int main(int argc, char** argv) {
    const char* out = argc>1 ? argv[1] : "/tmp/db_textured.png";
    Framebuffer fb(640,360);

    TexturePack tp; tp.loadOrGenerate("textures");
    const Texture* grassTex = tp.face(B_GRASS, 1);
    const Texture* woodTex  = tp.face(B_WOOD, 1);
    Texture camo = makeCamo();

    // 纯模型场景（无体素）：地面 + 房子(木头贴图) + 坦克(迷彩贴图)
    struct M { Mesh mesh; V3 pos; float scale; const Texture* tex; };
    std::vector<M> ms;
    Mesh ground; addBox(ground, -14,0,-10, 14,0.2f,10);
    ms.push_back(M{ground, V3(0,0,0), 1.0f, grassTex});
    Mesh house; loadOBJ("mods/house.obj", house);
    ms.push_back(M{house, V3(6,0.2f,2), 2.0f, woodTex});
    ms.push_back(M{makeTank(), V3(-2,0.2f,0), 1.1f, &camo});

    Camera3 cam; cam.pos=V3(0,4.0f,9.0f);
    V3 target(-1,1.0f,0); V3 dir=(target-cam.pos).norm();
    cam.yaw=std::atan2(dir.x,dir.z); cam.pitch=std::asin(dir.y);

    Scene3::drawSky(fb, Color(30,60,130), Color(170,200,240), V3(fb.w*0.8f,fb.h*0.15f,0), 20);
    MeshRenderer mr;
    std::vector<float> zbuf((size_t)fb.w*fb.h, 1e30f); mr.useZBuf(zbuf);
    for (auto& m : ms) mr.renderTextured(fb, m.mesh, cam, m.pos, m.scale, *m.tex, 1.0f);

    fb.drawText("DBENGINE TEXTURED MODELS (wood/camo/grass UV)", 8,8,1,Color(255,255,255));
    if (!save_png(fb, out)) { printf("save failed\n"); return 1; }
    printf("[textured] saved %s (带贴图的3D模型: 木头房/迷彩坦克/草地)\n", out);
    return 0;
}
