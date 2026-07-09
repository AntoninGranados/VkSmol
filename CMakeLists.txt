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

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

if(TARGET glfw)
  set(VKSMOL_GLFW_TARGET glfw)
elseif(TARGET glfw3)
  set(VKSMOL_GLFW_TARGET glfw3)
else()
  find_library(VKSMOL_GLFW_LIBRARY glfw)
  set(VKSMOL_GLFW_TARGET ${VKSMOL_GLFW_LIBRARY})
endif()

# ImGui — expect the parent to provide imgui, or build it here
if(NOT TARGET imgui)
  file(GLOB VKSMOL_IMGUI_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/imgui/*.cpp"
  )
  add_library(imgui STATIC ${VKSMOL_IMGUI_SOURCES})
  target_include_directories(imgui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/imgui"
  )
  target_link_libraries(imgui PUBLIC ${VKSMOL_GLFW_TARGET} Vulkan::Vulkan)
endif()

# ── Library target ────────────────────────────────────────────────────────────
add_library(VkSmol STATIC ${VKSMOL_SOURCES})

target_include_directories(VkSmol PUBLIC
  "${CMAKE_CURRENT_BINARY_DIR}"
)

target_include_directories(VkSmol PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if(DEFINED VKSMOL_INCLUDE_DIR)
  target_include_directories(VkSmol PUBLIC "${VKSMOL_INCLUDE_DIR}")
endif()

target_link_libraries(VkSmol PUBLIC
  ${VKSMOL_GLFW_TARGET}
  Vulkan::Vulkan
  imgui
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
