cmake_minimum_required(VERSION 3.20)
project(VulkanOBJRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vulkan
find_package(Vulkan REQUIRED)

# GLFW
find_package(glfw3 3.3 REQUIRED)

# GLM
find_package(glm REQUIRED)

# Sources
file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

# Shader compilation
find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin)

file(GLOB SHADERS shaders/*.vert shaders/*.frag shaders/*.comp)
foreach(SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPIRV ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
        COMMAND ${GLSLC} ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
    )
    list(APPEND SPIRV_FILES ${SPIRV})
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_FILES})
add_dependencies(${PROJECT_NAME} compile_shaders)
