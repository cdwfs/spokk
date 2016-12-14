cmake_minimum_required(VERSION 2.8)

project(spokk)

# Remove MinSizeRel and RelWithDebInfo targets
set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE )

# SPIRV-Cross
add_subdirectory(third_party/SPIRV-Cross)
set(SPIRV_CROSS_TARGETS
	spirv-cross-core
	spirv-cross-glsl
	spirv-cross-cpp
	spirv-cross-msl
	spirv-cross
	)
set_target_properties(${SPIRV_CROSS_TARGETS} PROPERTIES FOLDER "SPIRV-Cross")
# Skip executable targets; we only care about the library anyway.
FOREACH(SHADER_TARGET IN LISTS SPIRV_CROSS_TARGETS)
    get_target_property(TARGET_TYPE ${SHADER_TARGET} TYPE)
	IF(${TARGET_TYPE} STREQUAL EXECUTABLE)
		set_target_properties(${SHADER_TARGET} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
	ENDIF()
ENDFOREACH()

# GLFW
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
option(USE_MSVC_RUNTIME_LIBRARY_DLL "Use MSVC runtime library DLL" OFF)
add_subdirectory(third_party/glfw)

# Mathfu
set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
set(mathfu_enable_simd OFF CACHE BOOL "")
add_subdirectory(third_party/mathfu)

# Offline shader compilation from GLSL to SPIR-V
FUNCTION(SHADER_BUILD_RULE SOURCE_FILE SPV_FILE)
    ADD_CUSTOM_COMMAND(
        OUTPUT ${SPV_FILE}
        COMMAND $ENV{VULKAN_SDK}/bin/glslangValidator -V -o ${SPV_FILE} ${SOURCE_FILE}
        MAIN_DEPENDENCY ${SOURCE_FILE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Compiling ${SOURCE_FILE} -> ${SPV_FILE}"
        )
ENDFUNCTION(SHADER_BUILD_RULE SHADER_FILE SPV_FILE)

# Global build settings
include_directories(${VULKAN_INCLUDE_DIR})
#link_directories(${VULKAN_SDK}/lib)
IF(MSVC)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GS- /GR- /W4 /wd4996 /D _UNICODE /D UNICODE" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /Ob1 /Oi /GL /fp:fast /D NDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "/MTd /Od /Ob0 /GS- /Zi /D _DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /GS- /GR- /W4 /wd4996 /D _UNICODE /D UNICODE" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "/MT /O2 /Ob1 /Oi /GL /fp:fast /D NDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "/MTd /Od /Ob0 /GS- /Zi /D _DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" CACHE TYPE STRING FORCE)
    SET(libs ${VULKAN_LIBRARY})
ELSEIF(APPLE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -msse4.1 -std=c++11" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -msse4.1 -std=c99" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG" CACHE TYPE STRING FORCE)
    SET(libs m ${VULKAN_LIBRARY})
ELSEIF(UNIX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -msse4.1 -D_POSIX_C_SOURCE=199309L -DM_PI=3.141592653 -std=c++11" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -msse4.1 -D_POSIX_C_SOURCE=199309L -DM_PI=3.141592653 -std=c99" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -pthread" CACHE TYPE STRING FORCE)
    SET(libs m ${VULKAN_LIBRARY})
ENDIF()

# Mysterious macro to remove default VS "Source Files" and "Header Files" filters.
MACRO (cmp_IDE_SOURCE_PROPERTIES SOURCE_PATH HEADERS SOURCES)
    STRING(REPLACE "/" "\\\\" source_group_path ${SOURCE_PATH}  )
    source_group(${source_group_path} FILES ${HEADERS} ${SOURCES})
ENDMACRO (cmp_IDE_SOURCE_PROPERTIES NAME HEADERS SOURCES INSTALL_FILES)

# libspokk
SET(SPOKK_HEADERS
    image_file.h
    platform.h
    stb_image.h
    vk_application.h
    vk_debug.h
    vk_init.h
    vk_texture.h
    )
SET(SPOKK_SOURCES
    image_file.c
    vk_application.cpp
    vk_init.cpp
    vk_texture.cpp
    )
SET(SPOKK_LIB_NAME spokk)
cmp_IDE_SOURCE_PROPERTIES( "\\\\" "${SPOKK_HEADERS}" "${SPOKK_SOURCES}")
ADD_LIBRARY(${SPOKK_LIB_NAME} STATIC ${SPOKK_SOURCES} ${SPOKK_HEADERS})
TARGET_LINK_LIBRARIES(${SPOKK_LIB_NAME} glfw ${GLFW_LIBRARIES} spirv-cross-cpp ${libs})
TARGET_INCLUDE_DIRECTORIES(${SPOKK_LIB_NAME} PUBLIC ".")

FUNCTION (SPOKK_APP APP_NAME APP_SOURCES_LIST APP_HEADERS_LIST APP_SHADERS_LIST)
    # TODO(cort): ...really?
    SET(APP_HEADERS ${${APP_HEADERS_LIST}})
    SET(APP_SOURCES ${${APP_SOURCES_LIST}})
    SET(APP_SHADERS ${${APP_SHADERS_LIST}})
    SOURCE_GROUP(shaders FILES ${APP_SHADERS})
    FOREACH(SHADER_FILE IN LISTS APP_SHADERS)
        SHADER_BUILD_RULE(${SHADER_FILE} ${CMAKE_BINARY_DIR}/${SHADER_FILE}.spv)
    ENDFOREACH(SHADER_FILE)

    cmp_IDE_SOURCE_PROPERTIES( "\\\\" "${APP_HEADERS}" "${APP_SOURCES}")
    add_executable(${APP_NAME} ${APP_SOURCES} ${APP_SHADERS} ${APP_HEADERS})
    target_link_libraries(${APP_NAME} ${SPOKK_LIB_NAME})
    mathfu_configure_flags(${APP_NAME})
ENDFUNCTION(SPOKK_APP APP_NAME APP_SOURCES APP_HEADERS APP_SHADERS)

# spokk-cubeswarm sample
SET(CUBESWARM_SOURCES
    camera.cpp
    cubeswarm.cpp
    )
SET(CUBESWARM_HEADERS
    camera.h
    cds_mesh.h
    )
SET(CUBESWARM_SHADERS
    tri.vert
    tri.frag
    fullscreen.vert
    subpass_post.frag
    )
SPOKK_APP(spokk-cubeswarm CUBESWARM_SOURCES CUBESWARM_HEADERS CUBESWARM_SHADERS)


# Copy assets directory into build dir.
# TODO(cort): Which target should this be attached to? Or hey, what if every sample knows what assets
#             it uses, and copies them in?
ADD_CUSTOM_COMMAND(
    TARGET spokk-cubeswarm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/trevor
        ${CMAKE_BINARY_DIR}/trevor
    COMMENT "Copying assets to build directory"
    )
