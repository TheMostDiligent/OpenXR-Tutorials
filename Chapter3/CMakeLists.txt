# Copyright 2023, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

# XR_DOCS_TAG_BEGIN_SetProjectName3
cmake_minimum_required(VERSION 3.22.1)
set(PROJECT_NAME OpenXRTutorialChapter3)
project("${PROJECT_NAME}")
# XR_DOCS_TAG_END_SetProjectName3

# XR_DOCS_TAG_BEGIN_CMakeModulePath
# Additional Directories for find_package() to search within.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
# XR_DOCS_TAG_END_CMakeModulePath
#include("../cmake/graphics_api_select.cmake")

# XR_DOCS_TAG_BEGIN_FetchContent
# For FetchContent_Declare() and FetchContent_MakeAvailable()
include(FetchContent)

# openxr_loader - From github.com/KhronosGroup
set(BUILD_TESTS
    OFF
    CACHE INTERNAL "Build tests"
)
set(BUILD_API_LAYERS
    ON
    CACHE INTERNAL "Use OpenXR layers"
)
FetchContent_Declare(
    OpenXR
    URL_HASH MD5=924a94a2da0b5ef8e82154c623d88644
    URL https://github.com/KhronosGroup/OpenXR-SDK-Source/archive/refs/tags/release-1.0.34.zip
        SOURCE_DIR
        openxr
)
FetchContent_MakeAvailable(OpenXR)
# XR_DOCS_TAG_END_FetchContent

# Files
set(SOURCES
    main.cpp
    ../Common/OpenXRDebugUtils.cpp
)
set(HEADERS
    ../Common/DebugOutput.h
    ../Common/HelperFunctions.h
    ../Common/OpenXRDebugUtils.h
    ../Common/OpenXRHelper.h
)

# XR_DOCS_TAG_BEGIN_HLSLShaders
set(HLSL_SHADERS "../Shaders/VertexShader.hlsl" "../Shaders/PixelShader.hlsl")
# XR_DOCS_TAG_END_HLSLShaders
# XR_DOCS_TAG_BEGIN_GLSLShaders
set(GLSL_SHADERS "../Shaders/VertexShader.glsl" "../Shaders/PixelShader.glsl")
# XR_DOCS_TAG_END_GLSLShaders
# XR_DOCS_TAG_BEGIN_GLESShaders
set(ES_GLSL_SHADERS "../Shaders/VertexShader_GLES.glsl"
                    "../Shaders/PixelShader_GLES.glsl"
)
# XR_DOCS_TAG_END_GLESShaders

