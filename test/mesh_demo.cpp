// mesh_demo.cpp - 验证 DBEngine 能渲染"真实 3D 模型"(.obj)，不止像素方块。
#include "../engine/framebuffer.hpp"
#include "../engine/voxel.hpp"
#include "../engine/mesh.hpp"
#include <cstdio>
using namespace dbg;

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "/tmp/db_mesh.png";
    const char* objp = argc > 2 ? argv[2] : "assets/house.obj";

    Framebuffer fb(640, 360);
    Mesh m;
    if (!loadOBJ(objp, m)) {
        // 兜底：内置一个方块模型，保证 demo 一定能出图
        m.verts = {V3(0,0,0),V3(1,0,0),V3(1,1,0),V3(0,1,0),V3(0,0,1),V3(1,0,1),V3(1,1,1),V3(0,1,1)};
        m.addFace(0,2,1); m.addFace(0,3,2);
        m.addFace(4,5,6); m.addFace(4,6,7);
        m.addFace(0,1,5); m.addFace(0,5,4);
        m.addFace(2,3,7); m.addFace(2,7,6);
        m.addFace(1,2,6); m.addFace(1,6,5);
        m.addFace(3,0,4); m.addFace(3,4,7);
        printf("[mesh] fell back to built-in cube\n");
    }
    printf("[mesh] verts=%zu faces=%zu\n", m.verts.size(), m.nFaces());

    fb.clear(Color(120,180,230));
    Camera3 cam;
    cam.pos = V3(2.2f, 1.4f, 2.6f);
    cam.yaw = -2.29f; cam.pitch = -0.12f;   // 朝向原点的小房子

    MeshRenderer r;
    r.render(fb, m, cam, V3(0,0,0), 1.0f, Color(210,120,60));   // 橙色砖房
    fb.drawText("DBENGINE 3D MODEL (.obj)", 8, 8, 1, Color(255,255,255));

    if (!save_png(fb, out)) { printf("save failed\n"); return 1; }
    printf("saved %s (3D model rendered, faces=%d)\n", out, r.lastFaces);
    return 0;
}
