# VulkanOBJRenderer

Vulkan 渲染学习项目 — 从零搭建 OBJ 模型 PBR 渲染器。

## 当前进度

**第 1 步：清屏色** ✅ — 窗口 + Vulkan 渲染循环 + 清屏

## 文档

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — 项目结构与设计说明
- [docs/ROADMAP.md](docs/ROADMAP.md) — 分步学习路线（8 个里程碑）

## 环境安装（Ubuntu/Debian）

```bash
sudo apt update
sudo apt install -y \
    libvulkan-dev \
    vulkan-tools \
    vulkan-utility-libraries-dev \
    vulkan-validationlayers \
    glslang-tools \
    libglfw3-dev \
    libglm-dev
```

各包用途：
- `libvulkan-dev` — Vulkan 开发头文件和链接库
- `vulkan-tools` — vulkaninfo 等调试工具
- `vulkan-utility-libraries-dev` — Vulkan Utility Libraries
- `vulkan-validationlayers` — Validation Layer，Debug 模式必备
- `glslang-tools` — glslc 着色器编译器（GLSL → SPIR-V）
- `libglfw3-dev` — 窗口和输入管理
- `libglm-dev` — 数学库（向量/矩阵）

安装后验证：

```bash
vulkaninfo --summary   # 检查 Vulkan 运行时
glslc --version        # 检查着色器编译器
```

## 快速开始

```bash
# 构建 & 运行
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build && ./VulkanOBJRenderer
```

## 准备测试模型（第 4 步需要）

```bash
# Stanford Bunny（GitHub 镜像）
wget -O assets/models/model.obj \
    https://ghfast.top/https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/stanford-bunny.obj

# 如果下载不了，用最简三角形先跑通
cat > assets/models/model.obj << 'EOF'
v -0.5 -0.5 0.0
v  0.5 -0.5 0.0
v  0.0  0.5 0.0
f 1 2 3
EOF
```

## 依赖

- Vulkan SDK 1.3+
- GLFW 3.3+
- GLM
- tinyobjloader（header-only，已在 third_party/）
- stb_image（header-only，已在 third_party/）
