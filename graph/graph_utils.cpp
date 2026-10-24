#include "graph_utils.hpp"

#include <stdexcept>
#include <format>
#include <algorithm>

#include "builder_resource.hpp"
#include "../engine.hpp"

namespace VkSmol {

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

std::string bufferUsageToStr(BufferUsageType usage) {
    switch (usage) {
        case BufferUsageType::Storage:    return "Storage";
        case BufferUsageType::Uniform:    return "Uniform";
        case BufferUsageType::TransferSrc: return "TransferSrc";
        case BufferUsageType::TransferDst: return "TransferDst";
        case BufferUsageType::Vertex:     return "Vertex";
        case BufferUsageType::Index:      return "Index";
        case BufferUsageType::Indirect:   return "Indirect";
    }
}

VulkanBufferState bufferAccessToVk(const BufferAccessInfo& info) {
    switch (info.usage) {
        case BufferUsageType::Storage:
            return {
                // Storage shaders often both read and write — be conservative with the write flag
                .access = (info.access == AccessType::Read)
                    ? VK_ACCESS_2_SHADER_READ_BIT
                    : VkAccessFlags2(VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT),
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                         VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT  |
                         VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            };
        case BufferUsageType::Uniform:
            return {
                .access = VK_ACCESS_2_UNIFORM_READ_BIT,
                .stage  = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            };
        case BufferUsageType::TransferSrc:
            return {
                .access = VK_ACCESS_2_TRANSFER_READ_BIT,
                .stage  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            };
        case BufferUsageType::TransferDst:
            return {
                .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .stage  = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            };
        case BufferUsageType::Vertex:
            return {
                .access = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
                .stage  = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
            };
        case BufferUsageType::Index:
            return {
                .access = VK_ACCESS_2_INDEX_READ_BIT,
                .stage  = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
            };
        case BufferUsageType::Indirect:
            return {
                .access = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .stage  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            };
    }
}

VkDescriptorType bufferUsageTypeToDescriptorType(BufferUsageType usage) {
    switch (usage) {
        case BufferUsageType::Uniform: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case BufferUsageType::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        default:                       return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

VkDescriptorType imageUsageTypeToDescriptorType(ImageUsageType usage) {
    switch (usage) {
        case ImageUsageType::Sampled: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case ImageUsageType::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        default:                      return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

VkBufferUsageFlags bufferUsageTypeToVkFlags(BufferUsageType usage) {
    switch (usage) {
        case BufferUsageType::Storage:     return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        case BufferUsageType::Uniform:     return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        case BufferUsageType::TransferSrc: return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case BufferUsageType::TransferDst: return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case BufferUsageType::Vertex:      return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case BufferUsageType::Index:       return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case BufferUsageType::Indirect:    return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
}

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
        case ImageUsageType::Storage         : return "Storage";
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
                .access = VK_ACCESS_2_NONE,
                .stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            };

        case ImageUsageType::Sampled:
            return {
                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .access = VK_ACCESS_2_SHADER_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            };

        case ImageUsageType::Storage:
            return {
                .layout = VK_IMAGE_LAYOUT_GENERAL,
                .access = (info.access == AccessType::Read)
                    ? VK_ACCESS_2_SHADER_READ_BIT
                    : VK_ACCESS_2_SHADER_WRITE_BIT,
                .stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            };

        case ImageUsageType::ColorAttachment:
            return {
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            };

        case ImageUsageType::DepthAttachment:
            return {
                .layout = (info.access == AccessType::Read) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                            : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .access = (info.access == AccessType::Read) ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                                                            : VkAccessFlags2(
                                                                  VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                                  VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                                                              ),
                .stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            };

        case ImageUsageType::TransferSrc:
            return {
                .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .access = VK_ACCESS_2_TRANSFER_READ_BIT,
                .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            };

        case ImageUsageType::TransferDst:
            return {
                .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            };

        case ImageUsageType::Present:
            return {
                .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .access = VK_ACCESS_2_NONE,
                .stage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            };
    }
}

VkImageUsageFlags imageUsageTypeToVkFlags(ImageUsageType usage) {
    switch (usage) {
        case ImageUsageType::Sampled:         return VK_IMAGE_USAGE_SAMPLED_BIT;
        case ImageUsageType::Storage:         return VK_IMAGE_USAGE_STORAGE_BIT;
        case ImageUsageType::ColorAttachment: return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        case ImageUsageType::DepthAttachment: return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        case ImageUsageType::TransferSrc:     return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        case ImageUsageType::TransferDst:     return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        case ImageUsageType::Present:
        case ImageUsageType::Undefined:       return 0;
    }
}

std::vector<ImageAccess> findColorAttachments(const std::vector<ImageAccess>& accesses) {
    std::vector<ImageAccess> attachments;
    for (const ImageAccess& access : accesses)
        if (access.info.usage == ImageUsageType::ColorAttachment) attachments.push_back(access);
    return attachments;
}

std::optional<ImageAccess> findDepthAttachment(const std::vector<ImageAccess>& accesses) {
    for (const ImageAccess& access : accesses)
        if (access.info.usage == ImageUsageType::DepthAttachment) return access;
    return std::nullopt;
}

VkAttachmentLoadOp attachmentLoadToVk(const AttachmentLoad& attachmentLoad) {
    switch (attachmentLoad) {
        case AttachmentLoad::Load: return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
        case AttachmentLoad::Clear: return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
        case AttachmentLoad::DontCare: return VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}


namespace {

std::string passTypeToStr(PassType type) {
    switch (type) {
        case PassType::Compute:  return "Compute";
        case PassType::Graphics: return "Graphics";
        case PassType::Transfer: return "Transfer";
        case PassType::Present:  return "Present";
    }
    return "";
}

std::string rangeSuffix(const ImageSubresourceRange& subresources) {
    return subresources == ImageSubresourceRange{} ? "" : " (" + subresources.toString() + ")";
}

std::string descriptorTypeToStr(VkDescriptorType type) {
    switch (type) {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:         return "Uniform";
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:         return "Storage";
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "Sampled";
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:          return "StorageImage";
        default:                                        return "Unknown";
    }
}

} // namespace

std::string dumpGraph(const GraphDeclaration& graph, const Engine& engine) {
    const auto imageName  = [&](uint32_t id) { return engine.getImageName({ .id = id }); };
    const auto bufferName = [&](uint32_t id) { return engine.getBufferName({ .id = id }); };

    std::string out = std::format("Passes ({}):\n", graph.passes.size());
    for (const PassInfo& pass : graph.passes) {
        out += std::format("  [{}] {}  {}\n",
            pass.id, pass.name, passTypeToStr(pass.type));

        if (!pass.descriptorBindings.empty()) {
            auto decls = pass.descriptorBindings;
            std::sort(decls.begin(), decls.end(),
                [](const DescriptorBindingDecl& a, const DescriptorBindingDecl& b) {
                    return a.set != b.set ? a.set < b.set : a.binding < b.binding;
                });
            out += "      Bindings:\n";
            for (const DescriptorBindingDecl& d : decls) {
                const std::string name =
                    d.isImage ? imageName(d.handleId) + rangeSuffix(d.subresources) : bufferName(d.handleId);
                out += std::format("        {}.{:<2}  {:6}  {:36}  {}\n",
                    d.set, d.binding, d.isImage ? "image" : "buffer",
                    '"' + name + '"',
                    descriptorTypeToStr(d.descriptorType));
            }
        }

        for (const BufferAccess& a : pass.bufferAccesses) {
            if (a.info.usage == BufferUsageType::Vertex)
                out += std::format("      Vertex:     \"{}\"\n", bufferName(a.buffer.id));
            else if (a.info.usage == BufferUsageType::Index)
                out += std::format("      Index:      \"{}\"\n", bufferName(a.buffer.id));
        }
        for (const ImageAccess& a : pass.imageAccesses) {
            const ImageUsageType u = a.info.usage;
            if (u == ImageUsageType::ColorAttachment || u == ImageUsageType::DepthAttachment)
                out += std::format(
                    "      Attachment: \"{}\"  {}/{}  load:{}\n", imageName(a.image.id) + rangeSuffix(a.subresources),
                    imageUsageToStr(u), accessTypeToStr(a.info.access), attachmentLoadToStr(a.info.attachmentLoad)
                );
            else if (u == ImageUsageType::Present || u == ImageUsageType::TransferSrc || u == ImageUsageType::TransferDst)
                out += std::format(
                    "      {}:  \"{}\"\n", imageUsageToStr(u), imageName(a.image.id) + rangeSuffix(a.subresources)
                );
        }

        if (pass.pipelineId) {
            if (pass.type == PassType::Compute)
                out += std::format("      Pipeline:   {}\n", graph.computePipelines[*pass.pipelineId].name);
            else if (pass.type == PassType::Graphics)
                out += std::format("      Pipeline:   {}\n", graph.graphicsPipelines[*pass.pipelineId].name);
        }

        out += '\n';
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
        case ImageUsageType::Storage:
            isValid = access == AccessType::Read || access == AccessType::Write; break;
        case ImageUsageType::ColorAttachment:
            isValid = access == AccessType::Write; break;
        case ImageUsageType::DepthAttachment:
            isValid = access == AccessType::Read || access == AccessType::Write;
            break;
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

}
