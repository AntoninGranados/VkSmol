#pragma once

#include <vector>
#include <array>
#include <functional>
#include <memory>
#include <utility>

#include "../memory/dynamic_per_frame_buffer.hpp"
#include "../memory/shared_buffer.hpp"
#include "../memory/per_frame_buffer.hpp"

#include "../descriptor/descriptor_set_group.hpp"
#include "../descriptor/descriptor_set_layout.hpp"
#include "../descriptor/descriptor_writer.hpp"

#include "../resource/resource_registry.hpp"

#include "../physical_device.hpp"
#include "../utils.hpp"


struct ResourceManagerContext {
    LogicalDevice* device = nullptr;
    PhysicalDevice* physicalDevice = nullptr;
    CommandPool* transferCommandPool = nullptr;
    Queue* transferQueue = nullptr;
};

using RetirementPayload = std::function<void(const LogicalDevice&)>;

class ResourceManager {
public:
    void init(ResourceManagerContext context);
    void terminate();   // TODO

    // Resource management
    void initDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);
    void destroyDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);

    DescriptorSetGroup createDescriptorSetGroup(DescriptorSetLayout& layout);
    void updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer);
    void destroyDescriptorSetGroup(DescriptorSetGroup& group);

    Image createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
    Image createImageFromPath(std::string path);
    void destroyImage(Image& image);
    ImageView createImageView(Image& image);
    void destroyImageView(ImageView& imageView);
    Sampler createSampler();
    void destroySampler(Sampler& sampler);

    Buffer createBuffer(VkBufferUsageFlags usage, size_t size, void* data = nullptr);
    Buffer createReadbackBuffer(size_t size, bool persistentMapping = true);
    void fillBuffer(Buffer& buffer, void* data);
    void readBuffer(Buffer& buffer, void* out, size_t bytes, size_t offset = 0);
    void destroyBuffer(Buffer& buffer);
    
    template<typename T>
    PerFrameBuffer<T> createPerFrameBuffer(VkBufferUsageFlags usage, size_t count = 1) {
        PerFrameBuffer<T> buffer = PerFrameBuffer<T>::create(usage, count);
        initPerFrameBufferBase(buffer);
        return buffer;
    }
    template<typename T>
    void destroyPerFrameBuffer(PerFrameBuffer<T>& buffer) {
        destroyPerFrameBufferBase(buffer);
        buffer = PerFrameBuffer<T>{};
    }
    
    template <typename T, typename Header = std::monostate>
    DynamicPerFrameBuffer<T, Header> createDynamicPerFrameBuffer(VkBufferUsageFlags usage, size_t count = 0) {
        DynamicPerFrameBuffer<T, Header> buffer = DynamicPerFrameBuffer<T, Header>::create(usage, count);
        initPerFrameBufferBase(buffer);
        return buffer;
    }
    template <typename T, typename Header = std::monostate>
    bool setDynamicPerFrameBufferCount(DynamicPerFrameBuffer<T, Header>& buffer, size_t count) {
        if (buffer.setElementCount(count)) {
            reinitPerFrameBufferBase(buffer);
            return true;
        }
        return false;
    }
    template <typename T, typename Header = std::monostate>
    void clearDynamicPerFrameBuffer(DynamicPerFrameBuffer<T, Header>& buffer) {
        buffer.clear();
        reinitPerFrameBufferBase(buffer);
    }
    template <typename T, typename Header = std::monostate>
    void destroyDynamicPerFrameBuffer(DynamicPerFrameBuffer<T, Header>& buffer) {
        destroyPerFrameBufferBase(buffer);
        buffer = DynamicPerFrameBuffer<T, Header>{};
    }

    template<typename T>
    SharedBuffer<T> createSharedBuffer(VkBufferUsageFlags usage, size_t count = 1) {
        const ResourceManagerContext& context = requireInitialized("ResourceManager::createSharedBuffer");
        SharedBuffer<T> buffer = SharedBuffer<T>::create(usage, count);
        buffer.buffer.init(*context.device, buffer.getUsage(), buffer.getByteSize(), true);
        buffer.buffer.alloc(*context.device, *context.physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        buffer.buffer.setResourceId(registry.create(ResourceType::Buffer));
        return buffer;
    }
    template<typename T>
    void destroySharedBuffer(SharedBuffer<T>& buffer) {
        registry.destroy(buffer.buffer.getResourceId());
        buffer.buffer.resetResourceId();
        auto retired = std::make_shared<Buffer>(std::move(buffer.buffer));
        retirementQueue.at(currentRetirementFrame).push_back(
            [retired](const LogicalDevice& device) {
                retired->free(device);
                retired->destroy(device);
            }
        );
        buffer = SharedBuffer<T>{};
    }

    // Resource retirement
    void setRetirementFrame(uint32_t frameIndex);
    void collectFrame(uint32_t frameIndex);
    void flushAll();

private:
    const ResourceManagerContext& requireInitialized(const char* functionName, bool requireTransferContext = false) const;

    void initPerFrameBufferBase(PerFrameBufferBase& buffer) {
        const ResourceManagerContext& context = requireInitialized("ResourceManager::initPerFrameBuffer");
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            buffer.at(i).init(*context.device, buffer.getUsage(), buffer.getByteSize(), true);
            buffer.at(i).alloc(*context.device, *context.physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            buffer.at(i).setResourceId(registry.create(ResourceType::Buffer));
        }
    }
    void reinitPerFrameBufferBase(PerFrameBufferBase& buffer) {
        // Delete the old buffers and recreate new ones using the same resource ids
        const ResourceManagerContext& context = requireInitialized("ResourceManager::initPerFrameBuffer");
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            const auto resourceId = buffer.at(i).getResourceId();
            auto retired = std::make_shared<Buffer>(std::move(buffer.at(i)));
            retired->resetResourceId();
            retirementQueue.at(currentRetirementFrame).push_back(
                [retired](const LogicalDevice& device) {
                    retired->free(device);
                    retired->destroy(device);
                }
            );

            buffer.at(i).init(*context.device, buffer.getUsage(), buffer.getByteSize(), true);
            buffer.at(i).alloc(*context.device, *context.physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            buffer.at(i).setResourceId(resourceId);
        }
    }
    void destroyPerFrameBufferBase(PerFrameBufferBase& buffer) {
        // const ResourceManagerContext& context = requireInitialized("ResourceManager::initPerFrameBuffer");
        for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
            registry.destroy(buffer.at(i).getResourceId());
            auto retired = std::make_shared<Buffer>(std::move(buffer.at(i)));
            retired->resetResourceId();
            retirementQueue.at(currentRetirementFrame).push_back(
                [retired](const LogicalDevice& device) {
                    retired->free(device);
                    retired->destroy(device);
                }
            );
        }
    }

    ResourceManagerContext ctx;
    ResourceRegistry registry;

    uint32_t currentRetirementFrame = 0;

    std::array<std::vector<RetirementPayload>, MAX_FRAME_IN_FLIGHT> retirementQueue;    
};
