// desktop_main.cpp - DBEngine 桌面版（Linux X11）：既是编辑器又是游玩的通用场景构建器。
#include "../engine/framebuffer.hpp"
#include "../engine/voxel.hpp"
#include "../engine/texture.hpp"
#include "../engine/mesh.hpp"
#include "../engine/scene3.hpp"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

using namespace dbg;

static const int WIN_W = 640, WIN_H = 360;

struct App {
    Display* dpy; Window win; GC gc;
    Framebuffer fb;
    VoxelRenderer vr;
    MeshRenderer mr;                 // 3D 模型渲染器
    TexturePack tp;
    World world;
    Scene3 sc;                       // 场景（地形+模型+玩家）
    Mods mods;                       // mods/ 里的 3D 模型
    std::vector<PlacedModel> placed; // 已摆进场景的模型
    int selModel = 0;                // 当前选中的模组模型
    bool modelMode = false;          // 编辑模式：放方块 或 放模型
    bool editModelMode = false;      // 编辑已摆放的3D模型（平移/缩放/删除）
    int selPlaced = -1;              // 当前选中的已摆放模型
    Camera3 cam;
    float vy = 0;
    bool grounded = false;
    bool editMode = true;         // true=编辑模式(放/敲方块), false=游玩
    uint8_t selBlock = B_GRASS;
    float lookSens = 0.0025f;
    int lastMX = WIN_W/2, lastMY = WIN_H/2;
    int frame = 0;
    int score = 0;                 // 游玩模式射击得分
    bool consoleOn = false;        // 命令控制台
    std::string cmd, log;

