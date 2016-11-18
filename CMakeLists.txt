cmake_minimum_required(VERSION 2.8)

# Remove MinSizeRel and RelWithDebInfo targets
set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE )

# Build GLFW from source & link appropriate library
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
option(USE_MSVC_RUNTIME_LIBRARY_DLL "Use MSVC runtime library DLL" OFF)
add_subdirectory(third_party/glfw) # specify binary directory for out-of-tree glfw

project(vulkswagen)

SET(headers
    camera.h
    cds_mesh.h
    cds_vulkan.hpp
    image_file.h
    platform.h
    stb_image.h
    vk_texture.h
    )
SET(sources
    camera.cpp
    image_file.c
    vk_texture.cpp
    vulkswagen.cpp
    )

SET(shaders
    tri.vert
    tri.frag
    fullscreen.vert
    subpass_post.frag
    )

FUNCTION(SHADER_BUILD_RULE SOURCE_FILE SPV_FILE)
    ADD_CUSTOM_COMMAND(
        OUTPUT ${SPV_FILE}
        COMMAND $ENV{VULKAN_SDK}/bin/glslangValidator -V -o ${SPV_FILE} ${SOURCE_FILE}
        MAIN_DEPENDENCY ${SOURCE_FILE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Compiling ${SOURCE_FILE} -> ${SPV_FILE}"
        )
ENDFUNCTION(SHADER_BUILD_RULE SHADER_FILE SPV_FILE)
FOREACH(SHADER_FILE IN LISTS shaders)
    SHADER_BUILD_RULE(${SHADER_FILE} ${CMAKE_BINARY_DIR}/${SHADER_FILE}.spv)
ENDFOREACH(SHADER_FILE)
SOURCE_GROUP(shaders FILES ${shaders})

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

set(exe_name vulkswagen)
cmp_IDE_SOURCE_PROPERTIES( "\\\\" "${headers}" "${sources}")
add_executable(${exe_name} ${sources} ${shaders} ${headers})
target_link_libraries(${exe_name} glfw ${GLFW_LIBRARIES} ${libs})

set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
set(mathfu_enable_simd OFF CACHE BOOL "")
add_subdirectory(third_party/mathfu)
mathfu_configure_flags(vulkswagen)

ADD_CUSTOM_COMMAND(
    TARGET ${exe_name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/trevor
        ${CMAKE_BINARY_DIR}/trevor
    COMMENT "Copying assets to build directory"
    )

