#include "SSAO.h"
#include <random>
#include <stdexcept>
#include <cstring>

SSAO::SSAO(VkDevice device, VkPhysicalDevice physicalDevice)
    : m_device(device), m_physicalDevice(physicalDevice) {}

SSAO::~SSAO() { cleanup(); }

void SSAO::init(VkExtent2D extent, VkImageView positionView, VkImageView normalView) {
    m_extent = extent;
    generateKernelAndNoise();
    createNoiseTexture();
    createSSAOResources(extent);
    createBlurResources(extent);
    createRenderPass();
    createFramebuffers(extent);
    createDescriptorSets(positionView, normalView);
    createPipeline();
}

void SSAO::cleanup() {
    if (m_device == VK_NULL_HANDLE) return;

    vkDestroyPipeline(m_device, m_blurPipeline, nullptr);
    vkDestroyPipeline(m_device, m_ssaoPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

    vkDestroyFramebuffer(m_device, m_blurFramebuffer, nullptr);
    vkDestroyFramebuffer(m_device, m_ssaoFramebuffer, nullptr);
    vkDestroyRenderPass(m_device, m_blurRenderPass, nullptr);
    vkDestroyRenderPass(m_device, m_ssaoRenderPass, nullptr);

    auto destroyImage = [&](VkImage img, VkDeviceMemory mem, VkImageView view) {
        vkDestroyImageView(m_device, view, nullptr);
        vkDestroyImage(m_device, img, nullptr);
        vkFreeMemory(m_device, mem, nullptr);
    };
    destroyImage(m_blurImage, m_blurImageMemory, m_blurImageView);
    destroyImage(m_ssaoImage, m_ssaoImageMemory, m_ssaoImageView);
    destroyImage(m_noiseImage, m_noiseImageMemory, m_noiseImageView);

    vkDestroySampler(m_device, m_noiseSampler, nullptr);
    vkDestroyBuffer(m_device, m_uboBuffer, nullptr);
    vkFreeMemory(m_device, m_uboBufferMemory, nullptr);
}

void SSAO::generateKernelAndNoise() {
    std::default_random_engine gen;
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    // Hemisphere sample kernel
    for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; ++i) {
        glm::vec3 sample(
            dist(gen) * 2.0f - 1.0f,
            dist(gen) * 2.0f - 1.0f,
            dist(gen)  // hemisphere: z in [0, 1]
        );
        sample = glm::normalize(sample) * dist(gen);

        // Accelerating interpolation: bias samples closer to origin
        float scale = static_cast<float>(i) / static_cast<float>(SSAO_KERNEL_SIZE);
        scale = 0.1f + scale * scale * 0.9f; // lerp(0.1, 1.0, scale*scale)
        sample *= scale;

        m_uboData.samples[i] = glm::vec4(sample, 0.0f);
    }

    // 4x4 noise texture (random tangent-space rotations)
    for (auto& n : m_noiseData) {
        n = glm::vec4(
            dist(gen) * 2.0f - 1.0f,
            dist(gen) * 2.0f - 1.0f,
            0.0f, 0.0f
        );
    }
}

void SSAO::createNoiseTexture() {
    // Create 4x4 RGBA16F noise image
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageInfo.format      = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.extent      = {SSAO_NOISE_DIM, SSAO_NOISE_DIM, 1};
    imageInfo.mipLevels   = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_noiseImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO noise image");

    // TODO: allocate & bind memory, stage upload m_noiseData, transition layout

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image    = m_noiseImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_noiseImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO noise image view");

    // Repeating sampler for tiling noise across screen
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter    = VK_FILTER_NEAREST;
    samplerInfo.minFilter    = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_noiseSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO noise sampler");
}

