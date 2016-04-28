cmake_minimum_required(VERSION 2.8)

# Remove MinSizeRel and RelWithDebInfo targets
set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE TYPE INTERNAL FORCE )

# Build GLFW from source & link appropriate library
option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)
add_subdirectory(glfw) # specify binary directory for out-of-tree glfw
include_directories(glfw/include)

project(vulkswagen)

SET(headers
  platform.h
  stb_image.h
  )

#link_directories(${VK_SDK_PATH}/lib)

IF(MSVC)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /GS- /GR- /W4 /wd4996 /D _UNICODE /D UNICODE" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "/MT /O2 /Ob1 /Oi /GL /fp:fast /D NDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "/MTd /Od /Ob0 /GS- /Zi /D _DEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" CACHE TYPE STRING FORCE)
    SET(libs ${VULKAN_LIBRARY})
ELSEIF(APPLE)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -msse4.1 -std=c99" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DNDEBUG" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG" CACHE TYPE STRING FORCE)
    SET(libs m ${VULKAN_LIBRARY})
ELSEIF(UNIX)
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
set(sources vulkswagen.c)
cmp_IDE_SOURCE_PROPERTIES( "\\\\" "${headers}" "${sources}")
add_executable(${exe_name} ${sources} ${headers})
target_link_libraries(${exe_name} glfw ${GLFW_LIBRARIES} ${libs})

