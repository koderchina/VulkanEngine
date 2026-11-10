cmake_minimum_required(VERSION 3.16)
project(VulkanEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# vk-bootstrap: instance / physical device / logical device / swapchain setup.
# Pinned to v1.3.240 because it still builds against the system's 1.3.204 Vulkan
# headers; newer tags need newer headers than Ubuntu 22.04 ships.
include(FetchContent)
FetchContent_Declare(vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.3.240
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(vk_bootstrap)

find_program(GLSLC glslc HINTS /usr/local/bin)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Install the Vulkan SDK / shaderc.")
endif()

# --- Source discovery --------------------------------------------------
# CONFIGURE_DEPENDS re-runs this glob before every build, so a new .cpp
# (in core/ or any new subdirectory under it, e.g. core/systems) gets added
# automatically on the next build without touching this file.
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/core/*.cpp
)

add_executable(VulkanEngine ${ENGINE_SOURCES})

# Every directory under core/ that holds a header is added as an include
# path, so quoted includes like "SimpleRenderSystem.hpp" keep resolving
# no matter which subfolder the header lives in.
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/core/*.hpp
    ${CMAKE_SOURCE_DIR}/core/*.h
)
set(ENGINE_INCLUDE_DIRS "")
foreach(HEADER ${ENGINE_HEADERS})
    get_filename_component(HEADER_DIR ${HEADER} DIRECTORY)
    list(APPEND ENGINE_INCLUDE_DIRS ${HEADER_DIR})
endforeach()
list(REMOVE_DUPLICATES ENGINE_INCLUDE_DIRS)

target_include_directories(VulkanEngine PRIVATE
    core
    ${ENGINE_INCLUDE_DIRS}
    libs/tinyobjloader
    libs/stb_image
)

target_link_libraries(VulkanEngine PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    vk-bootstrap::vk-bootstrap
)

# Keep the binary at the project root so the relative "./shaders/..."
# paths in SimpleRenderSystem.cpp resolve the same way they did with the
# old Makefile (which also built a.out at the root).
set_target_properties(VulkanEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# --- Shader compilation -----------------------------------------------
# Mirrors the dependency tracking the old Makefile did: each .vert/.frag
# gets its own custom command, so `cmake --build` only recompiles a shader
# when its source changed, and the executable depends on all of them.
file(GLOB SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/*.vert
    ${CMAKE_SOURCE_DIR}/shaders/*.frag
)

set(SPIRV_BINARIES)
foreach(SHADER ${SHADER_SOURCES})
    message(COMPILING SHADER: ${SHADER})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPIRV ${CMAKE_SOURCE_DIR}/shaders/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLC} ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
        VERBATIM
    )
    list(APPEND SPIRV_BINARIES ${SPIRV})
endforeach()

add_custom_target(Shaders DEPENDS ${SPIRV_BINARIES})
add_dependencies(VulkanEngine Shaders)

# `cmake --build build --target run` builds (if needed) then launches the app.
add_custom_target(run
    COMMAND VulkanEngine
    DEPENDS VulkanEngine
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
)
