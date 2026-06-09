#pragma once

#include <vector>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <utility>


#include "../render/descriptor/descriptor_set_group.hpp"
#include "../render/descriptor/descriptor_set_layout.hpp"
#include "../render/descriptor/descriptor_writer.hpp"

#include "../resource/resource_registry.hpp"

#include "../device/physical_device.hpp"
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
    void terminate();

    // Resource management
    void initDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);
    void destroyDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout);

    DescriptorSetGroup createDescriptorSetGroup(DescriptorSetLayout& layout, std::string name = {});
    void updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer);
    void destroyDescriptorSetGroup(DescriptorSetGroup& group);

    Image createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, std::string name = {});
    Image createImageFromPath(std::string path, std::string name = {});
    void destroyImage(Image& image);
    ImageView createImageView(Image& image, std::string name = {});
    void destroyImageView(ImageView& imageView);
    Sampler createSampler(std::string name = {});
    void destroySampler(Sampler& sampler);

    Buffer createBuffer(VkBufferUsageFlags usage, size_t size, void* data = nullptr, std::string name = {});
    // Creates a HOST_VISIBLE | HOST_COHERENT buffer with persistent mapping (for CPU-written buffers).
    Buffer createMappedBuffer(VkBufferUsageFlags usage, size_t size, std::string name = {});
    Buffer createReadbackBuffer(size_t size, bool persistentMapping = true, std::string name = {});
    void fillBuffer(Buffer& buffer, void* data);
    void readBuffer(Buffer& buffer, void* out, size_t bytes, size_t offset = 0);
    void destroyBuffer(Buffer& buffer);
    
    // Resource retirement
    void setRetirementFrame(uint32_t frameIndex);
    void collectFrame(uint32_t frameIndex);
    void flushAll();

private:
    static std::string resolveResourceName(std::string name, const char* fallback) {
        return name.empty() ? std::string(fallback) : std::move(name);
    }
    static std::string makeIndexedResourceName(const std::string& baseName, size_t index) {
        return baseName + '[' + std::to_string(index) + ']';
    }

    const ResourceManagerContext& requireInitialized(const char* functionName, bool requireTransferContext = false) const;

    ResourceManagerContext ctx;
    ResourceRegistry registry;

    uint32_t currentRetirementFrame = 0;

    std::array<std::vector<RetirementPayload>, MAX_FRAME_IN_FLIGHT> retirementQueue;    
};
