# Suppress all warnings from external projects.
IF(${MSVC})
    SET_PROPERTY(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS /W0)
    SET(CMAKE_CXX_FLAGS_RELEASE "/MP /MD" CACHE STRING "C++ compiler options (release builds)" FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "/MP /MDd" CACHE STRING "C++ compiler options (debug builds)" FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "/MP /MD" CACHE STRING "C compiler options (release builds)" FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "/MP /MDd" CACHE STRING "C compiler options (debug builds)" FORCE)
ELSEIF(${UNIX})
    SET_PROPERTY(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS -w)
ENDIF()

# Vulkan
find_package(Vulkan REQUIRED)
# The case of these variables changed at some point...define the all-caps variants for compatibility
SET(VULKAN_FOUND Vulkan_FOUND)
SET(VULKAN_INCLUDE_DIR Vulkan_INCLUDE_DIR)
SET(VULKAN_LIBRARY Vulkan_LIBRARY)

# GLFW
OPTION(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
OPTION(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
OPTION(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
OPTION(GLFW_INSTALL "Generate installation target" OFF)
OPTION(USE_MSVC_RUNTIME_LIBRARY_DLL "Use MSVC runtime library DLL" ON)
ADD_SUBDIRECTORY(glfw)

# GLM
set(GLM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glm PARENT_SCOPE)

# assimp
OPTION(ASSIMP_BUILD_ASSIMP_TOOLS "If the supplementary tools for Assimp are built in addition to the library." OFF)
OPTION(ASSIMP_BUILD_TESTS "If the test suite for Assimp is built in addition to the library." OFF)
OPTION(ASSIMP_NO_EXPORT "Disable Assimp's export functionality." ON)
OPTION(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT "default value of all ASSIMP_BUILD_XXX_IMPORTER value" FALSE)
#OPTION(ASSIMP_BUILD_BLEND_IMPORTER "build the BLEND importer" ON)
#OPTION(ASSIMP_BUILD_FBX_IMPORTER "build the FBX importer" ON)
OPTION(ASSIMP_BUILD_OBJ_IMPORTER "build the OBJ importer" ON)
ADD_SUBDIRECTORY(assimp)
SET(ASSIMP_TARGETS
    assimp
    uninstall
    IrrXML
)
IF(${MSVC})
    LIST(APPEND ASSIMP_TARGETS
        UpdateAssimpLibsDebugSymbolsAndDLLs
        zlib
        zlibstatic
    )
ENDIF(${MSVC})
SET_TARGET_PROPERTIES(${ASSIMP_TARGETS} PROPERTIES FOLDER "assimp")

# dear imgui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui PARENT_SCOPE)
SET(IMGUI_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.h
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_internal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imconfig.h
    PARENT_SCOPE
)

# json.h
set(JSON_H_DIR ${CMAKE_CURRENT_SOURCE_DIR}/json.h PARENT_SCOPE)

# process.h
set(PROCESS_H_DIR ${CMAKE_CURRENT_SOURCE_DIR}/process.h PARENT_SCOPE)

# simple_vulkan_synchronization
set(SIMPLE_VULKAN_SYNCHRONIZATION_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/simple_vulkan_synchronization
    PARENT_SCOPE
)

# SPIRV-Reflect
SET(SPIRV_REFLECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Reflect PARENT_SCOPE)
SET(SPIRV_REFLECT_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Reflect/spirv_reflect.c
    ${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Reflect/spirv_reflect.h
    PARENT_SCOPE
)

# stb
SET(STB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stb PARENT_SCOPE)

# Skip executable targets; we only care about the library anyway.
FOREACH(SHADERC_TARGET IN LISTS
        GLSLANG_TARGETS
        SPIRV_HEADERS_TARGETS
        SPIRV_TOOLS_TARGETS)
    GET_TARGET_PROPERTY(TARGET_TYPE ${SHADERC_TARGET} TYPE)
    IF(${TARGET_TYPE} STREQUAL EXECUTABLE)
        SET_TARGET_PROPERTIES(${SHADERC_TARGET} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    ENDIF()
ENDFOREACH()

# VulkanMemoryAllocator
set(VULKAN_MEMORY_ALLOCATOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator PARENT_SCOPE)
