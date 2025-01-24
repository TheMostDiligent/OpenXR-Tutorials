// Copyright 2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

// OpenXR Tutorial for Khronos Group

#include <DebugOutput.h>

#if D3D11_SUPPORTED
#include "EngineFactoryD3D11.h"
#endif

#if D3D12_SUPPORTED
#include "EngineFactoryD3D12.h"
#endif

#if GL_SUPPORTED
#include "EngineFactoryOpenGL.h"
#endif

#if VULKAN_SUPPORTED
#include "EngineFactoryVk.h"
#endif

#include "DeviceContext.h"
#include "RenderDevice.h"
#include "SwapChain.h"

#include "GraphicsAccessories.hpp"
#include "GraphicsTypesX.hpp"
#include "GraphicsUtilities.h"
#include "MapHelper.hpp"
#include "OpenXRUtilities.h"
#include "RefCntAutoPtr.hpp"

#include "GLTFLoader.hpp"
#include "GLTF_PBR_Renderer.hpp"
#include "TextureUtilities.h"

namespace Diligent
{

namespace HLSL
{

#include "Shaders/Common/public/BasicStructures.fxh"
#include "Shaders/PBR/public/PBR_Structures.fxh"
#include "Shaders/PBR/private/RenderPBR_Structures.fxh"
#include "Shaders/PostProcess/ToneMapping/public/ToneMappingStructures.fxh"
#include "Shaders/PostProcess/ScreenSpaceReflection/public/ScreenSpaceReflectionStructures.fxh"

} // namespace HLSL

}

// XR_DOCS_TAG_BEGIN_include_OpenXRDebugUtils
#include <OpenXRDebugUtils.h>
// XR_DOCS_TAG_END_include_OpenXRDebugUtils

#include <GraphicsAPI.h>