    // ---- 构建器：存档 / 读档（世界 + 摆好的3D模型 一起存成文件） ----
    bool saveWorld(const std::string& path) {
        std::ofstream f(path, std::ios::binary);
        if (!f) { log = "save failed: " + path; return false; }
        f.write((char*)&world.sx,4); f.write((char*)&world.sy,4); f.write((char*)&world.sz,4);
        f.write((char*)world.blocks.data(), (long)world.blocks.size());
        int n = (int)placed.size(); f.write((char*)&n, 4);
        for (auto& m : placed) {
            uint8_t ln = (uint8_t)std::min((size_t)255, m.name.size());
            f.write((char*)&ln,1); f.write(m.name.data(), ln);
            f.write((char*)&m.pos.x,4); f.write((char*)&m.pos.y,4); f.write((char*)&m.pos.z,4);
            f.write((char*)&m.scale,4);
        }
        f.close();
        log = "saved scene: " + path; return true;
    }
    bool loadWorld(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) { log = "load failed: " + path; return false; }
        int sx,sy,sz; f.read((char*)&sx,4); f.read((char*)&sy,4); f.read((char*)&sz,4);
        if (sx<=0||sy<=0||sz<=0||sx>256||sy>256||sz>256) { log="bad world file"; return false; }
        World nw(sx,sy,sz);
        f.read((char*)nw.blocks.data(), (long)nw.blocks.size());
        if ((size_t)f.gcount() != nw.blocks.size()) { log="world file truncated"; return false; }
        world = nw;
        int n=0; if (f.read((char*)&n,4)) {
            placed.clear();
            for (int i=0;i<n && n<5000;i++) {
                uint8_t ln=0; if(!f.read((char*)&ln,1)) break;
                char nm[256]={0}; f.read(nm, ln);
                V3 p; float sc;
                f.read((char*)&p.x,4); f.read((char*)&p.y,4); f.read((char*)&p.z,4); f.read((char*)&sc,4);
                const PlacedModel* src = mods.byName(std::string(nm,ln));
                if (src) placed.push_back(PlacedModel(src->name, src->mesh, p, sc, Color(200,200,200)));
            }
        }
        log = "loaded scene: " + path + "  (models=" + std::to_string(placed.size()) + ")"; return true;
    }

    // 构建器：执行"模型编辑代码/命令"
    void runCommand(const std::string& line) {
        std::istringstream ss(line); std::string tok; ss>>tok;
        if (tok=="help") {
            log = "cmds: save F  load F  fill B  place B x y z  model NAME  modlist  tp x y z  host  clear";
        } else if (tok=="save" || tok=="load") {
            std::string p; ss>>p;
            if (p.empty()) { log="need filename"; return; }
            if (tok=="save") saveWorld(p); else loadWorld(p);
        } else if (tok=="fill") {
            std::string b; ss>>b; uint8_t id = blockFromName(b);
            int cx=(int)cam.pos.x, cy=(int)cam.pos.y, cz=(int)cam.pos.z;
            for (int x=cx-2;x<=cx+2;x++)for(int z=cz-2;z<=cz+2;z++)for(int y=0;y<=cy;y++)
                world.set(x,y,z,id);
            log = "filled " + b + " around player";
        } else if (tok=="place") {
            std::string b; int px,py,pz; ss>>b>>px>>py>>pz;
            world.set(px,py,pz, blockFromName(b));
            log = "placed " + b;
        } else if (tok=="modlist" || tok=="mods") {
            std::string s = "mods: ";
            for (auto& m : mods.models) { s += m.name + " "; }
            log = s.empty() ? "no mods (drop .obj into mods/)" : s;
        } else if (tok=="model") {
            std::string b; ss>>b;
            const PlacedModel* src = mods.byName(b);
            if (!src) { log = "no such model: "+b+"  (modlist)"; return; }
            int bx,by,bz,px,py,pz;
            if (pick(bx,by,bz,px,py,pz)) {
                float g = groundH(world, px, pz);
                placed.push_back(PlacedModel(src->name, src->mesh, V3(px+0.5f, g, pz+0.5f), 1.5f, Color(200,200,200)));
                log = "placed model " + src->name;
            } else log = "no surface to place on";
        } else if (tok=="tp") {
            float x,y,z; if (ss>>x>>y>>z) { cam.pos.x=x;cam.pos.y=y;cam.pos.z=z; log="teleported"; }
        } else if (tok=="host") {   // 一键搭建服务器（内置通用服务器脚本）
            int port = 25565; ss>>port; if (port<=0) port=25565;
            std::string c = "python3 platform/server/server.py start "+std::to_string(port)+
                            " --mode relay --name dbhost >/tmp/dbhost.log 2>&1 &";
            int r = system(c.c_str());
            log = (r==0) ? ("server started :"+std::to_string(port)+"  (F5)") : "server start failed";
        } else if (tok=="clear") {
            for (int x=0;x<world.sx;x++)for(int y=0;y<world.sy;y++)for(int z=0;z<world.sz;z++)
                world.set(x,y,z,B_AIR);
            world.generate();
            log = "world regenerated";
        } else {
            log = "unknown cmd: " + tok + "  (type help)";
        }
    }
    static float groundH(World& w, int x, int z) {
        for (int y=w.sy-1; y>=0; y--) if (w.solid(x,y,z)) return (float)y+1.0f;
        return 0.0f;
    }
    static uint8_t blockFromName(const std::string& b) {
        if (b=="grass") return B_GRASS; if (b=="dirt") return B_DIRT;
        if (b=="stone") return B_STONE; if (b=="wood") return B_WOOD;
        if (b=="leaves") return B_LEAVES; if (b=="sand") return B_SAND;
        if (b=="water") return B_WATER; if (b=="brick") return B_BRICK;
        return B_STONE;
    }

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
        mods.loadFolder("mods");         // 加载 mods/ 里的 3D 模型
        // 预置一个房子和几棵树，让场景不空
        if (const PlacedModel* h = mods.byName("house"))
            placed.push_back(PlacedModel(h->name, h->mesh, V3(30, groundH(world,30,28), 28), 2.0f, Color(205,120,60)));
        for (int i=0;i<4;i++) {
            const PlacedModel* t = mods.byName("tree");
            int x = 16 + i*6, z = 15 + i*7;
            if (t) placed.push_back(PlacedModel(t->name, t->mesh, V3(x, groundH(world,x,z), z), 1.0f, Color(70,150,60)));
        }
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
            if (ks == XK_F5) runCommand("host 25565");   // 一键搭建服务器
            if (ks == XK_m || ks == XK_g) modelMode = !modelMode;   // 放方块 <-> 放模型
            if (ks == XK_e) editModelMode = !editModelMode;         // 编辑已摆模型
            if (ks == XK_n) { if (!mods.models.empty()) selModel = (selModel+1)%mods.models.size(); }
            if (ks == XK_b) { if (!mods.models.empty()) selModel = (selModel+mods.models.size()-1)%mods.models.size(); }
            if (ks == XK_x) { if (editModelMode && selPlaced>=0 && selPlaced<(int)placed.size()) { placed.erase(placed.begin()+selPlaced); selPlaced=-1; log="model deleted"; } }
            // 数字键选方块
            static const uint8_t map[8] = {B_GRASS,B_DIRT,B_STONE,B_WOOD,B_LEAVES,B_SAND,B_WATER,B_BRICK};
            for (int i=0;i<8;i++) if (ks == XK_1+i) selBlock = map[i];
        }
        if (e.type == ButtonPress) {
            if (e.xbutton.button == 1) { // 左键：编辑放方块/模型；游玩开枪打靶
                if (editMode) {
                    int bx,by,bz,px,py,pz;
                    if (modelMode) {
                        // 放 3D 模型：立在拾取位置的地面上
                        if (!mods.models.empty()) {
                            const PlacedModel& src = mods.models[selModel % mods.models.size()];
                            if (pick(bx,by,bz,px,py,pz)) {
                                float g = groundH(world, px, pz);
                                placed.push_back(PlacedModel(src.name, src.mesh, V3(px+0.5f, g, pz+0.5f), 1.5f, Color(200,200,200)));
                            }
                        }
                    } else if (pick(bx,by,bz,px,py,pz) && world.in(px,py,pz) && !world.solid(px,py,pz))
                        world.set(px,py,pz, selBlock);
                }
            } else if (!editMode) {
                // 游玩模式：开枪，打中摆的3D模型=消除+得分
                float t = 0;
                PlacedModel* hit = hitTestModels(placed, cam.pos, cam.forward(), 60.0f, t);
                if (hit) {
                    for (size_t i=0;i<placed.size();i++) if (&placed[i]==hit) { placed.erase(placed.begin()+i); break; }
                    score++;
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
            fb.drawText("EDIT: [1-8]方块 [M]模型 [N/B]换模型 左=放 右=敲 WASD移动 F5开服", 8, 46, 1, Color(200,230,120));
            if (modelMode) {
                snprintf(buf,sizeof buf, "MODEL MODE: %s   (mods: %zu)", 
                         mods.models.empty()? "(none, drop .obj in mods/)" : mods.models[selModel%mods.models.size()].name.c_str(), mods.models.size());
                fb.drawText(buf, 8, 56, 1, Color(150,220,255));
            } else {
                snprintf(buf,sizeof buf, "BLOCK MODE: %s", blockName(selBlock));
                fb.drawText(buf, 8, 56, 1, Color(240,210,90));
            }
        } else {
            snprintf(buf,sizeof buf, "PLAY:  WASD=移动  空格=跳  左键=开枪打靶  [T]=回编辑   SCORE=%d", score);
            fb.drawText(buf, 8, 46, 1, Color(200,230,120));
        }
        // 准星
        fb.fillRect(WIN_W/2-5, WIN_H/2, 11, 1, Color(255,255,255));
        fb.fillRect(WIN_W/2, WIN_H/2-5, 1, 11, Color(255,255,255));
    }

    void render() {
        // 用场景渲染：天空+地形+3D模型+玩家，共享深度缓冲
        sc.terrain = world;
        sc.cam = cam;
        sc.models = placed;
        sc.hasPlayer = false;   // 相机=玩家视角，玩家本身不画
        sc.render(fb, vr, mr, tp, 16);
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
