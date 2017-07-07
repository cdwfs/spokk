cmake_minimum_required(VERSION 2.8)

project(spokk)

# Global build settings
# TODO(cort): Disable /W4 for third_party projects
IF(MSVC)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GS- /GR- /W4 /wd4996 /wd4201 /we4003 /D _UNICODE /D UNICODE /D NOMINMAX /D _HAS_EXCEPTIONS=0" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob1 /Oi /GL /fp:fast /D NDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "/MDd /MP /Od /Ob0 /GS- /Zi /D _DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /GS- /GR- /W4 /wd4996 /wd4201 /we4003 /D _UNICODE /D UNICODE /D NOMINMAX /D _HAS_EXCEPTIONS=0" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "/MD /O2 /Ob1 /Oi /GL /fp:fast /D NDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "/MDd /MP /Od /Ob0 /GS- /Zi /D _DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" CACHE TYPE STRING FORCE)
    SET(libs)
ELSEIF(APPLE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -msse4.1 -std=c++11" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -msse4.1 -std=c99" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG" CACHE TYPE STRING FORCE)
    SET(libs m)
ELSEIF(UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -msse4.1 -D_POSIX_C_SOURCE=199309L -DM_PI=3.141592653 -std=c++11" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -msse4.1 -D_POSIX_C_SOURCE=199309L -DM_PI=3.141592653 -std=c99" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -pthread" CACHE TYPE STRING FORCE)
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
OPTION(ASSIMP_BUILD_BLEND_IMPORTER "build the BLEND importer" ON)
OPTION(ASSIMP_BUILD_FBX_IMPORTER "build the FBX importer" ON)
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
# NOTE: if this option is disabled, the shadertoy sample won't run; it uses shaderc for runtime shader compilation.
OPTION(SPOKK_ENABLE_SHADERC "Use shaderc for runtime shader compilation" OFF)
IF(${SPOKK_ENABLE_SHADERC})
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
    add_definitions(-DSPOKK_ENABLE_SHADERC)
ENDIF()

# Skip executable targets; we only care about the library anyway.
FOREACH(SHADER_TARGET IN LISTS SPIRV_CROSS_TARGETS GLSLANG_TARGETS SPIRV_TOOLS_TARGETS SHADERC_TARGETS)
    get_target_property(TARGET_TYPE ${SHADER_TARGET} TYPE)
    IF(${TARGET_TYPE} STREQUAL EXECUTABLE)
        set_target_properties(${SHADER_TARGET} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    ENDIF()
ENDFOREACH()

# Offline shader compilation from GLSL to SPIR-V
# NOTE: we use glslc from the VULKAN_SDK here, not one we're building ourselves.
# cause we skip that project anyway.
# TODO(https://github.com/cdwfs/spokk/issues/21): Support specifying a shader stage and entry point
FUNCTION(SHADER_BUILD_RULE SOURCE_FILE SPV_FILE)
    ADD_CUSTOM_COMMAND(
        OUTPUT ${SPV_FILE}
        COMMAND $ENV{VULKAN_SDK}/bin/glslc
            -I ${CMAKE_SOURCE_DIR}/src/spokk -I ${CMAKE_SOURCE_DIR}/samples -o ${SPV_FILE} ${SOURCE_FILE}
        MAIN_DEPENDENCY ${SOURCE_FILE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Compiling ${SOURCE_FILE} -> ${SPV_FILE}"
    )
ENDFUNCTION(SHADER_BUILD_RULE SHADER_FILE SPV_FILE)

# Mysterious macro to remove default VS "Source Files" and "Header Files" filters.
# TODO(https://github.com/cdwfs/spokk/issues/10): Figure out why this works and refactor if necessary.
MACRO (cmp_IDE_SOURCE_PROPERTIES SOURCE_PATH HEADERS SOURCES)
    STRING(REPLACE "/" "\\\\" source_group_path ${SOURCE_PATH}  )
    source_group(${source_group_path} FILES ${HEADERS} ${SOURCES})
ENDMACRO (cmp_IDE_SOURCE_PROPERTIES NAME HEADERS SOURCES INSTALL_FILES)

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
TARGET_LINK_LIBRARIES(spokkle assimp)
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
IF(${SPOKK_ENABLE_SHADERC})
    TARGET_LINK_LIBRARIES(${SPOKK_LIB_NAME} shaderc)
ENDIF()
TARGET_INCLUDE_DIRECTORIES(${SPOKK_LIB_NAME} PUBLIC "third_party/stb")
TARGET_INCLUDE_DIRECTORIES(${SPOKK_LIB_NAME} PUBLIC "src/spokk")

# custom target to build/copy sample assets
ADD_CUSTOM_TARGET(build-assets
    COMMAND $<TARGET_FILE:spokkle> samples/assets/assets.json5
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building assets from ${CMAKE_SOURCE_DIR}/samples/assets/assets.json5"
)
SET_TARGET_PROPERTIES(build-assets PROPERTIES FOLDER "samples")
ADD_DEPENDENCIES(build-assets spokkle)

FUNCTION (SPOKK_APP APP_NAME APP_SOURCES_LIST APP_HEADERS_LIST APP_SHADERS_LIST)
    SET(APP_HEADERS ${${APP_HEADERS_LIST}})
    SET(APP_SOURCES ${${APP_SOURCES_LIST}})
    SET(APP_SHADERS ${${APP_SHADERS_LIST}})
    SOURCE_GROUP(shaders FILES ${APP_SHADERS})
    FOREACH(SHADER_FILE IN LISTS APP_SHADERS)
        SET(SPV_FILENAME ${CMAKE_BINARY_DIR}/${SHADER_FILE}.spv)
        STRING(REGEX REPLACE ".hlsl.spv$" ".spv" SPV_FILENAME ${SPV_FILENAME})
        SHADER_BUILD_RULE(${SHADER_FILE} ${SPV_FILENAME})
    ENDFOREACH(SHADER_FILE)

    cmp_IDE_SOURCE_PROPERTIES( "\\\\" "${APP_HEADERS}" "${APP_SOURCES}")
    ADD_EXECUTABLE(${APP_NAME} ${APP_SOURCES} ${APP_SHADERS} ${APP_HEADERS})
    TARGET_LINK_LIBRARIES(${APP_NAME} ${SPOKK_LIB_NAME})
    TARGET_INCLUDE_DIRECTORIES(${APP_NAME} PUBLIC "samples")
    SET_TARGET_PROPERTIES(${APP_NAME} PROPERTIES FOLDER "samples")

    ADD_DEPENDENCIES(${APP_NAME} build-assets)
    mathfu_configure_flags(${APP_NAME})
ENDFUNCTION(SPOKK_APP APP_NAME APP_SOURCES APP_HEADERS APP_SHADERS)

# spokk-cubeswarm
SET(CUBESWARM_SOURCES
    samples/common/camera.cpp
    samples/cubeswarm/cubeswarm.cpp
)
SET(CUBESWARM_HEADERS
    samples/common/camera.h
)
SET(CUBESWARM_SHADERS
    samples/cubeswarm/rigid_mesh.vert
    samples/cubeswarm/rigid_mesh.frag)
SPOKK_APP(spokk-cubeswarm CUBESWARM_SOURCES CUBESWARM_HEADERS CUBESWARM_SHADERS)

# spokk-compute
SET(COMPUTE_SOURCES samples/compute/compute.cpp)
SET(COMPUTE_HEADERS)
SET(COMPUTE_SHADERS samples/compute/double_ints.comp)
SPOKK_APP(spokk-compute COMPUTE_SOURCES COMPUTE_HEADERS COMPUTE_SHADERS)

# spokk-lights
SET(LIGHTS_SOURCES
    samples/common/camera.cpp
    samples/lights/lights.cpp
)
SET(LIGHTS_HEADERS
    samples/common/camera.h
)
SET(LIGHTS_SHADERS
    samples/lights/lit_mesh.vert
    samples/lights/lit_mesh.frag
    samples/lights/skybox.vert
    samples/lights/skybox.frag
)
SPOKK_APP(spokk-lights LIGHTS_SOURCES LIGHTS_HEADERS LIGHTS_SHADERS)

# spokk-pillars
SET(PILLARS_SOURCES
    samples/common/camera.cpp
    samples/pillars/pillars.cpp
)
SET(PILLARS_HEADERS
    samples/common/camera.h
)
SET(PILLARS_SHADERS
    samples/pillars/pillar.vert
    samples/pillars/pillar.frag
)
SPOKK_APP(spokk-pillars PILLARS_SOURCES PILLARS_HEADERS PILLARS_SHADERS)

# spokk-shadertoy
SET(SHADERTOY_SOURCES
    samples/shadertoy/shadertoy.cpp
    samples/common/shader_compiler.cpp
)
SET(SHADERTOY_HEADERS
    samples/shadertoy/shadertoy.frag # this shader is special; it's loaded at runtime, not built offline.
    samples/common/shader_compiler.h
)
SET(SHADERTOY_SHADERS
    samples/shadertoy/fullscreen.vert
)
SPOKK_APP(spokk-shadertoy SHADERTOY_SOURCES SHADERTOY_HEADERS SHADERTOY_SHADERS)
IF(${SPOKK_ENABLE_SHADERC})
    TARGET_LINK_LIBRARIES(spokk-shadertoy shaderc)
ENDIF()
