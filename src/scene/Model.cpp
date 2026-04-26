#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "scene/Model.h"

#include <stdexcept>
#include <cstring>
#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// Allow Vertex to be used as unordered_map key for deduplication
namespace std {
template<> struct hash<Vertex> {
    size_t operator()(const Vertex& v) const {
        size_t h = hash<glm::vec3>()(v.position);
        h ^= hash<glm::vec3>()(v.normal) << 1;
        h ^= hash<glm::vec2>()(v.texcoord) << 2;
        return h;
    }
};
}

static bool operator==(const Vertex& a, const Vertex& b) {
    return a.position == b.position && a.normal == b.normal && a.texcoord == b.texcoord;
}

Model::~Model() { cleanup(); }

void Model::cleanup() {
    if (device_ == VK_NULL_HANDLE) return;
    if (indexBuffer_)       { vkDestroyBuffer(device_, indexBuffer_, nullptr);  indexBuffer_ = VK_NULL_HANDLE; }
    if (indexBufferMemory_) { vkFreeMemory(device_, indexBufferMemory_, nullptr); indexBufferMemory_ = VK_NULL_HANDLE; }
    if (vertexBuffer_)       { vkDestroyBuffer(device_, vertexBuffer_, nullptr); vertexBuffer_ = VK_NULL_HANDLE; }
    if (vertexBufferMemory_) { vkFreeMemory(device_, vertexBufferMemory_, nullptr); vertexBufferMemory_ = VK_NULL_HANDLE; }
}

void Model::loadFromOBJ(const std::string& filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
        throw std::runtime_error("Failed to load OBJ: " + warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2],
            };

            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2],
                };
            }

            if (index.texcoord_index >= 0) {
                vertex.texcoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1], // flip V for Vulkan
                };
            }

            auto [it, inserted] = uniqueVertices.try_emplace(vertex, static_cast<uint32_t>(vertices_.size()));
            if (inserted) {
                vertices_.push_back(vertex);
            }
            indices_.push_back(it->second);
        }
    }
}

void Model::createBuffers(VkDevice device, VkPhysicalDevice physicalDevice,
                           VkCommandPool commandPool, VkQueue transferQueue) {
    device_ = device;
    physicalDevice_ = physicalDevice;
    commandPool_ = commandPool;
    transferQueue_ = transferQueue;

    // --- Vertex buffer via staging ---
    VkDeviceSize vertexSize = sizeof(Vertex) * vertices_.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(device_, stagingMemory, 0, vertexSize, 0, &data);
    std::memcpy(data, vertices_.data(), vertexSize);
    vkUnmapMemory(device_, stagingMemory);

    createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 vertexBuffer_, vertexBufferMemory_);

    copyBuffer(stagingBuffer, vertexBuffer_, vertexSize);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);

    // --- Index buffer via staging ---
    VkDeviceSize indexSize = sizeof(uint32_t) * indices_.size();

    createBuffer(indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    vkMapMemory(device_, stagingMemory, 0, indexSize, 0, &data);
    std::memcpy(data, indices_.data(), indexSize);
    vkUnmapMemory(device_, stagingMemory);

    createBuffer(indexSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 indexBuffer_, indexBufferMemory_);

    copyBuffer(stagingBuffer, indexBuffer_, indexSize);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);
}

void Model::draw(VkCommandBuffer commandBuffer) const {
    VkBuffer buffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices_.size()), 1, 0, 0, 0);
}

void Model::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);

    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device_, buffer, memory, 0);
}

void Model::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device_, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy region{0, 0, size};
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(transferQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(transferQueue_);

    vkFreeCommandBuffers(device_, commandPool_, 1, &cmd);
}

uint32_t Model::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}
