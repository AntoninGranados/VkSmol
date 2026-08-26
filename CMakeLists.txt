cmake_minimum_required(VERSION 3.20)

project(VkSmol LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Version ───────────────────────────────────────────────────────────────────
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VK_SMOL_VERSION_STRING)
string(STRIP "${VK_SMOL_VERSION_STRING}" VK_SMOL_VERSION_STRING)
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${VK_SMOL_VERSION_STRING}")
set(VK_SMOL_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(VK_SMOL_VERSION_MINOR "${CMAKE_MATCH_2}")
set(VK_SMOL_VERSION_PATCH "${CMAKE_MATCH_3}")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/engine_version.hpp"
)

# ── Sources ───────────────────────────────────────────────────────────────────
file(GLOB_RECURSE VKSMOL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(FILTER VKSMOL_SOURCES EXCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}/external/")

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)

if(NOT TARGET glfw)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/glfw" glfw)
endif()
set(VKSMOL_GLFW_TARGET glfw)

set(SHADERC_GLSLANG_DIR       "${CMAKE_CURRENT_SOURCE_DIR}/external/glslang"       CACHE STRING "" FORCE)
set(SHADERC_SPIRV_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/spirv-headers" CACHE STRING "" FORCE)
set(SHADERC_SPIRV_TOOLS_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/external/spirv-tools"   CACHE STRING "" FORCE)
set(SHADERC_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SHADERC_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
set(SHADERC_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SHADERC_SKIP_INSTALL ON CACHE BOOL "" FORCE)
set(SHADERC_SKIP_COPYRIGHT_CHECK ON CACHE BOOL "" FORCE)
set(SHADERC_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
macro(install)
endmacro()
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/shaderc" shaderc EXCLUDE_FROM_ALL)
target_include_directories(shaderc_combined PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/external/shaderc/libshaderc/include"
)

# ImGui — expect the parent to provide imgui, or build it here
if(NOT TARGET imgui)
  file(GLOB VKSMOL_IMGUI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/*.cpp"
  )
  add_library(imgui STATIC ${VKSMOL_IMGUI_SOURCES})
  target_include_directories(imgui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/external"
  )
  target_link_libraries(imgui PUBLIC ${VKSMOL_GLFW_TARGET} Vulkan::Vulkan)
  target_compile_options(imgui PRIVATE -Wno-nontrivial-memcall)
endif()

# ── Library target ────────────────────────────────────────────────────────────
add_library(VkSmol STATIC ${VKSMOL_SOURCES})

target_include_directories(VkSmol PUBLIC
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if(DEFINED VKSMOL_INCLUDE_DIR)
  target_include_directories(VkSmol PUBLIC "${VKSMOL_INCLUDE_DIR}")
endif()

target_link_libraries(VkSmol PUBLIC
  ${VKSMOL_GLFW_TARGET}
  Vulkan::Vulkan
  imgui
  shaderc_combined
)

target_compile_options(VkSmol PRIVATE -Wall)
if(APPLE)
  target_link_libraries(VkSmol PUBLIC
    "-framework Accelerate"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework Metal"
    "-framework QuartzCore"
  )
endif()
if(DEFINED ENV{VULKAN_SDK})
  target_link_directories(VkSmol PUBLIC "$ENV{VULKAN_SDK}/lib")
endif()
