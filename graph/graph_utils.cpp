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

std::string imageUsageToStr(ImageUsage usage) {
    switch (usage) {
        case ImageUsage::Sampled         : return "Sampled";
        case ImageUsage::ColorAttachment : return "ColorAttachment";
        case ImageUsage::DepthAttachment : return "DepthAttachment";
        case ImageUsage::Present         : return "Present";
        case ImageUsage::TransferSrc     : return "TransferSrc";
        case ImageUsage::TransferDst     : return "TransferDst";
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


std::string dumpGraph(RenderGraphBuilder builder) {
    std::string out = "";
    const std::vector<ImageInfo> images = builder.getImages();

    out.append("Resources:\n");
    for (const ImageInfo &imageInfo : images) {
        out.append(std::format("  image [{}]:\n", imageInfo.name));
        out.append(std::format("    ({}x{}), format: {}\n", imageInfo.width, imageInfo.height, imageFormatToStr(imageInfo.format)));
    }
    
    out.append("Passes:\n");
    for (const PassInfo &passInfo : builder.getPasses()) {
        out.append(std::format("  pass [{}]:\n", passInfo.name));

        for (const ImageUsageInfo &imageUsageInfo : passInfo.imageUsages) {
            if (imageUsageInfo.access != AccessType::Read) continue;
            const ImageInfo &imageInfo = images.at(imageUsageInfo.handle.id);

            out.append(std::format("    -> image [{}], usage: {}\n", imageInfo.name, imageUsageToStr(imageUsageInfo.usage)));
        }
        for (const ImageUsageInfo &imageUsageInfo : passInfo.imageUsages) {
            if (imageUsageInfo.access != AccessType::Write) continue;
            const ImageInfo &imageInfo = images.at(imageUsageInfo.handle.id);

            out.append(std::format("    <- image [{}], usage: {}\n", imageInfo.name, imageUsageToStr(imageUsageInfo.usage)));
        }
    }

    return out;
}

std::string dumpCompiledGraph(RenderGraphBuilder builder, CompiledRenderGraph compiled) {
    std::string out = "";

    out.append("Image Chains:\n");
    for (const ImageChain& chain : compiled.imageChains) {
        out.append(std::format("  image [{}]\n", builder.getImages()[chain.image.id].name));
        out.append(std::format("    "));
        for (uint32_t i = 0; i < chain.usages.size(); i++) {
            const ImageUsageRecord& usageRecord = chain.usages[i];
            out.append(std::format("{} ({}|{})", builder.getPasses()[usageRecord.pass.id].name, imageUsageToStr(usageRecord.usage), accessTypeToStr(usageRecord.access)));
            
            if (i < chain.usages.size() - 1) {
                out.append(" -> ");
            }
        }
        out.append(std::format("\n"));
    }

    out.append("Dependencies:\n");
    for (const PassDependency& dependency : compiled.passDependencies) {
        out.append(std::format("  {} <- {}\n", builder.getPasses()[dependency.src.id].name, builder.getPasses()[dependency.dst.id].name));
    }

    return out;
}

void checkUsageValidForAccess(ImageUsage usage, AccessType access) {
    bool isValid = false;

    switch (usage) {
        case ImageUsage::Sampled:
              isValid = access == AccessType::Read; break;
          case ImageUsage::ColorAttachment:
              isValid = access == AccessType::Write; break;
          case ImageUsage::DepthAttachment:
              isValid = access == AccessType::Write; break;
          case ImageUsage::TransferSrc:
              isValid = access == AccessType::Read; break;
          case ImageUsage::TransferDst:
              isValid = access == AccessType::Write; break;
          case ImageUsage::Present:
              isValid = access == AccessType::Read; break;
    }

    if (!isValid) {
        throw std::runtime_error(std::format("Can't use an image as [{}] with [{}] access", imageUsageToStr(usage), accessTypeToStr(access)));
    }
}
