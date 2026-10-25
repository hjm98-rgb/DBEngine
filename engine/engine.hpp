// engine.hpp - 通用游戏引擎内核：对象/组件/脚本 + 2D 精灵层
// 让引擎能做各种类型(2D横版/塔防/解密 + 3D体素/射击)，MC只是其中一个玩法。
#pragma once
#include "framebuffer.hpp"
#include <vector>
#include <string>
#include <cmath>

namespace dbg2 {

// 复用底层软件渲染器的帧缓冲与颜色（自写，非第三方引擎）
using dbg::Framebuffer;
using dbg::Color;

struct Vec2 { float x=0, y=0; Vec2(){} Vec2(float x,float y):x(x),y(y){} };

// 2D 渲染器：直接画到帧缓冲(屏幕坐标)
struct Renderer2D {
    Framebuffer& fb;
    explicit Renderer2D(Framebuffer& f) : fb(f) {}
    void fillRect(float x, float y, float w, float h, Color c) { fb.fillRect((int)x,(int)y,(int)w,(int)h,c); }
    void fillCircle(float cx, float cy, float r, Color c) { fb.fillCircle((int)cx,(int)cy,(int)r,c); }
    void text(const std::string& s, float x, float y, int scale, Color c) { fb.drawText(s,(int)x,(int)y,scale,c); }
    void clear(Color c) { fb.clear(c); }
};

// 组件基类
class Component {
public:
    class GameObject* owner = nullptr;
    virtual void start() {}
    virtual void update(float dt) {}
    virtual void render(Renderer2D& r) {}
};

// 游戏对象
class GameObject {
public:
    std::string name;
    Vec2 pos;            // 屏幕坐标(中心)
    float width = 0, height = 0;
    float rot = 0;
    Color color = Color(255,255,255);
    bool active = true;
    std::vector<Component*> comps;

    GameObject(const std::string& n = "obj") : name(n) {}
    ~GameObject() { for (auto c : comps) delete c; }

    template<class T> T* add(T* c) { c->owner = this; comps.push_back(c); return c; }
    template<class T> T* get() { for (auto c : comps) if (dynamic_cast<T*>(c)) return (T*)c; return nullptr; }
    void start() { for (auto c : comps) c->start(); }
    void update(float dt) { if (!active) return; for (auto c : comps) c->update(dt); }
    void render(Renderer2D& r) { if (!active) return; for (auto c : comps) c->render(r); }
};

// 精灵渲染组件：画矩形/圆形
class Sprite : public Component {
public:
    bool circle = false;
    void render(Renderer2D& r) override {
        if (owner->width<=0||owner->height<=0) return;
        float x = owner->pos.x - owner->width/2, y = owner->pos.y - owner->height/2;
        if (circle) r.fillCircle(owner->pos.x, owner->pos.y, std::min(owner->width,owner->height)/2, owner->color);
        else r.fillRect(x, y, owner->width, owner->height, owner->color);
    }
};

// 脚本基类：子类重写 update 写玩法
class Script : public Component {
};

// 场景
class Scene {
public:
    std::vector<GameObject*> objs;
    ~Scene() { for (auto o : objs) delete o; }
    template<class T> T* add(T* o) { objs.push_back(o); o->start(); return o; }
    void update(float dt) { for (auto o : objs) o->update(dt); }
    void render(Renderer2D& r) { for (auto o : objs) o->render(r); }
};

// 游戏壳
class Game {
public:
    Scene scene;
    virtual void init() {}
    virtual void update(float dt) { scene.update(dt); }
    void render(Renderer2D& r) { scene.render(r); }
};

} // namespace dbg2
