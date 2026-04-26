#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

// Push constant: model matrix (matches pbr.vert push_constant)
struct PushConstantData {
    glm::mat4 model;
};

// Set 0, binding 0 — matches pbr.vert CameraUBO
struct CameraUBO {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 camPos;
    glm::mat4 lightSpaceMatrix;
};

// Set 1, binding 0
struct MaterialUBO {
    glm::vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    float _pad[2]; // align to 16 bytes
};

// Set 2, binding 0
struct LightUBO {
    glm::vec4 position;
    glm::vec4 color;
    float intensity;
    float _pad[3];
};

class VulkanContext;

class PBRPipeline {
public:
    PBRPipeline(VulkanContext& ctx);
    ~PBRPipeline();

    PBRPipeline(const PBRPipeline&) = delete;
    PBRPipeline& operator=(const PBRPipeline&) = delete;

    void init();
    void cleanup();

    void beginRenderPass(VkCommandBuffer cmd, uint32_t imageIndex);
    void endRenderPass(VkCommandBuffer cmd);

    VkRenderPass renderPass() const { return renderPass_; }
    VkPipeline pipeline() const { return pipeline_; }
    VkPipelineLayout pipelineLayout() const { return pipelineLayout_; }
    VkDescriptorSetLayout cameraSetLayout() const { return cameraSetLayout_; }
    VkDescriptorSetLayout materialSetLayout() const { return materialSetLayout_; }
    VkDescriptorSetLayout lightSetLayout() const { return lightSetLayout_; }

private:
    void createRenderPass();
    void createDescriptorSetLayouts();
    void createPipelineLayout();
    void createGraphicsPipeline();
    void createDepthResources();
    void createFramebuffers();

    VkShaderModule loadShaderModule(const std::string& path);

    VulkanContext& ctx_;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout cameraSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout materialSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout lightSetLayout_ = VK_NULL_HANDLE;

    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> framebuffers_;
};
