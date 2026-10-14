#pragma once

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "./device/device.hpp"

#include "./present/window.hpp"
#include "./present/swapchain.hpp"

#include "./render/descriptor/descriptor_pool.hpp"
#include "./render/shader_file_system.hpp"

#include "./sync/semaphore.hpp"

#include "./utils.hpp"

namespace VkSmol {

class Engine {
public:
    Engine(const std::string& appName, uint32_t appVersion, std::optional<Window&> window = std::nullopt);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    bool isHeadless() const { return !window.has_value(); }

    Device& getDevice() { return device; }

    void setShaderFileSystem(std::shared_ptr<ShaderFileSystem> fileSystem) { device.setShaderFileSystem(std::move(fileSystem)); }

    VkExtent2D getExtent() const { return swapchain ? swapchain->getExtent() : VkExtent2D{}; }
    uint32_t getFrame() const { return currentFrame; }

    void waitIdle() { device.waitIdle(); }

private:
    std::optional<Window&> window;
    Device device;
    const LogicalDevice& logicalDevice;
    std::optional<Swapchain> swapchain;

    std::vector<Semaphore> imageAvailableSemaphores;
    bool framebufferResized = false;
    uint32_t currentFrame = 0;

    DescriptorPool uiDescriptorPool;
    VkFormat uiColorFormat = VK_FORMAT_UNDEFINED;

    std::optional<uint32_t> acquireImage();
    void presentImage(uint32_t imageIndex, const Semaphore& waitSemaphore);
    void recreateSwapchain();

    void initImgui();

    friend class RenderGraph;
};

}
