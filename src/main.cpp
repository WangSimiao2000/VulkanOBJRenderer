#include "core/VulkanContext.h"
#include "scene/Model.h"
#include "renderer/PBRPipeline.h"
#include "renderer/ShadowMap.h"
#include "renderer/SSAO.h"
#include "utils/Camera.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <iostream>
#include <chrono>

// ── Globals for GLFW callbacks ──────────────────────────────────────────────
static Camera g_camera;
static float  g_lastX = 640.0f, g_lastY = 360.0f;
static bool   g_firstMouse = true;
static bool   g_framebufferResized = false;

static void framebufferResizeCallback(GLFWwindow*, int, int) {
    g_framebufferResized = true;
}

static void mouseCallback(GLFWwindow*, double xpos, double ypos) {
    auto xf = static_cast<float>(xpos), yf = static_cast<float>(ypos);
    if (g_firstMouse) { g_lastX = xf; g_lastY = yf; g_firstMouse = false; }
    g_camera.processMouseMovement(xf - g_lastX, g_lastY - yf); // y inverted
    g_lastX = xf;
    g_lastY = yf;
}

static void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) g_camera.processKeyboard(FORWARD,  dt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) g_camera.processKeyboard(BACKWARD, dt);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) g_camera.processKeyboard(LEFT,     dt);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) g_camera.processKeyboard(RIGHT,    dt);
}

// ── Main ────────────────────────────────────────────────────────────────────
int main() {
    // GLFW init
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Vulkan OBJ Renderer", nullptr, nullptr);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    try {
        // ── Vulkan context ──────────────────────────────────────────────
        VulkanContext ctx(window);

        // ── Load model ──────────────────────────────────────────────────
        Model model;
        model.loadFromOBJ("assets/models/model.obj");
        model.createBuffers(ctx.device(), ctx.physicalDevice(),
                            ctx.commandPool(), ctx.graphicsQueue());

        // ── Renderer components ─────────────────────────────────────────
        PBRPipeline pbrPipeline(ctx);
        pbrPipeline.init();

        ShadowMap shadowMap(ctx.device(), ctx.physicalDevice());
        shadowMap.init(ctx.swapchainExtent(), pbrPipeline.cameraSetLayout());

        SSAO ssao(ctx.device(), ctx.physicalDevice());
        // NOTE: positionView / normalView would come from a G-buffer pass;
        // using VK_NULL_HANDLE as placeholder until G-buffer is implemented.
        ssao.init(ctx.swapchainExtent(), VK_NULL_HANDLE, VK_NULL_HANDLE);

        // ── Light setup ─────────────────────────────────────────────────
        glm::vec3 lightPos(5.0f, 10.0f, 5.0f);
        glm::mat4 lightProj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 50.0f);
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        shadowMap.setLightSpaceMatrix(lightProj * lightView);

        // ── Timing ──────────────────────────────────────────────────────
        uint32_t currentFrame = 0;
        auto lastTime = std::chrono::high_resolution_clock::now();

        // ── Main loop ───────────────────────────────────────────────────
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            auto now = std::chrono::high_resolution_clock::now();
            float dt = std::chrono::duration<float>(now - lastTime).count();
            lastTime = now;

            processInput(window, dt);

            // ── Wait for previous frame's fence ─────────────────────────
            vkWaitForFences(ctx.device(), 1, &ctx.inFlightFence(currentFrame),
                            VK_TRUE, UINT64_MAX);

            // ── Acquire next swapchain image ────────────────────────────
            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(
                ctx.device(), ctx.swapchain(), UINT64_MAX,
                ctx.imageAvailableSemaphore(currentFrame), VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                ctx.recreateSwapchain();
                continue;
            }
            if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
                throw std::runtime_error("failed to acquire swapchain image");

            vkResetFences(ctx.device(), 1, &ctx.inFlightFence(currentFrame));

            // ── Record command buffer ───────────────────────────────────
            VkCommandBuffer cmd = ctx.commandBuffers()[currentFrame];
            vkResetCommandBuffer(cmd, 0);

            VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            vkBeginCommandBuffer(cmd, &beginInfo);

            // 1) Shadow pass
            shadowMap.render(cmd, model);

            // 2) SSAO pass
            ssao.updateUbo(glm::perspective(
                glm::radians(45.0f),
                static_cast<float>(ctx.swapchainExtent().width) /
                    static_cast<float>(ctx.swapchainExtent().height),
                0.1f, 100.0f));
            ssao.render(cmd);

            // 3) PBR pass
            pbrPipeline.beginRenderPass(cmd, imageIndex);

            PushConstantData push{};
            push.model = glm::mat4(1.0f);
            vkCmdPushConstants(cmd, pbrPipeline.pipelineLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT, 0,
                               sizeof(PushConstantData), &push);

            model.draw(cmd);

            pbrPipeline.endRenderPass(cmd);

            vkEndCommandBuffer(cmd);

            // ── Submit ──────────────────────────────────────────────────
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
                throw std::runtime_error("failed to submit draw command buffer");

            // ── Present ─────────────────────────────────────────────────
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
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("failed to present swapchain image");
            }

            currentFrame = (currentFrame + 1) % VulkanContext::MAX_FRAMES_IN_FLIGHT;
        }

        vkDeviceWaitIdle(ctx.device());

        // Destructors handle cleanup in reverse order
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
