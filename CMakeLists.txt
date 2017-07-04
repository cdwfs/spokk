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
OPTION(SPOKK_ENABLE_SHADERC "Use shaderc for runtime shader compilation" ON)
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
FUNCTION(SHADER_BUILD_RULE SOURCE_FILE SPV_FILE)
    ADD_CUSTOM_COMMAND(
        OUTPUT ${SPV_FILE}
        COMMAND $ENV{VULKAN_SDK}/bin/glslc -o ${SPV_FILE} ${SOURCE_FILE}
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
    spokkle.cpp
    vk_vertex.cpp
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
    ${JSON_H_DIR}
    )
IF(MSVC)
    TARGET_LINK_LIBRARIES(spokkle shlwapi)
ENDIF(MSVC)

# libspokk
SET(SPOKK_HEADERS
    image_file.h
    platform.h
    vk_application.h
    vk_buffer.h
    vk_context.h
    vk_debug.h
    vk_image.h
    vk_memory.h
    vk_mesh.h
    vk_pipeline.h
    vk_renderpass.h
    vk_shader.h
    vk_shader_interface.h
    vk_utilities.h
    vk_vertex.h
)
SET(SPOKK_SOURCES
    image_file.c
    vk_application.cpp
    vk_buffer.cpp
    vk_context.cpp
    vk_image.cpp
    vk_memory.cpp
    vk_mesh.cpp
    vk_pipeline.cpp
    vk_renderpass.cpp
    vk_shader.cpp
    vk_utilities.cpp
    vk_vertex.cpp
)
SET(SPOKK_LIB_NAME spokk)
cmp_IDE_SOURCE_PROPERTIES( "\\\\" "${SPOKK_HEADERS}" "${SPOKK_SOURCES}")
ADD_LIBRARY(${SPOKK_LIB_NAME} STATIC ${SPOKK_SOURCES} ${SPOKK_HEADERS})
TARGET_LINK_LIBRARIES(${SPOKK_LIB_NAME} glfw ${GLFW_LIBRARIES} spirv-cross-cpp ${libs})
IF(${SPOKK_ENABLE_SHADERC})
    TARGET_LINK_LIBRARIES(${SPOKK_LIB_NAME} shaderc)
ENDIF()
TARGET_INCLUDE_DIRECTORIES(${SPOKK_LIB_NAME} PUBLIC "third_party/stb")

# Build assets
ADD_CUSTOM_TARGET(spokk-assets
    COMMAND $<TARGET_FILE:spokkle> assets.json5
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Bulding assets from ${CMAKE_SOURCE_DIR}/assets.json5"
)
ADD_DEPENDENCIES(spokk-assets spokkle)

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
    add_executable(${APP_NAME} ${APP_SOURCES} ${APP_SHADERS} ${APP_HEADERS})
    target_link_libraries(${APP_NAME} ${SPOKK_LIB_NAME})
    add_dependencies(${APP_NAME} spokk-assets)
    mathfu_configure_flags(${APP_NAME})
ENDFUNCTION(SPOKK_APP APP_NAME APP_SOURCES APP_HEADERS APP_SHADERS)

# spokk-cubeswarm
SET(CUBESWARM_SOURCES camera.cpp cubeswarm.cpp)
SET(CUBESWARM_HEADERS camera.h)
SET(CUBESWARM_SHADERS rigid_mesh.vert rigid_mesh.frag)
SPOKK_APP(spokk-cubeswarm CUBESWARM_SOURCES CUBESWARM_HEADERS CUBESWARM_SHADERS)

# spokk-compute
SET(COMPUTE_SOURCES compute.cpp)
SET(COMPUTE_HEADERS)
SET(COMPUTE_SHADERS double_ints.comp)
SPOKK_APP(spokk-compute COMPUTE_SOURCES COMPUTE_HEADERS COMPUTE_SHADERS)

# spokk-lights
SET(LIGHTS_SOURCES camera.cpp lights.cpp)
SET(LIGHTS_HEADERS camera.h)
SET(LIGHTS_SHADERS lit_mesh.vert lit_mesh.frag skybox.vert skybox.frag)
SPOKK_APP(spokk-lights LIGHTS_SOURCES LIGHTS_HEADERS LIGHTS_SHADERS)

# spokk-pillars
SET(PILLARS_SOURCES camera.cpp pillars.cpp)
SET(PILLARS_HEADERS camera.h)
SET(PILLARS_SHADERS pillar.vert pillar.frag)
SPOKK_APP(spokk-pillars PILLARS_SOURCES PILLARS_HEADERS PILLARS_SHADERS)

# spokk-shadertoy
SET(SHADERTOY_SOURCES shadertoy.cpp)
SET(SHADERTOY_HEADERS shadertoy.frag) # this shader is special; it's loaded at runtime, not built offline.
SET(SHADERTOY_SHADERS fullscreen.vert)
SPOKK_APP(spokk-shadertoy SHADERTOY_SOURCES SHADERTOY_HEADERS SHADERTOY_SHADERS)