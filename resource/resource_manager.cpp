#include "resource_manager.hpp"

#include "resource_reporting.hpp"

#include "engine/physical_device.hpp"
#include "engine/queue.hpp"
#include "engine/image/image.hpp"

#include <sstream>

void ResourceManager::init(ResourceManagerContext context) {
    ctx = context;
}

void ResourceManager::terminate() {
    ctx = {};
}

const ResourceManagerContext& ResourceManager::requireInitialized(const char* functionName, bool requireTransferContext) const {
    if (!ctx.device || !ctx.physicalDevice) {
        std::ostringstream message;
        message << functionName << ": ResourceManager is not initialized";
        throw std::runtime_error(message.str());
    }

    if (requireTransferContext && (!ctx.transferCommandPool || !ctx.transferQueue)) {
        std::ostringstream message;
        message << functionName << ": ResourceManager is missing transfer command pool or transfer queue";
        throw std::runtime_error(message.str());
    }

    return ctx;
}

void ResourceManager::initDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout) {
    descriptorSetLayout.init(*requireInitialized("ResourceManager::initDescriptorSetLayout").device);
}

void ResourceManager::destroyDescriptorSetLayout(DescriptorSetLayout& descriptorSetLayout) {
    descriptorSetLayout.destroy(*requireInitialized("ResourceManager::destroyDescriptorSetLayout").device);
}

DescriptorSetGroup ResourceManager::createDescriptorSetGroup(DescriptorSetLayout& layout) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::createDescriptorSetGroup");
    std::vector<VkDescriptorSetLayoutBinding> bindings = layout.getBindings();

    DescriptorPool descriptorPool;
    for (VkDescriptorSetLayoutBinding& binding : bindings)
        descriptorPool.addPoolSize(binding.descriptorType, MAX_FRAME_IN_FLIGHT);
    descriptorPool.init(*context.device);

    std::vector<DescriptorSet> descriptorSetList;
    descriptorSetList.resize(MAX_FRAME_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
        descriptorSetList[i].alloc(*context.device, descriptorPool, layout);
    }

    DescriptorSetGroup group(std::move(descriptorPool), std::move(descriptorSetList));

    for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
        group.at(i).setResourceId(registry.create(ResourceType::DescriptorSet));
    }
    return group;
}

