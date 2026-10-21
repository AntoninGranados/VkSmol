#pragma once

namespace VkSmol {

struct EngineFeatures {
#ifdef NDEBUG
    bool validation = false;
#else
    bool validation = true;
#endif
    bool samplerAnisotropy = true;
    bool fragmentStoresAndAtomics = true;
    bool bufferFloatAtomicAdd = false;
    bool sharedFloatAtomicAdd = false;
    bool imageFloatAtomicAdd = false;
};

} // namespace VkSmol
