# 学习路线图

从零开始，每一步都是**可编译、可运行**的里程碑。

---

## 第 1 步：清屏色 ← 你在这里

**目标：** 打开窗口，用 Vulkan 清屏为一个纯色，验证整个渲染循环。

**涉及文件：**
- `CMakeLists.txt` — 构建配置
- `src/core/VulkanContext.h/cpp` — Vulkan 基础设施
- `src/main.cpp` — 窗口 + 主循环

**你会学到：**
- VkInstance、VkDevice、VkQueue 的创建和关系
- Swapchain 是什么、为什么需要多个 image
- Image Layout Transition 和 Pipeline Barrier
- Command Buffer 的录制和提交
- 同步原语：Semaphore（GPU-GPU 同步）和 Fence（CPU-GPU 同步）

**验证标准：** 窗口显示纯色背景（深蓝色），可以正常关闭。

**关键概念：**
```
glfwCreateWindow
       ↓
VkInstance → VkSurface → VkPhysicalDevice → VkDevice
       ↓
VkSwapchain → VkImage[] → VkImageView[]
       ↓
主循环: acquireImage → recordCmd(clearColor) → submit → present
       ↓
同步: Semaphore(acquire→submit, submit→present) + Fence(CPU等GPU)
```

---

## 第 2 步：硬编码三角形

**目标：** 在屏幕上画一个彩色三角形。

**新增文件：**
- `src/renderer/Pipeline.h/cpp` — Graphics Pipeline
- `shaders/basic.vert` — 顶点着色器
- `shaders/basic.frag` — 片段着色器

**修改文件：**
- `src/main.cpp` — 创建 Pipeline，录制 draw call

**你会学到：**
- GLSL 着色器基础（顶点输入 → 光栅化 → 片段输出）
- VkPipeline 的创建（所有固定功能状态的配置）
- Dynamic Rendering（vkCmdBeginRendering / vkCmdEndRendering）
- Vertex Buffer 的创建和绑定
- `vkCmdDraw` 的调用

**实现要点：**
1. 写一个最简单的 vert/frag shader（位置 + 颜色，无变换）
2. 在 Pipeline.cpp 中配置完整的 pipeline state（用 VkPipelineRenderingCreateInfo 指定 attachment 格式）
3. 硬编码 3 个顶点到 vertex buffer
4. 在 render loop 中 vkCmdBeginRendering → bind pipeline → bind vertex buffer → draw → vkCmdEndRendering

**验证标准：** 屏幕中央显示一个彩色三角形。

---

## 第 3 步：Uniform Buffer + MVP 变换

**目标：** 用 Uniform Buffer 传递 MVP 矩阵，让三角形可以旋转。

**新增文件：**
- `src/renderer/Descriptors.h/cpp` — Descriptor Pool/Set 管理
- `src/core/Buffer.h/cpp` — 通用 Buffer 创建工具

**修改文件：**
- `src/renderer/Pipeline.h/cpp` — 添加 descriptor set layout
- `shaders/basic.vert` — 添加 UBO 声明，应用 MVP 变换
- `src/main.cpp` — 创建 UBO，每帧更新矩阵

**你会学到：**
- Descriptor Set 的概念（shader 如何访问 buffer/image）
- std140 内存布局规则（C++ struct 和 GLSL UBO 的对齐）
- Buffer 的内存类型选择（HOST_VISIBLE 用于 UBO）
- GLM 矩阵变换（model/view/projection）

**实现要点：**
1. 定义 `struct MVP { mat4 model, view, proj; }`
2. 创建 descriptor set layout（1 个 UBO binding）
3. 创建 descriptor pool，分配 descriptor set
4. 每帧 memcpy MVP 数据到 mapped buffer
5. `vkCmdBindDescriptorSets` 绑定到 pipeline

**验证标准：** 三角形绕 Y 轴自动旋转。

---

## 第 4 步：深度测试 + OBJ 模型加载

**目标：** 加载 OBJ 模型，正确处理深度遮挡。

**新增文件：**
- `src/scene/Model.h/cpp` — OBJ 加载 + Vertex/Index Buffer

**修改文件：**
- `src/renderer/Pipeline.h/cpp` — 启用深度测试，添加 depth attachment
- `src/main.cpp` — 用 Model 替换硬编码三角形

**你会学到：**
- tinyobjloader 的使用
- Index Buffer 和顶点去重
- Staging Buffer（GPU 专用内存的上传流程）
- 深度测试和 depth image 的创建

