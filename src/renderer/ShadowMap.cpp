#include "ShadowMap.h"
#include <stdexcept>
#include <array>

ShadowMap::ShadowMap(VkDevice device, VkPhysicalDevice physicalDevice)
    : m_device(device), m_physicalDevice(physicalDevice) {}

ShadowMap::~ShadowMap() { cleanup(); }

void ShadowMap::init(VkExtent2D /*extent*/, VkDescriptorSetLayout sceneLayout) {
    createDepthResources();
    createRenderPass();
    createFramebuffer();
    createPipeline(sceneLayout);
    createSampler();
}

void ShadowMap::cleanup() {
    if (m_device == VK_NULL_HANDLE) return;

    vkDestroySampler(m_device, m_sampler, nullptr);
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    vkDestroyImageView(m_device, m_depthImageView, nullptr);
    vkDestroyImage(m_device, m_depthImage, nullptr);
    vkFreeMemory(m_device, m_depthImageMemory, nullptr);
}

void ShadowMap::createDepthResources() {
    // Create depth image (SHADOW_MAP_SIZE x SHADOW_MAP_SIZE)
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageInfo.format      = VK_FORMAT_D32_SFLOAT;
    imageInfo.extent      = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1};
    imageInfo.mipLevels   = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage       = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow map depth image");

    // TODO: allocate memory (query memory requirements, find suitable memory type, bind)
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, m_depthImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    // TODO: find memory type index with VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    allocInfo.memoryTypeIndex = 0;

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_depthImageMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate shadow map memory");

    vkBindImageMemory(m_device, m_depthImage, m_depthImageMemory, 0);

    // Create image view
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image    = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow map image view");
}

void ShadowMap::createRenderPass() {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format         = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthRef;

    // Dependency: ensure depth writes complete before fragment shader reads
    VkSubpassDependency dependency{};
    dependency.srcSubpass    = 0;
    dependency.dstSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &depthAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_renderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow render pass");
}

void ShadowMap::createFramebuffer() {
    VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.renderPass      = m_renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments    = &m_depthImageView;
    fbInfo.width           = SHADOW_MAP_SIZE;
    fbInfo.height          = SHADOW_MAP_SIZE;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow framebuffer");
}

void ShadowMap::createPipeline(VkDescriptorSetLayout sceneLayout) {
    // Push constant for light-space MVP
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount         = 1;
    layoutInfo.pSetLayouts            = &sceneLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushRange;

    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow pipeline layout");

    // TODO: load shadow_depth.vert SPIR-V shader module
    // VkShaderModule vertModule = ...;

    // Depth-only pipeline: no color attachment, no fragment shader
    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    // TODO: fill in vertex input, input assembly, viewport/scissor, rasterization,
    //       depth/stencil state. Key settings:
    //   - Rasterization: enable depth bias (depthBiasEnable, depthBiasConstantFactor, depthBiasSlopeFactor)
    //   - Depth: depthTestEnable = VK_TRUE, depthWriteEnable = VK_TRUE
    //   - No color blend attachments (depth-only)
    //   - Viewport: SHADOW_MAP_SIZE x SHADOW_MAP_SIZE

    pipelineInfo.layout     = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass    = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow pipeline");

    // TODO: vkDestroyShaderModule(m_device, vertModule, nullptr);
}

void ShadowMap::createSampler() {
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor  = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp     = VK_COMPARE_OP_LESS_OR_EQUAL; // PCF-friendly

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow sampler");
}

void ShadowMap::render(VkCommandBuffer cmd, const Model& /*model*/) {
    VkRenderPassBeginInfo rpBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpBegin.renderPass  = m_renderPass;
    rpBegin.framebuffer = m_framebuffer;
    rpBegin.renderArea  = {{0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}};

    VkClearValue clearDepth{};
    clearDepth.depthStencil = {1.0f, 0};
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues    = &clearDepth;

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    // Push light-space matrix
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(glm::mat4), &m_lightSpaceMatrix);

    // TODO: bind vertex/index buffers from model, issue draw calls

    vkCmdEndRenderPass(cmd);
}
