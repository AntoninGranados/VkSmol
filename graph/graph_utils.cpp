#include "graph_utils.hpp"

#include <stdexcept>
#include <format>

#include "engine/graph/compiled_resource.hpp"
#include "render_graph_builder.hpp"
#include "builder_resource.hpp"

namespace {

bool isAttachmentWrite(const ImageAccessInfo& info) {
    return info.access == AccessType::Write &&
        (info.usage == ImageUsageType::ColorAttachment || info.usage == ImageUsageType::DepthAttachment);
}

std::string imageAccessInfoToStr(const ImageAccessInfo& info) {
    std::string out = std::format("{}/{}", imageUsageToStr(info.usage), accessTypeToStr(info.access));

    if (info.access == AccessType::Write) {
        out.append(std::format(", write: {}", writeModeToStr(info.writeMode)));
    }

    if (isAttachmentWrite(info)) {
        out.append(std::format(", load: {}", attachmentLoadToStr(info.attachmentLoad)));
    }

    return out;
}

} // namespace

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

std::string writeModeToStr(WriteMode writeMode) {
    switch (writeMode) {
        case WriteMode::Overwrite: return "Overwrite";
        case WriteMode::Preserve : return "Preserve";
    }
}

std::string attachmentLoadToStr(AttachmentLoad attachmentLoad) {
    switch (attachmentLoad) {
        case AttachmentLoad::DontCare: return "DontCare";
        case AttachmentLoad::Clear   : return "Clear";
        case AttachmentLoad::Load    : return "Load";
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

VulkanImageState imageAccessToVk(const ImageAccessInfo& info) {
    switch (info.usage) {
        case ImageUsageType::Undefined:
            return {
                .layout = VK_IMAGE_LAYOUT_UNDEFINED,
                .access = 0,
                .stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            };

        case ImageUsageType::Sampled:
            return {
                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .access = VK_ACCESS_SHADER_READ_BIT,
                .stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            };

        case ImageUsageType::ColorAttachment:
            return {
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            };

        case ImageUsageType::DepthAttachment:
            return {
                .layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
            };

        case ImageUsageType::TransferSrc:
            return {
                .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .access = VK_ACCESS_TRANSFER_READ_BIT,
                .stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            };

        case ImageUsageType::TransferDst:
            return {
                .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .access = VK_ACCESS_TRANSFER_WRITE_BIT,
                .stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            };

        case ImageUsageType::Present:
            return {
                .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .access = 0,
                .stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
            };
    }
}

VkAttachmentLoadOp attachmentLoadToVk(const AttachmentLoad& attachmentLoad) {
    switch (attachmentLoad) {
        case AttachmentLoad::Load: return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
        case AttachmentLoad::Clear: return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
        case AttachmentLoad::DontCare: return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}


std::string dumpGraph(const RenderGraphBuilder& builder) {
    std::string out = "";
    const std::vector<ImageInfo>& images = builder.getImages();

    out.append("Resources:\n");
    for (const ImageInfo &imageInfo : images) {
        out.append(std::format("  image [{}]:\n", imageInfo.name));
        out.append(std::format("    ({}x{}), format: {}\n", imageInfo.width, imageInfo.height, imageFormatToStr(imageInfo.format)));

        if (imageInfo.initialAccess.has_value()) {
            out.append(std::format("    initial: {}\n", imageAccessInfoToStr(imageInfo.initialAccess.value())));
        }

        if (imageInfo.finalAccess.has_value()) {
            out.append(std::format("    final: {}\n", imageAccessInfoToStr(imageInfo.finalAccess.value())));
        }
    }
    
    out.append("Passes:\n");
    for (const PassInfo &passInfo : builder.getPasses()) {
        out.append(std::format("  pass [{}]:\n", passInfo.name));

        for (const ImageAccess& imageAccess : passInfo.imageAccesses) {
            if (imageAccess.info.access != AccessType::Read) continue;

            const ImageInfo &imageInfo = images[imageAccess.image.id];
            out.append(std::format("    -> image [{}], {}\n", imageInfo.name, imageAccessInfoToStr(imageAccess.info)));
        }

        for (const ImageAccess& imageAccess : passInfo.imageAccesses) {
            if (imageAccess.info.access != AccessType::Write) continue;

            const ImageInfo &imageInfo = images[imageAccess.image.id];
            out.append(std::format("    <- image [{}], {}\n", imageInfo.name, imageAccessInfoToStr(imageAccess.info)));
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
            out.append(std::format("    before [{}]: {} -> {}\n", image.name, imageAccessInfoToStr(barrier.srcAccess), imageAccessInfoToStr(barrier.dstAccess)));
        }
    }

    out.append("Output Barriers:\n");
    for (const PassImageBarrier& barrier : compiled.outputBarriers) {
        const ImageInfo& image = builder.getImages()[barrier.image.id];
        out.append(std::format("  image [{}]: {} -> {}\n", image.name, imageAccessInfoToStr(barrier.srcAccess), imageAccessInfoToStr(barrier.dstAccess)));
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

void checkImageAccessInfoValid(const ImageAccessInfo& info) {
    checkUsageValidForAccess(info.usage, info.access);

    if (!isAttachmentWrite(info)) {
        if (info.writeMode != WriteMode::Overwrite) {
            throw std::runtime_error(std::format(
                "Only attachment writes can preserve previous contents, got [{}]",
                imageAccessInfoToStr(info)
            ));
        }

        if (info.attachmentLoad != AttachmentLoad::DontCare) {
            throw std::runtime_error(std::format(
                "Only attachment writes can specify an attachment load, got [{}]",
                imageAccessInfoToStr(info)
            ));
        }

        return;
    }

    if (info.writeMode == WriteMode::Preserve && info.attachmentLoad != AttachmentLoad::Load) {
        throw std::runtime_error(std::format(
            "Preserve writes must use AttachmentLoad::Load, got [{}]",
            imageAccessInfoToStr(info)
        ));
    }

    if (info.writeMode == WriteMode::Overwrite && info.attachmentLoad == AttachmentLoad::Load) {
        throw std::runtime_error(std::format(
            "AttachmentLoad::Load requires WriteMode::Preserve, got [{}]",
            imageAccessInfoToStr(info)
        ));
    }
}