void ResourceManager::updateDescriptorSetGroup(DescriptorSetGroup& group, uint32_t frameIndex, const DescriptorWriter& writer) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::updateDescriptorSetGroup");
    writer.validate();

    DescriptorSet& set = group.at(frameIndex);
    if (!set.hasResourceId()) {
        std::ostringstream message;
        message << "ResourceManager::updateDescriptorSetGroup: frame " << frameIndex
                << " resolved to an untracked descriptor set";
        throw std::runtime_error(message.str());
    }
    if (!registry.isAlive(set.getResourceId())) {
        std::ostringstream message;
        message << "ResourceManager::updateDescriptorSetGroup: frame " << frameIndex
                << " references a dead descriptor set " << formatResourceId(set.getResourceId());
        throw std::runtime_error(message.str());
    }

    registry.removeAllParents(set.getResourceId());
    set.clear();

    for (const DescriptorWrite& write : writer.getWrites()) {
        std::visit([&](const auto& payload) {
            using T = std::decay_t<decltype(payload)>;

            if constexpr (std::is_same_v<T, UniformBufferPayload>) {
                if (!payload.buffer) {
                    std::ostringstream message;
                    message << "ResourceManager::updateDescriptorSetGroup: binding " << write.binding
                            << " on frame " << frameIndex
                            << " in " << describeTrackedResource(registry, set.getResourceId(), "DescriptorSet")
                            << " received a null uniform buffer payload";
                    throw std::runtime_error(message.str());
                }
                if (!payload.buffer->hasResourceId()) {
                    std::ostringstream message;
                    message << "ResourceManager::updateDescriptorSetGroup: binding " << write.binding
                            << " on frame " << frameIndex
                            << " in " << describeTrackedResource(registry, set.getResourceId(), "DescriptorSet")
                            << " received an untracked uniform buffer";
                    throw std::runtime_error(message.str());
                }
                if (!registry.isAlive(payload.buffer->getResourceId())) {
                    std::ostringstream message;
                    message << "ResourceManager::updateDescriptorSetGroup: binding " << write.binding
                            << " on frame " << frameIndex
                            << " in " << describeTrackedResource(registry, set.getResourceId(), "DescriptorSet")
                            << " references a dead uniform buffer "
                            << formatResourceId(payload.buffer->getResourceId());
                    throw std::runtime_error(message.str());
                }
                set.linkUniformBuffer(*payload.buffer, write.binding);
                registry.addDependency(set.getResourceId(), payload.buffer->getResourceId());
            } else if constexpr (std::is_same_v<T, StorageBufferPayload>) {
                if (!payload.buffer) {
                    std::ostringstream message;
                    message << "ResourceManager::updateDescriptorSetGroup: binding " << write.binding
                            << " on frame " << frameIndex
                            << " in " << describeTrackedResource(registry, set.getResourceId(), "DescriptorSet")
                            << " received a null storage buffer payload";
                    throw std::runtime_error(message.str());
                }
                if (!payload.buffer->hasResourceId()) {
                    std::ostringstream message;
                    message << "ResourceManager::updateDescriptorSetGroup: binding " << write.binding
                            << " on frame " << frameIndex
                            << " in " << describeTrackedResource(registry, set.getResourceId(), "DescriptorSet")
                            << " received an untracked storage buffer";
                    throw std::runtime_error(message.str());
                }
                if (!registry.isAlive(payload.buffer->getResourceId())) {
                    std::ostringstream message;
                    message << "ResourceManager::updateDescriptorSetGroup: binding " << write.binding
                            << " on frame " << frameIndex
                            << " in " << describeTrackedResource(registry, set.getResourceId(), "DescriptorSet")
                            << " references a dead storage buffer "
                            << formatResourceId(payload.buffer->getResourceId());
                    throw std::runtime_error(message.str());
                }
                set.linkStorageBuffer(*payload.buffer, write.binding);
                registry.addDependency(set.getResourceId(), payload.buffer->getResourceId());
            } else if constexpr (std::is_same_v<T, CombinedImageSamplerPayload>) {
                if (!payload.view || !payload.sampler) {
                    std::ostringstream message;
                    message << "ResourceManager::updateDescriptorSetGroup: binding " << write.binding
                            << " on frame " << frameIndex
                            << " in " << describeTrackedResource(registry, set.getResourceId(), "DescriptorSet")
                            << " received an incomplete combined image sampler payload"
                            << " (view=" << (payload.view ? "set" : "null")
                            << ", sampler=" << (payload.sampler ? "set" : "null") << ')';
                    throw std::runtime_error(message.str());
                }
                if (!payload.view->hasResourceId()) {
                    std::ostringstream message;
                    message << "ResourceManager::updateDescriptorSetGroup: binding " << write.binding
                            << " on frame " << frameIndex
                            << " in " << describeTrackedResource(registry, set.getResourceId(), "DescriptorSet")
                            << " received an untracked image view";
                    throw std::runtime_error(message.str());
                }
                if (!registry.isAlive(payload.view->getResourceId())) {
                    std::ostringstream message;
                    message << "ResourceManager::updateDescriptorSetGroup: binding " << write.binding
                            << " on frame " << frameIndex
                            << " in " << describeTrackedResource(registry, set.getResourceId(), "DescriptorSet")
                            << " references a dead image view "
                            << formatResourceId(payload.view->getResourceId());
                    throw std::runtime_error(message.str());
                }
                if (!payload.sampler->hasResourceId()) {
                    std::ostringstream message;
                    message << "ResourceManager::updateDescriptorSetGroup: binding " << write.binding
                            << " on frame " << frameIndex
                            << " in " << describeTrackedResource(registry, set.getResourceId(), "DescriptorSet")
                            << " received an untracked sampler";
                    throw std::runtime_error(message.str());
                }
                if (!registry.isAlive(payload.sampler->getResourceId())) {
                    std::ostringstream message;
                    message << "ResourceManager::updateDescriptorSetGroup: binding " << write.binding
                            << " on frame " << frameIndex
                            << " in " << describeTrackedResource(registry, set.getResourceId(), "DescriptorSet")
                            << " references a dead sampler "
                            << formatResourceId(payload.sampler->getResourceId());
                    throw std::runtime_error(message.str());
                }
                set.linkCombinedImageSampler(*payload.view, *payload.sampler, write.binding);
                registry.addDependency(set.getResourceId(), payload.view->getResourceId());
                registry.addDependency(set.getResourceId(), payload.sampler->getResourceId());
            }
        }, write.payload);
    }

    set.update(*context.device);
}

