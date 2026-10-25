// offscreen.cpp - 无窗口渲染一帧并保存 PNG，用于验证引擎真实渲染。
#include "framebuffer.hpp"
#include "voxel.hpp"
#include <cstdio>
using namespace dbg;

int main(int argc, char** argv) {
    int W = 640, H = 360;
    const char* out = "shot.png";
    if (argc >= 3) { W = atoi(argv[1]); H = atoi(argv[2]); }
    if (argc >= 4) out = argv[3];

    Framebuffer fb(W, H);
    World world(48, 32, 48);
    world.generate();

    VoxelRenderer vr;
    TexturePack tp;
    tp.generateDefaults();   // 内置原版贴图（也可在程序目录放 textures/*.png 覆盖成材质包）
    Camera3 cam;
    cam.pos = V3(20, 13, 34);   // 相机放低，朝向世界中心
    cam.yaw = 3.15f;            // 面向 -Z
    cam.pitch = -0.30f;         // 略俯视
    vr.render(fb, cam, world, 26, tp);
    printf("visible faces submitted: %d\n", vr.lastFaces);

    // 统计非天空像素数
    int nsky = 0, nother = 0;
    for (size_t i = 0; i < fb.px.size(); i++) if (fb.px[i] == Color(110,170,235).abgr()) nsky++; else nother++;
    printf("sky pixels=%d other pixels=%d\n", nsky, nother);

    // 顶部文字水印
    fb.drawText("DBENGINE  BLOCK WORLD", 8, 6, 2, Color(255,255,255));
    fb.drawText("software-rendered voxel engine (C++)", 8, 24, 1, Color(230,230,230));

    if (save_png(fb, out)) { printf("saved %s (%dx%d)\n", out, W, H); return 0; }
    fprintf(stderr, "save failed\n");
    return 1;
}
