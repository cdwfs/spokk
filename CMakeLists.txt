cmake_minimum_required(VERSION 2.8)

project(spokk)

# Global build settings
IF(MSVC)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    SET(CMAKE_CXX_FLAGS "/GS- /GR- /W0 /EHsc /D _UNICODE /D UNICODE /D NOMINMAX /D _HAS_EXCEPTIONS=0" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob1 /Oi /GL /fp:fast /D NDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "/MDd /MP /Od /Ob0 /GS- /Zi /D _DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS "/GS- /GR- /W0 /D _UNICODE /D UNICODE /D NOMINMAX" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "/MD /O2 /Ob1 /Oi /GL /fp:fast /D NDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "/MDd /MP /Od /Ob0 /GS- /Zi /D _DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG" CACHE TYPE STRING FORCE)
    SET(libs)
ELSEIF(UNIX)
    # TODO(cort): -W?
    SET(CMAKE_CXX_FLAGS "-march=native -msse4.1 -D_POSIX_C_SOURCE=199309L -DM_PI=3.141592653 -std=c++11" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "-D_DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS "-march=native -msse4.1 -D_POSIX_C_SOURCE=199309L -DM_PI=3.141592653 -std=c99" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "-DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "-D_DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_EXE_LINKER_FLAGS  "-pthread" CACHE TYPE STRING FORCE)
    SET(libs m)
ENDIF()

# GLFW
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
option(USE_MSVC_RUNTIME_LIBRARY_DLL "Use MSVC runtime library DLL" ON)
add_subdirectory(third_party/glfw)

# assimp
OPTION(ASSIMP_BUILD_ASSIMP_TOOLS "If the supplementary tools for Assimp are built in addition to the library." OFF)
OPTION(ASSIMP_BUILD_TESTS "If the test suite for Assimp is built in addition to the library." OFF)
OPTION(ASSIMP_NO_EXPORT "Disable Assimp's export functionality." ON)
OPTION(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT "default value of all ASSIMP_BUILD_XXX_IMPORTER value" FALSE)
#OPTION(ASSIMP_BUILD_BLEND_IMPORTER "build the BLEND importer" ON)
#OPTION(ASSIMP_BUILD_FBX_IMPORTER "build the FBX importer" ON)
OPTION(ASSIMP_BUILD_OBJ_IMPORTER "build the OBJ importer" ON)
ADD_SUBDIRECTORY(third_party/assimp)
set(ASSIMP_TARGETS
    assimp
    uninstall
    UpdateAssimpLibsDebugSymbolsAndDLLs
    zlibstatic
)
set_target_properties(${ASSIMP_TARGETS} PROPERTIES FOLDER "assimp")

# json.h
set(JSON_H_DIR third_party/json.h)

# Vulkan -- uses VULKAN_* variables defined by GLFW
include_directories(${VULKAN_INCLUDE_DIR})
#link_directories(${VULKAN_SDK}/lib)
IF(MSVC)
    SET(libs ${libs} ${VULKAN_LIBRARY})
ELSEIF(APPLE)
    SET(libs ${libs} ${VULKAN_LIBRARY})
ELSEIF(UNIX)
    SET(libs ${libs} ${VULKAN_LIBRARY})
ENDIF()

# SPIRV-Cross
add_subdirectory(third_party/SPIRV-Cross)
set(SPIRV_CROSS_TARGETS
	spirv-cross-core
	spirv-cross-glsl
	spirv-cross-hlsl
	spirv-cross-cpp
	spirv-cross-msl
	spirv-cross
)
set_target_properties(${SPIRV_CROSS_TARGETS} PROPERTIES FOLDER "SPIRV-Cross")

# fplutil (dependency of mathfu)
set(dependencies_fplutil_dir ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fplutil CACHE PATH "base directory of fplutil repository" FORCE)
# Mathfu
set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
set(mathfu_enable_simd OFF CACHE BOOL "")
add_subdirectory(third_party/mathfu)

# shaderc
SET(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv-headers" CACHE PATH "Location of SPIRV-Headers project" FORCE)
SET(SHADERC_THIRD_PARTY_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party" CACHE PATH "Root location of shaderc third-party projets" FORCE)
option(SHADERC_SKIP_TESTS "Skip building tests" ON)
option(SHADERC_ENABLED_SHARED_CRT "Use the shared CRT instead of the static CRT" ON)
add_subdirectory(third_party/shaderc)
# shaderc pulls in a gajillion targets. Let's organize them a bit.
set(GLSLANG_TARGETS
    glslang
    glslang-default-resource-limits
    glslangValidator
    HLSL
    OGLCompiler
    OSDependent
    SPIRV
    spirv-remap
    SPVRemapper
    Continuous
    Experimental
    Nightly
    NightlyMemoryCheck

)
SET(SPIRV_TOOLS_TARGETS
    spirv-as
    spirv-cfg
    spirv-dis
    spirv-opt
    spirv-stats
    SPIRV-Tools
    spirv-tools-build-version
    spirv-tools-cpp-example
    spirv-tools-spv-amd-gcn-shader
    spirv-tools-vimsyntax
    SPIRV-Tools-opt
    spirv-val
)
set(SHADERC_TARGETS
    add-copyright
    build-version
    check-copyright
    glslc
    glslc_exe
    shaderc
    shaderc_combined_genfile
    shaderc_util
    shaderc-online-compile
    testdata
)
set_target_properties(${GLSLANG_TARGETS} PROPERTIES FOLDER "shaderc/glslang")
set_target_properties(${SPIRV_TOOLS_TARGETS} PROPERTIES FOLDER "shaderc/spirv-tools")
set_target_properties(${SHADERC_TARGETS} PROPERTIES FOLDER "shaderc")

# Skip executable targets; we only care about the library anyway.
FOREACH(SHADERC_TARGET IN LISTS SPIRV_CROSS_TARGETS GLSLANG_TARGETS SPIRV_TOOLS_TARGETS SHADERC_TARGETS)
    get_target_property(TARGET_TYPE ${SHADERC_TARGET} TYPE)
    IF(${TARGET_TYPE} STREQUAL EXECUTABLE)
        set_target_properties(${SHADERC_TARGET} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    ENDIF()
ENDFOREACH()

# Mysterious macro to remove default VS "Source Files" and "Header Files" filters.
# TODO(https://github.com/cdwfs/spokk/issues/10): Figure out why this works and refactor if necessary.
MACRO (cmp_IDE_SOURCE_PROPERTIES SOURCE_PATH HEADERS SOURCES)
    STRING(REPLACE "/" "\\\\" source_group_path ${SOURCE_PATH}  )
    source_group(${source_group_path} FILES ${HEADERS} ${SOURCES})
ENDMACRO (cmp_IDE_SOURCE_PROPERTIES NAME HEADERS SOURCES INSTALL_FILES)

# Add stricter warnings to my own code
IF(MSVC)
    ADD_COMPILE_OPTIONS(/W4 /WX /wd4996 /wd4201 /we4003)
ELSEIF(UNIX)
    # TODO: -Wall?
ENDIF()

# spokkle
SET(SPOKKLE_SOURCES
    src/spokkle/spokkle.cpp
    src/spokk/spokk_vertex.cpp
    ${JSON_H_DIR}/json.c
)
SET(SPOKKLE_HEADERS
    ${JSON_H_DIR}/json.h
)
cmp_IDE_SOURCE_PROPERTIES( "\\\\" "${SPOKKLE_HEADERS}" "${SPOKKLE_SOURCES}")
ADD_EXECUTABLE(spokkle ${SPOKKLE_SOURCES} ${SPOKKLE_HEADERS})
TARGET_LINK_LIBRARIES(spokkle assimp shaderc)
TARGET_INCLUDE_DIRECTORIES(spokkle PRIVATE
    "third_party/assimp/include"
    "src/spokk"
    ${JSON_H_DIR}
    )
IF(MSVC)
    # Windows puts some path-manipulation APIs in an optional library
    TARGET_LINK_LIBRARIES(spokkle shlwapi)
ENDIF(MSVC)

# libspokk
SET(SPOKK_HEADERS
    src/spokk/image_file.h
    src/spokk/spokk.h
    src/spokk/spokk_application.h
    src/spokk/spokk_buffer.h
    src/spokk/spokk_context.h
    src/spokk/spokk_debug.h
    src/spokk/spokk_image.h
    src/spokk/spokk_memory.h
    src/spokk/spokk_mesh.h
    src/spokk/spokk_pipeline.h
    src/spokk/spokk_platform.h
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
    src/spokk/spokk_context.cpp
    src/spokk/spokk_image.cpp
    src/spokk/spokk_memory.cpp
    src/spokk/spokk_mesh.cpp
    src/spokk/spokk_pipeline.cpp
    src/spokk/spokk_renderpass.cpp
    src/spokk/spokk_shader.cpp
    src/spokk/spokk_utilities.cpp
    src/spokk/spokk_vertex.cpp
)
SET(SPOKK_LIB_NAME spokk)
cmp_IDE_SOURCE_PROPERTIES( "\\\\" "${SPOKK_HEADERS}" "${SPOKK_SOURCES}")
ADD_LIBRARY(${SPOKK_LIB_NAME} STATIC ${SPOKK_SOURCES} ${SPOKK_HEADERS})
TARGET_LINK_LIBRARIES(${SPOKK_LIB_NAME} glfw ${GLFW_LIBRARIES} spirv-cross-cpp ${libs})
TARGET_INCLUDE_DIRECTORIES(${SPOKK_LIB_NAME} PUBLIC "third_party/stb")
TARGET_INCLUDE_DIRECTORIES(${SPOKK_LIB_NAME} PUBLIC "src/spokk")

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
ENDMACRO(SPOKK_ADD_SOURCES APP_TARGET SOURCE_LIST)

# usage: SPOKK_ADD_HEADERS(target [header1 header2 header3 ...])
MACRO(SPOKK_ADD_HEADERS APP_TARGET)
    LIST(APPEND ${APP_TARGET}_HEADERS ${ARGN})
ENDMACRO(SPOKK_ADD_HEADERS APP_TARGET HEADER_LIST)

MACRO(SPOKK_ADD_SHADER APP_TARGET SOURCE_FILE SPV_FILE SHADER_STAGE ENTRY_POINT)
    SOURCE_GROUP(shaders FILES ${SOURCE_FILE})
    # NOTE: we use glslc from the VULKAN_SDK here, not one we're building ourselves.
    # cause we skip that project anyway.
    ADD_CUSTOM_COMMAND(
        OUTPUT ${SPV_FILE}
        COMMAND $ENV{VULKAN_SDK}/bin/glslc
            -I ${CMAKE_SOURCE_DIR}/src/spokk
            -I ${CMAKE_SOURCE_DIR}/samples
            -o ${CMAKE_BINARY_DIR}/${SPV_FILE}
            -fentry-point=${ENTRY_POINT}
            -fshader-stage=${SHADER_STAGE}
            ${SOURCE_FILE}
        MAIN_DEPENDENCY ${SOURCE_FILE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Compiling ${SOURCE_FILE} -> ${SPV_FILE}"
    )
    LIST(APPEND ${APP_TARGET}_SHADERS ${SOURCE_FILE})
ENDMACRO(SPOKK_ADD_SHADER APP_TARGET SOURCE_FILE SPV_FILE SHADER_STAGE ENTRY_POINT)

# Adds a spokk-based executable target, onces its sources/headers/shaders have been defined.
FUNCTION (SPOKK_ADD_EXECUTABLE APP_TARGET)
    cmp_IDE_SOURCE_PROPERTIES( "\\\\" "${${APP_TARGET}_HEADERS}" "${${APP_TARGET}_SOURCES}")
    ADD_EXECUTABLE(${APP_TARGET} ${${APP_TARGET}_SOURCES} ${${APP_TARGET}_SHADERS} ${${APP_TARGET}_HEADERS})
    TARGET_LINK_LIBRARIES(${APP_TARGET} ${SPOKK_LIB_NAME})
    TARGET_INCLUDE_DIRECTORIES(${APP_TARGET} PUBLIC "samples")
    SET_TARGET_PROPERTIES(${APP_TARGET} PROPERTIES FOLDER "samples")

    ADD_DEPENDENCIES(${APP_TARGET} build-assets)
    mathfu_configure_flags(${APP_TARGET})
ENDFUNCTION(SPOKK_ADD_EXECUTABLE APP_TARGET)

# spokk-cubeswarm
SPOKK_ADD_SOURCES(spokk-cubeswarm
    samples/common/camera.cpp
    samples/cubeswarm/cubeswarm.cpp
)
SPOKK_ADD_HEADERS(spokk-cubeswarm
    samples/common/camera.h
)
SPOKK_ADD_SHADER(spokk-cubeswarm samples/cubeswarm/rigid_mesh.vert rigid_mesh.vert.spv vert main)
SPOKK_ADD_SHADER(spokk-cubeswarm samples/cubeswarm/rigid_mesh.frag rigid_mesh.frag.spv frag main)
SPOKK_ADD_EXECUTABLE(spokk-cubeswarm)

# spokk-compute
SPOKK_ADD_SOURCES(spokk-compute samples/compute/compute.cpp)
SPOKK_ADD_SHADER(spokk-compute samples/compute/double_ints.comp double_ints.comp.spv comp main)
SPOKK_ADD_EXECUTABLE(spokk-compute)

# spokk-lights
SPOKK_ADD_SOURCES(spokk-lights
    samples/common/camera.cpp
    samples/lights/lights.cpp
)
SPOKK_ADD_HEADERS(spokk-lights
    samples/common/camera.h
)
SPOKK_ADD_SHADER(spokk-lights samples/lights/lit_mesh.vert lit_mesh.vert.spv vert main)
SPOKK_ADD_SHADER(spokk-lights samples/lights/lit_mesh.frag lit_mesh.frag.spv frag main)
SPOKK_ADD_SHADER(spokk-lights samples/lights/skybox.vert skybox.vert.spv vert main)
SPOKK_ADD_SHADER(spokk-lights samples/lights/skybox.frag skybox.frag.spv frag main)
SPOKK_ADD_EXECUTABLE(spokk-lights)

# spokk-pillars
SPOKK_ADD_SOURCES(spokk-pillars
    samples/common/camera.cpp
    samples/pillars/pillars.cpp
)
SPOKK_ADD_HEADERS(spokk-pillars
    samples/common/camera.h
)
SPOKK_ADD_SHADER(spokk-pillars samples/pillars/pillar.vert pillar.vert.spv vert main)
SPOKK_ADD_SHADER(spokk-pillars samples/pillars/pillar.frag pillar.frag.spv frag main)
SPOKK_ADD_EXECUTABLE(spokk-pillars)

# spokk-shadertoy
SPOKK_ADD_SOURCES(spokk-shadertoy
    samples/shadertoy/shadertoy.cpp
    samples/common/shader_compiler.cpp
)
SPOKK_ADD_HEADERS(spokk-shadertoy
    samples/shadertoy/shadertoy.frag # this shader is special; it's loaded at runtime, not built offline.
    samples/common/shader_compiler.h
)
SPOKK_ADD_SHADER(spokk-shadertoy samples/shadertoy/fullscreen.vert fullscreen.vert.spv vert main)
SPOKK_ADD_EXECUTABLE(spokk-shadertoy)
TARGET_LINK_LIBRARIES(spokk-shadertoy shaderc)
