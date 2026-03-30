#pragma once

#include "engine/memory/dynamic_per_frame_buffer.hpp"
#include "engine/memory/shared_buffer.hpp"
#include "engine/resource/resource_registry.hpp"

#include "engine/descriptor/descriptor_set_group.hpp"
#include "engine/descriptor/descriptor_set_layout.hpp"
#include "engine/descriptor/descriptor_writer.hpp"

#include "engine/memory/per_frame_buffer.hpp"

struct ResourceManagerContext {
    LogicalDevice* device = nullptr;
    PhysicalDevice* physicalDevice = nullptr;
    CommandPool* transferCommandPool = nullptr;
    Queue* transferQueue = nullptr;
};

class ResourceManager {
public:
    void init(ResourceManagerContext context);
    void terminate();   // TODO

    void initDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);
    void destroyDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);

    DescriptorSetGroup createDescriptorSetGroup(DescriptorSetLayout& layout);
    void updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer);
    void destroyDescriptorSetGroup(DescriptorSetGroup& group);

    Buffer createBuffer(VkBufferUsageFlags usage, size_t size, void* data = nullptr);
    Buffer createReadbackBuffer(size_t size, bool persistentMapping = true);
    void fillBuffer(Buffer& buffer, void* data);
    void readBuffer(Buffer& buffer, void* out, size_t bytes, size_t offset = 0);
    void destroyBuffer(Buffer& buffer);
    
    template<typename T>
    PerFrameBuffer<T> createPerFrameBuffer(VkBufferUsageFlags usage, size_t count = 1) {
        const ResourceManagerContext& context = requireInitialized("ResourceManager::createPerFrameBuffer");
        PerFrameBuffer<T> buffer = PerFrameBuffer<T>::create(*context.device, *context.physicalDevice, usage, count);
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            buffer.at(i).setResourceId(registry.create(ResourceType::Buffer));
        }
        return buffer;
    }
    template<typename T>
    void destroyPerFrameBuffer(PerFrameBuffer<T>& buffer) {
        requireInitialized("ResourceManager::destroyPerFrameBuffer");
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            registry.destroy(buffer.at(i).getResourceId());
            buffer.at(i).resetResourceId();
        }
        buffer.destroy(*ctx.device);
    }
    
    template<typename T>
    SharedBuffer<T> createSharedBuffer(VkBufferUsageFlags usage, size_t count = 1) {
        const ResourceManagerContext& context = requireInitialized("ResourceManager::createSharedBuffer");
        SharedBuffer<T> buffer = SharedBuffer<T>::create(*context.device, *context.physicalDevice, usage, count);
        buffer.buffer.setResourceId(registry.create(ResourceType::Buffer));
        return buffer;
    }
    template<typename T>
    void destroySharedBuffer(SharedBuffer<T>& buffer) {
        requireInitialized("ResourceManager::destroySharedBuffer");
        registry.destroy(buffer.buffer.getResourceId());
        buffer.buffer.resetResourceId();
        buffer.destroy(*ctx.device);
    }
    
    template <typename T, typename Header = std::monostate>
    DynamicPerFrameBuffer<T, Header> createDynamicPerFrameBuffer(VkBufferUsageFlags usage, size_t count = 0) {
        const ResourceManagerContext& context = requireInitialized("ResourceManager::createDynamicPerFrameBuffer");
        DynamicPerFrameBuffer<T, Header> buffer = DynamicPerFrameBuffer<T, Header>::create(*context.device, *context.physicalDevice, usage, count);
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            buffer.at(i).setResourceId(registry.create(ResourceType::Buffer));
        }
        return buffer;
    }
    template <typename T, typename Header = std::monostate>
    bool setDynamicPerFrameBufferCount(DynamicPerFrameBuffer<T, Header>& buffer, size_t count) {
        const ResourceManagerContext& context = requireInitialized("ResourceManager::setDynamicPerFrameBufferCount");
        return buffer.setElementCount(*context.device, *context.physicalDevice, count);
    }
    template <typename T, typename Header = std::monostate>
    void clearDynamicPerFrameBuffer(DynamicPerFrameBuffer<T, Header>& buffer) {
        const ResourceManagerContext& context = requireInitialized("ResourceManager::clearDynamicPerFrameBuffer");
        buffer.clear(*context.device, *context.physicalDevice);
    }
    template <typename T, typename Header = std::monostate>
    void destroyDynamicPerFrameBuffer(DynamicPerFrameBuffer<T, Header>& buffer) {
        requireInitialized("ResourceManager::destroyDynamicPerFrameBuffer");
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            registry.destroy(buffer.at(i).getResourceId());
            buffer.at(i).resetResourceId();
        }
        buffer.destroy(*ctx.device);
    }

    Image createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
    Image createImageFromPath(std::string path);
    void destroyImage(Image& image);
    ImageView createImageView(Image& image);
    void destroyImageView(ImageView& imageView);
    Sampler createSampler();
    void destroySampler(Sampler& sampler);

private:
    const ResourceManagerContext& requireInitialized(const char* functionName, bool requireTransferContext = false) const;

    ResourceManagerContext ctx;
    ResourceRegistry registry;
};
