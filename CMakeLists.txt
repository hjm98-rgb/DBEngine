cmake_minimum_required(VERSION 3.16)
project(DBEngine LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_library(dbengine STATIC
  engine/framebuffer.cpp
)
target_include_directories(dbengine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine)
target_link_libraries(dbengine PUBLIC ZLIB::ZLIB)

# 桌面版（Linux X11）
find_package(X11 REQUIRED)
add_executable(dbengine-desktop platform/desktop_main.cpp)
target_link_libraries(dbengine-desktop PRIVATE dbengine ${X11_LIBRARIES} Threads::Threads)

# 离线渲染验证工具（无窗口，输出 PNG）
add_executable(dbengine-shot test/offscreen.cpp)
target_link_libraries(dbengine-shot PRIVATE dbengine)

# 通用2D引擎内核演示（证明是通用引擎，不只是MC）
add_executable(dbengine-2ddemo test/engine2d_demo.cpp)
target_link_libraries(dbengine-2ddemo PRIVATE dbengine)

# 3D 网格模型渲染演示（载入 .obj 真实3D模型，证明不只是像素方块）
add_executable(dbengine-mesh test/mesh_demo.cpp)
target_link_libraries(dbengine-mesh PRIVATE dbengine)

# 3D 场景系统演示（天空+地形+模型+模组+玩家）
add_executable(dbengine-scene test/scene_demo.cpp)
target_link_libraries(dbengine-scene PRIVATE dbengine)

# 射击玩法演示（射线命中模型=消除+得分，验证能做FPS向游戏）
add_executable(dbengine-shooter test/shooter_demo.cpp)
target_link_libraries(dbengine-shooter PRIVATE dbengine)

# 小游戏演示（横版躲避接金币，验证能做小游戏）
add_executable(dbengine-minigame test/minigame_demo.cpp)
target_link_libraries(dbengine-minigame PRIVATE dbengine)

# 大型3D演示（纯.obj模型场景：坦克+楼房，无体素，证明不锁MC风格）
add_executable(dbengine-big3d test/big3d_demo.cpp)
target_link_libraries(dbengine-big3d PRIVATE dbengine)

# GPU渲染后端验证（OpenGL: 光照+深度，读回像素）
find_package(X11 REQUIRED)
find_library(GLLIB GL REQUIRED)
add_executable(dbengine-gl test/gl_demo.cpp)
target_link_libraries(dbengine-gl PRIVATE dbengine ${GLLIB} ${X11_LIBRARIES} pthread)

# 开放世界开车(GTA类)演示
add_executable(dbengine-gta test/gta_demo.cpp)
target_link_libraries(dbengine-gta PRIVATE dbengine)

# 带贴图的3D模型演示（UV材质映射：木头/迷彩/草地）
add_executable(dbengine-textured test/textured_demo.cpp)
target_link_libraries(dbengine-textured PRIVATE dbengine)

# 物理演示（重力+落地+弹跳）
add_executable(dbengine-physics test/physics_demo.cpp)
target_link_libraries(dbengine-physics PRIVATE dbengine)

# 动画演示（2D精灵跳动+变色 / 3D模型旋转）
add_executable(dbengine-anim test/anim_demo.cpp)
target_link_libraries(dbengine-anim PRIVATE dbengine)
