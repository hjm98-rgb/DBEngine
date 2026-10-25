// texture.hpp - 贴图系统：方块面贴图 + 内置原版贴图生成 + 材质包(PNG文件夹)加载
#pragma once
#include "framebuffer.hpp"
#include <map>
#include <string>
#include <cstdint>
#include <cmath>

namespace dbg {

// 方块类型（供材质包与体素世界共用）
enum Block : uint8_t {
    B_AIR = 0, B_GRASS = 1, B_DIRT = 2, B_STONE = 3, B_WOOD = 4,
    B_LEAVES = 5, B_SAND = 6, B_WATER = 7, B_BRICK = 8
};
// 方块调色板（无贴图时的底色/兜底）
static Color blockColor(uint8_t id) {
    switch (id) {
        case B_GRASS:  return Color(96, 180, 78);
        case B_DIRT:   return Color(134, 96, 65);
        case B_STONE:  return Color(128, 128, 132);
        case B_WOOD:   return Color(150, 108, 62);
        case B_LEAVES: return Color(70, 140, 58);
        case B_SAND:   return Color(222, 210, 150);
        case B_WATER:  return Color(70, 120, 200);
        case B_BRICK:  return Color(170, 70, 60);
        default:       return Color(180, 60, 220);
    }
}
static const char* blockName(uint8_t id) {
    switch (id) {
        case B_GRASS: return "GRASS"; case B_DIRT: return "DIRT";
        case B_STONE: return "STONE"; case B_WOOD: return "WOOD";
        case B_LEAVES:return "LEAVES";case B_SAND: return "SAND";
        case B_WATER: return "WATER"; case B_BRICK:return "BRICK";
        default: return "???";
    }
}


// 一张贴图（RGBA，内存 ABGR）
struct Texture {
    int w = 0, h = 0;
    std::vector<uint32_t> px;

    Texture() {}
    Texture(int w, int h) : w(w), h(h), px((size_t)w*h, 0xffffffff) {}

    inline uint32_t pixel(int x, int y) const {
        if (w<=0||h<=0) return 0xffffffff;
        return px[(size_t)(y%h)*w + (x%w)];
    }
    // 最近邻采样，u,v∈[0,1]
    inline uint32_t sample(float u, float v) const {
        int x = (int)(u*w), y = (int)(v*h);
        return pixel(x, y);
    }
    void fromARGB(const std::vector<uint32_t>& argb) { // argb: AARRGGBB
        if ((int)argb.size() < w*h) return;
        for (int i = 0; i < w*h; i++) {
            uint32_t a = argb[i]>>24, r=(argb[i]>>16)&255, g=(argb[i]>>8)&255, b=argb[i]&255;
            px[i] = (a<<24)|(b<<16)|(g<<8)|r;
        }
    }
};

// 简单的确定性噪声，用于生成"不垃圾"的原版贴图
inline int hashNoise(int x, int y, int seed) {
    unsigned h = (unsigned)(x*374761393 + y*668265263 + seed*2147483647u);
    h = (h ^ (h>>13)) * 1274126177u;
    return (int)((h ^ (h>>16)) & 0xffff);
}

inline Color jitter(Color c, int n, int seed, int amount) {
    int dr = (n%(amount*2+1))-amount, dg = ((n>>3)%(amount*2+1))-amount, db=((n>>6)%(amount*2+1))-amount;
    return Color(c.r+dr, c.g+dg, c.b+db);
}

// 材质包：名字 -> 贴图。从文件夹加载 PNG，缺失则用内置生成。
class TexturePack {
public:
    std::map<std::string, Texture> tex;

    // 生成 16x16 内置原版贴图
    void generateDefaults() {
        const int S = 16;
        auto noise = [&](const std::string& name, Color base, int amount, int seed) {
            Texture t(S,S);
            for (int y=0;y<S;y++) for (int x=0;x<S;x++) {
                Color c = jitter(base, hashNoise(x,y,seed), seed, amount);
                t.px[(size_t)y*S+x] = c.abgr();
            }
            tex[name] = t;
        };
        noise("dirt",   Color(134,96,65),   16, 11);
        noise("stone",  Color(128,128,132), 18, 22);
        noise("sand",   Color(222,210,150), 12, 33);
        noise("leaves", Color(70,140,58),   22, 44);
        // 草方块顶（鲜绿）
        { Texture t(S,S); for (int y=0;y<S;y++)for(int x=0;x<S;x++)
            t.px[(size_t)y*S+x]=jitter(Color(96,180,78),hashNoise(x,y,55),55,14).abgr(); tex["grass_top"]=t; }
        // 草方块侧：顶部几行绿草，下面泥土
        { Texture t(S,S); for (int y=0;y<S;y++)for(int x=0;x<S;x++){
            Color c = y<4 ? jitter(Color(96,180,78),hashNoise(x,y,66),66,14)
                          : jitter(Color(134,96,65),hashNoise(x,y,11),11,16);
            t.px[(size_t)y*S+x]=c.abgr(); } tex["grass_side"]=t; }
        // 木头：木板纹
        { Texture t(S,S); for (int y=0;y<S;y++)for(int x=0;x<S;x++){
            int grain = (y%4==0||y%4==1)?1:0;
            Color c = grain ? Color(172,124,70) : Color(150,108,62);
            c = jitter(c, hashNoise(x,y,77),77,10);
            t.px[(size_t)y*S+x]=c.abgr(); } tex["wood"]=t; }
        // 砖墙
        { Texture t(S,S); for (int y=0;y<S;y++)for(int x=0;x<S;x++){
            int row=y/4; int off=(row%2)*4;
            bool mortar = (y%4==3) || ((x+off)%8==7);
            Color c = mortar ? Color(90,90,95) : jitter(Color(170,70,60),hashNoise(x,y,88),88,12);
            t.px[(size_t)y*S+x]=c.abgr(); } tex["brick"]=t; }
        // 水
        { Texture t(S,S); for (int y=0;y<S;y++)for(int x=0;x<S;x++){
            int wave=(y%4<2)?1:0;
            Color c = wave ? Color(90,140,215) : Color(70,120,200);
            c=jitter(c,hashNoise(x,y,99),99,8);
            t.px[(size_t)y*S+x]=c.abgr(); } tex["water"]=t; }
    }

    // 从文件夹加载材质包（<dir>/<name>.png），缺失则用内置
    void loadOrGenerate(const std::string& dir) {
        generateDefaults();
        const char* names[] = {"dirt","stone","sand","leaves","grass_top","grass_side","wood","brick","water"};
        for (const char* n : names) {
            std::string path = dir + "/" + n + ".png";
            int w=0,h=0; std::vector<uint32_t> argb;
            if (load_png(path.c_str(), w, h, argb) && w>0 && h>0) {
                Texture t(w,h); t.fromARGB(argb); tex[n]=t;
            }
        }
    }

    // 取某方块某面的贴图（顶/侧/底）
    const Texture* face(uint8_t block, int face) const {
        std::string key;
        switch (block) {
            case B_GRASS: key = (face==1) ? "grass_top" : (face==-1 ? "dirt" : "grass_side"); break;
            case B_DIRT:  key = "dirt"; break;
            case B_STONE: key = "stone"; break;
            case B_WOOD:  key = "wood"; break;
            case B_LEAVES:key = "leaves"; break;
            case B_SAND:  key = "sand"; break;
            case B_WATER: key = "water"; break;
            case B_BRICK: key = "brick"; break;
            default: return nullptr;
        }
        auto it = tex.find(key);
        return it == tex.end() ? nullptr : &it->second;
    }
};

} // namespace dbg
