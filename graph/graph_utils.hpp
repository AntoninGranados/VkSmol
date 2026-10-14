#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>
#include <string>

#include "builder_resource.hpp"

namespace VkSmol {

enum class AccessType : uint16_t;
enum class ImageUsageType : uint16_t;
enum class BufferUsageType : uint16_t;
enum class WriteMode : uint16_t;
enum class AttachmentLoad : uint16_t;
struct ImageAccessInfo;
struct BufferAccessInfo;
struct VulkanImageState;
struct VulkanBufferState;

struct CompiledPass;
struct CompiledRenderGraph;
class Engine;

std::string accessTypeToStr(AccessType access);
std::string imageUsageToStr(ImageUsageType usage);
std::string bufferUsageToStr(BufferUsageType usage);
std::string writeModeToStr(WriteMode writeMode);
std::string attachmentLoadToStr(AttachmentLoad attachmentLoad);

VulkanImageState  imageAccessToVk(const ImageAccessInfo& info);
VulkanBufferState bufferAccessToVk(const BufferAccessInfo& info);
VkAttachmentLoadOp attachmentLoadToVk(const AttachmentLoad& attachmentLoad);
VkImageUsageFlags  imageUsageTypeToVkFlags(ImageUsageType usage);
VkBufferUsageFlags bufferUsageTypeToVkFlags(BufferUsageType usage);
VkDescriptorType   bufferUsageTypeToDescriptorType(BufferUsageType usage);
VkDescriptorType   imageUsageTypeToDescriptorType(ImageUsageType usage);

std::optional<ImageAccess> findColorAttachment(const CompiledPass& pass);

std::string dumpGraph(const CompiledRenderGraph& compiled, const Engine& engine);

void checkUsageValidForAccess(ImageUsageType usage, AccessType access);
void checkImageAccessInfoValid(const ImageAccessInfo& info);

}
