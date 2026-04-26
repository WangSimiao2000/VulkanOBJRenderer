#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>

constexpr uint32_t SSAO_KERNEL_SIZE = 64;
constexpr uint32_t SSAO_NOISE_DIM   = 4;

struct SSAOUbo {
    std::array<glm::vec4, SSAO_KERNEL_SIZE> samples;
    glm::mat4 projection;
};

class SSAO {
public:
    SSAO(VkDevice device, VkPhysicalDevice physicalDevice);
    ~SSAO();

    SSAO(const SSAO&) = delete;
    SSAO& operator=(const SSAO&) = delete;

    void init(VkExtent2D extent, VkImageView positionView, VkImageView normalView);
    void cleanup();

    void render(VkCommandBuffer cmd);
    void updateUbo(const glm::mat4& projection);

    VkImageView getSSAOResultView() const { return m_ssaoImageView; }

private:
    void generateKernelAndNoise();
    void createNoiseTexture();
    void createSSAOResources(VkExtent2D extent);
    void createRenderPass();
    void createFramebuffers(VkExtent2D extent);
    void createDescriptorSets(VkImageView positionView, VkImageView normalView);
    void createPipeline();
    void createBlurResources(VkExtent2D extent);

    VkDevice         m_device         = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkExtent2D       m_extent{};

    // Kernel & noise data
    SSAOUbo m_uboData{};
    std::array<glm::vec4, SSAO_NOISE_DIM * SSAO_NOISE_DIM> m_noiseData{};

    // Noise texture
    VkImage        m_noiseImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_noiseImageMemory = VK_NULL_HANDLE;
    VkImageView    m_noiseImageView   = VK_NULL_HANDLE;
    VkSampler      m_noiseSampler     = VK_NULL_HANDLE;

    // SSAO output (pre-blur)
    VkImage        m_ssaoImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_ssaoImageMemory = VK_NULL_HANDLE;
    VkImageView    m_ssaoImageView   = VK_NULL_HANDLE;

    // Blur output
    VkImage        m_blurImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_blurImageMemory = VK_NULL_HANDLE;
    VkImageView    m_blurImageView   = VK_NULL_HANDLE;

    // UBO buffer
    VkBuffer       m_uboBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory m_uboBufferMemory = VK_NULL_HANDLE;

    // Render passes & framebuffers
    VkRenderPass  m_ssaoRenderPass  = VK_NULL_HANDLE;
    VkFramebuffer m_ssaoFramebuffer = VK_NULL_HANDLE;
    VkRenderPass  m_blurRenderPass  = VK_NULL_HANDLE;
    VkFramebuffer m_blurFramebuffer = VK_NULL_HANDLE;

    // Pipelines
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_descriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       m_ssaoDescriptorSet   = VK_NULL_HANDLE;
    VkDescriptorSet       m_blurDescriptorSet   = VK_NULL_HANDLE;
    VkPipelineLayout      m_pipelineLayout      = VK_NULL_HANDLE;
    VkPipeline            m_ssaoPipeline        = VK_NULL_HANDLE;
    VkPipeline            m_blurPipeline        = VK_NULL_HANDLE;
};
