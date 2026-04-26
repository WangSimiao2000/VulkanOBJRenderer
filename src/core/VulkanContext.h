#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <cstdint>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapchainDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanContext {
public:
    VulkanContext(GLFWwindow* window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void recreateSwapchain();

    // Accessors
    VkDevice device() const { return device_; }
    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
    VkSwapchainKHR swapchain() const { return swapchain_; }
    VkFormat swapchainFormat() const { return swapchainFormat_; }
    VkExtent2D swapchainExtent() const { return swapchainExtent_; }
    const std::vector<VkImageView>& swapchainImageViews() const { return swapchainImageViews_; }
    VkQueue graphicsQueue() const { return graphicsQueue_; }
    VkQueue presentQueue() const { return presentQueue_; }
    VkCommandPool commandPool() const { return commandPool_; }
    const std::vector<VkCommandBuffer>& commandBuffers() const { return commandBuffers_; }
    VkSemaphore imageAvailableSemaphore(uint32_t i) const { return imageAvailableSemaphores_[i]; }
    VkSemaphore renderFinishedSemaphore(uint32_t i) const { return renderFinishedSemaphores_[i]; }
    VkFence inFlightFence(uint32_t i) const { return inFlightFences_[i]; }

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

private:
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    void cleanupSwapchain();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapchainDetails querySwapchainSupport(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);

    GLFWwindow* window_;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_;
    VkExtent2D swapchainExtent_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;

#ifdef NDEBUG
    static constexpr bool enableValidation_ = false;
#else
    static constexpr bool enableValidation_ = true;
#endif
    const std::vector<const char*> validationLayers_ = { "VK_LAYER_KHRONOS_validation" };
    const std::vector<const char*> deviceExtensions_ = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
};
