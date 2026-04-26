#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>

// Forward declarations
struct Model; // TODO: replace with actual Model type

constexpr uint32_t SHADOW_MAP_SIZE = 2048;

class ShadowMap {
public:
    ShadowMap(VkDevice device, VkPhysicalDevice physicalDevice);
    ~ShadowMap();

    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;

    void init(VkExtent2D extent, VkDescriptorSetLayout sceneLayout);
    void cleanup();

    void render(VkCommandBuffer cmd, const Model& model);

    void setLightSpaceMatrix(const glm::mat4& lightSpace) { m_lightSpaceMatrix = lightSpace; }
    const glm::mat4& getLightSpaceMatrix() const { return m_lightSpaceMatrix; }

    // Expose shadow map for sampling in PBR pass
    VkImageView getShadowMapView() const { return m_depthImageView; }
    VkSampler   getShadowSampler() const { return m_sampler; }

private:
    void createDepthResources();
    void createRenderPass();
    void createFramebuffer();
    void createPipeline(VkDescriptorSetLayout sceneLayout);
    void createSampler();

    VkDevice         m_device         = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    // Depth attachment
    VkImage        m_depthImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView    m_depthImageView   = VK_NULL_HANDLE;

    // Render pass & framebuffer
    VkRenderPass  m_renderPass  = VK_NULL_HANDLE;
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    // Depth-only pipeline
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       m_pipeline       = VK_NULL_HANDLE;

    // Sampler for PBR shadow sampling
    VkSampler m_sampler = VK_NULL_HANDLE;

    glm::mat4 m_lightSpaceMatrix{1.0f};
};
