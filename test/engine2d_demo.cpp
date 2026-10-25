// engine2d_demo.cpp - 用通用引擎内核(2D对象/组件/脚本)做一个会动的小游戏，渲染成PNG验证。
#include "engine.hpp"
#include "framebuffer.hpp"
#include <cstdio>
using namespace dbg2;

// 一个自动移动的脚本：让对象左右来回走
class Mover : public Script {
public:
    float speed = 60;
    void update(float dt) override {
        owner->pos.x += speed*dt;
        if (owner->pos.x > 520 || owner->pos.x < 40) speed = -speed;
    }
};

class Bounce : public Script {
public:
    float vx=0, vy=0, g=300;
    void update(float dt) override {
        vy += g*dt; owner->pos.x += vx*dt; owner->pos.y += vy*dt;
        if (owner->pos.y > 300) { owner->pos.y = 300; vy = -vy*0.8f; }
        if (owner->pos.x < 30 || owner->pos.x > 610) vx = -vx;
    }
};

int main(int argc, char** argv) {
    Framebuffer fb(640, 360);
    Scene sc;
    // 地面
    auto* ground = sc.add(new GameObject("ground"));
    ground->pos = Vec2(320, 330); ground->width = 640; ground->height = 40; ground->color = Color(90,150,80);
    ground->add(new Sprite);
    // 玩家(方块)
    auto* player = sc.add(new GameObject("player"));
    player->pos = Vec2(100, 200); player->width = 40; player->height = 60; player->color = Color(60,110,240);
    player->add(new Sprite); player->add(new Bounce)->vx = 140;
    // 敌人(移动圆)
    for (int i=0;i<4;i++) {
        auto* e = sc.add(new GameObject("enemy"));
        e->pos = Vec2(80+i*130, 120+(i%2)*40); e->width=44; e->height=44; e->color = Color(230,50,50);
        e->add(new Sprite)->circle = true;
        e->add(new Mover)->speed = 50 + i*25;
    }
    // 头顶一个弹跳球
    auto* ball = sc.add(new GameObject("ball"));
    ball->pos = Vec2(320, 80); ball->width=26; ball->height=26; ball->color = Color(245,210,60);
    ball->add(new Sprite)->circle = true;
    ball->add(new Bounce)->vx = 120;

    Renderer2D r(fb);
    // 模拟 120 帧，验证脚本真的在动
    for (int i=0;i<120;i++) sc.update(1.0f/60.0f);
    fb.clear(Color(110,170,235));
    sc.render(r);
    r.text("DBENGINE 2D ENGINE", 10, 8, 2, Color(255,255,255));
    char buf[64]; snprintf(buf,sizeof buf,"enemy0 pos.x=%.0f after 120 frames of script", sc.objs[2]->pos.x);
    r.text(buf, 10, 26, 1, Color(230,230,230));
    r.text("scripted: bounce + mover components", 10, 36, 1, Color(230,230,230));

    const char* out = (argc>1)?argv[1]:"engine2d_demo.png";
    if (dbg::save_png(fb, out)) { printf("saved %s\n", out); return 0; }
    return 1;
}