// XR_DOCS_TAG_BEGIN_include_linear_algebra
// include xr linear algebra for XrVector and XrMatrix classes.
#include <xr_linear_algebra.h>
// Declare some useful operators for vectors:
XrVector3f operator-(XrVector3f a, XrVector3f b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
XrVector3f operator*(XrVector3f a, float b) {
    return {a.x * b, a.y * b, a.z * b};
}
// XR_DOCS_TAG_END_include_linear_algebra

#define XR_DOCS_CHAPTER_VERSION XR_DOCS_CHAPTER_3_3

const char *GetGraphicsAPIInstanceExtensionString(Diligent::RENDER_DEVICE_TYPE Type) {
    switch (Type) {
#if D3D11_SUPPORTED
    case Diligent::RENDER_DEVICE_TYPE_D3D11:
        return "XR_KHR_D3D11_enable";
#endif

#if D3D12_SUPPORTED
    case Diligent::RENDER_DEVICE_TYPE_D3D12:
        return "XR_KHR_D3D12_enable";
#endif

#if GL_SUPPORTED
    case Diligent::RENDER_DEVICE_TYPE_GL:
        return "XR_KHR_opengl_enable";
#endif

#if VULKAN_SUPPORTED
    case Diligent::RENDER_DEVICE_TYPE_VULKAN:
        return "XR_KHR_vulkan_enable2";
#endif

    default:
        UNEXPECTED("Unknown device type");
        return nullptr;
    }
}

inline GraphicsAPI_Type DiligentDeviceTypeToGraphicsAPIType(Diligent::RENDER_DEVICE_TYPE deviceType) {
    switch (deviceType) {
    case Diligent::RENDER_DEVICE_TYPE_D3D11:
        return GraphicsAPI_Type::D3D11;
    case Diligent::RENDER_DEVICE_TYPE_D3D12:
        return GraphicsAPI_Type::D3D12;
    case Diligent::RENDER_DEVICE_TYPE_GL:
        return GraphicsAPI_Type::OPENGL;
    case Diligent::RENDER_DEVICE_TYPE_GLES:
        return GraphicsAPI_Type::OPENGL_ES;
    case Diligent::RENDER_DEVICE_TYPE_VULKAN:
        return GraphicsAPI_Type::VULKAN;
    default:
        return GraphicsAPI_Type::UNKNOWN;
    }
}

class OpenXRTutorial {
private:
    struct RenderLayerInfo;

public:
    OpenXRTutorial(Diligent::RENDER_DEVICE_TYPE apiType)
        : m_apiType(apiType) {
        // Check API compatibility with Platform.
        // if (!CheckGraphicsAPI_TypeIsValidForPlatform(m_apiType)) {
        //    XR_TUT_LOG_ERROR("ERROR: The provided Graphics API is not valid for this platform.");
        //    DEBUG_BREAK;
        //}
    }
    ~OpenXRTutorial() = default;

    void Run() {
        CreateInstance();
        CreateDebugMessenger();

        GetInstanceProperties();
        GetSystemID();

#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_3_1
        GetViewConfigurationViews();
#endif
#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_3_2
        GetEnvironmentBlendModes();
#endif

        InitializeGraphics();

#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_2_2
        CreateSession();
#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_3_2
        CreateReferenceSpace();
#endif
#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_3_1
        CreateSwapchains();
#endif
#endif
#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_3_3
        CreateResources();
#endif

        CreateGLTFRenderer();
        LoadGltfModel("../Assets/DamagedHelmet.gltf");

#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_2_3
        while (m_applicationRunning) {
            PollSystemEvents();
            PollEvents();
            if (m_sessionRunning) {
                RenderFrame();
            }
        }
#endif

        // Flush any remaining commands
        m_context->Flush();
        // Make sure that the swap chains are not used by the GPU before they are destroyed
        m_renderDevice.IdleGPU();

#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_3_1
        DestroySwapchains();
#endif
#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_3_2
        DestroyReferenceSpace();
#endif
#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_3_3
        DestroyResources();
#endif
#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_2_2
        DestroySession();
#endif

        DestroyDebugMessenger();
        DestroyInstance();
    }

private:
    void CreateInstance() {
        // Fill out an XrApplicationInfo structure detailing the names and OpenXR version.
        // The application/engine name and version are user-definied. These may help IHVs or runtimes.
        // XR_DOCS_TAG_BEGIN_XrApplicationInfo
        XrApplicationInfo AI;
        strncpy(AI.applicationName, "OpenXR Tutorial Chapter 3", XR_MAX_APPLICATION_NAME_SIZE);
        AI.applicationVersion = 1;
        strncpy(AI.engineName, "OpenXR Engine", XR_MAX_ENGINE_NAME_SIZE);
        AI.engineVersion = 1;
        AI.apiVersion = XR_CURRENT_API_VERSION;
        // XR_DOCS_TAG_END_XrApplicationInfo

        // Add additional instance layers/extensions that the application wants.
        // Add both required and requested instance extensions.
        {
            // XR_DOCS_TAG_BEGIN_instanceExtensions
            m_instanceExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
            // Ensure m_apiType is already defined when we call this line.
            m_instanceExtensions.push_back(GetGraphicsAPIInstanceExtensionString(m_apiType));
            // XR_DOCS_TAG_END_instanceExtensions
        }

        // XR_DOCS_TAG_BEGIN_find_apiLayer_extension
        // Get all the API Layers from the OpenXR runtime.
        uint32_t apiLayerCount = 0;
        std::vector<XrApiLayerProperties> apiLayerProperties;
        OPENXR_CHECK(xrEnumerateApiLayerProperties(0, &apiLayerCount, nullptr), "Failed to enumerate ApiLayerProperties.");
        apiLayerProperties.resize(apiLayerCount, {XR_TYPE_API_LAYER_PROPERTIES});
        OPENXR_CHECK(xrEnumerateApiLayerProperties(apiLayerCount, &apiLayerCount, apiLayerProperties.data()), "Failed to enumerate ApiLayerProperties.");

        // Check the requested API layers against the ones from the OpenXR. If found add it to the Active API Layers.
        for (auto &requestLayer : m_apiLayers) {
            for (auto &layerProperty : apiLayerProperties) {
                // strcmp returns 0 if the strings match.
                if (strcmp(requestLayer.c_str(), layerProperty.layerName) != 0) {
                    continue;
                } else {
                    m_activeAPILayers.push_back(requestLayer.c_str());
                    break;
                }
            }
        }

        // Get all the Instance Extensions from the OpenXR instance.
        uint32_t extensionCount = 0;
        std::vector<XrExtensionProperties> extensionProperties;
        OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr), "Failed to enumerate InstanceExtensionProperties.");
        extensionProperties.resize(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
        OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data()), "Failed to enumerate InstanceExtensionProperties.");

        // Check the requested Instance Extensions against the ones from the OpenXR runtime.
        // If an extension is found add it to Active Instance Extensions.
        // Log error if the Instance Extension is not found.
        for (auto &requestedInstanceExtension : m_instanceExtensions) {
            bool found = false;
            for (auto &extensionProperty : extensionProperties) {
                // strcmp returns 0 if the strings match.
                if (strcmp(requestedInstanceExtension.c_str(), extensionProperty.extensionName) != 0) {
                    continue;
                } else {
                    m_activeInstanceExtensions.push_back(requestedInstanceExtension.c_str());
                    found = true;
                    break;
                }
            }
            if (!found) {
                XR_TUT_LOG_ERROR("Failed to find OpenXR instance extension: " << requestedInstanceExtension);
            }
        }
        // XR_DOCS_TAG_END_find_apiLayer_extension

        // Fill out an XrInstanceCreateInfo structure and create an XrInstance.
        // XR_DOCS_TAG_BEGIN_XrInstanceCreateInfo
        XrInstanceCreateInfo instanceCI{XR_TYPE_INSTANCE_CREATE_INFO};
        instanceCI.createFlags = 0;
        instanceCI.applicationInfo = AI;
        instanceCI.enabledApiLayerCount = static_cast<uint32_t>(m_activeAPILayers.size());
        instanceCI.enabledApiLayerNames = m_activeAPILayers.data();
        instanceCI.enabledExtensionCount = static_cast<uint32_t>(m_activeInstanceExtensions.size());
        instanceCI.enabledExtensionNames = m_activeInstanceExtensions.data();
        OPENXR_CHECK(xrCreateInstance(&instanceCI, &m_xrInstance), "Failed to create Instance.");
        // XR_DOCS_TAG_END_XrInstanceCreateInfo
    }

    void DestroyInstance() {
        // Destroy the XrInstance.
        // XR_DOCS_TAG_BEGIN_XrInstanceDestroy
        OPENXR_CHECK(xrDestroyInstance(m_xrInstance), "Failed to destroy Instance.");
        // XR_DOCS_TAG_END_XrInstanceDestroy
    }

    void CreateDebugMessenger() {
        // XR_DOCS_TAG_BEGIN_CreateDebugMessenger
        // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before creating an XrDebugUtilsMessengerEXT.
        if (IsStringInVector(m_activeInstanceExtensions, XR_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            m_debugUtilsMessenger = CreateOpenXRDebugUtilsMessenger(m_xrInstance);  // From OpenXRDebugUtils.h.
        }
        // XR_DOCS_TAG_END_CreateDebugMessenger
    }
    void DestroyDebugMessenger() {
        // XR_DOCS_TAG_BEGIN_DestroyDebugMessenger
        // Check that "XR_EXT_debug_utils" is in the active Instance Extensions before destroying the XrDebugUtilsMessengerEXT.
        if (m_debugUtilsMessenger != XR_NULL_HANDLE) {
            DestroyOpenXRDebugUtilsMessenger(m_xrInstance, m_debugUtilsMessenger);  // From OpenXRDebugUtils.h.
        }
        // XR_DOCS_TAG_END_DestroyDebugMessenger
    }

    void GetInstanceProperties() {
        // Get the instance's properties and log the runtime name and version.
        // XR_DOCS_TAG_BEGIN_GetInstanceProperties
        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
        OPENXR_CHECK(xrGetInstanceProperties(m_xrInstance, &instanceProperties), "Failed to get InstanceProperties.");

        XR_TUT_LOG("OpenXR Runtime: " << instanceProperties.runtimeName << " - "
                                      << XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "."
                                      << XR_VERSION_MINOR(instanceProperties.runtimeVersion) << "."
                                      << XR_VERSION_PATCH(instanceProperties.runtimeVersion));
        // XR_DOCS_TAG_END_GetInstanceProperties
    }

    void GetSystemID() {
        // XR_DOCS_TAG_BEGIN_GetSystemID
        // Get the XrSystemId from the instance and the supplied XrFormFactor.
        XrSystemGetInfo systemGI{XR_TYPE_SYSTEM_GET_INFO};
        systemGI.formFactor = m_formFactor;
        OPENXR_CHECK(xrGetSystem(m_xrInstance, &systemGI, &m_systemID), "Failed to get SystemID.");

        // Get the System's properties for some general information about the hardware and the vendor.
        OPENXR_CHECK(xrGetSystemProperties(m_xrInstance, m_systemID, &m_systemProperties), "Failed to get SystemProperties.");
        // XR_DOCS_TAG_END_GetSystemID
    }

    void GetEnvironmentBlendModes() {
        // XR_DOCS_TAG_BEGIN_GetEnvironmentBlendModes
        // Retrieves the available blend modes. The first call gets the count of the array that will be returned. The next call fills out the array.
        uint32_t environmentBlendModeCount = 0;
        OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_systemID, m_viewConfiguration, 0, &environmentBlendModeCount, nullptr), "Failed to enumerate EnvironmentBlend Modes.");
        m_environmentBlendModes.resize(environmentBlendModeCount);
        OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_systemID, m_viewConfiguration, environmentBlendModeCount, &environmentBlendModeCount, m_environmentBlendModes.data()), "Failed to enumerate EnvironmentBlend Modes.");

        // Pick the first application supported blend mode supported by the hardware.
        for (const XrEnvironmentBlendMode &environmentBlendMode : m_applicationEnvironmentBlendModes) {
            if (std::find(m_environmentBlendModes.begin(), m_environmentBlendModes.end(), environmentBlendMode) != m_environmentBlendModes.end()) {
                m_environmentBlendMode = environmentBlendMode;
                break;
            }
        }
        if (m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM) {
            XR_TUT_LOG_ERROR("Failed to find a compatible blend mode. Defaulting to XR_ENVIRONMENT_BLEND_MODE_OPAQUE.");
            m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        }
        // XR_DOCS_TAG_END_GetEnvironmentBlendModes
    }

    void GetViewConfigurationViews() {
        // XR_DOCS_TAG_BEGIN_GetViewConfigurationViews
        // Gets the View Configuration Types. The first call gets the count of the array that will be returned. The next call fills out the array.
        uint32_t viewConfigurationCount = 0;
        OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_systemID, 0, &viewConfigurationCount, nullptr), "Failed to enumerate View Configurations.");
        m_viewConfigurations.resize(viewConfigurationCount);
        OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_systemID, viewConfigurationCount, &viewConfigurationCount, m_viewConfigurations.data()), "Failed to enumerate View Configurations.");

        // Pick the first application supported View Configuration Type con supported by the hardware.
        for (const XrViewConfigurationType &viewConfiguration : m_applicationViewConfigurations) {
            if (std::find(m_viewConfigurations.begin(), m_viewConfigurations.end(), viewConfiguration) != m_viewConfigurations.end()) {
                m_viewConfiguration = viewConfiguration;
                break;
            }
        }
        if (m_viewConfiguration == XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM) {
            std::cerr << "Failed to find a view configuration type. Defaulting to XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO." << std::endl;
            m_viewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        }

        // Gets the View Configuration Views. The first call gets the count of the array that will be returned. The next call fills out the array.
        uint32_t viewConfigurationViewCount = 0;
        OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_systemID, m_viewConfiguration, 0, &viewConfigurationViewCount, nullptr), "Failed to enumerate ViewConfiguration Views.");
        m_viewConfigurationViews.resize(viewConfigurationViewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_systemID, m_viewConfiguration, viewConfigurationViewCount, &viewConfigurationViewCount, m_viewConfigurationViews.data()), "Failed to enumerate ViewConfiguration Views.");
        // XR_DOCS_TAG_END_GetViewConfigurationViews
    }

    void InitializeGraphics() {
        Diligent::OpenXRAttribs xrAttribs;
        static_assert(sizeof(xrAttribs.Instance) == sizeof(m_xrInstance), "XrInstance size mismatch");
        memcpy(&xrAttribs.Instance, &m_xrInstance, sizeof(m_xrInstance));
        static_assert(sizeof(xrAttribs.SystemId) == sizeof(m_systemID), "XrSystemID size mismatch");
        memcpy(&xrAttribs.SystemId, &m_systemID, sizeof(m_systemID));
        xrAttribs.GetInstanceProcAddr = xrGetInstanceProcAddr;

        Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice;
        switch (m_apiType) {
#if D3D11_SUPPORTED
        case Diligent::RENDER_DEVICE_TYPE_D3D11: {
            Diligent::EngineD3D11CreateInfo engineCI;
            engineCI.pXRAttribs = &xrAttribs;
#if ENGINE_DLL
            // Load the dll and import GetEngineFactoryD3D11() function
            auto *GetEngineFactoryD3D11 = LoadGraphicsEngineD3D11();
#endif
            auto *factoryD3D11 = Diligent::GetEngineFactoryD3D11();
            factoryD3D11->CreateDeviceAndContextsD3D11(engineCI, &renderDevice, &m_context);
        } break;
#endif

#if D3D12_SUPPORTED
        case Diligent::RENDER_DEVICE_TYPE_D3D12: {
#if ENGINE_DLL
            // Load the dll and import GetEngineFactoryD3D12() function
            auto GetEngineFactoryD3D12 = LoadGraphicsEngineD3D12();
#endif
            Diligent::EngineD3D12CreateInfo engineCI;
            engineCI.pXRAttribs = &xrAttribs;

            auto *factoryD3D12 = Diligent::GetEngineFactoryD3D12();
            factoryD3D12->CreateDeviceAndContextsD3D12(engineCI, &renderDevice, &m_context);
        } break;
#endif

#if GL_SUPPORTED
        case Diligent::RENDER_DEVICE_TYPE_GL: {
#if 0
#if EXPLICITLY_LOAD_ENGINE_GL_DLL
                // Load the dll and import GetEngineFactoryOpenGL() function
                auto GetEngineFactoryOpenGL = LoadGraphicsEngineOpenGL();
#endif
                auto* factoryOpenGL = GetEngineFactoryOpenGL();

                Diligent::EngineGLCreateInfo engineCI;
                engineCI.pXRAttribs  = &XRAttribs;
                engineCI.Window.hWnd = hWnd;

                factoryOpenGL->CreateDeviceAndSwapChainGL(EngineCI, &renderDevice, &m_context, scDesc, &m_swapChain);
#endif
        } break;
#endif

#if VULKAN_SUPPORTED
        case Diligent::RENDER_DEVICE_TYPE_VULKAN: {
#if EXPLICITLY_LOAD_ENGINE_VK_DLL
            // Load the dll and import GetEngineFactoryVk() function
            auto GetEngineFactoryVk = LoadGraphicsEngineVk();
#endif
            Diligent::EngineVkCreateInfo engineCI;
            engineCI.pXRAttribs = &xrAttribs;

            auto *factoryVk = Diligent::GetEngineFactoryVk();
            factoryVk->CreateDeviceAndContextsVk(engineCI, &renderDevice, &m_context);
        } break;
#endif

        default:
            XR_TUT_LOG_ERROR("Unknown/unsupported device type");
            DEBUG_BREAK;
            break;
        }

        m_renderDevice = renderDevice;
    }

    void CreateSession() {
        // Create an XrSessionCreateInfo structure.
        // XR_DOCS_TAG_BEGIN_CreateSession1
        XrSessionCreateInfo sessionCI{XR_TYPE_SESSION_CREATE_INFO};
        // XR_DOCS_TAG_END_CreateSession1

        Diligent::RefCntAutoPtr<Diligent::IDataBlob> graphicsBindingData;
        Diligent::GetOpenXRGraphicsBinding(m_renderDevice, m_context, &graphicsBindingData);

        // Fill out the XrSessionCreateInfo structure and create an XrSession.
        //  XR_DOCS_TAG_BEGIN_CreateSession2
        sessionCI.next = graphicsBindingData->GetConstDataPtr();
        sessionCI.createFlags = 0;
        sessionCI.systemId = m_systemID;

        OPENXR_CHECK(xrCreateSession(m_xrInstance, &sessionCI, &m_session), "Failed to create Session.");
        // XR_DOCS_TAG_END_CreateSession2
    }

    void DestroySession() {
        // Destroy the XrSession.
        // XR_DOCS_TAG_BEGIN_DestroySession
        OPENXR_CHECK(xrDestroySession(m_session), "Failed to destroy Session.");
        // XR_DOCS_TAG_END_DestroySession
    }

    // XR_DOCS_TAG_BEGIN_CreateResources1
    struct CameraConstants {
        XrMatrix4x4f viewProj;
        XrMatrix4x4f modelViewProj;
        XrMatrix4x4f model;
        XrVector4f color;
        XrVector4f pad1;
        XrVector4f pad2;
        XrVector4f pad3;
    };
    XrMatrix4x4f cameraProj;
    CameraConstants cameraConstants;
    XrVector4f normals[6] = {
        {1.00f, 0.00f, 0.00f, 0},
        {-1.00f, 0.00f, 0.00f, 0},
        {0.00f, 1.00f, 0.00f, 0},
        {0.00f, -1.00f, 0.00f, 0},
        {0.00f, 0.00f, 1.00f, 0},
        {0.00f, 0.0f, -1.00f, 0}};
    // XR_DOCS_TAG_END_CreateResources1

    void CreateResources() {
        // XR_DOCS_TAG_BEGIN_CreateResources1_1
        // Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
        constexpr XrVector4f vertexPositions[] = {
            {+0.5f, +0.5f, +0.5f, 1.0f},
            {+0.5f, +0.5f, -0.5f, 1.0f},
            {+0.5f, -0.5f, +0.5f, 1.0f},
            {+0.5f, -0.5f, -0.5f, 1.0f},
            {-0.5f, +0.5f, +0.5f, 1.0f},
            {-0.5f, +0.5f, -0.5f, 1.0f},
            {-0.5f, -0.5f, +0.5f, 1.0f},
            {-0.5f, -0.5f, -0.5f, 1.0f}};

#define CUBE_FACE(V1, V2, V3, V4, V5, V6) vertexPositions[V1], vertexPositions[V2], vertexPositions[V3], vertexPositions[V4], vertexPositions[V5], vertexPositions[V6],

        XrVector4f cubeVertices[] = {
            CUBE_FACE(2, 1, 0, 2, 3, 1)  // -X
            CUBE_FACE(6, 4, 5, 6, 5, 7)  // +X
            CUBE_FACE(0, 1, 5, 0, 5, 4)  // -Y
            CUBE_FACE(2, 6, 7, 2, 7, 3)  // +Y
            CUBE_FACE(0, 4, 6, 0, 6, 2)  // -Z
            CUBE_FACE(1, 3, 7, 1, 7, 5)  // +Z
        };

        uint32_t cubeIndices[36] = {
            0, 1, 2, 3, 4, 5,        // -X
            6, 7, 8, 9, 10, 11,      // +X
            12, 13, 14, 15, 16, 17,  // -Y
            18, 19, 20, 21, 22, 23,  // +Y
            24, 25, 26, 27, 28, 29,  // -Z
            30, 31, 32, 33, 34, 35,  // +Z
        };

        m_vertexBuffer = m_renderDevice.CreateBuffer("Vertices", sizeof(cubeVertices), Diligent::USAGE_DEFAULT, Diligent::BIND_VERTEX_BUFFER, Diligent::CPU_ACCESS_NONE, cubeVertices);
        m_indexBuffer = m_renderDevice.CreateBuffer("Indices", sizeof(cubeIndices), Diligent::USAGE_DEFAULT, Diligent::BIND_INDEX_BUFFER, Diligent::CPU_ACCESS_NONE, cubeIndices);
        m_uniformBuffer_Normals = m_renderDevice.CreateBuffer("Normals", sizeof(normals), Diligent::USAGE_DEFAULT, Diligent::BIND_UNIFORM_BUFFER, Diligent::CPU_ACCESS_NONE, &normals);
        m_uniformBuffer_Camera = m_renderDevice.CreateBuffer("Camera Constants", sizeof(CameraConstants), Diligent::USAGE_DYNAMIC, Diligent::BIND_UNIFORM_BUFFER);

        Diligent::GraphicsPipelineStateCreateInfoX psoCreateInfo{"Cuboid"};
        psoCreateInfo
            .AddRenderTarget(m_colorFormat)
            .SetDepthFormat(m_depthFormat)
            .SetPrimitiveTopology(Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        psoCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_BACK;
        psoCreateInfo.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
        psoCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;

        Diligent::ShaderCreateInfo shaderCI;
        shaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;

        Diligent::RefCntAutoPtr<Diligent::IShaderSourceInputStreamFactory> shaderSourceFactory;
        m_renderDevice.GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("../Shaders", &shaderSourceFactory);
        shaderCI.pShaderSourceStreamFactory = shaderSourceFactory;

        // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
        constexpr bool UseCombinedTextureSamplers = true;

        Diligent::RefCntAutoPtr<Diligent::IShader> vs;
        {
            shaderCI.Desc = {"VS", Diligent::SHADER_TYPE_VERTEX, UseCombinedTextureSamplers};
            shaderCI.EntryPoint = "main";
            shaderCI.FilePath = "VertexShader.hlsl";

            vs = m_renderDevice.CreateShader(shaderCI);
            VERIFY_EXPR(vs);
        }

        Diligent::RefCntAutoPtr<Diligent::IShader> ps;
        {
            shaderCI.Desc = {"PS", Diligent::SHADER_TYPE_PIXEL, UseCombinedTextureSamplers};
            shaderCI.EntryPoint = "main";
            shaderCI.FilePath = "PixelShader.hlsl";

            ps = m_renderDevice.CreateShader(shaderCI);
            VERIFY_EXPR(ps);
        }

        Diligent::InputLayoutDescX inputLayout{
            // Attribute 0 - vertex position
            Diligent::LayoutElement{0, 0, 4, Diligent::VT_FLOAT32},
        };

        psoCreateInfo
            .AddShader(vs)
            .AddShader(ps)
            .SetInputLayout(inputLayout);

        // Static variables are set once in the pipeline state (like immutable samplers)
        // Mutable variables are set once in each instance of the SRB
        // Dynamic variables can be set multiple times in each instance of the SRB
        psoCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
        // Merge resources with the same name in all stages
        psoCreateInfo.PSODesc.ResourceLayout.DefaultVariableMergeStages = Diligent::SHADER_TYPE_VS_PS;

        m_pipeline = m_renderDevice.CreateGraphicsPipelineState(psoCreateInfo);
        m_pipeline->CreateShaderResourceBinding(&m_srb, true);

        m_srb->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "Normals")->Set(m_uniformBuffer_Normals);
        m_srb->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "CameraConstants")->Set(m_uniformBuffer_Camera);
    }

    void DestroyResources() {
        // XR_DOCS_TAG_BEGIN_DestroyResources
        m_pipeline.Release();
        m_srb.Release();
        m_uniformBuffer_Camera.Release();
        m_uniformBuffer_Normals.Release();
        m_indexBuffer.Release();
        m_vertexBuffer.Release();
        // XR_DOCS_TAG_END_DestroyResources

        m_gltfRenderer.reset();
        m_gltfModel.reset();
        m_gltfModelResourceBindings.Clear();
        m_frameAttribsCB.Release();
    }

    void CreateGLTFRenderer() {
        Diligent::GLTF_PBR_Renderer::CreateInfo rendererCI;

        rendererCI.EnableClearCoat = true;
        rendererCI.EnableSheen = true;
        rendererCI.EnableIridescence = true;
        rendererCI.EnableTransmission = true;
        rendererCI.EnableAnisotropy = true;
        rendererCI.FrontCounterClockwise = true;
        rendererCI.PackMatrixRowMajor = true;
        rendererCI.SheenAlbedoScalingLUTPath = "../Textures/sheen_albedo_scaling.jpg";
        rendererCI.PreintegratedCharlieBRDFPath = "../Textures/charlie_preintegrated.jpg";

        m_gltfRenderInfo.Flags =
            Diligent::GLTF_PBR_Renderer::PSO_FLAG_DEFAULT |
            Diligent::GLTF_PBR_Renderer::PSO_FLAG_ENABLE_CLEAR_COAT |
            Diligent::GLTF_PBR_Renderer::PSO_FLAG_ALL_TEXTURES |
            Diligent::GLTF_PBR_Renderer::PSO_FLAG_ENABLE_SHEEN |
            Diligent::GLTF_PBR_Renderer::PSO_FLAG_ENABLE_ANISOTROPY |
            Diligent::GLTF_PBR_Renderer::PSO_FLAG_ENABLE_IRIDESCENCE |
            Diligent::GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TRANSMISSION |
            Diligent::GLTF_PBR_Renderer::PSO_FLAG_ENABLE_VOLUME |
            Diligent::GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TEXCOORD_TRANSFORM;

        rendererCI.NumRenderTargets = 1;
        rendererCI.RTVFormats[0]    = m_colorFormat;
        rendererCI.DSVFormat        = m_depthFormat;

        if (rendererCI.RTVFormats[0] == Diligent::TEX_FORMAT_RGBA8_UNORM || rendererCI.RTVFormats[0] == Diligent::TEX_FORMAT_BGRA8_UNORM)
            m_gltfRenderInfo.Flags |= Diligent::GLTF_PBR_Renderer::PSO_FLAG_CONVERT_OUTPUT_TO_SRGB;

        m_gltfRenderer = std::make_unique<Diligent::GLTF_PBR_Renderer>(m_renderDevice, nullptr, m_context, rendererCI);

        // Load environment map and precompute IBL
        Diligent::RefCntAutoPtr<Diligent::ITexture> environmentMap;
        Diligent::CreateTextureFromFile("../Textures/papermill.ktx", Diligent::TextureLoadInfo{"Environment map"}, m_renderDevice, &environmentMap);
        m_gltfRenderer->PrecomputeCubemaps(m_context, environmentMap->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

        Diligent::StateTransitionDesc Barriers[] = {
            {environmentMap, Diligent::RESOURCE_STATE_UNKNOWN, Diligent::RESOURCE_STATE_SHADER_RESOURCE, Diligent::STATE_TRANSITION_FLAG_UPDATE_STATE},
        };
        m_context->TransitionResourceStates(_countof(Barriers), Barriers);
    }

    void LoadGltfModel(const char* Path) {
        Diligent::GLTF::ModelCreateInfo modelCI;
        modelCI.FileName = Path;

        m_gltfModel = std::make_unique<Diligent::GLTF::Model>(m_renderDevice, m_context, modelCI);

        Diligent::CreateUniformBuffer(m_renderDevice, m_gltfRenderer->GetPRBFrameAttribsSize(), "PBR frame attribs buffer", &m_frameAttribsCB);
        Diligent::StateTransitionDesc Barriers[] = {
            {m_frameAttribsCB, Diligent::RESOURCE_STATE_UNKNOWN, Diligent::RESOURCE_STATE_CONSTANT_BUFFER, Diligent::STATE_TRANSITION_FLAG_UPDATE_STATE},
        };
        m_context->TransitionResourceStates(_countof(Barriers), Barriers);

        m_gltfModelResourceBindings = m_gltfRenderer->CreateResourceBindings(*m_gltfModel, m_frameAttribsCB);

        const Diligent::Uint32 SceneIndex = 0;
        const Diligent::float4x4 modelTransform = Diligent::float4x4::Scale(0.25f) * Diligent::float4x4::Translation(0.0f, -m_viewHeightM + 1.2f, -0.7f);
        m_gltfModel->ComputeTransforms(0, m_gltfTransforms, modelTransform);
    }

    void PollEvents() {
        // XR_DOCS_TAG_BEGIN_PollEvents
        // Poll OpenXR for a new event.
        XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
        auto XrPollEvents = [&]() -> bool {
            eventData = {XR_TYPE_EVENT_DATA_BUFFER};
            return xrPollEvent(m_xrInstance, &eventData) == XR_SUCCESS;
        };

        while (XrPollEvents()) {
            switch (eventData.type) {
            // Log the number of lost events from the runtime.
            case XR_TYPE_EVENT_DATA_EVENTS_LOST: {
                XrEventDataEventsLost *eventsLost = reinterpret_cast<XrEventDataEventsLost *>(&eventData);
                XR_TUT_LOG("OPENXR: Events Lost: " << eventsLost->lostEventCount);
                break;
            }
            // Log that an instance loss is pending and shutdown the application.
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                XrEventDataInstanceLossPending *instanceLossPending = reinterpret_cast<XrEventDataInstanceLossPending *>(&eventData);
                XR_TUT_LOG("OPENXR: Instance Loss Pending at: " << instanceLossPending->lossTime);
                m_sessionRunning = false;
                m_applicationRunning = false;
                break;
            }
            // Log that the interaction profile has changed.
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
                XrEventDataInteractionProfileChanged *interactionProfileChanged = reinterpret_cast<XrEventDataInteractionProfileChanged *>(&eventData);
                XR_TUT_LOG("OPENXR: Interaction Profile changed for Session: " << interactionProfileChanged->session);
                if (interactionProfileChanged->session != m_session) {
                    XR_TUT_LOG("XrEventDataInteractionProfileChanged for unknown Session");
                    break;
                }
                break;
            }
            // Log that there's a reference space change pending.
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
                XrEventDataReferenceSpaceChangePending *referenceSpaceChangePending = reinterpret_cast<XrEventDataReferenceSpaceChangePending *>(&eventData);
                XR_TUT_LOG("OPENXR: Reference Space Change pending for Session: " << referenceSpaceChangePending->session);
                if (referenceSpaceChangePending->session != m_session) {
                    XR_TUT_LOG("XrEventDataReferenceSpaceChangePending for unknown Session");
                    break;
                }
                break;
            }
            // Session State changes:
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                XrEventDataSessionStateChanged *sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged *>(&eventData);
                if (sessionStateChanged->session != m_session) {
                    XR_TUT_LOG("XrEventDataSessionStateChanged for unknown Session");
                    break;
                }

                if (sessionStateChanged->state == XR_SESSION_STATE_READY) {
                    // SessionState is ready. Begin the XrSession using the XrViewConfigurationType.
                    XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                    sessionBeginInfo.primaryViewConfigurationType = m_viewConfiguration;
                    OPENXR_CHECK(xrBeginSession(m_session, &sessionBeginInfo), "Failed to begin Session.");
                    m_sessionRunning = true;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_STOPPING) {
                    // SessionState is stopping. End the XrSession.
                    OPENXR_CHECK(xrEndSession(m_session), "Failed to end Session.");
                    m_sessionRunning = false;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_EXITING) {
                    // SessionState is exiting. Exit the application.
                    m_sessionRunning = false;
                    m_applicationRunning = false;
                }
                if (sessionStateChanged->state == XR_SESSION_STATE_LOSS_PENDING) {
                    // SessionState is loss pending. Exit the application.
                    // It's possible to try a reestablish an XrInstance and XrSession, but we will simply exit here.
                    m_sessionRunning = false;
                    m_applicationRunning = false;
                }
                // Store state for reference across the application.
                m_sessionState = sessionStateChanged->state;
                break;
            }
            default: {
                break;
            }
            }
        }
        // XR_DOCS_TAG_END_PollEvents
    }

    void CreateReferenceSpace() {
        // XR_DOCS_TAG_BEGIN_CreateReferenceSpace
        // Fill out an XrReferenceSpaceCreateInfo structure and create a reference XrSpace, specifying a Local space with an identity pose as the origin.
        XrReferenceSpaceCreateInfo referenceSpaceCI{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        referenceSpaceCI.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        referenceSpaceCI.poseInReferenceSpace = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
        OPENXR_CHECK(xrCreateReferenceSpace(m_session, &referenceSpaceCI, &m_localSpace), "Failed to create ReferenceSpace.");
        // XR_DOCS_TAG_END_CreateReferenceSpace
    }

    void DestroyReferenceSpace() {
        // XR_DOCS_TAG_BEGIN_DestroyReferenceSpace
        // Destroy the reference XrSpace.
        OPENXR_CHECK(xrDestroySpace(m_localSpace), "Failed to destroy Space.")
        // XR_DOCS_TAG_END_DestroyReferenceSpace
    }

    void CreateSwapchains() {
        // XR_DOCS_TAG_BEGIN_EnumerateSwapchainFormats
        // Get the supported swapchain formats as an array of int64_t and ordered by runtime preference.
        uint32_t formatCount = 0;
        OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr), "Failed to enumerate Swapchain Formats");
        std::vector<int64_t> formats(formatCount);
        OPENXR_CHECK(xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data()), "Failed to enumerate Swapchain Formats");

        int64_t nativeColorFormat = 0;
        int64_t nativeDepthFormat = 0;
        for (int64_t nativeFormat : formats) {
            const Diligent::TEXTURE_FORMAT format = Diligent::GetTextureFormatFromNative(nativeFormat, m_apiType);
            const Diligent::TextureFormatAttribs &fmtAttribs = Diligent::GetTextureFormatAttribs(format);
            if (fmtAttribs.IsDepthStencil()) {
                if (nativeDepthFormat == 0) {
                    m_depthFormat = format;
                    nativeDepthFormat = nativeFormat;
                }
            } else {
                if (nativeColorFormat == 0) {
                    m_colorFormat = format;
                    nativeColorFormat = nativeFormat;
                }
            }

            if (nativeColorFormat != 0 && nativeDepthFormat != 0)
                break;
        }

        if (nativeColorFormat == 0) {
            std::cerr << "Failed to find a compatible color format for Swapchain";
            DEBUG_BREAK;
        }
        if (nativeDepthFormat == 0) {
            std::cerr << "Failed to find a compatible depth format for Swapchain";
            DEBUG_BREAK;
        }

        // XR_DOCS_TAG_END_EnumerateSwapchainFormats

        // XR_DOCS_TAG_BEGIN_ResizeSwapchainInfos
        // Resize the SwapchainInfo to match the number of view in the View Configuration.
        m_colorSwapchainInfos.resize(m_viewConfigurationViews.size());
        m_depthSwapchainInfos.resize(m_viewConfigurationViews.size());
        // XR_DOCS_TAG_END_ResizeSwapchainInfos

        // Per view, create a color and depth swapchain, and their associated image views.
        for (size_t i = 0; i < m_viewConfigurationViews.size(); i++) {
            // XR_DOCS_TAG_BEGIN_CreateSwapchains
            SwapchainInfo &colorSwapchainInfo = m_colorSwapchainInfos[i];
            SwapchainInfo &depthSwapchainInfo = m_depthSwapchainInfos[i];

            // Fill out an XrSwapchainCreateInfo structure and create an XrSwapchain.
            // Color.
            XrSwapchainCreateInfo swapchainCI{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            swapchainCI.createFlags = 0;
            swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            swapchainCI.format = nativeColorFormat;
            swapchainCI.sampleCount = m_viewConfigurationViews[i].recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
            swapchainCI.width = m_viewConfigurationViews[i].recommendedImageRectWidth;
            swapchainCI.height = m_viewConfigurationViews[i].recommendedImageRectHeight;
            swapchainCI.faceCount = 1;
            swapchainCI.arraySize = 1;
            swapchainCI.mipCount = 1;
            OPENXR_CHECK(xrCreateSwapchain(m_session, &swapchainCI, &colorSwapchainInfo.swapchain), "Failed to create Color Swapchain");
            colorSwapchainInfo.swapchainFormat = swapchainCI.format;  // Save the swapchain format for later use.

            // Depth.
            swapchainCI.createFlags = 0;
            swapchainCI.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            swapchainCI.format = nativeDepthFormat;
            swapchainCI.sampleCount = m_viewConfigurationViews[i].recommendedSwapchainSampleCount;  // Use the recommended values from the XrViewConfigurationView.
            swapchainCI.width = m_viewConfigurationViews[i].recommendedImageRectWidth;
            swapchainCI.height = m_viewConfigurationViews[i].recommendedImageRectHeight;
            swapchainCI.faceCount = 1;
            swapchainCI.arraySize = 1;
            swapchainCI.mipCount = 1;
            OPENXR_CHECK(xrCreateSwapchain(m_session, &swapchainCI, &depthSwapchainInfo.swapchain), "Failed to create Depth Swapchain");
            depthSwapchainInfo.swapchainFormat = swapchainCI.format;  // Save the swapchain format for later use.
            // XR_DOCS_TAG_END_CreateSwapchains

            // XR_DOCS_TAG_BEGIN_EnumerateSwapchainImages
            // Get the number of images in the color/depth swapchain and allocate Swapchain image data via GraphicsAPI to store the returned array.
            uint32_t colorSwapchainImageCount = 0;
            OPENXR_CHECK(xrEnumerateSwapchainImages(colorSwapchainInfo.swapchain, 0, &colorSwapchainImageCount, nullptr), "Failed to enumerate Color Swapchain Images.");
            Diligent::RefCntAutoPtr<Diligent::IDataBlob> colorSwapchainImages;
            Diligent::AllocateOpenXRSwapchainImageData(m_apiType, colorSwapchainImageCount, &colorSwapchainImages);
            OPENXR_CHECK(xrEnumerateSwapchainImages(colorSwapchainInfo.swapchain, colorSwapchainImageCount, &colorSwapchainImageCount, colorSwapchainImages->GetDataPtr<XrSwapchainImageBaseHeader>()), "Failed to enumerate Color Swapchain Images.");

            uint32_t depthSwapchainImageCount = 0;
            OPENXR_CHECK(xrEnumerateSwapchainImages(depthSwapchainInfo.swapchain, 0, &depthSwapchainImageCount, nullptr), "Failed to enumerate Depth Swapchain Images.");
            Diligent::RefCntAutoPtr<Diligent::IDataBlob> depthSwapchainImages;
            Diligent::AllocateOpenXRSwapchainImageData(m_apiType, depthSwapchainImageCount, &depthSwapchainImages);
            OPENXR_CHECK(xrEnumerateSwapchainImages(depthSwapchainInfo.swapchain, depthSwapchainImageCount, &depthSwapchainImageCount, depthSwapchainImages->GetDataPtr<XrSwapchainImageBaseHeader>()), "Failed to enumerate Depth Swapchain Images.");
            // XR_DOCS_TAG_END_EnumerateSwapchainImages

            // XR_DOCS_TAG_BEGIN_CreateImageViews
            // Per image in the swapchains, fill out a GraphicsAPI::ImageViewCreateInfo structure and create a color/depth image view.
            colorSwapchainInfo.views.resize(colorSwapchainImageCount);
            for (uint32_t j = 0; j < colorSwapchainImageCount; j++) {
                Diligent::TextureDesc imgDesc;
                std::string name = "Color Swapchain Image " + std::to_string(j);
                imgDesc.Name = name.c_str();
                imgDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
                imgDesc.Format = m_colorFormat;
                imgDesc.Width = swapchainCI.width;
                imgDesc.Height = swapchainCI.height;
                imgDesc.MipLevels = 1;
                imgDesc.BindFlags = Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;

                Diligent::RefCntAutoPtr<Diligent::ITexture> image;
                Diligent::GetOpenXRSwapchainImage(m_renderDevice, colorSwapchainImages->GetConstDataPtr<XrSwapchainImageBaseHeader>(), j, imgDesc, &image);

                Diligent::TextureViewDesc viewDesc;
                viewDesc.ViewType = Diligent::TEXTURE_VIEW_RENDER_TARGET;
                image->CreateView(viewDesc, &colorSwapchainInfo.views[j]);
            }
            depthSwapchainInfo.views.resize(colorSwapchainImageCount);
            for (uint32_t j = 0; j < depthSwapchainImageCount; j++) {
                Diligent::TextureDesc imgDesc;
                std::string name = "Depth Swapchain Image " + std::to_string(j);
                imgDesc.Name = name.c_str();
                imgDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
                imgDesc.Format = m_depthFormat;
                imgDesc.Width = swapchainCI.width;
                imgDesc.Height = swapchainCI.height;
                imgDesc.MipLevels = 1;
                imgDesc.BindFlags = Diligent::BIND_DEPTH_STENCIL | Diligent::BIND_SHADER_RESOURCE;

                Diligent::RefCntAutoPtr<Diligent::ITexture> image;
                Diligent::GetOpenXRSwapchainImage(m_renderDevice, depthSwapchainImages->GetConstDataPtr<XrSwapchainImageBaseHeader>(), j, imgDesc, &image);

                Diligent::TextureViewDesc viewDesc;
                viewDesc.ViewType = Diligent::TEXTURE_VIEW_DEPTH_STENCIL;
                image->CreateView(viewDesc, &depthSwapchainInfo.views[j]);
            }
            // XR_DOCS_TAG_END_CreateImageViews
        }
    }

    void DestroySwapchains() {
        // XR_DOCS_TAG_BEGIN_DestroySwapchains
        // Per view in the view configuration:
        for (size_t i = 0; i < m_viewConfigurationViews.size(); i++) {
            SwapchainInfo &colorSwapchainInfo = m_colorSwapchainInfos[i];
            SwapchainInfo &depthSwapchainInfo = m_depthSwapchainInfos[i];

            // Destroy the color and depth image views from GraphicsAPI.
            colorSwapchainInfo.views.clear();
            depthSwapchainInfo.views.clear();

            // Destroy the swapchains.
            OPENXR_CHECK(xrDestroySwapchain(colorSwapchainInfo.swapchain), "Failed to destroy Color Swapchain");
            OPENXR_CHECK(xrDestroySwapchain(depthSwapchainInfo.swapchain), "Failed to destroy Depth Swapchain");
        }
        // XR_DOCS_TAG_END_DestroySwapchains
    }

    // XR_DOCS_TAG_BEGIN_RenderCuboid1
    // XR_DOCS_TAG_END_RenderCuboid1
    void RenderCuboid(XrPosef pose, XrVector3f scale, XrVector3f color) {
        // XR_DOCS_TAG_BEGIN_RenderCuboid2
        XrMatrix4x4f_CreateTranslationRotationScale(&cameraConstants.model, &pose.position, &pose.orientation, &scale);

        XrMatrix4x4f_Multiply(&cameraConstants.modelViewProj, &cameraConstants.viewProj, &cameraConstants.model);
        cameraConstants.color = {color.x, color.y, color.z, 1.0};
        {
            Diligent::MapHelper<CameraConstants> gpuConstants{m_context, m_uniformBuffer_Camera, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD};
            *gpuConstants = cameraConstants;
        }

        m_context->SetPipelineState(m_pipeline);
        m_context->CommitShaderResources(m_srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::IBuffer *vbs[] = {m_vertexBuffer};
        m_context->SetVertexBuffers(0, 1, vbs, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        m_context->SetIndexBuffer(m_indexBuffer, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_context->DrawIndexed({36, Diligent::VT_UINT32, Diligent::DRAW_FLAG_VERIFY_ALL});
        // XR_DOCS_TAG_END_RenderCuboid2
    }

    void RenderGltfModel(const XrVector3f& cameraPos, const XrMatrix4x4f& view, const XrMatrix4x4f& proj, float nearZ, float farZ){
        {
            Diligent::MapHelper<Diligent::HLSL::PBRFrameAttribs> FrameAttribs{m_context, m_frameAttribsCB, Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD};
            memcpy(&FrameAttribs->Camera.mView, view.m, sizeof(XrMatrix4x4f));
            memcpy(&FrameAttribs->Camera.mProj, proj.m, sizeof(XrMatrix4x4f));
            memcpy(&FrameAttribs->Camera.mViewProj, cameraConstants.viewProj.m, sizeof(XrMatrix4x4f));
            FrameAttribs->Camera.f4Position = {cameraPos.x, cameraPos.y, cameraPos.z, 1};
            FrameAttribs->Camera.fNearPlaneZ = nearZ;
            FrameAttribs->Camera.fFarPlaneZ = farZ;
            FrameAttribs->Camera.fNearPlaneDepth = 0;
            FrameAttribs->Camera.fFarPlaneDepth = 1;

            FrameAttribs->Camera.fHandness = 1;
            FrameAttribs->Camera.uiFrameIndex = 0;

            int LightCount = 0;
#ifdef PBR_MAX_LIGHTS
#    error PBR_MAX_LIGHTS should not be defined here
#endif
            {
                auto& Renderer = FrameAttribs->Renderer;
                m_gltfRenderer->SetInternalShaderParameters(Renderer);

                Renderer.OcclusionStrength = 1;
                Renderer.EmissionScale     = 1;
                Renderer.AverageLogLum     = 0.3f;
                Renderer.MiddleGray        = 0.18f;
                Renderer.WhitePoint        = 3.f;
                Renderer.IBLScale          = Diligent::float4{1.f};
                Renderer.PointSize         = 1;
                Renderer.MipBias           = 0;
                Renderer.LightCount        = LightCount;
            }
        }

        m_gltfRenderInfo.AlphaModes = Diligent::GLTF_PBR_Renderer::RenderInfo::ALPHA_MODE_FLAG_ALL;
        m_gltfRenderer->Render(m_context, *m_gltfModel, m_gltfTransforms, nullptr, m_gltfRenderInfo, &m_gltfModelResourceBindings);
    }

    void RenderFrame() {
#if XR_DOCS_CHAPTER_VERSION >= XR_DOCS_CHAPTER_3_2
        // XR_DOCS_TAG_BEGIN_RenderFrame
        // Get the XrFrameState for timing and rendering info.
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        OPENXR_CHECK(xrWaitFrame(m_session, &frameWaitInfo, &frameState), "Failed to wait for XR Frame.");

        // Tell the OpenXR compositor that the application is beginning the frame.
        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        OPENXR_CHECK(xrBeginFrame(m_session, &frameBeginInfo), "Failed to begin the XR Frame.");

        // Variables for rendering and layer composition.
        bool rendered = false;
        RenderLayerInfo renderLayerInfo;
        renderLayerInfo.predictedDisplayTime = frameState.predictedDisplayTime;

        // Check that the session is active and that we should render.
        bool sessionActive = (m_sessionState == XR_SESSION_STATE_SYNCHRONIZED || m_sessionState == XR_SESSION_STATE_VISIBLE || m_sessionState == XR_SESSION_STATE_FOCUSED);
        if (sessionActive && frameState.shouldRender) {
            // Render the stereo image and associate one of swapchain images with the XrCompositionLayerProjection structure.
            rendered = RenderLayer(renderLayerInfo);
            if (rendered) {
                renderLayerInfo.layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&renderLayerInfo.layerProjection));
            }
        }

        // Tell OpenXR that we are finished with this frame; specifying its display time, environment blending and layers.
        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = m_environmentBlendMode;
        frameEndInfo.layerCount = static_cast<uint32_t>(renderLayerInfo.layers.size());
        frameEndInfo.layers = renderLayerInfo.layers.data();
        OPENXR_CHECK(xrEndFrame(m_session, &frameEndInfo), "Failed to end the XR Frame.");
        // XR_DOCS_TAG_END_RenderFrame
#endif
    }

    bool RenderLayer(RenderLayerInfo &renderLayerInfo) {
        // XR_DOCS_TAG_BEGIN_RenderLayer1
        // Locate the views from the view configuration within the (reference) space at the display time.
        std::vector<XrView> views(m_viewConfigurationViews.size(), {XR_TYPE_VIEW});

        XrViewState viewState{XR_TYPE_VIEW_STATE};  // Will contain information on whether the position and/or orientation is valid and/or tracked.
        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.viewConfigurationType = m_viewConfiguration;
        viewLocateInfo.displayTime = renderLayerInfo.predictedDisplayTime;
        viewLocateInfo.space = m_localSpace;
        uint32_t viewCount = 0;
        XrResult result = xrLocateViews(m_session, &viewLocateInfo, &viewState, static_cast<uint32_t>(views.size()), &viewCount, views.data());
        if (result != XR_SUCCESS) {
            XR_TUT_LOG("Failed to locate Views.");
            return false;
        }

        // Resize the layer projection views to match the view count. The layer projection views are used in the layer projection.
        renderLayerInfo.layerProjectionViews.resize(viewCount, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

        m_gltfRenderer->Begin(m_context);

        // Per view in the view configuration:
        for (uint32_t i = 0; i < viewCount; i++) {
            SwapchainInfo &colorSwapchainInfo = m_colorSwapchainInfos[i];
            SwapchainInfo &depthSwapchainInfo = m_depthSwapchainInfos[i];

            // Acquire and wait for an image from the swapchains.
            // Get the image index of an image in the swapchains.
            // The timeout is infinite.
            uint32_t colorImageIndex = 0;
            uint32_t depthImageIndex = 0;
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            OPENXR_CHECK(xrAcquireSwapchainImage(colorSwapchainInfo.swapchain, &acquireInfo, &colorImageIndex), "Failed to acquire Image from the Color Swapchian");
            OPENXR_CHECK(xrAcquireSwapchainImage(depthSwapchainInfo.swapchain, &acquireInfo, &depthImageIndex), "Failed to acquire Image from the Depth Swapchian");

            XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout = XR_INFINITE_DURATION;
            OPENXR_CHECK(xrWaitSwapchainImage(colorSwapchainInfo.swapchain, &waitInfo), "Failed to wait for Image from the Color Swapchain");
            OPENXR_CHECK(xrWaitSwapchainImage(depthSwapchainInfo.swapchain, &waitInfo), "Failed to wait for Image from the Depth Swapchain");

            Diligent::ITextureView *rtv = colorSwapchainInfo.views[colorImageIndex];
            Diligent::ITextureView *dsv = depthSwapchainInfo.views[depthImageIndex];

            // Swap chain images acquired by xrAcquireSwapchainImage are guaranteed to be in
            // COLOR_ATTACHMENT_OPTIMAL/DEPTH_STENCIL_ATTACHMENT_OPTIMAL state.
            rtv->GetTexture()->SetState(Diligent::RESOURCE_STATE_RENDER_TARGET);
            dsv->GetTexture()->SetState(Diligent::RESOURCE_STATE_DEPTH_WRITE);

            // SetRenderTargets sets the viewport and scissor rect
            m_context->SetRenderTargets(1, &rtv, dsv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            XrVector4f gray{0.17f, 0.17f, 0.17f, 1.00f};
            XrVector4f black{0.00f, 0.00f, 0.00f, 1.00f};
            m_context->ClearRenderTarget(rtv, m_environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE ? &gray.x : &black.x,
                                         Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_context->ClearDepthStencil(dsv, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            const uint32_t width = m_viewConfigurationViews[i].recommendedImageRectWidth;
            const uint32_t height = m_viewConfigurationViews[i].recommendedImageRectHeight;
            float nearZ = 0.05f;
            float farZ = 100.0f;

            // Fill out the XrCompositionLayerProjectionView structure specifying the pose and fov from the view.
            // This also associates the swapchain image with this layer projection view.
            renderLayerInfo.layerProjectionViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            renderLayerInfo.layerProjectionViews[i].pose = views[i].pose;
            renderLayerInfo.layerProjectionViews[i].fov = views[i].fov;
            renderLayerInfo.layerProjectionViews[i].subImage.swapchain = colorSwapchainInfo.swapchain;
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.x = 0;
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.offset.y = 0;
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.width = static_cast<int32_t>(width);
            renderLayerInfo.layerProjectionViews[i].subImage.imageRect.extent.height = static_cast<int32_t>(height);
            renderLayerInfo.layerProjectionViews[i].subImage.imageArrayIndex = 0;  // Useful for multiview rendering.

            // XR_DOCS_TAG_END_RenderLayer1

            // XR_DOCS_TAG_BEGIN_SetupFrameRendering

            // Compute the view-projection transform.
            // All matrices (including OpenXR's) are column-major, right-handed.
            XrMatrix4x4f proj;
            XrMatrix4x4f_CreateProjectionFov(&proj, DiligentDeviceTypeToGraphicsAPIType(m_apiType), views[i].fov, nearZ, farZ);
            XrMatrix4x4f toView;
            XrVector3f scale1m{1.0f, 1.0f, 1.0f};
            XrMatrix4x4f_CreateTranslationRotationScale(&toView, &views[i].pose.position, &views[i].pose.orientation, &scale1m);
            XrMatrix4x4f view;
            XrMatrix4x4f_InvertRigidBody(&view, &toView);
            XrMatrix4x4f_Multiply(&cameraConstants.viewProj, &proj, &view);
            // XR_DOCS_TAG_END_SetupFrameRendering

            // XR_DOCS_TAG_BEGIN_CallRenderCuboid
            // Draw a floor. Scale it by 2 in the X and Z, and 0.1 in the Y,
            RenderCuboid({{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -m_viewHeightM, 0.0f}}, {2.0f, 0.1f, 2.0f}, {0.4f, 0.5f, 0.5f});
            // Draw a "table".
            RenderCuboid({{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -m_viewHeightM + 0.9f, -0.7f}}, {1.0f, 0.2f, 1.0f}, {0.6f, 0.6f, 0.4f});
            // XR_DOCS_TAG_END_CallRenderCuboid

            RenderGltfModel(views[i].pose.position, view, proj, nearZ, farZ);

            // XR_DOCS_TAG_BEGIN_RenderLayer2
            //
            // Swap chain images must be in COLOR_ATTACHMENT_OPTIMAL/DEPTH_STENCIL_ATTACHMENT_OPTIMAL state
            // when they are released by xrReleaseSwapchainImage.
            // Since they are already in the correct states, no transitions are necessary.

            // Give the swapchain image back to OpenXR, allowing the compositor to use the image.
            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            OPENXR_CHECK(xrReleaseSwapchainImage(colorSwapchainInfo.swapchain, &releaseInfo), "Failed to release Image back to the Color Swapchain");
            OPENXR_CHECK(xrReleaseSwapchainImage(depthSwapchainInfo.swapchain, &releaseInfo), "Failed to release Image back to the Depth Swapchain");
        }

        // Submit the rendering commands to the GPU.
        m_context->Flush();
        // Normally, the following operations are performed by the engine when the primiary swap chain is presented.
        // Since we are rendering to OpenXR swap chains, we need to perform these operations manually.
        m_context->FinishFrame();
        m_renderDevice.ReleaseStaleResources();

        // Fill out the XrCompositionLayerProjection structure for usage with xrEndFrame().
        renderLayerInfo.layerProjection.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
        renderLayerInfo.layerProjection.space = m_localSpace;
        renderLayerInfo.layerProjection.viewCount = static_cast<uint32_t>(renderLayerInfo.layerProjectionViews.size());
        renderLayerInfo.layerProjection.views = renderLayerInfo.layerProjectionViews.data();

        return true;
        // XR_DOCS_TAG_END_RenderLayer2
    }

#if defined(__ANDROID__)
    // XR_DOCS_TAG_BEGIN_Android_System_Functionality1
public:
    // Stored pointer to the android_app structure from android_main().
    static android_app *androidApp;

    // Custom data structure that is used by PollSystemEvents().
    // Modified from https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/d6b6d7a10bdcf8d4fe806b4f415fde3dd5726878/src/tests/hello_xr/main.cpp#L133C1-L189C2
    struct AndroidAppState {
        ANativeWindow *nativeWindow = nullptr;
        bool resumed = false;
    };
    static AndroidAppState androidAppState;

    // Processes the next command from the Android OS. It updates AndroidAppState.
    static void AndroidAppHandleCmd(struct android_app *app, int32_t cmd) {
        AndroidAppState *appState = (AndroidAppState *)app->userData;

        switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the application thread from onCreate().
        // The application thread then calls android_main().
        case APP_CMD_START: {
            break;
        }
        case APP_CMD_RESUME: {
            appState->resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            appState->resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            break;
        }
        case APP_CMD_DESTROY: {
            appState->nativeWindow = nullptr;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            appState->nativeWindow = app->window;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            appState->nativeWindow = nullptr;
            break;
        }
        }
    }
    // XR_DOCS_TAG_END_Android_System_Functionality1

private:
    void PollSystemEvents() {
        // XR_DOCS_TAG_BEGIN_Android_System_Functionality2
        // Checks whether Android has requested that application should by destroyed.
        if (androidApp->destroyRequested != 0) {
            m_applicationRunning = false;
            return;
        }
        while (true) {
            // Poll and process the Android OS system events.
            struct android_poll_source *source = nullptr;
            int events = 0;
            // The timeout depends on whether the application is active.
            const int timeoutMilliseconds = (!androidAppState.resumed && !m_sessionRunning && androidApp->destroyRequested == 0) ? -1 : 0;
            if (ALooper_pollOnce(timeoutMilliseconds, nullptr, &events, (void **)&source) >= 0) {
                if (source != nullptr) {
                    source->process(androidApp, source);
                }
            } else {
                break;
            }
        }
        // XR_DOCS_TAG_END_Android_System_Functionality2
    }
#else
    void PollSystemEvents() {
        return;
    }
#endif

private:
    XrInstance m_xrInstance = XR_NULL_HANDLE;
    std::vector<const char *> m_activeAPILayers = {};
    std::vector<const char *> m_activeInstanceExtensions = {};
    std::vector<std::string> m_apiLayers = {};
    std::vector<std::string> m_instanceExtensions = {};

    XrDebugUtilsMessengerEXT m_debugUtilsMessenger = XR_NULL_HANDLE;

    XrFormFactor m_formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId m_systemID = {};
    XrSystemProperties m_systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};

    Diligent::RENDER_DEVICE_TYPE m_apiType = Diligent::RENDER_DEVICE_TYPE_UNDEFINED;
    Diligent::RenderDeviceX_N m_renderDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_context;

    XrSession m_session = {};
    XrSessionState m_sessionState = XR_SESSION_STATE_UNKNOWN;
    bool m_applicationRunning = true;
    bool m_sessionRunning = false;

    std::vector<XrViewConfigurationType> m_applicationViewConfigurations = {XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO};
    std::vector<XrViewConfigurationType> m_viewConfigurations;
    XrViewConfigurationType m_viewConfiguration = XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
    std::vector<XrViewConfigurationView> m_viewConfigurationViews;

    struct SwapchainInfo {
        XrSwapchain swapchain = XR_NULL_HANDLE;
        int64_t swapchainFormat = 0;
        std::vector<Diligent::RefCntAutoPtr<Diligent::ITextureView>> views;
    };
    Diligent::TEXTURE_FORMAT m_colorFormat = Diligent::TEX_FORMAT_UNKNOWN;
    Diligent::TEXTURE_FORMAT m_depthFormat = Diligent::TEX_FORMAT_UNKNOWN;

    std::vector<SwapchainInfo> m_colorSwapchainInfos = {};
    std::vector<SwapchainInfo> m_depthSwapchainInfos = {};

    std::vector<XrEnvironmentBlendMode> m_applicationEnvironmentBlendModes = {XR_ENVIRONMENT_BLEND_MODE_OPAQUE, XR_ENVIRONMENT_BLEND_MODE_ADDITIVE};
    std::vector<XrEnvironmentBlendMode> m_environmentBlendModes = {};
    XrEnvironmentBlendMode m_environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;

    XrSpace m_localSpace = XR_NULL_HANDLE;
    struct RenderLayerInfo {
        XrTime predictedDisplayTime = 0;
        std::vector<XrCompositionLayerBaseHeader *> layers;
        XrCompositionLayerProjection layerProjection = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        std::vector<XrCompositionLayerProjectionView> layerProjectionViews;
    };

    // In STAGE space, viewHeightM should be 0. In LOCAL space, it should be offset downwards, below the viewer's initial position.
    float m_viewHeightM = 1.5f;

    // Vertex and index buffers: geometry for our cuboids.
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_vertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_indexBuffer;
    // Camera values constant buffer for the shaders.
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_uniformBuffer_Camera;
    // The normals are stored in a uniform buffer to simplify our vertex geometry.
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_uniformBuffer_Normals;

    // The pipeline is a graphics-API specific state object.
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pipeline;
    // Shader resource binding object encapsulates shader resources required by the pipeline.
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_srb;


    std::unique_ptr<Diligent::GLTF_PBR_Renderer> m_gltfRenderer;
    std::unique_ptr<Diligent::GLTF::Model> m_gltfModel;
    Diligent::GLTF_PBR_Renderer::ModelResourceBindings m_gltfModelResourceBindings;
    Diligent::GLTF::ModelTransforms m_gltfTransforms; 
    Diligent::GLTF_PBR_Renderer::RenderInfo m_gltfRenderInfo;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_frameAttribsCB;
};

void OpenXRTutorial_Main(Diligent::RENDER_DEVICE_TYPE apiType) {
    DebugOutput debugOutput;  // This redirects std::cerr and std::cout to the IDE's output or Android Studio's logcat.
    XR_TUT_LOG("OpenXR Tutorial Chapter 3");

    OpenXRTutorial app(apiType);
    app.Run();
}

#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))
int main(int argc, char **argv) {
    OpenXRTutorial_Main(Diligent::RENDER_DEVICE_TYPE_VULKAN);
}
/*
// XR_DOCS_TAG_BEGIN_main_Windows_Linux_OPENGL
int main(int argc, char **argv) {
    OpenXRTutorial_Main(OPENGL);
}
// XR_DOCS_TAG_END_main_Windows_Linux_OPENGL
// XR_DOCS_TAG_BEGIN_main_Windows_Linux_VULKAN
int main(int argc, char **argv) {
    OpenXRTutorial_Main(VULKAN);
}
// XR_DOCS_TAG_END_main_Windows_Linux_VULKAN
// XR_DOCS_TAG_BEGIN_main_Windows_Linux_D3D11
int main(int argc, char **argv) {
    OpenXRTutorial_Main(D3D11);
}
// XR_DOCS_TAG_END_main_Windows_Linux_D3D11
// XR_DOCS_TAG_BEGIN_main_Windows_Linux_D3D12
int main(int argc, char **argv) {
    OpenXRTutorial_Main(D3D12);
}
// XR_DOCS_TAG_END_main_Windows_Linux_D3D12
*/
#elif (__ANDROID__)
// XR_DOCS_TAG_BEGIN_android_main___ANDROID__
android_app *OpenXRTutorial::androidApp = nullptr;
OpenXRTutorial::AndroidAppState OpenXRTutorial::androidAppState = {};

void android_main(struct android_app *app) {
    // Allow interaction with JNI and the JVM on this thread.
    // https://developer.android.com/training/articles/perf-jni#threads
    JNIEnv *env;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    // https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#XR_KHR_loader_init
    // Load xrInitializeLoaderKHR() function pointer. On Android, the loader must be initialized with variables from android_app *.
    // Without this, there's is no loader and thus our function calls to OpenXR would fail.
    XrInstance m_xrInstance = XR_NULL_HANDLE;  // Dummy XrInstance variable for OPENXR_CHECK macro.
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR = nullptr;
    OPENXR_CHECK(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction *)&xrInitializeLoaderKHR), "Failed to get InstanceProcAddr for xrInitializeLoaderKHR.");
    if (!xrInitializeLoaderKHR) {
        return;
    }

    // Fill out an XrLoaderInitInfoAndroidKHR structure and initialize the loader for Android.
    XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
    loaderInitializeInfoAndroid.applicationVM = app->activity->vm;
    loaderInitializeInfoAndroid.applicationContext = app->activity->clazz;
    OPENXR_CHECK(xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR *)&loaderInitializeInfoAndroid), "Failed to initialize Loader for Android.");

    // Set userData and Callback for PollSystemEvents().
    app->userData = &OpenXRTutorial::androidAppState;
    app->onAppCmd = OpenXRTutorial::AndroidAppHandleCmd;

    OpenXRTutorial::androidApp = app;
    // XR_DOCS_TAG_END_android_main___ANDROID__
    OpenXRTutorial_Main(XR_TUTORIAL_GRAPHICS_API);
}
/*
// XR_DOCS_TAG_BEGIN_android_main_OPENGL_ES
    OpenXRTutorial_Main(OPENGL_ES);
}
// XR_DOCS_TAG_END_android_main_OPENGL_ES
// XR_DOCS_TAG_BEGIN_android_main_VULKAN
    OpenXRTutorial_Main(VULKAN);
}
// XR_DOCS_TAG_END_android_main_VULKAN
*/
#endif
