#include "shader.hpp"

#include <slang-com-ptr.h>
#include <slang.h>
#include <spirv-tools/optimizer.hpp>

#include <algorithm>
#include <filesystem>
#include <format>
#include <functional>
#include <optional>

#include "shader_file_system.hpp"
#include "../device/logical_device.hpp"
#include "../utils.hpp"

namespace VkSmol {

void Shader::init(const LogicalDevice& device, VkShaderStageFlagBits stage_, const std::vector<uint32_t>& spirv) {
    stage = stage_;

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();

    vkCheck(vkCreateShaderModule(device.get(), &createInfo, nullptr, &rawHandle()), "Failed to create shader module");
}

void Shader::destroy(const LogicalDevice& device) {
    vkDestroyShaderModule(device.get(), rawHandle(), nullptr);
}

VkPipelineShaderStageCreateInfo Shader::getShaderStageCreateInfo() const {
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = rawHandle();
    shaderStageInfo.pName = "main";

    return shaderStageInfo;
}

class Shader::SlangFileSystem final: public ISlangFileSystem {
public:
    SlangFileSystem(ShaderFileSystem& fileSystem, std::vector<ShaderDependency>& dependencies): fileSystem(fileSystem), dependencies(dependencies) {}

    SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(const SlangUUID& uuid, void** outObject) override {
        *outObject = castAs(uuid);
        return *outObject ? SLANG_OK : SLANG_E_NO_INTERFACE;
    }
    SLANG_NO_THROW uint32_t SLANG_MCALL addRef() override { return 1; }
    SLANG_NO_THROW uint32_t SLANG_MCALL release() override { return 1; }

    SLANG_NO_THROW void* SLANG_MCALL castAs(const SlangUUID& uuid) override {
        const bool supported = uuid == ISlangUnknown::getTypeGuid() || uuid == ISlangCastable::getTypeGuid() || uuid == ISlangFileSystem::getTypeGuid();
        return supported ? static_cast<ISlangFileSystem*>(this) : nullptr;
    }

    SLANG_NO_THROW SlangResult SLANG_MCALL loadFile(const char* path, ISlangBlob** outBlob) override {
        const std::optional<std::string> content = readFile(path);
        if (!content) return SLANG_E_NOT_FOUND;

        *outBlob = slang_createBlob(content->data(), content->size());
        return SLANG_OK;
    }

