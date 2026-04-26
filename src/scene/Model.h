#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <vector>
#include <string>
#include <array>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;

    static VkVertexInputBindingDescription bindingDescription() {
        return {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions() {
        return {{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
            {2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, texcoord)},
        }};
    }
};

class Model {
public:
    Model() = default;
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // Load geometry from .obj file
    void loadFromOBJ(const std::string& filepath);

    // Upload vertex/index data to GPU buffers
    void createBuffers(VkDevice device, VkPhysicalDevice physicalDevice,
                       VkCommandPool commandPool, VkQueue transferQueue);

    // Bind buffers and issue draw call
    void draw(VkCommandBuffer commandBuffer) const;

    const std::vector<Vertex>& vertices() const { return vertices_; }
    const std::vector<uint32_t>& indices() const { return indices_; }

private:
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory);

    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void cleanup();

    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkQueue transferQueue_ = VK_NULL_HANDLE;

    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer indexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory_ = VK_NULL_HANDLE;
};
