# 环境安装指南

系统：Ubuntu 22.04 / GCC 11.4 / CMake 3.22

## 1. 安装系统依赖

```bash
sudo apt update
sudo apt install -y \
    libvulkan-dev \
    vulkan-tools \
    vulkan-utility-libraries-dev \
    glslang-tools \
    libglfw3-dev \
    libglm-dev
```

各包用途：
- `libvulkan-dev` — Vulkan 开发头文件和链接库
- `vulkan-tools` — vulkaninfo 等调试工具
- `vulkan-utility-libraries-dev` — Vulkan Utility Libraries（替代已废弃的 vulkan-validationlayers-dev）
- `glslang-tools` — glslc 着色器编译器（GLSL → SPIR-V）
- `libglfw3-dev` — 窗口和输入管理
- `libglm-dev` — 数学库（向量/矩阵）

## 2. 下载 header-only 第三方库

在项目根目录下执行：

```bash
# 直连 GitHub（需要稳定的外网访问）
wget -P third_party \
    https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/release/tiny_obj_loader.h
wget -P third_party \
    https://raw.githubusercontent.com/nothings/stb/master/stb_image.h

# 如果下载卡住，使用 GitHub 镜像
wget -P third_party \
    https://ghfast.top/https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/release/tiny_obj_loader.h
wget -P third_party \
    https://ghfast.top/https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
```

## 3. 验证安装

```bash
# 检查 Vulkan 是否可用
vulkaninfo --summary

# 检查着色器编译器
glslc --version

# 检查 GLFW
pkg-config --modversion glfw3
```

vulkaninfo 应该能看到你的 Intel GPU 信息。如果报错，检查 mesa-vulkan-drivers 是否已装（你的环境已有）。

## 4. 构建项目

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## 5. 准备测试模型

放一个 OBJ 模型到 `assets/models/model.obj`。可以用这些免费资源：

```bash
# Stanford Bunny（GitHub 镜像）
wget -O assets/models/model.obj \
    https://ghfast.top/https://raw.githubusercontent.com/alecjacobson/common-3d-test-models/master/data/stanford-bunny.obj

# 或者直连原始地址（需要稳定的外网访问）
wget -O assets/models/model.obj \
    https://graphics.stanford.edu/~mdfisher/Data/Meshes/bunny.obj

# 如果都下载不了，用最简三角形先跑通渲染管线
cat > assets/models/model.obj << 'EOF'
v -0.5 -0.5 0.0
v  0.5 -0.5 0.0
v  0.0  0.5 0.0
f 1 2 3
EOF
```

或者从 [Morgan McGuire's Archive](https://casual-effects.com/data/) 下载更多测试模型。
