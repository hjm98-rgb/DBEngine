// minigame_demo.cpp - 验证引擎能做"小游戏"：一个横版躲避小游戏
// 玩家左右移动躲从上掉下的方块，接住金币得分；脚本驱动 + 碰撞检测。
#include "../engine/framebuffer.hpp"
#include "../engine/engine.hpp"
#include <cstdio>
#include <cstdlib>
#include <ctime>
using namespace dbg;
using namespace dbg2;

// 接住金币：玩家与金币碰撞检测
struct Catch : Script {
    int& score; std::vector<GameObject*>& coins;
    GameObject* player;
    Catch(int& s, std::vector<GameObject*>& c, GameObject* p) : score(s), coins(c), player(p) {}
    void update(float dt) {
        for (auto it = coins.begin(); it != coins.end(); ) {
            GameObject* g = *it;
            // AABB 碰撞
            bool hit = std::abs(g->pos.x - player->pos.x) < (g->width+player->width)/2 &&
                       std::abs(g->pos.y - player->pos.y) < (g->height+player->height)/2;
            if (hit) { score += 10; g->pos.y = -9999; g->width = 0; g->height = 0; it = coins.erase(it); }
            else ++it;
        }
    }
};
// 金币：匀速下落，掉出屏幕就重置回顶部
struct Fall : Script {
    float speed = 70.0f;
    void update(float dt) {
        GameObject* g = owner;
        g->pos.y += speed * dt;
        if (g->pos.y > 380) g->pos.y = -20;
    }
};

int main(int argc, char** argv) {
    const char* out = argc>1 ? argv[1] : "/tmp/db_minigame.png";
    Framebuffer fb(640, 360);
    Scene sc;
    int score = 0;

    // 玩家（底部）
    auto* player = sc.add(new GameObject("player"));
    player->pos = Vec2(150, 330); player->width=48; player->height=22; player->color=Color(60,130,240);
    player->add(new Sprite);

    // 金币（下落的方块）
    std::vector<GameObject*> coins;
    std::srand(7);
    for (int i=0;i<5;i++) {
        auto* c = sc.add(new GameObject("coin"));
        c->pos = Vec2(60.0f+i*110, -20.0f-i*50); c->width=26; c->height=26; c->color=Color(245,205,60);
        c->add(new Sprite)->circle = true;
        Fall* f = c->add(new Fall); f->speed = 60 + i*25;
        coins.push_back(c);
    }
    // 接金币脚本
    sc.add(new GameObject("catch"))->add(new Catch(score, coins, player));

    Renderer2D r(fb);
    // 模拟 320 帧：金币掉到玩家那行被接住加分
    for (int i=0;i<320;i++) {
        // 让玩家左右来回移动（演示脚本在动）
        player->pos.x = 150 + 120.0f * std::sin(i*0.06f);
        sc.update(1.0f/60.0f);
    }
    fb.clear(Color(20,26,40));
    sc.render(r);
    char buf[80]; snprintf(buf,sizeof buf,"DBENGINE MINIGAME  SCORE=%d", score);
    r.text(buf, 8, 8, 2, Color(255,255,255));

    if (!save_png(fb, out)) { printf("save failed\n"); return 1; }
    printf("[minigame] saved %s  score=%d (碰撞接金币加分验证)\n", out, score);
    return score>0 ? 0 : 1;
}
