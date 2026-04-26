#include "core/VulkanContext.h"

#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

// ── Step 1: Clear Screen ────────────────────────────────────────────────────
//
// 这是最小的 Vulkan 渲染循环：
//   1. 等待上一帧的 fence（CPU 等 GPU 完成）
//   2. 获取 swapchain 的下一张 image
//   3. 录制 command buffer（这里只做 clear）
//   4. 提交 command buffer 到 GPU
//   5. 呈现 image 到屏幕
//
// 理解这个循环后，后续所有步骤都是在第 3 步里加东西。

static bool g_framebufferResized = false;

static void framebufferResizeCallback(GLFWwindow*, int, int) {
    g_framebufferResized = true;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // 不要 OpenGL context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Step 1: Clear Screen", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    try {
        VulkanContext ctx(window);
        uint32_t currentFrame = 0;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);

            // ── 1. 等待上一帧完成 ───────────────────────────────────────
            vkWaitForFences(ctx.device(), 1, &ctx.inFlightFence(currentFrame),
                            VK_TRUE, UINT64_MAX);

            // ── 2. 获取下一张 swapchain image ───────────────────────────
            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(
                ctx.device(), ctx.swapchain(), UINT64_MAX,
                ctx.imageAvailableSemaphore(currentFrame), VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                ctx.recreateSwapchain();
                continue;
            }

            vkResetFences(ctx.device(), 1, &ctx.inFlightFence(currentFrame));

            // ── 3. 录制 command buffer ──────────────────────────────────
            VkCommandBuffer cmd = ctx.commandBuffers()[currentFrame];
            vkResetCommandBuffer(cmd, 0);

            VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            vkBeginCommandBuffer(cmd, &beginInfo);

            // 用 image memory barrier 把 swapchain image 转换到可写状态
            // （后续加了 RenderPass 后这一步会自动处理，但现在手动做）
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image               = ctx.swapchainImages()[imageIndex];
            barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            barrier.srcAccessMask       = 0;
            barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);

            // 清屏：深蓝色
            VkClearColorValue clearColor = {{0.01f, 0.01f, 0.05f, 1.0f}};
            VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdClearColorImage(cmd, ctx.swapchainImages()[imageIndex],
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

            // 转换到 present 状态
            barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = 0;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);

            vkEndCommandBuffer(cmd);

            // ── 4. 提交到 GPU ───────────────────────────────────────────
            VkSemaphore waitSems[]   = {ctx.imageAvailableSemaphore(currentFrame)};
            VkSemaphore signalSems[] = {ctx.renderFinishedSemaphore(currentFrame)};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

            VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submitInfo.waitSemaphoreCount   = 1;
            submitInfo.pWaitSemaphores      = waitSems;
            submitInfo.pWaitDstStageMask    = waitStages;
            submitInfo.commandBufferCount   = 1;
            submitInfo.pCommandBuffers      = &cmd;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores    = signalSems;

            if (vkQueueSubmit(ctx.graphicsQueue(), 1, &submitInfo,
                              ctx.inFlightFence(currentFrame)) != VK_SUCCESS)
                throw std::runtime_error("Failed to submit command buffer");

            // ── 5. 呈现到屏幕 ───────────────────────────────────────────
            VkSwapchainKHR swapchains[] = {ctx.swapchain()};
            VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores    = signalSems;
            presentInfo.swapchainCount     = 1;
            presentInfo.pSwapchains        = swapchains;
            presentInfo.pImageIndices      = &imageIndex;

            result = vkQueuePresentKHR(ctx.presentQueue(), &presentInfo);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
                g_framebufferResized) {
                g_framebufferResized = false;
                ctx.recreateSwapchain();
            }

            currentFrame = (currentFrame + 1) % VulkanContext::MAX_FRAMES_IN_FLIGHT;
        }

        vkDeviceWaitIdle(ctx.device());

    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
