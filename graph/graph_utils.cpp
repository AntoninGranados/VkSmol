#include "graph_utils.hpp"

#include <format>

#include "engine/graph/compiled_resource.hpp"
#include "render_graph_builder.hpp"
#include "builder_resource.hpp"

std::string accessTypeToStr(AccessType access) {
    switch (access) {
        case AccessType::None  : return "None";
        case AccessType::Read  : return "Read";
        case AccessType::Write : return "Write";
    }
}

std::string imageUsageToStr(ImageUsageType usage) {
    switch (usage) {
        case ImageUsageType::Undefined       : return "Undefined";
        case ImageUsageType::Sampled         : return "Sampled";
        case ImageUsageType::ColorAttachment : return "ColorAttachment";
        case ImageUsageType::DepthAttachment : return "DepthAttachment";
        case ImageUsageType::Present         : return "Present";
        case ImageUsageType::TransferSrc     : return "TransferSrc";
        case ImageUsageType::TransferDst     : return "TransferDst";
    }
}

std::string imageFormatToStr(VkFormat format) {
    switch (format) {
        case VK_FORMAT_UNDEFINED: return "Undefined";
        case VK_FORMAT_R16G16B16A16_SFLOAT: return "RGB16 (sfloat)";
        case VK_FORMAT_R32G32B32A32_SFLOAT: return "RGB32 (sfloat)";
        case VK_FORMAT_R64G64B64A64_SFLOAT: return "RGB64 (sfloat)";
        default: return "FORMAT NOT SUPPORTED";
    }
}


std::string dumpGraph(const RenderGraphBuilder& builder) {
    std::string out = "";
    const std::vector<ImageInfo>& images = builder.getImages();

    out.append("Resources:\n");
    for (const ImageInfo &imageInfo : images) {
        out.append(std::format("  image [{}]:\n", imageInfo.name));
        out.append(std::format("    ({}x{}), format: {}\n", imageInfo.width, imageInfo.height, imageFormatToStr(imageInfo.format)));
    }
    
    out.append("Passes:\n");
    for (const PassInfo &passInfo : builder.getPasses()) {
        out.append(std::format("  pass [{}]:\n", passInfo.name));

        for (const ImageAccess& imageAccess : passInfo.imageAccesses) {
            if (imageAccess.info.access != AccessType::Read) continue;

            const ImageInfo &imageInfo = images[imageAccess.image.id];
            out.append(std::format("    -> image [{}], usage: {}\n", imageInfo.name, imageUsageToStr(imageAccess.info.usage)));
        }

        for (const ImageAccess& imageAccess : passInfo.imageAccesses) {
            if (imageAccess.info.access != AccessType::Write) continue;

            const ImageInfo &imageInfo = images[imageAccess.image.id];
            out.append(std::format("    <- image [{}], usage: {}\n", imageInfo.name, imageUsageToStr(imageAccess.info.usage)));
        }
    }

    return out;
}

std::string dumpCompiledGraph(const RenderGraphBuilder& builder, const CompiledRenderGraph& compiled) {
    std::string out = "";

    out.append("Compiled Passes:\n");
    for (const CompiledPass& compiledPass : compiled.compiledPasses) {
        out.append(std::format("  pass [{}]:\n", builder.getPasses()[compiledPass.pass.id].name));

        for (const PassImageBarrier& barrier : compiledPass.barriersBefore) {
            const ImageInfo& image = builder.getImages()[barrier.image.id];
            out.append(std::format("    before [{}]: {} -> {}\n", image.name, imageUsageToStr(barrier.srcAccess.usage), imageUsageToStr(barrier.dstAccess.usage)));
        }
    }

    out.append("Output Barriers:\n");
    for (const PassImageBarrier& barrier : compiled.outputBarriers) {
        const ImageInfo& image = builder.getImages()[barrier.image.id];
        out.append(std::format("  image [{}]: {} -> {}\n", image.name, imageUsageToStr(barrier.srcAccess.usage), imageUsageToStr(barrier.dstAccess.usage)));
    }

    return out;
}

void checkUsageValidForAccess(ImageUsageType usage, AccessType access) {
    bool isValid = false;

    switch (usage) {
        case ImageUsageType::Undefined: 
            isValid = access == AccessType::None; break;
        case ImageUsageType::Sampled:
              isValid = access == AccessType::Read; break;
          case ImageUsageType::ColorAttachment:
              isValid = access == AccessType::Write; break;
          case ImageUsageType::DepthAttachment:
              isValid = access == AccessType::Write; break;
          case ImageUsageType::TransferSrc:
              isValid = access == AccessType::Read; break;
          case ImageUsageType::TransferDst:
              isValid = access == AccessType::Write; break;
          case ImageUsageType::Present:
              isValid = access == AccessType::Read; break;
    }

    if (!isValid) {
        throw std::runtime_error(std::format("Can't use an image as [{}] with [{}] access", imageUsageToStr(usage), accessTypeToStr(access)));
    }
}