void SSAO::createSSAOResources(VkExtent2D extent) {
    // Single-channel R8 output for SSAO occlusion factor
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageInfo.format      = VK_FORMAT_R8_UNORM;
    imageInfo.extent      = {extent.width, extent.height, 1};
    imageInfo.mipLevels   = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_ssaoImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO image");

    // TODO: allocate & bind memory

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image    = m_ssaoImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_ssaoImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO image view");

    // UBO buffer
    VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufInfo.size  = sizeof(SSAOUbo);
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (vkCreateBuffer(m_device, &bufInfo, nullptr, &m_uboBuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO UBO buffer");

    // TODO: allocate host-visible memory, bind, map persistently
}

void SSAO::createBlurResources(VkExtent2D extent) {
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType   = VK_IMAGE_TYPE_2D;
    imageInfo.format      = VK_FORMAT_R8_UNORM;
    imageInfo.extent      = {extent.width, extent.height, 1};
    imageInfo.mipLevels   = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &m_blurImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO blur image");

    // TODO: allocate & bind memory

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image    = m_blurImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_blurImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO blur image view");
}

void SSAO::createRenderPass() {
    // --- SSAO render pass (single R8 color attachment) ---
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = VK_FORMAT_R8_UNORM;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &colorAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;

    if (vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_ssaoRenderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO render pass");

    // Blur render pass uses identical structure
    if (vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_blurRenderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO blur render pass");
}

void SSAO::createFramebuffers(VkExtent2D extent) {
    VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.renderPass      = m_ssaoRenderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments    = &m_ssaoImageView;
    fbInfo.width           = extent.width;
    fbInfo.height          = extent.height;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_ssaoFramebuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO framebuffer");

    fbInfo.renderPass   = m_blurRenderPass;
    fbInfo.pAttachments = &m_blurImageView;

    if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_blurFramebuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO blur framebuffer");
}

void SSAO::createDescriptorSets(VkImageView positionView, VkImageView normalView) {
    // Bindings: 0=UBO(samples+proj), 1=position, 2=normal, 3=noise
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    bindings[3] = {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};

    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO descriptor set layout");

    // Pool: 2 sets (SSAO + blur)
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6};

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets       = 2;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO descriptor pool");

    // Allocate descriptor sets
    std::array<VkDescriptorSetLayout, 2> layouts = {m_descriptorSetLayout, m_descriptorSetLayout};
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool     = m_descriptorPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts        = layouts.data();

    std::array<VkDescriptorSet, 2> sets{};
    if (vkAllocateDescriptorSets(m_device, &allocInfo, sets.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate SSAO descriptor sets");

    m_ssaoDescriptorSet = sets[0];
    m_blurDescriptorSet = sets[1];

    // TODO: write descriptor sets:
    //   SSAO set: UBO, gPosition, gNormal, noise texture
    //   Blur set: UBO (unused), SSAO output as input sampler
    (void)positionView;
    (void)normalView;
}

void SSAO::createPipeline() {
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &m_descriptorSetLayout;

    if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO pipeline layout");

    // TODO: load ssao.vert + ssao.frag SPIR-V modules (fullscreen quad)
    // TODO: load ssao_blur.frag SPIR-V module

    // Both pipelines render a fullscreen triangle/quad with no vertex input
    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    // TODO: fill in shader stages, empty vertex input, input assembly (triangle strip),
    //       viewport/scissor (m_extent), rasterization, no depth test,
    //       single color blend attachment (R8)

    pipelineInfo.layout     = m_pipelineLayout;
    pipelineInfo.renderPass = m_ssaoRenderPass;
    pipelineInfo.subpass    = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_ssaoPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO pipeline");

    // Blur pipeline (same layout, blur render pass)
    pipelineInfo.renderPass = m_blurRenderPass;
    // TODO: swap fragment shader to blur shader

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_blurPipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create SSAO blur pipeline");

    // TODO: destroy shader modules
}

void SSAO::updateUbo(const glm::mat4& projection) {
    m_uboData.projection = projection;

    // TODO: memcpy m_uboData to mapped UBO buffer
    // void* mapped;
    // vkMapMemory(m_device, m_uboBufferMemory, 0, sizeof(SSAOUbo), 0, &mapped);
    // memcpy(mapped, &m_uboData, sizeof(SSAOUbo));
    // vkUnmapMemory(m_device, m_uboBufferMemory);
}

void SSAO::render(VkCommandBuffer cmd) {
    // --- SSAO pass ---
    VkRenderPassBeginInfo rpBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpBegin.renderPass  = m_ssaoRenderPass;
    rpBegin.framebuffer = m_ssaoFramebuffer;
    rpBegin.renderArea  = {{0, 0}, m_extent};

    VkClearValue clear{};
    clear.color = {{1.0f, 1.0f, 1.0f, 1.0f}};
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues    = &clear;

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssaoPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                            0, 1, &m_ssaoDescriptorSet, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0); // fullscreen triangle
    vkCmdEndRenderPass(cmd);

    // --- Blur pass ---
    rpBegin.renderPass  = m_blurRenderPass;
    rpBegin.framebuffer = m_blurFramebuffer;

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blurPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                            0, 1, &m_blurDescriptorSet, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0); // fullscreen triangle
    vkCmdEndRenderPass(cmd);
}
