# DBEngine · 豆包自研跨平台游戏引擎

> 用 C++ 从零写成的游戏引擎：**原生（不是网页引擎）**，软件渲染，可出 **Windows / macOS / Linux / 安卓 / 苹果** 安装包。

- 引擎内核、渲染器、体素世界、贴图系统全部自写，不依赖任何第三方游戏引擎。
- 目标：做出《我的世界》风格的方块世界，并支持**材质包**与**模组扩展**。
- 本项目开源，任何人都可以 Fork 继续扩展。

## 已实现
- 软件渲染器（帧缓冲 / 点阵字体 / PNG 输出与加载）
- 3D 体素世界（地形生成 / 逐面投影 + 深度缓冲）
- 贴图系统 + 内置原版贴图 + 材质包（`textures/` 文件夹放 PNG 即可换皮）
- 桌面版（Linux X11）：编辑器 + 游玩二合一

## 怎么跑（桌面）
```
cmake -S . -B build && cmake --build build
./build/dbengine-desktop     # 编辑器/游玩窗口
./build/dbengine-shot        # 无窗口渲染一帧，存 shot.png
```
操作：`WASD` 移动 · 空格/Shift 上下 · `1-8` 选方块 · 左键放 · 右键敲 · `T/Tab` 切换编辑/游玩 · `Esc` 退出

## 材质包
在程序目录建 `textures/`，放 `grass_top.png grass_side.png dirt.png stone.png wood.png leaves.png sand.png water.png brick.png`，引擎会自动加载替换内置贴图。

## 三端打包（待接入 GitHub Actions）
- Windows `.exe` / macOS `.app` / Linux `.AppImage` / 安卓 `.apk` / 苹果未签名 `.ipa`
