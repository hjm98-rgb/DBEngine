// icon_demo.cpp - 生成 DBEngine 应用图标（引擎 logo：一块带高光的 3D 立方体），存成 PNG。
#include "../engine/framebuffer.hpp"
#include <cstdio>
#include <cmath>
using namespace dbg;

int main(int argc, char** argv) {
    const char* out = argc>1 ? argv[1] : "/tmp/dbengine_icon.png";
    const int S = 128;
    Framebuffer fb(S,S);
    fb.clear(Color(28,34,58));           // 深蓝底
    // 画一个 3D 方块 logo：顶面亮、前面中、右面深
    // 顶点（等距方块）
    int x0=S/2, y0=S/2, a=S/3;           // 中心、边长
    // 顶面 (亮)
    int tx[4]={x0, x0+a, x0, x0-a}; int ty[4]={y0-a, y0, y0+a, y0};
    // 简化：画三个菱形拼成立方体
    // 顶面
    int t1[3]={x0, y0-a, x0+a, y0, x0, y0+a};
    fillPoly(fb, 3, x0,y0-a, x0+a,y0, x0,y0+a, Color(120,150,220));
    // 左面
    fillPoly(fb, 3, x0,y0-a, x0-a,y0, x0,y0, Color(80,100,170));
    // 右面
    fillPoly(fb, 3, x0+a,y0, x0,y0+a, x0,y0, Color(55,75,140));
    // 描边
    for (int i=0;i<4;i++){ int j=(i+1)%4; line(fb,tx[i],ty[i],tx[j],ty[j],Color(220,235,255)); }
    // 中心高光
    fb.fillCircle(x0+ a/4, y0-a/2, 4, Color(255,255,255,200));

    if (!save_png(fb, out)) { printf("save failed\n"); return 1; }
    printf("[icon] saved %s (%dx%d DBEngine logo)\n", out, S, S);
    return 0;
}
