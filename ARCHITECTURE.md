# 项目架构

## 目录结构

```
VulkanOBJRenderer/
├── src/
│   ├── core/                   # Vulkan 基础设施（不涉及"画什么"）
│   │   ├── VulkanContext.h/cpp #   Instance, Device, Swapchain, CommandPool, Sync
│   │   └── Buffer.h/cpp       #   通用 Buffer/Image 创建 + 内存分配工具
│   │
│   ├── renderer/               # 渲染管线（决定"怎么画"）
│   │   ├── Pipeline.h/cpp     #   基础渲染管线（RenderPass, Pipeline, Framebuffer）
│   │   ├── Descriptors.h/cpp  #   Descriptor Pool/Set 管理
│   │   ├── ShadowMap.h/cpp    #   阴影贴图 Pass
│   │   └── SSAO.h/cpp         #   屏幕空间环境光遮蔽 Pass
│   │
│   ├── scene/                  # 场景数据（决定"画什么"）
│   │   └── Model.h/cpp        #   OBJ 模型加载 + Vertex/Index Buffer
│   │
│   ├── utils/                  # 独立工具（不依赖 Vulkan）
│   │   └── Camera.h           #   FPS 相机
│   │
│   └── main.cpp                # 入口：窗口创建、主循环、组装各模块
│
├── shaders/                    # GLSL 着色器源码（构建时自动编译为 SPIR-V）
│   ├── basic.vert/frag        #   第 2-3 步：基础顶点/片段着色器
│   ├── pbr.vert/frag          #   第 6 步：PBR 着色器
│   ├── shadow.vert/frag       #   第 7 步：阴影深度 Pass
│   └── ssao.vert/frag         #   第 8 步：SSAO Pass
│
├── assets/
│   ├── models/                 # OBJ 模型文件
│   └── textures/               # 纹理贴图（后续阶段使用）
│
├── third_party/                # Header-only 第三方库
│   ├── tiny_obj_loader.h
│   └── stb_image.h
│
├── old_src/                    # 旧代码备份（可随时参考或删除）
│
├── CMakeLists.txt              # 构建配置
├── ARCHITECTURE.md             # ← 你正在读的这个文件
├── ROADMAP.md                  # 分步学习路线
├── SETUP.md                    # 环境安装指南
└── README.md                   # 项目简介
```

## 设计原则

### 1. 分层解耦

```
main.cpp（组装层）
    ↓ 使用
renderer/（渲染层）──→ 决定怎么画
    ↓ 依赖
core/（基础设施层）──→ 管理 Vulkan 资源
    ↑ 被使用
scene/（数据层）────→ 提供几何数据
```

- **core/** 不知道要画什么，只负责 Vulkan 资源的生命周期
- **renderer/** 不知道数据从哪来，只负责配置渲染管线
- **scene/** 不知道怎么渲染，只负责加载和管理几何数据
- **main.cpp** 把它们组装在一起

### 2. 渐进式扩展

每个阶段只新增文件，不需要大改已有代码：

| 阶段 | 新增/修改 | 已有代码改动 |
|------|----------|-------------|
| 清屏 | VulkanContext + main | 无 |
| 三角形 | Pipeline + basic shader | main 加几行 |
| Uniform | Descriptors + Buffer | Pipeline 加 layout |
| OBJ 模型 | Model | main 替换顶点数据 |
| 光照 | 修改 shader | 无 C++ 改动 |
| PBR | 新 shader + 改 Descriptors | Pipeline 换 shader |
| Shadow | ShadowMap + shadow shader | main 加一个 Pass |
| SSAO | SSAO + ssao shader | main 加一个 Pass |

### 3. 文件命名约定

- 每个类一对 `.h/.cpp` 文件，文件名与类名一致
- Shader 文件名 = `用途.阶段`，如 `pbr.vert`、`shadow.frag`
- 头文件用 `#pragma once`
- Include 路径相对于 `src/`，如 `#include "core/VulkanContext.h"`