    std::optional<std::string> readFile(const std::string& path) {
        std::optional<std::string> content = fileSystem.readFile(path);
        if (content) dependencies.push_back({ path, std::hash<std::string>{}(*content) });
        return content;
    }

private:
    ShaderFileSystem&              fileSystem;
    std::vector<ShaderDependency>& dependencies;
};

bool Shader::isUpToDate(const std::vector<ShaderDependency>& dependencies, ShaderFileSystem& fileSystem) {
    for (const ShaderDependency& dependency : dependencies) {
        const std::optional<std::string> content = fileSystem.readFile(dependency.path);
        if (!content || std::hash<std::string>{}(*content) != dependency.hash) return false;
    }
    return true;
}

static std::optional<std::pair<VkDescriptorType, bool>> reflectDescriptorType(slang::TypeLayoutReflection* type) {
    if (type->getBindingRangeCount() == 0) return std::nullopt;
    switch (type->getBindingRangeType(0)) {
        case slang::BindingType::CombinedTextureSampler: return std::pair{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, false };
        case slang::BindingType::Texture:                return std::pair{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, false };
        case slang::BindingType::MutableTexture:         return std::pair{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, true };
        case slang::BindingType::Sampler:                return std::pair{ VK_DESCRIPTOR_TYPE_SAMPLER, false };
        case slang::BindingType::ConstantBuffer:         return std::pair{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, false };
        case slang::BindingType::RawBuffer:              return std::pair{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, false };
        case slang::BindingType::MutableRawBuffer:       return std::pair{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, true };
        case slang::BindingType::TypedBuffer:            return std::pair{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, false };
        case slang::BindingType::MutableTypedBuffer:     return std::pair{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, true };
        default:                                         return std::nullopt;
    }
}

ShaderCompilation Shader::compile(const std::string& path, const ShaderDefines& defines, ShaderFileSystem& fileSystem) {
    ShaderCompilation compilation;
    SlangFileSystem slangFileSystem(fileSystem, compilation.dependencies);
    const std::optional<std::string> code = slangFileSystem.readFile(path);
    if (!code) {
        throw std::runtime_error(std::format("Failed to open the shader [{}{}{}]", MSG_RED_B, path, MSG_RESET));
    }

    static const Slang::ComPtr<slang::IGlobalSession> globalSession = [] {
        SlangGlobalSessionDesc globalSessionDesc{};

        Slang::ComPtr<slang::IGlobalSession> created;
        if (SLANG_FAILED(slang::createGlobalSession(&globalSessionDesc, created.writeRef()))) {
            throw std::runtime_error("Failed to create the Slang global session");
        }
        return created;
    }();

    slang::TargetDesc targetDesc{};
    targetDesc.format = SLANG_SPIRV;

    std::vector<slang::PreprocessorMacroDesc> macros;
    for (const auto& [defineName, defineValue] : defines) macros.push_back({ defineName.c_str(), defineValue.c_str() });

    slang::SessionDesc sessionDesc{};
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;
    sessionDesc.fileSystem = &slangFileSystem;
    sessionDesc.preprocessorMacros = macros.data();
    sessionDesc.preprocessorMacroCount = static_cast<SlangInt>(macros.size());

    Slang::ComPtr<slang::ISession> session;
    if (SLANG_FAILED(globalSession->createSession(sessionDesc, session.writeRef()))) {
        throw std::runtime_error(std::format("Failed to create the Slang session for the shader [{}{}{}]", MSG_RED_B, path, MSG_RESET));
    }

    Slang::ComPtr<slang::IBlob> diagnostics;
    const auto fail = [&] {
        const std::string message = diagnostics ? std::string(static_cast<const char*>(diagnostics->getBufferPointer()), diagnostics->getBufferSize()) : std::string();
        return std::runtime_error(std::format("Failed to compile the shader [{}{}{}]: {}", MSG_RED_B, path, MSG_RESET, message));
    };

    const std::string moduleName = std::filesystem::path(path).stem().string();
    slang::IModule* module = session->loadModuleFromSourceString(moduleName.c_str(), path.c_str(), code->c_str(), diagnostics.writeRef());
    if (!module) throw fail();

    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    if (SLANG_FAILED(module->findEntryPointByName("main", entryPoint.writeRef()))) {
        throw std::runtime_error(std::format("The shader [{}{}{}] has no [shader(...)] entry point named main", MSG_RED_B, path, MSG_RESET));
    }

    slang::IComponentType* components[] = { module, entryPoint.get() };
    Slang::ComPtr<slang::IComponentType> program;
    if (SLANG_FAILED(session->createCompositeComponentType(components, 2, program.writeRef(), diagnostics.writeRef()))) throw fail();

    Slang::ComPtr<slang::IComponentType> linkedProgram;
    if (SLANG_FAILED(program->link(linkedProgram.writeRef(), diagnostics.writeRef()))) throw fail();

    Slang::ComPtr<slang::IBlob> spirv;
    if (SLANG_FAILED(linkedProgram->getEntryPointCode(0, 0, spirv.writeRef(), diagnostics.writeRef()))) throw fail();

    Slang::ComPtr<slang::IMetadata> metadata;
    if (SLANG_FAILED(linkedProgram->getEntryPointMetadata(0, 0, metadata.writeRef(), diagnostics.writeRef()))) throw fail();

    slang::ProgramLayout* layout = linkedProgram->getLayout();
    switch (layout->getEntryPointByIndex(0)->getStage()) {
        case SLANG_STAGE_VERTEX:   compilation.stage = VK_SHADER_STAGE_VERTEX_BIT; break;
        case SLANG_STAGE_FRAGMENT: compilation.stage = VK_SHADER_STAGE_FRAGMENT_BIT; break;
        case SLANG_STAGE_COMPUTE:  compilation.stage = VK_SHADER_STAGE_COMPUTE_BIT; break;
        default:
            throw std::runtime_error(std::format("The shader [{}{}{}] does not have a supported stage for the compilation", MSG_RED_B, path, MSG_RESET));
    }

    const auto collectParameters = [&](this const auto& self, slang::VariableLayoutReflection* variable, const std::string& prefix, uint32_t set, uint32_t binding) -> void {
        const std::string name = prefix.empty() ? variable->getName() : prefix + "." + variable->getName();
        slang::TypeLayoutReflection* type = variable->getTypeLayout();
        set += static_cast<uint32_t>(variable->getOffset(SLANG_PARAMETER_CATEGORY_SUB_ELEMENT_REGISTER_SPACE));
        binding += static_cast<uint32_t>(variable->getOffset(SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT));

        if (variable->getCategory() == slang::ParameterCategory::PushConstantBuffer) {
            slang::TypeLayoutReflection* element = type->getElementTypeLayout();
            compilation.pushConstantSize = std::max(compilation.pushConstantSize, static_cast<uint32_t>((element ? element : type)->getSize()));
            return;
        }

        if (type->getKind() == slang::TypeReflection::Kind::ParameterBlock) {
            slang::VariableLayoutReflection* element = type->getElementVarLayout();
            slang::TypeLayoutReflection* elementType = element->getTypeLayout();
            const uint32_t elementBinding = static_cast<uint32_t>(element->getOffset(SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT));
            for (unsigned int i = 0; i < elementType->getFieldCount(); i++) self(elementType->getFieldByIndex(i), name, set, elementBinding);
            return;
        }
        if (type->getKind() == slang::TypeReflection::Kind::Struct) {
            for (unsigned int i = 0; i < type->getFieldCount(); i++) self(type->getFieldByIndex(i), name, set, binding);
            return;
        }

        const std::optional<std::pair<VkDescriptorType, bool>> descriptor = reflectDescriptorType(type);
        if (!descriptor) return;

        bool used = false;
        metadata->isParameterLocationUsed(SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT, set, binding, used);
        compilation.parameters.push_back({ name, ShaderLocation(set, binding), descriptor->first, descriptor->second, used });
    };
    for (unsigned int i = 0; i < layout->getParameterCount(); i++) collectParameters(layout->getParameterByIndex(i), "", 0, 0);

    slang::EntryPointReflection* entryPointLayout = layout->getEntryPointByIndex(0);
    if (compilation.stage == VK_SHADER_STAGE_COMPUTE_BIT) {
        SlangUInt size[3];
        entryPointLayout->getComputeThreadGroupSize(3, size);
        compilation.threadGroupSize = { static_cast<uint32_t>(size[0]), static_cast<uint32_t>(size[1]), static_cast<uint32_t>(size[2]) };
    }
    for (unsigned int i = 0; i < entryPointLayout->getParameterCount(); i++) {
        slang::VariableLayoutReflection* parameter = entryPointLayout->getParameterByIndex(i);
        if (parameter->getCategory() != slang::ParameterCategory::Uniform) continue;
        const size_t end = parameter->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM) + parameter->getTypeLayout()->getSize();
        compilation.pushConstantSize = std::max(compilation.pushConstantSize, static_cast<uint32_t>(end));
    }

