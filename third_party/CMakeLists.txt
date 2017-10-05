# Suppress all warnings from external projects.
IF(${MSVC})
    SET_PROPERTY(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS /W0)
    SET(CMAKE_CXX_FLAGS_RELEASE "/MD" CACHE TYPE STRING FORCE)
    SET(CMAKE_CXX_FLAGS_DEBUG   "/MDd" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_RELEASE "/MD" CACHE TYPE STRING FORCE)
    SET(CMAKE_C_FLAGS_DEBUG   "/MDd" CACHE TYPE STRING FORCE)
ELSEIF(${UNIX})
    SET_PROPERTY(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS -w)
ENDIF()

# GLFW
OPTION(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
OPTION(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
OPTION(GLFW_BUILD_DOCS "Build the GLFW documentation" OFF)
OPTION(GLFW_INSTALL "Generate installation target" OFF)
OPTION(USE_MSVC_RUNTIME_LIBRARY_DLL "Use MSVC runtime library DLL" ON)
ADD_SUBDIRECTORY(glfw)

# GLM
# Nothing to do here if not building self-tests

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
        zlibstatic
    )
ENDIF(${MSVC})
SET_TARGET_PROPERTIES(${ASSIMP_TARGETS} PROPERTIES FOLDER "assimp")

# json.h
set(JSON_H_DIR ${CMAKE_CURRENT_SOURCE_DIR}/json.h PARENT_SCOPE)

# SPIRV-Cross
ADD_SUBDIRECTORY(SPIRV-Cross)
SET(SPIRV_CROSS_TARGETS
    spirv-cross-core
    spirv-cross-glsl
    spirv-cross-hlsl
    spirv-cross-cpp
    spirv-cross-msl
    spirv-cross
)
SET_TARGET_PROPERTIES(${SPIRV_CROSS_TARGETS} PROPERTIES FOLDER "SPIRV-Cross")

# shaderc
SET(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/spirv-headers" CACHE PATH "Location of SPIRV-Headers project" FORCE)
SET(SHADERC_THIRD_PARTY_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Root location of shaderc third-party projets" FORCE)
OPTION(SHADERC_SKIP_TESTS "Skip building tests" ON)
OPTION(SHADERC_ENABLE_SHARED_CRT "Use the shared CRT instead of the static CRT" ON)
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
    spirv-markv
    spirv-opt
    spirv-stats
    SPIRV-Tools
    SPIRV-Tools-comp
    spirv-tools-build-version
    spirv-tools-cpp-example
    spirv-tools-spv-amd-gcn-shader
    spirv-tools-spv-amd-shader-ballot
    spirv-tools-spv-amd-shader-explicit-vertex-parameter
    spirv-tools-spv-amd-shader-trinary-minmax
    spirv-tools-vimsyntax
    SPIRV-Tools-opt
    spirv-val
)
SET(SHADERC_TARGETS
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
SET_TARGET_PROPERTIES(${GLSLANG_TARGETS} PROPERTIES FOLDER "shaderc/glslang")
SET_TARGET_PROPERTIES(${SPIRV_TOOLS_TARGETS} PROPERTIES FOLDER "shaderc/spirv-tools")
SET_TARGET_PROPERTIES(${SHADERC_TARGETS} PROPERTIES FOLDER "shaderc")

# Skip executable targets; we only care about the library anyway.
FOREACH(SHADERC_TARGET IN LISTS SPIRV_CROSS_TARGETS GLSLANG_TARGETS SPIRV_TOOLS_TARGETS SHADERC_TARGETS)
    GET_TARGET_PROPERTY(TARGET_TYPE ${SHADERC_TARGET} TYPE)
    IF(${TARGET_TYPE} STREQUAL EXECUTABLE)
        SET_TARGET_PROPERTIES(${SHADERC_TARGET} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    ENDIF()
ENDFOREACH()
