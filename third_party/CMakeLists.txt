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
set(PROCESS_H_DIR ${CMAKE_CURRENT_BINARY_DIR}/process.h/include)
# Copy the process.h header into the build directory with a new name
# (workaround for https://github.com/sheredom/process.h/issues/4)
file(MAKE_DIRECTORY ${PROCESS_H_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/process.h/process.h DESTINATION ${PROCESS_H_DIR})
file(RENAME ${PROCESS_H_DIR}/process.h ${PROCESS_H_DIR}/nph_process.h)
set(PROCESS_H_DIR ${PROCESS_H_DIR} PARENT_SCOPE)

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

# shaderc
SET(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spirv-headers" CACHE PATH "Location of SPIRV-Headers project" FORCE)
SET(SHADERC_THIRD_PARTY_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Root location of shaderc third-party projets" FORCE)
OPTION(ENABLE_AMD_EXTENSIONS "Enable AMD shader extensions" OFF)
OPTION(BUILD_TESTING "Skip building shaderc tests" OFF)
OPTION(ENABLE_SPVREMAPPER "Enable building SPVRemapper tool" OFF)
OPTION(SHADERC_SKIP_TESTS "Skip building tests" ON)
OPTION(SHADERC_ENABLE_SHARED_CRT "Use the shared CRT instead of the static CRT" ON)
OPTION(SHADERC_SKIP_INSTALL "Skip shaderc installation" ON)
OPTION(SKIP_GLSLANG_INSTALL "Skip glslang installation" ON)
OPTION(SKIP_SPIRV_TOOLS_INSTALL "Skip SPIRV-Tools installation" ON)
OPTION(SPIRV_CHECK_CONTEXT "Enable expensive SPIRV IR tests" OFF)
OPTION(SPIRV_SKIP_EXECUTABLES "Skip building SPIRV-Tools executables" ON)
ADD_SUBDIRECTORY(shaderc)
# shaderc pulls in a gajillion targets. Let's organize them a bit.
SET(GLSLANG_TARGETS
    glslang
    glslang-default-resource-limits
    glslangValidator
    HLSL
    OGLCompiler
    OSDependent
    SPIRV
    spirv-remap
)
SET(SPIRV_HEADERS_TARGETS
    install-headers
)
SET(SPIRV_TOOLS_TARGETS
    core_tables
    enum_string_mapping
    extinst_tables
    SPIRV-Tools
    SPIRV-Tools-link
    spirv-tools-build-version
    spirv-tools-header-DebugInfo
    spirv-tools-pkg-config
    spirv-tools-shared-pkg-config
    spirv-tools-vimsyntax
    SPIRV-Tools-opt
    SPIRV-Tools-reduce
    SPIRV-Tools-shared
    spv-tools-debuginfo
    spv-tools-spv-amd-gs
    spv-tools-spv-amd-sb
    spv-tools-spv-amd-sevp
    spv-tools-spv-amd-stm
)
SET(SHADERC_TARGETS
    add-copyright
    build-version
    check-copyright
    glslc
    glslc_exe
    shaderc
    shaderc-pkg-config
    shaderc_combined_genfile
    shaderc_combined-pkg-config
    shaderc_shared
    shaderc_static-pkg-config
    shaderc_util
    shaderc-online-compile
    testdata
)
# This target is only created if asciidoctor happens to be installed. If so, it should
# be grouped with the shaderc targets as well.
IF(TARGET glslc_doc_README)
    LIST(APPEND SHADERC_TARGETS glslc_doc_README)
ENDIF(TARGET glslc_doc_README)
SET_TARGET_PROPERTIES(${GLSLANG_TARGETS} PROPERTIES FOLDER "shaderc/glslang")
SET_TARGET_PROPERTIES(${SPIRV_HEADERS_TARGETS} PROPERTIES FOLDER "shaderc/spirv-headers")
SET_TARGET_PROPERTIES(${SPIRV_TOOLS_TARGETS} PROPERTIES FOLDER "shaderc/spirv-tools")
SET_TARGET_PROPERTIES(${SHADERC_TARGETS} PROPERTIES FOLDER "shaderc")

# Skip executable targets; we only care about the library anyway.
FOREACH(SHADERC_TARGET IN LISTS
        GLSLANG_TARGETS
        SPIRV_HEADERS_TARGETS
        SPIRV_TOOLS_TARGETS
        SHADERC_TARGETS)
    GET_TARGET_PROPERTY(TARGET_TYPE ${SHADERC_TARGET} TYPE)
    IF(${TARGET_TYPE} STREQUAL EXECUTABLE)
        SET_TARGET_PROPERTIES(${SHADERC_TARGET} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    ENDIF()
ENDFOREACH()

# VulkanMemoryAllocator
set(VULKAN_MEMORY_ALLOCATOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/src PARENT_SCOPE)
