#include "renderer/PBRPipeline.h"
#include "core/VulkanContext.h"

#include <fstream>
#include <stdexcept>
#include <array>

PBRPipeline::PBRPipeline(VulkanContext& ctx) : ctx_(ctx) {}

PBRPipeline::~PBRPipeline() { cleanup(); }

void PBRPipeline::init() {
    createRenderPass();
    createDescriptorSetLayouts();
    createPipelineLayout();
    createDepthResources();
    createFramebuffers();
    createGraphicsPipeline();
}

void PBRPipeline::cleanup() {
    VkDevice dev = ctx_.device();
    for (auto fb : framebuffers_) vkDestroyFramebuffer(dev, fb, nullptr);
    framebuffers_.clear();

    if (depthImageView_) { vkDestroyImageView(dev, depthImageView_, nullptr); depthImageView_ = VK_NULL_HANDLE; }
    if (depthImage_) { vkDestroyImage(dev, depthImage_, nullptr); depthImage_ = VK_NULL_HANDLE; }
    if (depthMemory_) { vkFreeMemory(dev, depthMemory_, nullptr); depthMemory_ = VK_NULL_HANDLE; }
    if (pipeline_) { vkDestroyPipeline(dev, pipeline_, nullptr); pipeline_ = VK_NULL_HANDLE; }
    if (pipelineLayout_) { vkDestroyPipelineLayout(dev, pipelineLayout_, nullptr); pipelineLayout_ = VK_NULL_HANDLE; }
    if (cameraSetLayout_) { vkDestroyDescriptorSetLayout(dev, cameraSetLayout_, nullptr); cameraSetLayout_ = VK_NULL_HANDLE; }
    if (materialSetLayout_) { vkDestroyDescriptorSetLayout(dev, materialSetLayout_, nullptr); materialSetLayout_ = VK_NULL_HANDLE; }
    if (lightSetLayout_) { vkDestroyDescriptorSetLayout(dev, lightSetLayout_, nullptr); lightSetLayout_ = VK_NULL_HANDLE; }
    if (renderPass_) { vkDestroyRenderPass(dev, renderPass_, nullptr); renderPass_ = VK_NULL_HANDLE; }
}

// --- Render pass: color + depth ---

void PBRPipeline::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = ctx_.swapchainFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = static_cast<uint32_t>(attachments.size());
    info.pAttachments = attachments.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(ctx_.device(), &info, nullptr, &renderPass_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass");
}

// --- Descriptor set layouts: set0=camera, set1=material, set2=light ---

void PBRPipeline::createDescriptorSetLayouts() {
    auto makeLayout = [&](VkDescriptorSetLayout& layout) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = 1;
        ci.pBindings = &binding;

        if (vkCreateDescriptorSetLayout(ctx_.device(), &ci, nullptr, &layout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor set layout");
    };

    makeLayout(cameraSetLayout_);
    makeLayout(materialSetLayout_);
    makeLayout(lightSetLayout_);
}

// --- Pipeline layout: push constant + 3 descriptor sets ---

void PBRPipeline::createPipelineLayout() {
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushConstantData);

    std::array<VkDescriptorSetLayout, 3> setLayouts = {
        cameraSetLayout_, materialSetLayout_, lightSetLayout_
    };

    VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    info.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    info.pSetLayouts = setLayouts.data();
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(ctx_.device(), &info, nullptr, &pipelineLayout_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");
}

// --- Graphics pipeline ---

void PBRPipeline::createGraphicsPipeline() {
    // TODO: adjust shader paths to match your build output directory
    auto vertModule = loadShaderModule("shaders/pbr.vert.spv");
    auto fragModule = loadShaderModule("shaders/pbr.frag.spv");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    // Vertex input: pos(vec3) + normal(vec3) + uv(vec2) — matches pbr.vert
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(float) * 8; // 3+3+2
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attrDescs{};
    attrDescs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};                    // position
    attrDescs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3};    // normal
    attrDescs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6};       // uv

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkExtent2D extent = ctx_.swapchainExtent();
    VkViewport viewport{0.f, 0.f, (float)extent.width, (float)extent.height, 0.f, 1.f};
    VkRect2D scissor{{0, 0}, extent};

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(ctx_.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create graphics pipeline");

    vkDestroyShaderModule(ctx_.device(), vertModule, nullptr);
    vkDestroyShaderModule(ctx_.device(), fragModule, nullptr);
}

// --- Depth resources ---

void PBRPipeline::createDepthResources() {
    VkExtent2D extent = ctx_.swapchainExtent();

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (vkCreateImage(ctx_.device(), &imageInfo, nullptr, &depthImage_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(ctx_.device(), depthImage_, &memReqs);

    // Find device-local memory type
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(ctx_.physicalDevice(), &memProps);

    uint32_t memTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memTypeIndex = i;
            break;
        }
    }
    if (memTypeIndex == UINT32_MAX)
        throw std::runtime_error("Failed to find suitable memory type for depth image");

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    if (vkAllocateMemory(ctx_.device(), &allocInfo, nullptr, &depthMemory_) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate depth image memory");

    vkBindImageMemory(ctx_.device(), depthImage_, depthMemory_, 0);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = depthImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

    if (vkCreateImageView(ctx_.device(), &viewInfo, nullptr, &depthImageView_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image view");
}

// --- Framebuffers ---

void PBRPipeline::createFramebuffers() {
    const auto& imageViews = ctx_.swapchainImageViews();
    framebuffers_.resize(imageViews.size());
    VkExtent2D extent = ctx_.swapchainExtent();

    for (size_t i = 0; i < imageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {imageViews[i], depthImageView_};

        VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        info.renderPass = renderPass_;
        info.attachmentCount = static_cast<uint32_t>(attachments.size());
        info.pAttachments = attachments.data();
        info.width = extent.width;
        info.height = extent.height;
        info.layers = 1;

        if (vkCreateFramebuffer(ctx_.device(), &info, nullptr, &framebuffers_[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer");
    }
}

// --- Begin/end render pass ---

void PBRPipeline::beginRenderPass(VkCommandBuffer cmd, uint32_t imageIndex) {
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.01f, 0.01f, 0.02f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    info.renderPass = renderPass_;
    info.framebuffer = framebuffers_[imageIndex];
    info.renderArea = {{0, 0}, ctx_.swapchainExtent()};
    info.clearValueCount = static_cast<uint32_t>(clearValues.size());
    info.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
}

void PBRPipeline::endRenderPass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

// --- SPIR-V loader ---

VkShaderModule PBRPipeline::loadShaderModule(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open shader file: " + path);

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), static_cast<std::streamsize>(fileSize));

    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;
    if (vkCreateShaderModule(ctx_.device(), &info, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module");

    return module;
}