    compilation.spirv = optimize(static_cast<const uint32_t*>(spirv->getBufferPointer()), spirv->getBufferSize() / sizeof(uint32_t), path);
    return compilation;
}

std::vector<uint32_t> Shader::optimize(const uint32_t* words, size_t wordCount, const std::string& name) {
    std::string messages;
    spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_4);
    optimizer.SetMessageConsumer([&](spv_message_level_t, const char*, const spv_position_t&, const char* message) {
        if (message) messages += std::format("\n{}", message);
    });

    optimizer.RegisterPass(spvtools::CreateWrapOpKillPass());
    optimizer.RegisterPass(spvtools::CreateDeadBranchElimPass());
    optimizer.RegisterPass(spvtools::CreateMergeReturnPass());
    optimizer.RegisterPass(spvtools::CreateInlineExhaustivePass());
    optimizer.RegisterPass(spvtools::CreateEliminateDeadFunctionsPass());
    optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass());
    optimizer.RegisterPass(spvtools::CreatePrivateToLocalPass());
    optimizer.RegisterPass(spvtools::CreateScalarReplacementPass(100));
    optimizer.RegisterPass(spvtools::CreateCCPPass());
    optimizer.RegisterPass(spvtools::CreateSimplificationPass());
    optimizer.RegisterPass(spvtools::CreateLocalAccessChainConvertPass());
    optimizer.RegisterPass(spvtools::CreateLocalSingleBlockLoadStoreElimPass());
    optimizer.RegisterPass(spvtools::CreateAggressiveDCEPass());
    optimizer.RegisterPass(spvtools::CreateVectorDCEPass());

    spvtools::OptimizerOptions options;
    options.set_max_id_bound(0x3FFFFFFF);
    options.set_run_validator(false);

    std::vector<uint32_t> optimized;
    if (!optimizer.Run(words, wordCount, &optimized, options)) {
        throw std::runtime_error(std::format("Failed to optimize the shader [{}{}{}]:{}", MSG_RED_B, name, MSG_RESET, messages));
    }

    return optimized;
}

}
