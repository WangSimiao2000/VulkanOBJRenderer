#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <stdexcept>

class VulkanContext {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    explicit VulkanContext(GLFWwindow* window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // ── Accessors ───────────────────────────────────────────────────────
    VkDevice device() const { return device_; }
    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
    VkQueue graphicsQueue() const { return graphicsQueue_; }
    VkQueue presentQueue() const { return presentQueue_; }
    VkSwapchainKHR swapchain() const { return swapchain_; }
    VkFormat swapchainFormat() const { return swapchainFormat_; }
    VkExtent2D swapchainExtent() const { return swapchainExtent_; }
    const std::vector<VkImageView>& swapchainImageViews() const { return swapchainImageViews_; }
    const std::vector<VkImage>& swapchainImages() const { return swapchainImages_; }

    VkCommandPool commandPool() const { return commandPool_; }
    const std::vector<VkCommandBuffer>& commandBuffers() const { return commandBuffers_; }

    const VkSemaphore& imageAvailableSemaphore(uint32_t i) const { return imageAvailableSems_[i]; }
    const VkSemaphore& renderFinishedSemaphore(uint32_t i) const { return renderFinishedSems_[i]; }
    const VkFence& inFlightFence(uint32_t i) const { return inFlightFences_[i]; }

    void recreateSwapchain();

private:
    // ── Init steps (called in constructor order) ────────────────────────
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    // ── Cleanup helpers ─────────────────────────────────────────────────
    void cleanupSwapchain();

    // ── Queue family lookup ─────────────────────────────────────────────
    struct QueueFamilyIndices {
        uint32_t graphics = UINT32_MAX;
        uint32_t present  = UINT32_MAX;
        bool isComplete() const { return graphics != UINT32_MAX && present != UINT32_MAX; }
    };
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev);

    // ── Handles ─────────────────────────────────────────────────────────
    GLFWwindow* window_;

    VkInstance instance_                = VK_NULL_HANDLE;
    VkSurfaceKHR surface_              = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_   = VK_NULL_HANDLE;
    VkDevice device_                   = VK_NULL_HANDLE;
    VkQueue graphicsQueue_             = VK_NULL_HANDLE;
    VkQueue presentQueue_              = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_          = VK_NULL_HANDLE;
    VkFormat swapchainFormat_;
    VkExtent2D swapchainExtent_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;

    VkCommandPool commandPool_         = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    std::vector<VkSemaphore> imageAvailableSems_;
    std::vector<VkSemaphore> renderFinishedSems_;
    std::vector<VkFence> inFlightFences_;

    QueueFamilyIndices queueFamilyIndices_;

#ifdef NDEBUG
    static constexpr bool enableValidation_ = false;
#else
    static constexpr bool enableValidation_ = true;
#endif
    const std::vector<const char*> validationLayers_ = {"VK_LAYER_KHRONOS_validation"};
};
