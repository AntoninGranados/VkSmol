#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <cstdint>

enum class AccessType : uint16_t;
enum class ImageUsageType : uint16_t;
enum class WriteMode : uint16_t;
enum class AttachmentLoad : uint16_t;
struct ImageAccessInfo;
struct VulkanImageState;

class RenderGraphBuilder;
struct CompiledRenderGraph;

std::string accessTypeToStr(AccessType access);
std::string imageUsageToStr(ImageUsageType usage);
std::string writeModeToStr(WriteMode writeMode);
std::string attachmentLoadToStr(AttachmentLoad attachmentLoad);

VulkanImageState imageAccessToVk(const ImageAccessInfo& info);
VkAttachmentLoadOp attachmentLoadToVk(const AttachmentLoad& attachmentLoad);

std::string dumpGraph(const RenderGraphBuilder& builder);
std::string dumpCompiledGraph(const RenderGraphBuilder& builder, const CompiledRenderGraph& compiled);

void checkUsageValidForAccess(ImageUsageType usage, AccessType access);
void checkImageAccessInfoValid(const ImageAccessInfo& info);
