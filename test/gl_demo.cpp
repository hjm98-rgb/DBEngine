// gl_demo.cpp - DBEngine GPU 渲染后端验证：用 OpenGL(GLX) 渲染带光照+深度的3D场景，
// 读回像素存 PNG。证明引擎能走显卡渲染管线（不只是CPU软件渲染），这是迈向高级3D的一步。
#include "../engine/framebuffer.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
using namespace dbg;

static const int W = 640, H = 360;

static void cube(float s) {
    // 六个面，逐面法线（光照正确）
    float h = s/2;
    const float v[6][4][3] = {
        {{-h,-h, h},{ h,-h, h},{ h, h, h},{-h, h, h}},  // 前
        {{ h,-h,-h},{-h,-h,-h},{-h, h,-h},{ h, h,-h}}, // 后
        {{ h,-h, h},{ h,-h,-h},{ h, h,-h},{ h, h, h}}, // 右
        {{-h,-h,-h},{-h,-h, h},{-h, h, h},{-h, h,-h}}, // 左
        {{-h, h, h},{ h, h, h},{ h, h,-h},{-h, h,-h}}, // 上
        {{-h,-h,-h},{ h,-h,-h},{ h,-h, h},{-h,-h, h}}  // 下
    };
    const float n[6][3] = {{0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
    glBegin(GL_QUADS);
    for (int f=0; f<6; f++) {
        glNormal3f(n[f][0],n[f][1],n[f][2]);
        for (int i=0;i<4;i++) glVertex3f(v[f][i][0],v[f][i][1],v[f][i][2]);
    }
    glEnd();
}

static void pyramid(float s, float hgt) {
    float h = s/2;
    glBegin(GL_TRIANGLES);
    // 四面（逐面法线）
    const float p[4][3][3] = {
        {{0,hgt,0},{-h,0, h},{ h,0, h}},  // 前
        {{0,hgt,0},{ h,0, h},{ h,0,-h}},  // 右
        {{0,hgt,0},{ h,0,-h},{-h,0,-h}},  // 后
        {{0,hgt,0},{-h,0,-h},{-h,0, h}}   // 左
    };
    for (int f=0; f<4; f++) {
        float ux=p[f][1][0]-p[f][0][0], uy=p[f][1][1]-p[f][0][1], uz=p[f][1][2]-p[f][0][2];
        float vx=p[f][2][0]-p[f][0][0], vy=p[f][2][1]-p[f][0][1], vz=p[f][2][2]-p[f][0][2];
        float nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx;
        float l=std::sqrt(nx*nx+ny*ny+nz*nz); if(l>0){nx/=l;ny/=l;nz/=l;}
        glNormal3f(nx,ny,nz);
        for (int i=0;i<3;i++) glVertex3f(p[f][i][0],p[f][i][1],p[f][i][2]);
    }
    glEnd();
}

int main(int argc, char** argv) {
    const char* out = argc>1 ? argv[1] : "/tmp/db_gl.png";
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) { fprintf(stderr, "no X display (run under xvfb-run)\n"); return 2; }
    int scr = DefaultScreen(dpy);
    int attrib[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, True, None };
    XVisualInfo* vi = glXChooseVisual(dpy, scr, attrib);
    if (!vi) { fprintf(stderr, "no suitable GL visual\n"); return 2; }
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy,scr), 0,0,W,H,0,0,0);
    GLXContext ctx = glXCreateContext(dpy, vi, nullptr, True);
    if (!ctx) { fprintf(stderr, "glXCreateContext failed\n"); return 2; }
    glXMakeCurrent(dpy, win, ctx);
    const char* rend = (const char*)glGetString(GL_RENDERER);
    const char* ver = (const char*)glGetString(GL_VERSION);
    printf("[gl] renderer=%s  version=%s  (真GPU管线)\n", rend?rend:"?", ver?ver:"?");

    glViewport(0,0,W,H);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    // 手工透视矩阵，避免额外依赖 libGLU
    float fov=55.0f*3.14159265f/180.0f, asp=(float)W/H, nearp=0.1f, farp=100.0f;
    float f=1.0f/std::tan(fov/2.0f);
    float m[16]={ f/asp,0,0,0,  0,f,0,0,  0,0,(farp+nearp)/(nearp-farp),-1,  0,0,2*farp*nearp/(nearp-farp),0 };
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    float lp[4]={ 1.0f,1.5f,0.5f,0.0f }, ld[4]={0.8f,0.8f,0.8f,1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lp);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, ld);
    float amb[4]={0.25f,0.25f,0.25f,1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);

    // 相机
    glLoadIdentity();
    glTranslatef(0,0,-6);
    glRotatef(-15,1,0,0);
    glRotatef(28,0,1,0);

    glClearColor(0.35f,0.55f,0.9f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    // 地面
    glColor3f(0.25f,0.45f,0.25f);
    glNormal3f(0,1,0);
    glBegin(GL_QUADS);
    glVertex3f(-4,-1.2f,-4); glVertex3f( 4,-1.2f,-4);
    glVertex3f( 4,-1.2f, 4); glVertex3f(-4,-1.2f, 4);
    glEnd();

    // 红色方块（绕Y转一点）
    glColor3f(0.85f,0.2f,0.2f);
    glPushMatrix(); glTranslatef(-1.2f,0,0); glRotatef(20,0,1,0); cube(1.4f); glPopMatrix();
    // 蓝色金字塔
    glColor3f(0.25f,0.45f,0.95f);
    glPushMatrix(); glTranslatef(1.4f,-1.2f,0.4f); pyramid(1.6f,1.6f); glPopMatrix();

    glFinish();
    // 读回像素
    std::vector<uint8_t> rgba((size_t)W*H*4);
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    // 转成引擎 ABGR 帧缓冲（并上下翻转）
    Framebuffer fb(W,H);
    for (int y=0;y<H;y++) for (int x=0;x<W;x++) {
        int sy=H-1-y; size_t i=((size_t)sy*W+x)*4;
        uint32_t c = ((uint32_t)rgba[i+3]<<24)|((uint32_t)rgba[i+2]<<16)|((uint32_t)rgba[i+1]<<8)|(uint32_t)rgba[i+0];
        fb.px[(size_t)y*W+x]=c;
    }
    fb.drawText("DBENGINE GPU (OpenGL)  LIGHT+DEPTH", 8,8,1,Color(255,255,255));
    if (!save_png(fb, out)) { printf("save failed\n"); return 1; }
    printf("[gl] saved %s  (GPU渲染带光照+深度)\n", out);

    glXMakeCurrent(dpy, None, nullptr);
    glXDestroyContext(dpy, ctx);
    XCloseDisplay(dpy);
    return 0;
}