**实现要点：**
1. 创建 depth image（VK_FORMAT_D32_SFLOAT）+ image view
2. 在 vkCmdBeginRendering 中通过 pDepthAttachment 指定 depth attachment
3. Pipeline 启用 depthTestEnable + depthWriteEnable
4. Model 类：加载 OBJ → staging buffer → device local buffer

**验证标准：** 屏幕上显示 OBJ 模型，旋转时遮挡关系正确。

---

## 第 5 步：Blinn-Phong 光照

**目标：** 添加基础光照，模型不再是纯色。

**修改文件：**
- `shaders/basic.vert` — 输出世界空间法线和位置
- `shaders/basic.frag` — 实现 Blinn-Phong 光照模型
- `src/main.cpp` — 添加光源 UBO

**你会学到：**
- Blinn-Phong 光照模型（ambient + diffuse + specular）
- 法线变换（为什么需要 normal matrix）
- 多个 Uniform Buffer 的管理（camera UBO + light UBO）
- Push Constant 的使用（传递 model 矩阵）

**实现要点：**
1. Vertex shader 输出 worldPos 和 worldNormal
2. Fragment shader 实现 `ambient + diffuse + specular`
3. 添加 LightUBO（position, color, intensity）
4. 用 Push Constant 传递每个物体的 model 矩阵

**验证标准：** 模型有明暗变化，能看到高光。

---

## 第 6 步：PBR 材质

**目标：** 从 Blinn-Phong 升级到基于物理的渲染。

**新增文件：**
- `shaders/pbr.vert` — PBR 顶点着色器
- `shaders/pbr.frag` — PBR 片段着色器（Cook-Torrance BRDF）

**修改文件：**
- `src/renderer/Pipeline.h/cpp` — 切换到 PBR shader
- `src/renderer/Descriptors.h/cpp` — 添加 MaterialUBO
- `src/main.cpp` — 添加材质参数

**你会学到：**
- 微表面理论（为什么 PBR 比 Blinn-Phong 更真实）
- Cook-Torrance BRDF 的三个组成部分：
  - D：GGX/Trowbridge-Reitz 法线分布函数
  - G：Smith-Schlick 几何遮蔽函数
  - F：Fresnel-Schlick 菲涅尔近似
- metallic/roughness 工作流
- HDR → Tone Mapping → Gamma Correction

**验证标准：** 模型表面有金属/非金属质感，粗糙度可调。

---

## 第 7 步：Shadow Map

**目标：** 添加阴影，增强场景真实感。

**新增文件：**
- `src/renderer/ShadowMap.h/cpp` — 阴影 Pass
- `shaders/shadow.vert` — 深度 Pass 顶点着色器

**修改文件：**
- `shaders/pbr.frag` — 采样阴影贴图
- `src/renderer/Descriptors.h/cpp` — 添加 sampler descriptor set
- `src/main.cpp` — 添加阴影 Pass 到渲染循环

**你会学到：**
- 两 Pass 渲染（shadow pass → main pass）
- 从光源视角渲染深度图
- PCF（Percentage Closer Filtering）软阴影
- 深度偏移（解决 shadow acne 和 peter panning）
- 光空间矩阵的计算

**验证标准：** 模型在地面上投射阴影，阴影边缘柔和。

---

## 第 8 步：SSAO

**目标：** 添加屏幕空间环境光遮蔽，增强空间感。

**新增文件：**
- `src/renderer/SSAO.h/cpp` — SSAO Pass
- `shaders/ssao.vert/frag` — SSAO 着色器

**修改文件：**
- `shaders/pbr.frag` — 采样 SSAO 贴图
- `src/main.cpp` — 添加 SSAO Pass

**你会学到：**
- 屏幕空间技术的基本思路
- 半球采样核心的生成
- 噪声纹理 + 模糊 Pass 去噪
- 多 Pass 渲染的组织方式

**验证标准：** 模型凹陷处和角落有自然的暗色遮蔽效果。

---

## 可选进阶

完成以上 8 步后，可以继续探索：

- **纹理贴图** — Albedo/Normal/Metallic/Roughness Map
- **IBL** — Image-Based Lighting（环境光照）
- **Bloom** — 后处理辉光效果
- **TAA** — 时间性抗锯齿

---

## 参考资源

- [Vulkan Tutorial](https://vulkan-tutorial.com/) — 入门必读
- [vulkan-guide.dev](https://vkguide.dev/) — 更现代的 Vulkan 教程
- [LearnOpenGL PBR](https://learnopengl.com/PBR/Theory) — PBR 理论（概念通用）
- [Sascha Willems Vulkan Examples](https://github.com/SaschaWillems/Vulkan) — 各种技术的参考实现
