#include "graph_validation.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "../device/device.hpp"
#include "../image/image_format.hpp"
#include "../resource/resource_registry.hpp"
#include "../utils.hpp"
#include "graph_utils.hpp"

namespace VkSmol {

namespace {

std::optional<std::string> checkImageBinding(
    const ResourceRegistry& resources, const NamedBinding& binding, const ShaderParameter& parameter,
    ImageUsageType usage
) {
    ImageSubresourceRange range;
    try {
        range = resources.resolve(ImageRange({.id = binding.handleId}, binding.subresources));
    } catch (const std::exception& e) { return e.what(); }
    if (parameter.viewType != VK_IMAGE_VIEW_TYPE_2D && parameter.viewType != VK_IMAGE_VIEW_TYPE_2D_ARRAY)
        return "the shader declares a texture shape the graph cannot bind (only 2D and 2D arrays)";
    if (parameter.viewType == VK_IMAGE_VIEW_TYPE_2D && range.layerCount != 1)
        return std::format(
            "the shader declares a 2D texture but [{}] spans {} layers", range.toString(), range.layerCount
        );
    if (usage == ImageUsageType::Storage && range.mipCount != 1)
        return std::format(
            "a storage image must be a single mip but [{}] spans {} mips", range.toString(), range.mipCount
        );
    return std::nullopt;
}

} // namespace

void resolveBindings(
    std::vector<PassInfo>& passes, const std::vector<ComputePipelineInfo>& computePipelines,
    const std::vector<GraphicsPipelineInfo>& graphicsPipelines, Device& device, const ResourceRegistry& resources,
    std::vector<std::string>& errors
) {
    for (uint32_t passId = 0; passId < passes.size(); ++passId) {
        PassInfo& pass = passes[passId];
        if (pass.namedBindings.empty()) continue;

        std::vector<
            std::tuple<std::vector<std::pair<VkShaderStageFlagBits, std::string>>, ShaderDefines, ShaderConformances>>
            programs;
        VkShaderStageFlags stageFlags = 0;
        for (const ComputePipelineInfo& info : computePipelines)
            if (info.passId == passId) {
                programs.emplace_back(
                    std::vector{std::pair{VK_SHADER_STAGE_COMPUTE_BIT, info.shaderPath}}, info.shaderDefines,
                    info.shaderConformances
                );
                stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            }
        for (const GraphicsPipelineInfo& info : graphicsPipelines)
            if (info.passId == passId) {
                programs.emplace_back(info.shaderPaths, info.shaderDefines, info.shaderConformances);
                stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            }
        if (programs.empty()) {
            errors.push_back(std::format("The pass [{}] binds shader parameters but has no pipeline", pass.name));
            continue;
        }

        std::vector<const ShaderParameter*> parameters;
        try {
            for (const auto& [stages, defines, conformances] : programs)
                for (const ShaderParameter& parameter :
                     device.compileShader(stages, defines, conformances).compilation.parameters)
                    parameters.push_back(&parameter);
        } catch (const std::exception& e) {
            errors.push_back(e.what());
            continue;
        }

        for (const NamedBinding& binding : pass.namedBindings) {
            std::vector<const ShaderParameter*> matches;
            for (const ShaderParameter* parameter : parameters)
                if (parameter->name == binding.name) matches.push_back(parameter);
            if (matches.empty()) {
                errors.push_back(
                    std::format(
                        "The shaders of the pass [{}] have no parameter named [{}{}{}]", pass.name, MSG_RED_B,
                        binding.name, MSG_RESET
                    )
                );
                continue;
            }
            for (const ShaderParameter* match : matches) {
                const ShaderParameter& parameter = *match;
                if (binding.access == AccessType::Write && !parameter.writable) {
                    errors.push_back(
                        std::format(
                            "The pass [{}] writes [{}{}{}] but the shader declares it read-only", pass.name, MSG_RED_B,
                            binding.name, MSG_RESET
                        )
                    );
                    continue;
                }

                if (binding.isImage) {
                    std::optional<ImageUsageType> usage;
                    if (parameter.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) usage = ImageUsageType::Storage;
                    if (parameter.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                        usage = ImageUsageType::Sampled;
                    if (!usage) {
                        errors.push_back(
                            std::format(
                                "The parameter [{}{}{}] of the pass [{}] is not an image the graph can bind", MSG_RED_B,
                                binding.name, MSG_RESET, pass.name
                            )
                        );
                        continue;
                    }
                    if (const std::optional<std::string> error =
                            checkImageBinding(resources, binding, parameter, *usage)) {
                        errors.push_back(
                            std::format(
                                "The pass [{}] binds [{}{}{}]: {}", pass.name, MSG_RED_B, binding.name, MSG_RESET,
                                *error
                            )
                        );
                        continue;
                    }
                    pass.imageAccesses.push_back({
                        .image = {.id = binding.handleId},
                        .subresources = binding.subresources,
                        .info = {.usage = *usage, .access = binding.access},
                    });
                } else {
                    std::optional<BufferUsageType> usage;
                    if (parameter.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) usage = BufferUsageType::Storage;
                    if (parameter.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) usage = BufferUsageType::Uniform;
                    if (!usage) {
                        errors.push_back(
                            std::format(
                                "The parameter [{}{}{}] of the pass [{}] is not a buffer the graph can bind", MSG_RED_B,
                                binding.name, MSG_RESET, pass.name
                            )
                        );
                        continue;
                    }
                    pass.bufferAccesses.push_back({
                        .buffer = {.id = binding.handleId},
                        .info = {.usage = *usage, .access = binding.access},
                    });
                }

                pass.descriptorBindings.push_back({
                    .set = parameter.location.set,
                    .binding = parameter.location.binding,
                    .isImage = binding.isImage,
                    .handleId = binding.handleId,
                    .subresources = binding.subresources,
                    .viewType = parameter.viewType,
                    .descriptorType = parameter.descriptorType,
                    .stageFlags = stageFlags,
                });
            }
        }

        const std::vector<DescriptorBindingDecl>& decls = pass.descriptorBindings;
        for (size_t i = 0; i < decls.size(); ++i)
            for (size_t j = i + 1; j < decls.size(); ++j)
                if (decls[i].set == decls[j].set && decls[i].binding == decls[j].binding)
                    errors.push_back(
                        std::format(
                            "The pass [{}] binds two resources to set {}, binding {}", pass.name, decls[i].set,
                            decls[i].binding
                        )
                    );
    }
}

void checkImageAccesses(
    const std::vector<PassInfo>& passes, const ResourceRegistry& resources, std::vector<std::string>& errors
) {
    for (const PassInfo& pass : passes) {
        std::vector<std::optional<ImageSubresourceRange>> ranges;
        for (const ImageAccess& access : pass.imageAccesses) {
            try {
                ranges.push_back(resources.resolve(ImageRange(access.image, access.subresources)));
            } catch (const std::exception& e) {
                errors.push_back(std::format("The pass [{}] accesses an invalid range: {}", pass.name, e.what()));
                ranges.push_back(std::nullopt);
                continue;
            }
            const bool attachment = access.info.usage == ImageUsageType::ColorAttachment ||
                                    access.info.usage == ImageUsageType::DepthAttachment;
            if (attachment && (ranges.back()->mipCount != 1 || ranges.back()->layerCount != 1))
                errors.push_back(
                    std::format(
                        "The pass [{}] attaches [{}] of the image [{}] but attachments must be a single mip and layer",
                        pass.name, ranges.back()->toString(), resources.getImage(access.image).name
                    )
                );
        }

        for (size_t i = 0; i < ranges.size(); ++i) {
            for (size_t j = i + 1; j < ranges.size(); ++j) {
                const ImageAccess& a = pass.imageAccesses[i];
                const ImageAccess& b = pass.imageAccesses[j];
                if (!ranges[i] || !ranges[j] || a.image != b.image || a.info.usage == b.info.usage) continue;
                if (!ranges[i]->overlaps(*ranges[j])) continue;
                errors.push_back(
                    std::format(
                        "The pass [{}] uses [{}] of the image [{}] as {} and [{}] as {}", pass.name,
                        ranges[i]->toString(), resources.getImage(a.image).name, imageUsageToStr(a.info.usage),
                        ranges[j]->toString(), imageUsageToStr(b.info.usage)
                    )
                );
            }
        }
    }
}

void checkAttachments(
    const std::vector<PassInfo>& passes, const std::vector<GraphicsPipelineInfo>& graphicsPipelines,
    const ResourceRegistry& resources, std::vector<std::string>& errors
) {
    for (uint32_t passId = 0; passId < passes.size(); ++passId) {
        const PassInfo& pass = passes[passId];
        const std::vector<ImageAccess> colors = findColorAttachments(pass.imageAccesses);
        const std::optional<ImageAccess> depth = findDepthAttachment(pass.imageAccesses);
        const auto imageName = [&](const ImageAccess& access) { return resources.getImage(access.image).name; };
        const auto formatAspect = [&](const ImageAccess& access) {
            return formatAspects(resources.getImage(access.image).format);
        };

        if (pass.type != PassType::Graphics) {
            if (!colors.empty() || depth)
                errors.push_back(std::format("The pass [{}] attaches images but is not a graphics pass", pass.name));
            continue;
        }
        if (colors.empty() && !depth)
            errors.push_back(std::format("The graphics pass [{}] has no attachment", pass.name));

        const size_t depthCount = std::ranges::count_if(pass.imageAccesses, [](const ImageAccess& access) {
            return access.info.usage == ImageUsageType::DepthAttachment;
        });
        if (depthCount > 1)
            errors.push_back(
                std::format("The graphics pass [{}] has {} depth attachments, at most one", pass.name, depthCount)
            );

        for (const ImageAccess& color : colors)
            if (formatAspect(color) != VK_IMAGE_ASPECT_COLOR_BIT)
                errors.push_back(
                    std::format(
                        "The graphics pass [{}] uses the depth or stencil image [{}] as a color attachment", pass.name,
                        imageName(color)
                    )
                );
        if (depth && !(formatAspect(*depth) & VK_IMAGE_ASPECT_DEPTH_BIT))
            errors.push_back(
                std::format(
                    "The graphics pass [{}] uses the image [{}] without depth as a depth attachment", pass.name,
                    imageName(*depth)
                )
            );

        for (size_t i = 0; i < colors.size(); ++i)
            for (size_t j = i + 1; j < colors.size(); ++j) {
                if (colors[i].image != colors[j].image) continue;
                const ImageSubresourceRange first =
                    resources.resolve(ImageRange(colors[i].image, colors[i].subresources));
                const ImageSubresourceRange second =
                    resources.resolve(ImageRange(colors[j].image, colors[j].subresources));
                if (first.overlaps(second))
                    errors.push_back(
                        std::format(
                            "The graphics pass [{}] attaches [{}] of the image [{}] twice", pass.name, first.toString(),
                            imageName(colors[i])
                        )
                    );
            }

        for (const GraphicsPipelineInfo& pipeline : graphicsPipelines) {
            if (pipeline.passId != passId) continue;
            const std::optional<DepthState>& depthState = pipeline.state.depth;
            if (depthState && (depthState->test || depthState->write) && !depth)
                errors.push_back(
                    std::format(
                        "The pipeline of the pass [{}] tests or writes depth without a depth attachment", pass.name
                    )
                );
            if (depthState && depthState->write && depth && depth->info.access == AccessType::Read)
                errors.push_back(
                    std::format("The pipeline of the pass [{}] writes its read-only depth attachment", pass.name)
                );
            if (!pipeline.state.blend.empty() && pipeline.state.blend.size() != colors.size())
                errors.push_back(
                    std::format(
                        "The pipeline of the pass [{}] has {} blend states for {} color attachments", pass.name,
                        pipeline.state.blend.size(), colors.size()
                    )
                );
        }
    }
}

} // namespace VkSmol
