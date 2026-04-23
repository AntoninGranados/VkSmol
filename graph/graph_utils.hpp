#pragma once

#include <string>
#include <cstdint>

enum class AccessType : uint16_t;
enum class ImageUsageType : uint16_t;

class RenderGraphBuilder;
struct CompiledRenderGraph;

std::string accessTypeToStr(AccessType access);
std::string imageUsageToStr(ImageUsageType usage);

std::string dumpGraph(const RenderGraphBuilder& builder);
std::string dumpCompiledGraph(const RenderGraphBuilder& builder, const CompiledRenderGraph& compiled);

void checkUsageValidForAccess(ImageUsageType usage, AccessType access);
