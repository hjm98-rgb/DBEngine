// desktop_main.cpp - DBEngine 桌面版（Linux X11）：一个既是编辑器又是游玩的《我的世界》风格示例。
#include "../engine/framebuffer.hpp"
#include "../engine/voxel.hpp"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>

using namespace dbg;

static const int WIN_W = 640, WIN_H = 360;

struct App {
    Display* dpy; Window win; GC gc;
    Framebuffer fb;
    VoxelRenderer vr;
    TexturePack tp;
    World world;
    Camera3 cam;
    float vy = 0;
    bool grounded = false;
    bool editMode = true;         // true=编辑模式(放/敲方块), false=游玩
    uint8_t selBlock = B_GRASS;
    float lookSens = 0.0025f;
    int lastMX = WIN_W/2, lastMY = WIN_H/2;
    int frame = 0;

    App() : fb(WIN_W, WIN_H) {}

    bool init() {
        dpy = XOpenDisplay(nullptr);
        if (!dpy) { fprintf(stderr, "无法打开 X 显示\n"); return false; }
        int scr = DefaultScreen(dpy);
        win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0,0, WIN_W,WIN_H, 0,
                                  BlackPixel(dpy,scr), WhitePixel(dpy,scr));
        XStoreName(dpy, win, "DBEngine - 方块世界 编辑器/游玩");
        XSelectInput(dpy, win, KeyPressMask | KeyReleaseMask | ButtonPressMask |
                                ButtonReleaseMask | PointerMotionMask | ExposureMask);
        XMapWindow(dpy, win);
        gc = XCreateGC(dpy, win, 0, nullptr);
        // 抓住鼠标以获得连续视角
        XGrabPointer(dpy, win, True, PointerMotionMask|ButtonPressMask|ButtonReleaseMask,
                     GrabModeAsync, GrabModeAsync, win, None, CurrentTime);
        XWarpPointer(dpy, None, win, 0,0,0,0, WIN_W/2, WIN_H/2);
        XFlush(dpy);
        world = World(48, 32, 48);
        world.generate();
        tp.loadOrGenerate("textures");   // 优先加载程序目录 textures/ 材质包，缺失用内置
        cam.pos = V3(24, 24, 24);
        cam.yaw = 0.7f;
        return true;
    }

    // 射线拾取：返回命中方块坐标和其"面向相机"的邻格坐标（用于放置）
    bool pick(int& bx, int& by, int& bz, int& placeX, int& placeY, int& placeZ) {
        V3 o = cam.pos;
        V3 d = cam.forward();
        float step = 0.08f, t = 0, maxD = 40;
        int px = INT_MAX, py = INT_MAX, pz = INT_MAX;
        while (t < maxD) {
            V3 p = o + d * t;
            int x = (int)std::floor(p.x), y = (int)std::floor(p.y), z = (int)std::floor(p.z);
            if (x!=px || y!=py || z!=pz) {
                if (world.solid(x,y,z)) {
                    bx=x; by=y; bz=z;
                    // 计算进入面，得到放置邻格
                    V3 prev = o + d * (t - step);
                    placeX = (int)std::floor(prev.x);
                    placeY = (int)std::floor(prev.y);
                    placeZ = (int)std::floor(prev.z);
                    return true;
                }
                px=x; py=y; pz=z;
            }
            t += step;
        }
        return false;
    }

    void handleEvent(XEvent& e) {
        if (e.type == Expose) return;
        if (e.type == KeyPress) {
            KeySym ks = XLookupKeysym(&e.xkey, 0);
            if (ks == XK_Escape) { exit(0); }
            if (ks == XK_t || ks == XK_Tab) editMode = !editMode;
            // 数字键选方块
            static const uint8_t map[8] = {B_GRASS,B_DIRT,B_STONE,B_WOOD,B_LEAVES,B_SAND,B_WATER,B_BRICK};
            for (int i=0;i<8;i++) if (ks == XK_1+i) selBlock = map[i];
        }
        if (e.type == ButtonPress) {
            if (e.xbutton.button == 1) { // 左键：放置（编辑模式）
                if (editMode) {
                    int bx,by,bz,px,py,pz;
                    if (pick(bx,by,bz,px,py,pz) && world.in(px,py,pz) && !world.solid(px,py,pz))
                        world.set(px,py,pz, selBlock);
                }
            }
            if (e.xbutton.button == 3) { // 右键：敲掉
                int bx,by,bz,px,py,pz;
                if (pick(bx,by,bz,px,py,pz)) world.set(bx,by,bz, B_AIR);
            }
        }
        if (e.type == MotionNotify) {
            int dx = e.xmotion.x - lastMX, dy = e.xmotion.y - lastMY;
            cam.yaw -= dx * lookSens;
            cam.pitch += dy * lookSens;
            if (cam.pitch > 1.5f) cam.pitch = 1.5f;
            if (cam.pitch < -1.5f) cam.pitch = -1.5f;
            XWarpPointer(dpy, None, win, 0,0,0,0, WIN_W/2, WIN_H/2);
            XFlush(dpy);
        }
    }

    bool keyHeld(KeySym k) {
        char keys[32];
        KeySym k2;
        XQueryKeymap(dpy, keys);
        // 用 XKeysymToKeycode 判断
        KeyCode kc = XKeysymToKeycode(dpy, k);
        return kc && (keys[kc/8] & (1 << (kc%8)));
    }

    void update(float dt) {
        V3 fwd = cam.forward();
        V3 right = cam.right();
        // 水平移动
        float sp = editMode ? 12.0f : 6.0f;
        float mx = 0, mz = 0;
        if (keyHeld(XK_w)) { mx += fwd.x; mz += fwd.z; }
        if (keyHeld(XK_s)) { mx -= fwd.x; mz -= fwd.z; }
        if (keyHeld(XK_d)) { mx += right.x; mz += right.z; }
        if (keyHeld(XK_a)) { mx -= right.x; mz -= right.z; }
        float l = std::sqrt(mx*mx+mz*mz);
        if (l > 0) { mx = mx/l*sp*dt; mz = mz/l*sp*dt; }
        cam.pos.x += mx; cam.pos.z += mz;

        // 竖直：编辑模式自由升降，游玩模式有重力+跳跃
        if (editMode) {
            if (keyHeld(XK_space)) cam.pos.y += sp*dt;
            if (keyHeld(XK_Shift_L) || keyHeld(XK_Shift_R)) cam.pos.y -= sp*dt;
            vy = 0; grounded = false;
        } else {
            bool jump = keyHeld(XK_space);
            // 检测脚下是否实心（对 4 个角取最高实心格）
            grounded = false;
            float footY = cam.pos.y - 1.6f;
            for (int i=0;i<4 && !grounded;i++) {
                float ox = (i==1||i==3)?0.25f:-0.25f, oz=(i>=2)?0.25f:-0.25f;
                int fx=(int)std::floor(cam.pos.x+ox), fz=(int)std::floor(cam.pos.z+oz);
                int fy=(int)std::floor(footY);
                if (world.solid(fx,fy,fz)) grounded = true;
            }
            if (grounded && jump) vy = 8.0f;
            vy -= 20.0f * dt;
            cam.pos.y += vy * dt;
            // 防止掉进地下：脚下实心则贴地
            int fy=(int)std::floor(cam.pos.y-1.6f);
            int fx=(int)std::floor(cam.pos.x), fz=(int)std::floor(cam.pos.z);
            if (world.solid(fx,fy,fz)) { cam.pos.y = fy+1+1.6f; vy = 0; }
        }
        // 夹在世界内
        if (cam.pos.x < 1) cam.pos.x=1; if (cam.pos.x > world.sx-2) cam.pos.x=world.sx-2;
        if (cam.pos.z < 1) cam.pos.z=1; if (cam.pos.z > world.sz-2) cam.pos.z=world.sz-2;
        if (cam.pos.y < 2) cam.pos.y=2; if (cam.pos.y > world.sy-2) cam.pos.y=world.sy-2;
    }

    void drawHUD() {
        fb.drawText("DBENGINE  BLOCK WORLD", 8, 6, 2, Color(255,255,255));
        char buf[160];
        snprintf(buf,sizeof buf, "MODE: %s   [T/Tab 切换]", editMode?"EDIT":"PLAY");
        fb.drawText(buf, 8, 22, 1, Color(255,255,255));
        snprintf(buf,sizeof buf, "POS: %.1f %.1f %.1f   PITCH %.2f YAW %.2f",
                 cam.pos.x,cam.pos.y,cam.pos.z,cam.pitch,cam.yaw);
        fb.drawText(buf, 8, 32, 1, Color(220,220,220));
        if (editMode) {
            fb.drawText("EDIT: [1-8]选方块  左键=放  右键=敲  WASD+空格/Shift=移动", 8, 46, 1, Color(200,230,120));
            snprintf(buf,sizeof buf, "CURRENT: %s", blockName(selBlock));
            fb.drawText(buf, 8, 56, 1, Color(240,210,90));
        } else {
            fb.drawText("PLAY:  WASD=移动  空格=跳  [T]=回编辑", 8, 46, 1, Color(200,230,120));
        }
        // 准星
        fb.fillRect(WIN_W/2-5, WIN_H/2, 11, 1, Color(255,255,255));
        fb.fillRect(WIN_W/2, WIN_H/2-5, 1, 11, Color(255,255,255));
    }

    void render() {
        vr.render(fb, cam, world, 14, tp);
        drawHUD();
        // XCreateImage 传 NULL 时不会分配像素缓冲(data=NULL)，需自己分配，用完释放
        XImage* img = XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)),
                                   24, ZPixmap, 0, nullptr, WIN_W, WIN_H, 32, 0);
        if (img) {
            img->data = (char*)malloc((size_t)WIN_H * img->bytes_per_line);
            if (img->data) {
                for (int y = 0; y < WIN_H; y++)
                    memcpy(img->data + (size_t)y*img->bytes_per_line,
                           fb.px.data() + (size_t)y*WIN_W, (size_t)WIN_W*4);
                XPutImage(dpy, win, gc, img, 0,0, 0,0, WIN_W, WIN_H);
                free(img->data);
                img->data = nullptr;
            }
            XDestroyImage(img);
        }
    }

    void loop() {
        XEvent e;
        fprintf(stderr, "[dbg] loop enter\n");
        while (true) {
            fprintf(stderr, "[dbg] frame=%d\n", frame);
            while (XPending(dpy)) { XNextEvent(dpy, &e); fprintf(stderr,"[dbg] event %d\n",(int)e.type); handleEvent(e); }
            update(1.0f/60.0f);
            fprintf(stderr, "[dbg] before render\n");
            render();
            fprintf(stderr, "[dbg] after render\n");
            XFlush(dpy);
            usleep(16000);
            frame++;
        }
    }
};

int main() {
    App app;
    if (!app.init()) return 1;
    app.loop();
    return 0;
}
