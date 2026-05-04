#pragma once

#include <string>
#include <cstdint>

enum class AccessType : uint16_t;
enum class ImageUsageType : uint16_t;
enum class WriteMode : uint16_t;
enum class AttachmentLoad : uint16_t;
struct ImageAccessInfo;

class RenderGraphBuilder;
struct CompiledRenderGraph;

std::string accessTypeToStr(AccessType access);
std::string imageUsageToStr(ImageUsageType usage);
std::string writeModeToStr(WriteMode writeMode);
std::string attachmentLoadToStr(AttachmentLoad attachmentLoad);

std::string dumpGraph(const RenderGraphBuilder& builder);
std::string dumpCompiledGraph(const RenderGraphBuilder& builder, const CompiledRenderGraph& compiled);

void checkUsageValidForAccess(ImageUsageType usage, AccessType access);
void checkImageAccessInfoValid(const ImageAccessInfo& info);