// TODO: needs to store the available spots to refill them
void ResourceManager::destroyDescriptorSetGroup(DescriptorSetGroup& group) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::destroyDescriptorSetGroup");
    if (!group.isValide()) return;

    for (size_t i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
        if (!group.at(i).hasResourceId()) {
            std::ostringstream message;
            message << "ResourceManager::destroyDescriptorSetGroup: frame " << i
                    << " resolved to an untracked descriptor set";
            throw std::runtime_error(message.str());
        }
        registry.destroy(group.at(i).getResourceId());
        group.at(i).resetResourceId();
    }

    group.destroy(*context.device);
    group = DescriptorSetGroup{};
}


Buffer ResourceManager::createBuffer(VkBufferUsageFlags usage, size_t size, void* data) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::createBuffer", data != nullptr);
    Buffer buffer;

    if (data == nullptr) {
        buffer.init(*context.device, usage, size);
    } else {
        buffer.initFilled(
            *context.device, *context.physicalDevice,
            usage,
            *context.transferCommandPool, *context.transferQueue,
            size, data
        );
    }

    buffer.setResourceId(registry.create(ResourceType::Buffer));
    return buffer;
}

Buffer ResourceManager::createReadbackBuffer(size_t size, bool persistentMapping) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::createReadbackBuffer");
    Buffer buffer;
    buffer.init(*context.device, VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, persistentMapping);
    buffer.alloc(*context.device, *context.physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    buffer.setResourceId(registry.create(ResourceType::Buffer));
    return buffer;
}

void ResourceManager::fillBuffer(Buffer& buffer, void* data) {
    buffer.fill(*requireInitialized("ResourceManager::fillBuffer").device, data);
}

void ResourceManager::readBuffer(Buffer& buffer, void* out, size_t bytes, size_t offset) {
    buffer.read(*requireInitialized("ResourceManager::readBuffer").device, out, bytes, offset);
}

void ResourceManager::destroyBuffer(Buffer& buffer) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::destroyBuffer");
    if (!buffer.hasResourceId()) {
        throw std::runtime_error("ResourceManager::destroyBuffer: can't destroy an untracked buffer");
    }
    registry.destroy(buffer.getResourceId());
    buffer.free(*context.device);
    buffer.destroy(*context.device);
    buffer.resetResourceId();
}

Image ResourceManager::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::createImage");
    Image image;
    image.init(*context.device, width, height, format, usage);
    image.alloc(*context.device, *context.physicalDevice, properties);
    image.setResourceId(registry.create(ResourceType::Image));
    return image;
}

Image ResourceManager::createImageFromPath(std::string path) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::createImageFromPath", true);
    Image image;
    image.initFromPath(*context.device, *context.physicalDevice, *context.transferCommandPool, *context.transferQueue, path);
    image.setResourceId(registry.create(ResourceType::Image));
    return image;
}

void ResourceManager::destroyImage(Image& image) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::destroyImage");
    if (!image.hasResourceId()) {
        throw std::runtime_error("ResourceManager::destroyImage: can't destroy an untracked image");
    }
    registry.destroy(image.getResourceId());
    image.free(*context.device);
    image.destroy(*context.device);
    image.resetResourceId();
}

ImageView ResourceManager::createImageView(Image& image) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::createImageView");
    if (!image.hasResourceId()) {
        throw std::runtime_error("ResourceManager::createImageView: can't create an image view from an untracked image");
    }
    ImageView imageView;
    imageView.init(*context.device, image);
    imageView.setResourceId(registry.create(ResourceType::ImageView));
    registry.addDependency(imageView.getResourceId(), image.getResourceId());
    return imageView;
}

void ResourceManager::destroyImageView(ImageView& imageView) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::destroyImageView");
    if (!imageView.hasResourceId()) {
        throw std::runtime_error("ResourceManager::destroyImageView: can't destroy an untracked image view");
    }
    registry.destroy(imageView.getResourceId());
    imageView.destroy(*context.device);
    imageView.resetResourceId();
}

Sampler ResourceManager::createSampler() {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::createSampler");
    Sampler sampler;
    sampler.init(*context.device, *context.physicalDevice);
    sampler.setResourceId(registry.create(ResourceType::Sampler));
    return sampler;
}

void ResourceManager::destroySampler(Sampler& sampler) {
    const ResourceManagerContext& context = requireInitialized("ResourceManager::destroySampler");
    if (!sampler.hasResourceId()) {
        throw std::runtime_error("ResourceManager::destroySampler: can't destroy an untracked sampler");
    }
    registry.destroy(sampler.getResourceId());
    sampler.destroy(*context.device);
    sampler.resetResourceId();
}
