# VulkanOBJRenderer

Vulkan 渲染学习项目 — 从零搭建 OBJ 模型 PBR 渲染器。

## 当前进度

**第 1 步：清屏色** ✅ — 窗口 + Vulkan 渲染循环 + 清屏

## 文档

- [SETUP.md](SETUP.md) — 环境安装指南
- [ARCHITECTURE.md](ARCHITECTURE.md) — 项目结构与设计说明
- [ROADMAP.md](ROADMAP.md) — 分步学习路线（8 个里程碑）

## 快速开始

```bash
# 安装系统依赖（Ubuntu/Debian）
sudo apt install vulkan-tools vulkan-utility-libraries-dev glslang-tools libglfw3-dev libglm-dev

# 下载 header-only 第三方库
wget -P third_party https://ghfast.top/https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/release/tiny_obj_loader.h
wget -P third_party https://ghfast.top/https://raw.githubusercontent.com/nothings/stb/master/stb_image.h

# 构建 & 运行
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build && ./VulkanOBJRenderer
```

## 依赖

- Vulkan SDK 1.3+
- GLFW 3.3+
- GLM
- tinyobjloader（header-only，放入 third_party/）
- stb_image（header-only，放入 third_party/）
