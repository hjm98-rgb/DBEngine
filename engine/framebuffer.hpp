// framebuffer.hpp - DBEngine 软件渲染器：把图形画到一个像素缓冲，再由各平台把缓冲贴到屏幕。
// 完全自写，不依赖任何第三方引擎。
#pragma once
#include <vector>
#include <cstdint>
#include <string>

namespace dbg {

struct Color {
    int r, g, b, a;
    Color() : r(255), g(255), b(255), a(255) {}
    Color(int r, int g, int b, int a = 255) : r(r), g(g), b(b), a(a) {}
    static Color rgba(uint32_t v) { return Color((v>>16)&255,(v>>8)&255,v&255,(v>>24)&255); }
    uint32_t abgr() const { return ((uint32_t)a<<24)|((uint32_t)b<<16)|((uint32_t)g<<8)|(uint32_t)r; }
};

struct Framebuffer {
    int w = 0, h = 0;
    std::vector<uint32_t> px; // ABGR (小端内存即 BGRA)，可直接贴屏

    Framebuffer() {}
    Framebuffer(int w, int h) : w(w), h(h), px((size_t)w*h, 0xff000000) {}

    void resize(int nw, int nh) { w=nw; h=nh; px.assign((size_t)w*h, 0xff000000); }

    inline void setPixel(int x, int y, uint32_t c) {
        if (x<0||y<0||x>=w||y>=h) return;
        px[(size_t)y*w+x] = c;
    }
    uint32_t at(int x, int y) const {
        if (x<0||y<0||x>=w||y>=h) return 0xff000000;
        return px[(size_t)y*w+x];
    }

    void clear(Color c) { uint32_t v = c.abgr(); std::fill(px.begin(), px.end(), v); }

    void fillRect(int x, int y, int rw, int rh, Color c) {
        uint32_t v = c.abgr();
        for (int yy=y; yy<y+rh; ++yy) for (int xx=x; xx<x+rw; ++xx) setPixel(xx, yy, v);
    }
    void drawRect(int x, int y, int rw, int rh, Color c) {
        uint32_t v = c.abgr();
        for (int xx=x; xx<x+rw; ++xx) { setPixel(xx,y,v); setPixel(xx,y+rh-1,v); }
        for (int yy=y; yy<y+rh; ++yy) { setPixel(x,yy,v); setPixel(x+rw-1,yy,v); }
    }
    void fillCircle(int cx, int cy, int r, Color c) {
        uint32_t v = c.abgr();
        for (int yy=cy-r; yy<=cy+r; ++yy)
            for (int xx=cx-r; xx<=cx+r; ++xx)
                if ((xx-cx)*(xx-cx)+(yy-cy)*(yy-cy) <= r*r) setPixel(xx,yy,v);
    }
    void drawLine(int x0, int y0, int x1, int y1, Color c) {
        uint32_t v = c.abgr();
        int dx = x1>x0?x1-x0:x0-x1, dy = y1>y0?y1-y0:y0-y1;
        int sx = x0<x1?1:-1, sy = y0<y1?1:-1, err = dx-dy;
        for (;;) {
            setPixel(x0,y0,v);
            if (x0==x1 && y0==y1) break;
            int e2 = err*2;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx)  { err += dx; y0 += sy; }
        }
    }

    // 内置 5x7 点阵字体，画字符串（每字符占 fw x fh，fh 含行距）
    void drawText(const std::string& s, int x, int y, int scale, Color c) {
        uint32_t v = c.abgr();
        int cx = x, cy = y;
        for (unsigned char ch : s) {
            if (ch == '\n') { cy += 8*scale; cx = x; continue; }
            for (int r = 0; r < 7; ++r) {
                int bits = font5x7[(ch>=32&&ch<128)?(ch-32):0][r];
                for (int c2 = 0; c2 < 5; ++c2)
                    if (bits & (1 << (4-c2)))
                        for (int dy=0; dy<scale; ++dy) for (int dx=0; dx<scale; ++dx)
                            setPixel(cx + c2*scale + dx, cy + r*scale + dy, v);
            }
            cx += 6*scale;
        }
    }

    // 5x7 点阵（空格起，覆盖可打印 ASCII 基本字形）
    static const unsigned char font5x7[96][7];
};

// 把帧缓冲存成 PNG（本机截图验证用；Linux 下链接 -lz）
bool save_png(const Framebuffer& fb, const char* path);
// 读取 PNG（仅支持 8bit RGB/RGBA），成功返回 true 并填充 rgba(每像素 0xAABBGGRR, 内存字节序 BGRA)
bool load_png(const char* path, int& w, int& h, std::vector<uint32_t>& rgba);

} // namespace dbg