if(ANDROID) # Android
    # XR_DOCS_TAG_BEGIN_Android
    add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})
    target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
            # In this repo
            ../Common/
            # From OpenXR repo
            "${openxr_SOURCE_DIR}/src/common"
            "${openxr_SOURCE_DIR}/external/include"
    )

    # export ANativeActivity_onCreate for java to call.
    set_property(
        TARGET ${PROJECT_NAME}
        APPEND_STRING
        PROPERTY LINK_FLAGS " -u ANativeActivity_onCreate"
    )

    # native_app_glue
    include(AndroidNdkModules)
    android_ndk_import_module_native_app_glue()

    target_link_libraries(${PROJECT_NAME} android native_app_glue openxr_loader)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wno-cast-calling-convention)
    # XR_DOCS_TAG_END_Android
    addgraphicsapidefine(${PROJECT_NAME})

    # XR_DOCS_TAG_BEGIN_VulkanNDK
    # VulkanNDK
    find_library(vulkan-lib vulkan)
    if(vulkan-lib)
        target_include_directories(
            ${PROJECT_NAME}
            PUBLIC ${ANDROID_NDK}/sources/third_party/vulkan/src/include
        )
        target_link_libraries(${PROJECT_NAME} ${vulkan-lib})
        target_compile_definitions(
            ${PROJECT_NAME} PUBLIC XR_TUTORIAL_USE_VULKAN
        )
    endif()
    # XR_DOCS_TAG_END_VulkanNDK

    # XR_DOCS_TAG_BEGIN_Gfxwrapper
    include(../cmake/gfxwrapper.cmake)
    if(TARGET openxr-gfxwrapper)
        target_include_directories(
            ${PROJECT_NAME} PUBLIC ${openxr_SOURCE_DIR}/src/common
        )
        target_link_libraries(${PROJECT_NAME} openxr-gfxwrapper)
        target_compile_definitions(
            ${PROJECT_NAME} PUBLIC XR_TUTORIAL_USE_OPENGL_ES
        )
    endif()
    # XR_DOCS_TAG_END_Gfxwrapper

    # XR_DOCS_TAG_BEGIN_CompileAndroidGLSLShaders
    # Vulkan GLSL
    set(SHADER_DEST "${CMAKE_CURRENT_SOURCE_DIR}/app/src/main/assets/shaders")
    include(glsl_shader)
    set_source_files_properties(
        ../Shaders/VertexShader.glsl PROPERTIES ShaderType "vert"
    )
    set_source_files_properties(
        ../Shaders/PixelShader.glsl PROPERTIES ShaderType "frag"
    )

    foreach(FILE ${GLSL_SHADERS})
        get_filename_component(FILE_WE ${FILE} NAME_WE)
        get_source_file_property(shadertype ${FILE} ShaderType)
        glsl_spv_shader(
            INPUT
            "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
            OUTPUT
            "${SHADER_DEST}/${FILE_WE}.spv"
            STAGE
            ${shadertype}
            ENTRY_POINT
            main
            TARGET_ENV
            vulkan1.0
        )
        # Make our project depend on these files
        target_sources(${PROJECT_NAME} PRIVATE "${SHADER_DEST}/${FILE_WE}.spv")
    endforeach()
    # XR_DOCS_TAG_END_CompileAndroidGLSLShaders

    # XR_DOCS_TAG_BEGIN_CompileAndroidGLESShaders
    #OpenGL ES GLSL
    set(SHADER_DEST "${CMAKE_CURRENT_SOURCE_DIR}/app/src/main/assets/shaders")
    foreach(FILE ${ES_GLSL_SHADERS})
        get_filename_component(FILE_WE ${FILE} NAME_WE)
        add_custom_command(
            OUTPUT "${SHADER_DEST}/${FILE_WE}.glsl"
            COMMAND
                ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}"
                "${SHADER_DEST}/${FILE_WE}.glsl"
            COMMENT "GLSL ${FILE}"
            MAIN_DEPENDENCY "${FILE}"
            DEPEND "${FILE}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            VERBATIM
        )
        # Make our project depend on these files
        target_sources(${PROJECT_NAME} PRIVATE "${SHADER_DEST}/${FILE_WE}.glsl")
    endforeach(FILE)
    # XR_DOCS_TAG_END_CompileAndroidGLESShaders

else() # Windows / Linux
    # XR_DOCS_TAG_BEGIN_WindowsLinux
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

    if(NOT "${XR_RUNTIME_JSON}" STREQUAL "")
        set_target_properties(
            ${PROJECT_NAME} PROPERTIES VS_DEBUGGER_ENVIRONMENT
                                       "XR_RUNTIME_JSON=${XR_RUNTIME_JSON}"
        )
    endif()

    target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
            # In this repo
            ../Common/
            # From OpenXR repo
            "${openxr_SOURCE_DIR}/src/common"
            "${openxr_SOURCE_DIR}/external/include"
    )
    target_link_libraries(${PROJECT_NAME} openxr_loader)
    # XR_DOCS_TAG_END_WindowsLinux
    #addgraphicsapidefine(${PROJECT_NAME})

    if(WIN32) # Windows

    else() # Linux
        # XR_DOCS_TAG_BEGIN_Linux
        target_compile_definitions(
            ${PROJECT_NAME} PUBLIC XR_TUTORIAL_USE_LINUX_XLIB
        )
        # XR_DOCS_TAG_END_Linux
    endif()

    set_source_files_properties(${HLSL_SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_sources(${PROJECT_NAME} PRIVATE ${HLSL_SHADERS})
endif()

target_link_libraries(${PROJECT_NAME}
    Diligent-GraphicsEngineD3D11-static
    Diligent-GraphicsEngineD3D12-static
    Diligent-GraphicsEngineOpenGL-static
    Diligent-GraphicsEngineVk-static
    Diligent-Common
    Diligent-GraphicsTools
    Diligent-GraphicsAccessories
    DiligentFX
)
# EOF