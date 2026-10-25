// input.hpp - DBEngine 统一输入层：一套"动作键"，电脑用键盘、手机用屏幕虚拟按键。
// 游戏只读这套动作，不关心来源 → 写一次游戏，电脑/手机都能玩。
#pragma once
#include "framebuffer.hpp"
#include <cmath>

namespace dbg {

struct Input {
    // 动作键状态（持续按住）
    bool up=false, down=false, left=false, right=false;
    bool jump=false, run=false, act1=false, act2=false;
    // 瞬发（本帧按下一次）
    bool jumpPressed=false, act1Pressed=false, act2Pressed=false;

    void clearPressed() { jumpPressed=act1Pressed=act2Pressed=false; }

    // ---- 电脑：键盘映射（WASD + 方向键 + 空格/Shift/E/Q）----
    // 键盘码：X11 keysym 转成通用键名再映射；这里用简化码
    enum K { K_NONE, K_UP, K_DOWN, K_LEFT, K_RIGHT, K_JUMP, K_RUN, K_ACT1, K_ACT2 };
    K mapKey(unsigned long ks) {
        switch (ks) {
            case 0xff52: case 0x77: return K_UP;      // 上/W
            case 0xff54: case 0x73: return K_DOWN;    // 下/S
            case 0xff51: case 0x61: return K_LEFT;    // 左/A
            case 0xff53: case 0x64: return K_RIGHT;   // 右/D
            case 0x20:            return K_JUMP;      // 空格
            case 0xffe1: case 0xffe2: return K_RUN;   // Shift
            case 0x65: case 0xff57: return K_ACT1;    // E / 小键盘+ 动作1
            case 0x71: case 0xff58: return K_ACT2;    // Q / 小键盘- 动作2
            default: return K_NONE;
        }
    }
    void press(K k) {
        switch (k) {
            case K_UP: up=true; break; case K_DOWN: down=true; break;
            case K_LEFT: left=true; break; case K_RIGHT: right=true; break;
            case K_JUMP: jump=true; jumpPressed=true; break;
            case K_RUN: run=true; break;
            case K_ACT1: act1=true; act1Pressed=true; break;
            case K_ACT2: act2=true; act2Pressed=true; break;
            default: break;
        }
    }
    void release(K k) {
        switch (k) {
            case K_UP: up=false; break; case K_DOWN: down=false; break;
            case K_LEFT: left=false; break; case K_RIGHT: right=false; break;
            case K_JUMP: jump=false; break; case K_RUN: run=false; break;
            case K_ACT1: act1=false; break; case K_ACT2: act2=false; break;
            default: break;
        }
    }

    // ---- 手机：屏幕虚拟按键（左下摇杆 + 右下动作按钮）----
    struct TouchBtn { int x, y, r; const char* label; int action; };
    // 布局（按屏幕比例，单位像素）
    enum { JOY_CX=80, JOY_CY=300, JOY_R=55,   // 左下虚拟摇杆
           BTN_JUMP_X=520, BTN_JUMP_Y=300,    // 跳跃
           BTN_ACT_X=600, BTN_ACT_Y=240,      // 动作1
           BTN_ACT2_X=600, BTN_ACT2_Y=320 };  // 动作2

    // 绘制虚拟按键（叠在游戏画面最上层）
    void drawTouchUI(Framebuffer& fb) {
        int w=fb.w, h=fb.h;
        // 摇杆底盘
        fb.fillCircle(JOY_CX, JOY_CY, JOY_R, Color(255,255,255,70));
        fb.fillCircle(JOY_CX, JOY_CY, JOY_R, Color(255,255,255,160));
        fb.fillCircle(JOY_CX, JOY_CY, JOY_R*0.45f, Color(255,255,255,120));
        // 动作按钮
        fb.fillCircle(BTN_JUMP_X, BTN_JUMP_Y, 30, Color(255,120,60,140));
        fb.fillCircle(BTN_JUMP_X, BTN_JUMP_Y, 30, Color(255,200,150,220));
        fb.drawText("JUMP", BTN_JUMP_X-18, BTN_JUMP_Y-6, 1, Color(255,255,255));
        fb.fillCircle(BTN_ACT_X, BTN_ACT_Y, 30, Color(60,150,255,140));
        fb.fillCircle(BTN_ACT_X, BTN_ACT_Y, 30, Color(180,220,255,220));
        fb.drawText("ACT1", BTN_ACT_X-20, BTN_ACT_Y-6, 1, Color(255,255,255));
        fb.fillCircle(BTN_ACT2_X, BTN_ACT2_Y, 26, Color(120,210,120,140));
        fb.fillCircle(BTN_ACT2_X, BTN_ACT2_Y, 26, Color(200,255,200,220));
        fb.drawText("ACT2", BTN_ACT2_X-18, BTN_ACT2_Y-6, 1, Color(255,255,255));
    }
    // 手机：检测点按落到哪个控件，返回动作键并置位。返回0=未命中。
    int tap(int tx, int ty) {
        int dx=tx-JOY_CX, dy=ty-JOY_CY;
        if (dx*dx+dy*dy <= JOY_R*JOY_R) { // 摇杆：按方向
            float a = atan2f((float)dy,(float)dx);
            left = (a>2.6f||a<-2.6f); right=(a>-0.5f&&a<0.5f);
            up = (a>0.5f&&a<2.6f); down=(a<-0.5f&&a>-2.6f);
            return 0;
        }
        if (dist2(tx,ty,BTN_JUMP_X,BTN_JUMP_Y)<=900) { jump=true; jumpPressed=true; return 1; }
        if (dist2(tx,ty,BTN_ACT_X,BTN_ACT_Y)<=900)   { act1=true; act1Pressed=true; return 2; }
        if (dist2(tx,ty,BTN_ACT2_X,BTN_ACT2_Y)<=676) { act2=true; act2Pressed=true; return 3; }
        return 0;
    }
    static int dist2(int x,int y,int cx,int cy) { int dx=x-cx,dy=y-cy; return dx*dx+dy*dy; }
};

} // namespace dbg
