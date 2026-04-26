# VulkanOBJRenderer

Vulkan 渲染学习项目 — 从零搭建 OBJ 模型 PBR 渲染器。

## 项目结构

```
VulkanOBJRenderer/
├── src/
│   ├── core/VulkanContext.{h,cpp}    # Vulkan 基础设施（Instance/Device/Swapchain）
│   ├── renderer/
│   │   ├── PBRPipeline.{h,cpp}       # PBR 渲染管线
│   │   ├── ShadowMap.{h,cpp}         # 阴影贴图
│   │   └── SSAO.{h,cpp}             # 屏幕空间环境光遮蔽
│   ├── scene/Model.{h,cpp}          # OBJ 模型加载
│   ├── utils/Camera.h               # FPS 相机
│   └── main.cpp
├── shaders/                          # GLSL 着色器
├── assets/models/                    # OBJ 模型文件
├── third_party/                      # tiny_obj_loader.h, stb_image.h
└── CMakeLists.txt
```

## 依赖

- Vulkan SDK 1.3+
- GLFW 3.3+
- GLM
- tinyobjloader（放入 third_party/）
- stb_image（放入 third_party/）

## 构建

```bash
# 下载 header-only 依赖
wget -P third_party https://raw.githubusercontent.com/tinyobjloader/tinyobjloader/release/tiny_obj_loader.h
wget -P third_party https://raw.githubusercontent.com/nothings/stb/master/stb_image.h

# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## 渐进式学习路线

按以下顺序逐步实现，每一步都是可运行的里程碑：

### 阶段 1：Vulkan 基础（1-2 周）
- [ ] 跑通 VulkanContext：窗口 + Instance + Device + Swapchain
- [ ] 渲染一个清屏颜色（验证渲染循环正确）
- [ ] 理解：命令缓冲、同步原语（Semaphore/Fence）、渲染流程

### 阶段 2：OBJ 加载 + 基础渲染（1 周）
- [ ] 加载 OBJ 模型，创建 Vertex/Index Buffer
- [ ] 实现基础顶点/片段着色器（纯颜色或法线可视化）
- [ ] 理解：Pipeline State Object、Descriptor Set、Push Constant

### 阶段 3：PBR 材质（1-2 周）
- [ ] 实现 Cook-Torrance BRDF（GGX + Smith + Fresnel-Schlick）
- [ ] 添加 metallic/roughness/albedo 材质参数
- [ ] 理解：能量守恒、微表面理论、HDR + Tone Mapping

### 阶段 4：Shadow Map（1 周）
- [ ] 实现深度 Pass（从光源视角渲染）
- [ ] 在 PBR Pass 中采样阴影贴图
- [ ] 添加 PCF 软阴影
- [ ] 理解：深度偏移（Peter Panning / Shadow Acne）、级联阴影（CSM）原理

### 阶段 5：SSAO（1 周）
- [ ] 实现屏幕空间环境光遮蔽
- [ ] 生成采样核心 + 噪声纹理
- [ ] 添加模糊 Pass
- [ ] 理解：G-Buffer、延迟渲染基础、屏幕空间技术

### 阶段 6：进阶（可选）
- [ ] IBL（Image-Based Lighting）
- [ ] 纹理贴图（Albedo/Normal/Metallic/Roughness Map）
- [ ] Bloom 后处理
- [ ] TAA 抗锯齿
