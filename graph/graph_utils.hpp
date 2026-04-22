#pragma once

#include "engine/graph/compiled_resource.hpp"
#include <string>
#include <cstdint>

enum class AccessType : uint16_t;
enum class ImageUsage : uint16_t;

class RenderGraphBuilder;

std::string accessTypeToStr(AccessType access);
std::string imageUsageToStr(ImageUsage usage);

std::string dumpGraph(RenderGraphBuilder builder);
std::string dumpCompiledGraph(RenderGraphBuilder builder, CompiledRenderGraph compiled);

void checkUsageValidForAccess(ImageUsage usage, AccessType access);
