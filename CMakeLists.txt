CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(spokk)

# Add third-party projects
ADD_SUBDIRECTORY(third_party)

IF(${MSVC})
    SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
    SET(CMAKE_CXX_FLAGS "/GS- /GR- /W4 /WX /wd4996 /wd4201 /we4003 /EHsc /D _UNICODE /D UNICODE /D NOMINMAX /D _HAS_EXCEPTIONS=0" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob1 /Oi /fp:fast /D NDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "/MDd /MP /Od /Ob0 /GS- /Zi /D _DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS "/GS- /GR- /W4 /WX  /wd4996 /wd4201 /we4003 /D _UNICODE /D UNICODE /D NOMINMAX" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "/MD /O2 /Ob1 /Oi /fp:fast /D NDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "/MDd /MP /Od /Ob0 /GS- /Zi /D _DEBUG" CACHE TYPE STRING FORCE)
    LIST(APPEND libs)
ELSEIF(${UNIX})
    SET(CMAKE_CXX_FLAGS "-Wall -Werror -march=native -msse4.1 -D_POSIX_C_SOURCE=199309L -DM_PI=3.141592653 -std=c++11" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "-D_DEBUG -g" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS "-Wall -Werror -march=native -msse4.1 -D_POSIX_C_SOURCE=199309L -DM_PI=3.141592653 -std=c99" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "-DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "-D_DEBUG -g" CACHE TYPE STRING FORCE)
    SET(CMAKE_EXE_LINKER_FLAGS  "-pthread" CACHE TYPE STRING FORCE)
    LIST(APPEND libs m)
ENDIF()

# spokkle
SET(SPOKKLE_SOURCES
    src/spokkle/spokkle.cpp
    src/spokk/spokk_vertex.cpp
)
SET(SPOKKLE_HEADERS
)
SOURCE_GROUP("" FILES ${SPOKKLE_HEADERS} ${SPOKKLE_SOURCES})
SOURCE_GROUP("json.h" REGULAR_EXPRESSION "json.[ch]$")
ADD_EXECUTABLE(spokkle
    ${SPOKKLE_SOURCES}
    ${SPOKKLE_HEADERS}
    ${JSON_H_DIR}/json.c
    ${JSON_H_DIR}/json.h
)
TARGET_LINK_LIBRARIES(spokkle assimp shaderc ${VULKAN_LIBRARY})
TARGET_INCLUDE_DIRECTORIES(spokkle PRIVATE
    ${CMAKE_BINARY_DIR}/third_party/assimp/include # for config.h
    ${CMAKE_SOURCE_DIR}/third_party/assimp/include # for everything else
    ${VULKAN_INCLUDE_DIR}
    "third_party/glm"
    ${JSON_H_DIR}
    "src/spokk"
)
IF(${MSVC})
    # Windows puts some path-manipulation APIs in an optional library
    TARGET_LINK_LIBRARIES(spokkle shlwapi)
ENDIF(${MSVC})

# Certain source files from third_party are included directly
# in spokk targets, but must still be exempt from the -Wall -Werror
# policy.
IF(${MSVC})
    SET_PROPERTY(
      SOURCE ${IMGUI_SOURCE_FILES}
      APPEND PROPERTY COMPILE_FLAGS /W0
    )
ELSEIF(${UNIX})
    SET_PROPERTY(
      SOURCE ${IMGUI_SOURCE_FILES}
      APPEND PROPERTY COMPILE_FLAGS -w
    )
ENDIF()


# libspokk
SET(SPOKK_HEADERS
    src/spokk/image_file.h
    src/spokk/spokk.h
    src/spokk/spokk_application.h
    src/spokk/spokk_buffer.h
    src/spokk/spokk_debug.h
    src/spokk/spokk_device.h
    src/spokk/spokk_image.h
    src/spokk/spokk_imgui_impl_glfw_vulkan.h
    src/spokk/spokk_input.h
    src/spokk/spokk_math.h
    src/spokk/spokk_memory.h
    src/spokk/spokk_mesh.h
    src/spokk/spokk_pipeline.h
    src/spokk/spokk_platform.h
    src/spokk/spokk_renderer.h
    src/spokk/spokk_renderpass.h
    src/spokk/spokk_shader.h
    src/spokk/spokk_shader_interface.h
    src/spokk/spokk_utilities.h
    src/spokk/spokk_vertex.h
)
SET(SPOKK_SOURCES
    src/spokk/image_file.c
    src/spokk/spokk_application.cpp
    src/spokk/spokk_buffer.cpp
    src/spokk/spokk_device.cpp
    src/spokk/spokk_image.cpp
    src/spokk/spokk_imgui_impl_glfw_vulkan.cpp
    src/spokk/spokk_input.cpp
    src/spokk/spokk_math.cpp
    src/spokk/spokk_memory.cpp
    src/spokk/spokk_mesh.cpp
    src/spokk/spokk_pipeline.cpp
    src/spokk/spokk_renderer.cpp
    src/spokk/spokk_renderpass.cpp
    src/spokk/spokk_shader.cpp
    src/spokk/spokk_utilities.cpp
    src/spokk/spokk_vertex.cpp
)
SOURCE_GROUP("" FILES ${SPOKK_HEADERS} ${SPOKK_SOURCES})
SET(SPOKK_LIB_NAME spokk)
SOURCE_GROUP("imgui" REGULAR_EXPRESSION "imgui/.*$")
ADD_LIBRARY(${SPOKK_LIB_NAME} STATIC
    ${SPOKK_SOURCES}
    ${SPOKK_HEADERS}
    ${IMGUI_SOURCE_FILES}
)
TARGET_LINK_LIBRARIES(${SPOKK_LIB_NAME} glfw ${GLFW_LIBRARIES} spirv-cross-cpp ${libs} ${VULKAN_LIBRARY})
TARGET_INCLUDE_DIRECTORIES(${SPOKK_LIB_NAME} PUBLIC
    "third_party/glm"
    "third_party/imgui"
    ${VULKAN_MEMORY_ALLOCATOR_DIR}
    "third_party"
    ${VULKAN_INCLUDE_DIR}
    "src/spokk"
)

# custom target to build/copy sample assets
ADD_CUSTOM_TARGET(build-assets
    COMMAND $<TARGET_FILE:spokkle> samples/assets/assets.json5
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building assets from ${CMAKE_SOURCE_DIR}/samples/assets/assets.json5"
    SOURCES samples/assets/assets.json5
)
SET_TARGET_PROPERTIES(build-assets PROPERTIES FOLDER "samples")
ADD_DEPENDENCIES(build-assets spokkle)

# usage: SPOKK_ADD_SOURCES(target [source1 source2 source3 ...])
MACRO(SPOKK_ADD_SOURCES APP_TARGET)
    LIST(APPEND ${APP_TARGET}_SOURCES ${ARGN})
ENDMACRO(SPOKK_ADD_SOURCES APP_TARGET)

# usage: SPOKK_ADD_HEADERS(target [header1 header2 header3 ...])
MACRO(SPOKK_ADD_HEADERS APP_TARGET)
    LIST(APPEND ${APP_TARGET}_HEADERS ${ARGN})
ENDMACRO(SPOKK_ADD_HEADERS APP_TARGET)

MACRO(SPOKK_ADD_SHADERS APP_TARGET)
    LIST(APPEND ${APP_TARGET}_SHADERS ${ARGN})
ENDMACRO(SPOKK_ADD_SHADERS APP_TARGET)

# Adds a spokk-based executable target, onces its sources/headers/shaders have been defined.
FUNCTION (SPOKK_ADD_EXECUTABLE APP_TARGET)
    SOURCE_GROUP("" REGULAR_EXPRESSION ".h(pp)?$")
    SOURCE_GROUP("" REGULAR_EXPRESSION ".c(pp)?$")
    SOURCE_GROUP("shaders" REGULAR_EXPRESSION ".(hlsl|vert|frag|comp)$")
    ADD_EXECUTABLE(${APP_TARGET} ${${APP_TARGET}_SOURCES} ${${APP_TARGET}_SHADERS} ${${APP_TARGET}_HEADERS})
    TARGET_LINK_LIBRARIES(${APP_TARGET} ${SPOKK_LIB_NAME})
    TARGET_INCLUDE_DIRECTORIES(${APP_TARGET} PUBLIC "${CMAKE_SOURCE_DIR}/samples")
    SET_TARGET_PROPERTIES(${APP_TARGET} PROPERTIES FOLDER "samples")

    ADD_DEPENDENCIES(${APP_TARGET} build-assets)
ENDFUNCTION(SPOKK_ADD_EXECUTABLE APP_TARGET)

# spokk-cubeswarm
SPOKK_ADD_SOURCES(spokk-cubeswarm
    samples/common/camera.cpp
    samples/cubeswarm/cubeswarm.cpp
)
SPOKK_ADD_HEADERS(spokk-cubeswarm
    samples/common/camera.h
)
SPOKK_ADD_SHADERS(spokk-cubeswarm
    samples/cubeswarm/rigid_mesh.vert
    samples/cubeswarm/rigid_mesh.frag
)
SPOKK_ADD_EXECUTABLE(spokk-cubeswarm)

# spokk-compute
SPOKK_ADD_SOURCES(spokk-compute samples/compute/compute.cpp)
SPOKK_ADD_SHADERS(spokk-compute samples/compute/double_ints.comp)
SPOKK_ADD_EXECUTABLE(spokk-compute)

# spokk-lights
SPOKK_ADD_SOURCES(spokk-lights
    samples/common/camera.cpp
    samples/lights/lights.cpp
)
SPOKK_ADD_HEADERS(spokk-lights
    samples/common/camera.h
)
SPOKK_ADD_SHADERS(spokk-lights
    samples/lights/emissive.vert
    samples/lights/emissive.frag
    samples/lights/lit_mesh.vert
    samples/lights/lit_mesh.frag
    samples/lights/skybox.vert
    samples/lights/skybox.frag
)
SPOKK_ADD_EXECUTABLE(spokk-lights)

# spokk-pillars
SPOKK_ADD_SOURCES(spokk-pillars
    samples/common/camera.cpp
    samples/pillars/pillars.cpp
)
SPOKK_ADD_HEADERS(spokk-pillars
    samples/common/camera.h
)
SPOKK_ADD_SHADERS(spokk-pillars
    samples/pillars/pillar.vert
    samples/pillars/pillar.frag
)
SPOKK_ADD_EXECUTABLE(spokk-pillars)

# spokk-shadertoy
SPOKK_ADD_SOURCES(spokk-shadertoy
    samples/shadertoy/shadertoy.cpp
    samples/common/shader_compiler.cpp
)
SPOKK_ADD_HEADERS(spokk-shadertoy
    samples/common/shader_compiler.h
)
SPOKK_ADD_SHADERS(spokk-shadertoy
    samples/shadertoy/fullscreen.vert
    samples/shadertoy/shadertoy.frag # this shader is special; it's loaded at runtime, not built offline.
)
SPOKK_ADD_EXECUTABLE(spokk-shadertoy)
TARGET_LINK_LIBRARIES(spokk-shadertoy shaderc)
