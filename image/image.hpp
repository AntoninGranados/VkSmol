#pragma once

#include <vulkan/vulkan.h>

#include <string>

#include "../base/tracked_resource.hpp"

class LogicalDevice;
class PhysicalDevice;

class CommandBuffer;
class CommandPool;
class Queue;

class Buffer;

class Image: public TrackedResource<ResourceType::Image> {
public:
    Image() = default;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other);
    Image& operator=(Image&& other);

    void init(
        const LogicalDevice& device,
        uint32_t width, uint32_t height,
        VkFormat format, VkImageUsageFlags usage
    );
    void initFromPath(
        const LogicalDevice& device, PhysicalDevice physicalDevice,
        const CommandPool& transferCommandPool, Queue transferQueue,
        std::string path
    );
    void alloc(const LogicalDevice& device, PhysicalDevice physicalDevice, VkMemoryPropertyFlags properties);
    void fillFromBuffer(
        const LogicalDevice& device, const CommandBuffer& commandBuffer,
        const Buffer& buffer
    );
    void copyToBuffer(const CommandBuffer& commandBuffer, const Buffer& buffer);
    void free(const LogicalDevice& device);
    void destroy(const LogicalDevice& device);

    VkImage& get() { return image; }
    const VkImage& get() const { return image; }
    VkFormat& getFormat() { return format; }
    const VkFormat& getFormat() const { return format; }
    int getWidth() { return width; }
    int getHeight() { return height; }

private:
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 0, height = 0;
};
