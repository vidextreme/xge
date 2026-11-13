// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "RendererVulkan.h"
#include "xgWindow.h"
#include "xgLog.h"
#include "xgShaderTypes.h"
#include "spv.ui.textured.h"
#include <cstring>

namespace xg {

    namespace
    {
        const char* _validationLayer = "VK_LAYER_KHRONOS_validation";

        VKAPI_ATTR VkBool32 VKAPI_CALL OnDebugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
        {
            const MessageType type = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? MessageType::Error : MessageType::Warning;
            xg::Log(type, "RendererVulkan (validation): %s", data && data->pMessage ? data->pMessage : "");
            return VK_FALSE;
        }

        bool HasLayer(const char* name)
        {
            uint32_t count = 0;
            vkEnumerateInstanceLayerProperties(&count, nullptr);
            std::vector<VkLayerProperties> layers(count);
            vkEnumerateInstanceLayerProperties(&count, layers.data());
            for (const VkLayerProperties& layer : layers)
                if (std::strcmp(layer.layerName, name) == 0)
                    return true;
            return false;
        }

        bool HasInstanceExtension(const char* name)
        {
            uint32_t count = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
            std::vector<VkExtensionProperties> extensions(count);
            vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
            for (const VkExtensionProperties& extension : extensions)
                if (std::strcmp(extension.extensionName, name) == 0)
                    return true;
            return false;
        }

        bool HasDeviceExtension(VkPhysicalDevice device, const char* name)
        {
            uint32_t count = 0;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
            std::vector<VkExtensionProperties> extensions(count);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
            for (const VkExtensionProperties& extension : extensions)
                if (std::strcmp(extension.extensionName, name) == 0)
                    return true;
            return false;
        }

        VkImageMemoryBarrier2 ImageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
            VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess,
            VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT)
        {
            VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
            barrier.srcStageMask = srcStage;
            barrier.srcAccessMask = srcAccess;
            barrier.dstStageMask = dstStage;
            barrier.dstAccessMask = dstAccess;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange = { aspect, 0, 1, 0, 1 };
            return barrier;
        }

        void PipelineBarrier(VkCommandBuffer commands, const VkImageMemoryBarrier2* barriers, uint32_t count)
        {
            VkDependencyInfo dependency = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            dependency.imageMemoryBarrierCount = count;
            dependency.pImageMemoryBarriers = barriers;
            vkCmdPipelineBarrier2(commands, &dependency);
        }
    }

    RendererVulkan::~RendererVulkan()
    {
        if (_device)
        {
            vkDeviceWaitIdle(_device);

            for (auto& surface : _surfaces)
                DestroySurface(*surface);
            _surfaces.clear();
            DestroySurface(_main);

            for (RenderView& view : _renderViews)
                DestroyImage(view.Depth);
            _renderViews.clear();
            for (UITexture& texture : _textures)
                DestroyImage(texture.Target);
            _textures.clear();

            DestroyBuffer(_sceneVertexBuffer);
            if (_sceneTrianglePipeline) vkDestroyPipeline(_device, _sceneTrianglePipeline, nullptr);
            if (_sceneLinePipeline) vkDestroyPipeline(_device, _sceneLinePipeline, nullptr);
            if (_scenePipelineLayout) vkDestroyPipelineLayout(_device, _scenePipelineLayout, nullptr);

            for (auto& entry : _uiPipelines)
                vkDestroyPipeline(_device, entry.second, nullptr);
            _uiPipelines.clear();
            if (_uiVertexShader) vkDestroyShaderModule(_device, _uiVertexShader, nullptr);
            if (_uiPixelShader) vkDestroyShaderModule(_device, _uiPixelShader, nullptr);
            if (_uiPipelineLayout) vkDestroyPipelineLayout(_device, _uiPipelineLayout, nullptr);
            if (_descriptorPool) vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
            if (_uiSetLayout) vkDestroyDescriptorSetLayout(_device, _uiSetLayout, nullptr);
            if (_sampler) vkDestroySampler(_device, _sampler, nullptr);

            if (_fence) vkDestroyFence(_device, _fence, nullptr);
            if (_commandPool) vkDestroyCommandPool(_device, _commandPool, nullptr);
            vkDestroyDevice(_device, nullptr);
            _device = VK_NULL_HANDLE;
        }
        else
        {
            DestroySurface(_main);
        }

        if (_instance)
        {
            if (_debugMessenger)
                vkDestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
            vkDestroyInstance(_instance, nullptr);
            _instance = VK_NULL_HANDLE;
        }
    }

    bool RendererVulkan::Check(VkResult result, const char* what)
    {
        if (result == VK_SUCCESS)
            return true;
        xg::Log(xg::MessageType::Error, "RendererVulkan: %s failed (VkResult=%d)", what, static_cast<int>(result));
        return false;
    }

    bool RendererVulkan::Initialize(Window& window)
    {
        _main.Hwnd = static_cast<HWND>(window.GetNativeHandle());
        if (!_main.Hwnd)
        {
            xg::Log(xg::MessageType::Error, "RendererVulkan: window has no native handle");
            return false;
        }

        if (!CreateInstance() || !PickPhysicalDevice() || !CreateDevice() || !CreateCommandObjects()
            || !CreateUIPipelineObjects() || !CreateSurface(_main))
            return false;
        if (!CreateScenePipeline())
            xg::Log(xg::MessageType::Warning, "RendererVulkan: scene pipeline unavailable, render views disabled");

        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(_physicalDevice, &properties);
        xg::Log(xg::MessageType::Success, "RendererVulkan: initialized (%ux%u, %s)", _main.Width, _main.Height, properties.deviceName);
        return true;
    }

    bool RendererVulkan::CreateInstance()
    {
        if (volkInitialize() != VK_SUCCESS)
        {
            xg::Log(xg::MessageType::Error, "RendererVulkan: Vulkan loader (vulkan-1.dll) not found");
            return false;
        }
        if (volkGetInstanceVersion() < VK_API_VERSION_1_3)
        {
            xg::Log(xg::MessageType::Error, "RendererVulkan: Vulkan 1.3 is required");
            return false;
        }

        std::vector<const char*> extensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
        std::vector<const char*> layers;
        bool debugUtils = false;
#ifdef _DEBUG
        if (HasLayer(_validationLayer))
            layers.push_back(_validationLayer);
        if (HasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            debugUtils = true;
        }
#endif

        VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
        app.pApplicationName = "xgEngine";
        app.pEngineName = "xgEngine";
        app.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        info.pApplicationInfo = &app;
        info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        info.ppEnabledExtensionNames = extensions.data();
        info.enabledLayerCount = static_cast<uint32_t>(layers.size());
        info.ppEnabledLayerNames = layers.data();
        if (!Check(vkCreateInstance(&info, nullptr, &_instance), "vkCreateInstance"))
            return false;
        volkLoadInstanceOnly(_instance);

        if (debugUtils)
        {
            VkDebugUtilsMessengerCreateInfoEXT messenger = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
            messenger.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            messenger.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            messenger.pfnUserCallback = OnDebugMessage;
            vkCreateDebugUtilsMessengerEXT(_instance, &messenger, nullptr, &_debugMessenger);
        }
        if (!layers.empty())
            xg::Log(xg::MessageType::Info, "RendererVulkan: validation layer enabled");
        return true;
    }

    bool RendererVulkan::PickPhysicalDevice()
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(_instance, &count, nullptr);
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(_instance, &count, devices.data());

        int bestScore = -1;
        for (VkPhysicalDevice device : devices)
        {
            VkPhysicalDeviceProperties properties = {};
            vkGetPhysicalDeviceProperties(device, &properties);
            if (properties.apiVersion < VK_API_VERSION_1_3 || !HasDeviceExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
                continue;

            VkPhysicalDeviceVulkan13Features features13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
            VkPhysicalDeviceFeatures2 features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
            features.pNext = &features13;
            vkGetPhysicalDeviceFeatures2(device, &features);
            if (!features13.dynamicRendering || !features13.synchronization2)
                continue;

            uint32_t familyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
            std::vector<VkQueueFamilyProperties> families(familyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());
            uint32_t family = UINT32_MAX;
            for (uint32_t i = 0; i < familyCount; ++i)
            {
                if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && vkGetPhysicalDeviceWin32PresentationSupportKHR(device, i))
                {
                    family = i;
                    break;
                }
            }
            if (family == UINT32_MAX)
                continue;

            const int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3
                : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : 1;
            if (score > bestScore)
            {
                bestScore = score;
                _physicalDevice = device;
                _queueFamily = family;
            }
        }

        if (!_physicalDevice)
        {
            xg::Log(xg::MessageType::Error, "RendererVulkan: no GPU with Vulkan 1.3, dynamic rendering and Win32 presentation");
            return false;
        }
        vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &_memoryProperties);
        return true;
    }

    bool RendererVulkan::CreateDevice()
    {
        const float priority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        queueInfo.queueFamilyIndex = _queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;

        VkPhysicalDeviceVulkan13Features features13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;

        const char* extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        info.pNext = &features13;
        info.queueCreateInfoCount = 1;
        info.pQueueCreateInfos = &queueInfo;
        info.enabledExtensionCount = 1;
        info.ppEnabledExtensionNames = extensions;
        if (!Check(vkCreateDevice(_physicalDevice, &info, nullptr, &_device), "vkCreateDevice"))
            return false;
        volkLoadDevice(_device);
        vkGetDeviceQueue(_device, _queueFamily, 0, &_queue);
        return true;
    }

    bool RendererVulkan::CreateCommandObjects()
    {
        VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = _queueFamily;
        if (!Check(vkCreateCommandPool(_device, &poolInfo, nullptr, &_commandPool), "vkCreateCommandPool"))
            return false;

        VkCommandBuffer buffers[2] = {};
        VkCommandBufferAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocInfo.commandPool = _commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 2;
        if (!Check(vkAllocateCommandBuffers(_device, &allocInfo, buffers), "vkAllocateCommandBuffers"))
            return false;
        _frameCommands = buffers[0];
        _uploadCommands = buffers[1];

        VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        return Check(vkCreateFence(_device, &fenceInfo, nullptr, &_fence), "vkCreateFence");
    }

    VkShaderModule RendererVulkan::CreateShaderModule(const uint32_t* code, size_t size)
    {
        VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        info.codeSize = size;
        info.pCode = code;
        VkShaderModule module = VK_NULL_HANDLE;
        Check(vkCreateShaderModule(_device, &info, nullptr, &module), "vkCreateShaderModule");
        return module;
    }

    bool RendererVulkan::CreateUIPipelineObjects()
    {
        VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        if (!Check(vkCreateSampler(_device, &samplerInfo, nullptr, &_sampler), "vkCreateSampler"))
            return false;

        VkDescriptorSetLayoutBinding bindings[2] = {};
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = &_sampler;

        VkDescriptorSetLayoutCreateInfo setInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        setInfo.bindingCount = 2;
        setInfo.pBindings = bindings;
        if (!Check(vkCreateDescriptorSetLayout(_device, &setInfo, nullptr, &_uiSetLayout), "vkCreateDescriptorSetLayout"))
            return false;

        VkPushConstantRange push = { VK_SHADER_STAGE_VERTEX_BIT, 0, shaders::ConstantsSize };
        VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &_uiSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &push;
        if (!Check(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_uiPipelineLayout), "vkCreatePipelineLayout (UI)"))
            return false;

        _uiVertexShader = CreateShaderModule(shaders::spirv::_uiTexturedVS, sizeof(shaders::spirv::_uiTexturedVS));
        _uiPixelShader = CreateShaderModule(shaders::spirv::_uiTexturedPS, sizeof(shaders::spirv::_uiTexturedPS));
        if (!_uiVertexShader || !_uiPixelShader)
            return false;

        VkDescriptorPoolSize sizes[2] = { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, TextureSlots }, { VK_DESCRIPTOR_TYPE_SAMPLER, TextureSlots } };
        VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        poolInfo.maxSets = TextureSlots;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = sizes;
        if (!Check(vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_descriptorPool), "vkCreateDescriptorPool"))
            return false;

        std::vector<VkDescriptorSetLayout> layouts(TextureSlots, _uiSetLayout);
        _textureSets.resize(TextureSlots);
        VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = _descriptorPool;
        allocInfo.descriptorSetCount = TextureSlots;
        allocInfo.pSetLayouts = layouts.data();
        if (!Check(vkAllocateDescriptorSets(_device, &allocInfo, _textureSets.data()), "vkAllocateDescriptorSets"))
            return false;

        _textures.resize(TextureSlots);
        _freeTextureSlots.reserve(TextureSlots);
        for (uint32_t i = TextureSlots; i > 0; --i)
            _freeTextureSlots.push_back(i - 1);
        return true;
    }

    VkPipeline RendererVulkan::GetUIPipeline(VkFormat format)
    {
        for (const auto& entry : _uiPipelines)
            if (entry.first == format)
                return entry.second;

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = _uiVertexShader;
        stages[0].pName = "VSMain";
        stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = _uiPixelShader;
        stages[1].pName = "PSMain";

        static_assert(sizeof(UIVertex) == 20, "UIVertex layout must match the UI input layout");
        VkVertexInputBindingDescription binding = { 0, sizeof(UIVertex), VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attributes[3] = {
            { 0, 0, VK_FORMAT_R32G32_SFLOAT,  static_cast<uint32_t>(offsetof(UIVertex, Position)) },
            { 1, 0, VK_FORMAT_R32G32_SFLOAT,  static_cast<uint32_t>(offsetof(UIVertex, UV)) },
            { 2, 0, VK_FORMAT_R8G8B8A8_UNORM, static_cast<uint32_t>(offsetof(UIVertex, Color)) },
        };
        VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = 3;
        vertexInput.pVertexAttributeDescriptions = attributes;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
        raster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

        VkPipelineColorBlendAttachmentState blend = {};
        blend.blendEnable = VK_TRUE;
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp = VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.alphaBlendOp = VK_BLEND_OP_ADD;
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &blend;

        const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates;

        VkPipelineRenderingCreateInfo rendering = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &format;

        VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        info.pNext = &rendering;
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &inputAssembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pDepthStencilState = &depth;
        info.pColorBlendState = &colorBlend;
        info.pDynamicState = &dynamic;
        info.layout = _uiPipelineLayout;

        VkPipeline pipeline = VK_NULL_HANDLE;
        if (!Check(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline), "vkCreateGraphicsPipelines (UI)"))
            return VK_NULL_HANDLE;
        _uiPipelines.emplace_back(format, pipeline);
        return pipeline;
    }

    bool RendererVulkan::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, uint32_t& index) const
    {
        for (uint32_t i = 0; i < _memoryProperties.memoryTypeCount; ++i)
        {
            if ((typeBits & (1u << i)) && (_memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                index = i;
                return true;
            }
        }
        return false;
    }

    bool RendererVulkan::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, Buffer& buffer)
    {
        DestroyBuffer(buffer);

        VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        info.size = size;
        info.usage = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (!Check(vkCreateBuffer(_device, &info, nullptr, &buffer.Handle), "vkCreateBuffer"))
            return false;

        VkMemoryRequirements requirements = {};
        vkGetBufferMemoryRequirements(_device, buffer.Handle, &requirements);
        VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        alloc.allocationSize = requirements.size;
        if (!FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, alloc.memoryTypeIndex))
        {
            xg::Log(xg::MessageType::Error, "RendererVulkan: no host-visible memory type for a buffer");
            DestroyBuffer(buffer);
            return false;
        }
        if (!Check(vkAllocateMemory(_device, &alloc, nullptr, &buffer.Memory), "vkAllocateMemory (buffer)")
            || !Check(vkBindBufferMemory(_device, buffer.Handle, buffer.Memory, 0), "vkBindBufferMemory")
            || !Check(vkMapMemory(_device, buffer.Memory, 0, VK_WHOLE_SIZE, 0, &buffer.Mapped), "vkMapMemory"))
        {
            DestroyBuffer(buffer);
            return false;
        }
        buffer.Size = size;
        return true;
    }

    void RendererVulkan::DestroyBuffer(Buffer& buffer)
    {
        if (buffer.Handle)
            vkDestroyBuffer(_device, buffer.Handle, nullptr);
        if (buffer.Memory)
            vkFreeMemory(_device, buffer.Memory, nullptr);
        buffer = {};
    }

    bool RendererVulkan::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, Image& image)
    {
        VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = { width, height, 1 };
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (!Check(vkCreateImage(_device, &info, nullptr, &image.Handle), "vkCreateImage"))
            return false;

        VkMemoryRequirements requirements = {};
        vkGetImageMemoryRequirements(_device, image.Handle, &requirements);
        VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        alloc.allocationSize = requirements.size;
        if (!FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, alloc.memoryTypeIndex)
            && !FindMemoryType(requirements.memoryTypeBits, 0, alloc.memoryTypeIndex))
        {
            DestroyImage(image);
            return false;
        }
        if (!Check(vkAllocateMemory(_device, &alloc, nullptr, &image.Memory), "vkAllocateMemory (image)")
            || !Check(vkBindImageMemory(_device, image.Handle, image.Memory, 0), "vkBindImageMemory"))
        {
            DestroyImage(image);
            return false;
        }

        VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image = image.Handle;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = { aspect, 0, 1, 0, 1 };
        if (!Check(vkCreateImageView(_device, &viewInfo, nullptr, &image.View), "vkCreateImageView"))
        {
            DestroyImage(image);
            return false;
        }
        return true;
    }

    void RendererVulkan::DestroyImage(Image& image)
    {
        if (image.View)
            vkDestroyImageView(_device, image.View, nullptr);
        if (image.Handle)
            vkDestroyImage(_device, image.Handle, nullptr);
        if (image.Memory)
            vkFreeMemory(_device, image.Memory, nullptr);
        image = {};
    }

    VkCommandBuffer RendererVulkan::BeginUpload()
    {
        VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (!Check(vkResetCommandBuffer(_uploadCommands, 0), "vkResetCommandBuffer (upload)")
            || !Check(vkBeginCommandBuffer(_uploadCommands, &begin), "vkBeginCommandBuffer (upload)"))
            return VK_NULL_HANDLE;
        return _uploadCommands;
    }

    bool RendererVulkan::EndUpload()
    {
        if (!Check(vkEndCommandBuffer(_uploadCommands), "vkEndCommandBuffer (upload)"))
            return false;
        VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &_uploadCommands;
        return Check(vkResetFences(_device, 1, &_fence), "vkResetFences (upload)")
            && Check(vkQueueSubmit(_queue, 1, &submit, _fence), "vkQueueSubmit (upload)")
            && Check(vkWaitForFences(_device, 1, &_fence, VK_TRUE, UINT64_MAX), "vkWaitForFences (upload)");
    }

    bool RendererVulkan::CreateSurface(Surface& surface)
    {
        VkWin32SurfaceCreateInfoKHR info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
        info.hinstance = GetModuleHandleW(nullptr);
        info.hwnd = surface.Hwnd;
        if (!Check(vkCreateWin32SurfaceKHR(_instance, &info, nullptr, &surface.Handle), "vkCreateWin32SurfaceKHR"))
            return false;

        VkBool32 supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(_physicalDevice, _queueFamily, surface.Handle, &supported);
        if (!supported)
        {
            xg::Log(xg::MessageType::Error, "RendererVulkan: the graphics queue cannot present to this window");
            return false;
        }

        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, surface.Handle, &count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, surface.Handle, &count, formats.data());
        if (formats.empty())
        {
            xg::Log(xg::MessageType::Error, "RendererVulkan: surface reports no formats");
            return false;
        }
        surface.Format = formats[0].format;
        for (VkFormat preferred : { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM })
        {
            bool found = false;
            for (const VkSurfaceFormatKHR& format : formats)
                if (format.format == preferred && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                    found = true;
            if (found)
            {
                surface.Format = preferred;
                break;
            }
        }

        vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, surface.Handle, &count, nullptr);
        std::vector<VkPresentModeKHR> modes(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, surface.Handle, &count, modes.data());
        surface.PresentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (surface.Immediate)
        {
            for (VkPresentModeKHR preferred : { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR })
            {
                bool found = false;
                for (VkPresentModeKHR mode : modes)
                    found = found || mode == preferred;
                if (found)
                {
                    surface.PresentMode = preferred;
                    break;
                }
            }
        }

        VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        if (!Check(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &surface.Acquired), "vkCreateSemaphore"))
            return false;

        RECT rect = {};
        GetClientRect(surface.Hwnd, &rect);
        surface.Width = static_cast<uint32_t>((std::max)(1L, rect.right - rect.left));
        surface.Height = static_cast<uint32_t>((std::max)(1L, rect.bottom - rect.top));
        return CreateSwapchain(surface);
    }

    bool RendererVulkan::CreateSwapchain(Surface& surface)
    {
        VkSurfaceCapabilitiesKHR caps = {};
        if (!Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, surface.Handle, &caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"))
            return false;

        VkExtent2D extent = caps.currentExtent;
        if (extent.width == UINT32_MAX)
        {
            extent.width = (std::min)((std::max)(surface.Width, caps.minImageExtent.width), caps.maxImageExtent.width);
            extent.height = (std::min)((std::max)(surface.Height, caps.minImageExtent.height), caps.maxImageExtent.height);
        }
        if (extent.width == 0 || extent.height == 0)
            return true;

        uint32_t imageCount = (std::max)(caps.minImageCount + 1, 2u);
        if (caps.maxImageCount > 0)
            imageCount = (std::min)(imageCount, caps.maxImageCount);

        VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        if (!(caps.supportedCompositeAlpha & alpha))
        {
            for (VkCompositeAlphaFlagBitsKHR candidate : { VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR })
            {
                if (caps.supportedCompositeAlpha & candidate)
                {
                    alpha = candidate;
                    break;
                }
            }
        }

        VkSwapchainKHR old = surface.Swapchain;
        VkSwapchainCreateInfoKHR info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        info.surface = surface.Handle;
        info.minImageCount = imageCount;
        info.imageFormat = surface.Format;
        info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = alpha;
        info.presentMode = surface.PresentMode;
        info.clipped = VK_TRUE;
        info.oldSwapchain = old;
        const VkResult result = vkCreateSwapchainKHR(_device, &info, nullptr, &surface.Swapchain);

        DestroySwapchainViews(surface);
        if (old)
            vkDestroySwapchainKHR(_device, old, nullptr);
        if (!Check(result, "vkCreateSwapchainKHR"))
        {
            surface.Swapchain = VK_NULL_HANDLE;
            return false;
        }

        surface.Width = extent.width;
        surface.Height = extent.height;
        surface.Stale = false;

        uint32_t count = 0;
        vkGetSwapchainImagesKHR(_device, surface.Swapchain, &count, nullptr);
        surface.Images.resize(count);
        vkGetSwapchainImagesKHR(_device, surface.Swapchain, &count, surface.Images.data());

        VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        for (VkImage image : surface.Images)
        {
            VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = surface.Format;
            viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            VkImageView view = VK_NULL_HANDLE;
            VkSemaphore finished = VK_NULL_HANDLE;
            if (!Check(vkCreateImageView(_device, &viewInfo, nullptr, &view), "vkCreateImageView (swapchain)")
                || !Check(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &finished), "vkCreateSemaphore (swapchain)"))
                return false;
            surface.Views.push_back(view);
            surface.RenderFinished.push_back(finished);
        }
        return true;
    }

    void RendererVulkan::DestroySwapchainViews(Surface& surface)
    {
        for (VkImageView view : surface.Views)
            vkDestroyImageView(_device, view, nullptr);
        for (VkSemaphore semaphore : surface.RenderFinished)
            vkDestroySemaphore(_device, semaphore, nullptr);
        surface.Views.clear();
        surface.RenderFinished.clear();
        surface.Images.clear();
    }

    void RendererVulkan::DestroySurface(Surface& surface)
    {
        if (_device)
        {
            DestroySwapchainViews(surface);
            if (surface.Swapchain)
                vkDestroySwapchainKHR(_device, surface.Swapchain, nullptr);
            if (surface.Acquired)
                vkDestroySemaphore(_device, surface.Acquired, nullptr);
            DestroyBuffer(surface.VertexBuffer);
            DestroyBuffer(surface.IndexBuffer);
        }
        if (surface.Handle && _instance)
            vkDestroySurfaceKHR(_instance, surface.Handle, nullptr);
        surface.Swapchain = VK_NULL_HANDLE;
        surface.Acquired = VK_NULL_HANDLE;
        surface.Handle = VK_NULL_HANDLE;
        surface.VertexCapacity = 0;
        surface.IndexCapacity = 0;
    }

    bool RendererVulkan::ResizeIfNeeded(Surface& surface, bool& skipFrame)
    {
        skipFrame = false;
        RECT rect = {};
        if (IsIconic(surface.Hwnd) || !GetClientRect(surface.Hwnd, &rect)
            || rect.right <= rect.left || rect.bottom <= rect.top)
        {
            skipFrame = true;
            return true;
        }

        const uint32_t width = static_cast<uint32_t>(rect.right - rect.left);
        const uint32_t height = static_cast<uint32_t>(rect.bottom - rect.top);
        if (surface.Swapchain && !surface.Stale && width == surface.Width && height == surface.Height)
            return true;

        vkDeviceWaitIdle(_device);
        surface.Width = width;
        surface.Height = height;
        if (!CreateSwapchain(surface))
            return false;
        skipFrame = surface.Swapchain == VK_NULL_HANDLE;
        return true;
    }

    bool RendererVulkan::EnsureUIBuffers(Surface& surface, size_t vertexCount, size_t indexCount)
    {
        if (vertexCount > surface.VertexCapacity)
        {
            const size_t capacity = vertexCount + 5000;
            if (!CreateBuffer(capacity * sizeof(UIVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, surface.VertexBuffer))
                return false;
            surface.VertexCapacity = capacity;
        }
        if (indexCount > surface.IndexCapacity)
        {
            const size_t capacity = indexCount + 10000;
            if (!CreateBuffer(capacity * sizeof(uint16_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, surface.IndexBuffer))
                return false;
            surface.IndexCapacity = capacity;
        }
        return true;
    }

    void RendererVulkan::RecordUI(Surface& surface)
    {
        if (surface.Commands.empty() || surface.Vertices.empty() || surface.Indices.empty())
            return;
        if (surface.Display[2] <= 0.0f || surface.Display[3] <= 0.0f)
            return;
        if (!EnsureUIBuffers(surface, surface.Vertices.size(), surface.Indices.size()))
            return;
        const VkPipeline pipeline = GetUIPipeline(surface.Format);
        if (!pipeline)
            return;

        std::memcpy(surface.VertexBuffer.Mapped, surface.Vertices.data(), surface.Vertices.size() * sizeof(UIVertex));
        std::memcpy(surface.IndexBuffer.Mapped, surface.Indices.data(), surface.Indices.size() * sizeof(uint16_t));

        const float left = surface.Display[0];
        const float right = surface.Display[0] + surface.Display[2];
        const float top = surface.Display[1];
        const float bottom = surface.Display[1] + surface.Display[3];
        const float projection[4][4] =
        {
            { 2.0f / (right - left),           0.0f,                            0.0f, 0.0f },
            { 0.0f,                            2.0f / (top - bottom),           0.0f, 0.0f },
            { 0.0f,                            0.0f,                            0.5f, 0.0f },
            { (right + left) / (left - right), (top + bottom) / (bottom - top), 0.5f, 1.0f },
        };

        const float viewportWidth = (std::min)(surface.Display[2] * surface.Scale[0], static_cast<float>(surface.Width));
        const float viewportHeight = (std::min)(surface.Display[3] * surface.Scale[1], static_cast<float>(surface.Height));

        const float fullHeight = surface.Display[3] * surface.Scale[1];
        const VkViewport viewport = { 0.0f, fullHeight, surface.Display[2] * surface.Scale[0], -fullHeight, 0.0f, 1.0f };
        vkCmdSetViewport(_frameCommands, 0, 1, &viewport);

        vkCmdBindPipeline(_frameCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(_frameCommands, 0, 1, &surface.VertexBuffer.Handle, &offset);
        vkCmdBindIndexBuffer(_frameCommands, surface.IndexBuffer.Handle, 0, VK_INDEX_TYPE_UINT16);
        vkCmdPushConstants(_frameCommands, _uiPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, shaders::ConstantsSize, projection);

        for (const UIDrawCommand& command : surface.Commands)
        {
            if (command.IndexCount == 0 || !FindTexture(command.TextureId))
                continue;

            const float minX = (std::max)(0.0f, (command.Clip.min.x - surface.Display[0]) * surface.Scale[0]);
            const float minY = (std::max)(0.0f, (command.Clip.min.y - surface.Display[1]) * surface.Scale[1]);
            const float maxX = (std::min)(viewportWidth, (command.Clip.max.x - surface.Display[0]) * surface.Scale[0]);
            const float maxY = (std::min)(viewportHeight, (command.Clip.max.y - surface.Display[1]) * surface.Scale[1]);
            if (maxX <= minX || maxY <= minY)
                continue;

            const int32_t x0 = static_cast<int32_t>(minX);
            const int32_t y0 = static_cast<int32_t>(minY);
            const int32_t x1 = static_cast<int32_t>(maxX);
            const int32_t y1 = static_cast<int32_t>(maxY);
            if (x1 <= x0 || y1 <= y0)
                continue;
            const VkRect2D scissor = { { x0, y0 }, { static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0) } };
            vkCmdSetScissor(_frameCommands, 0, 1, &scissor);

            vkCmdBindDescriptorSets(_frameCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, _uiPipelineLayout, 0, 1,
                &_textureSets[static_cast<size_t>(command.TextureId - 1)], 0, nullptr);
            vkCmdDrawIndexed(_frameCommands, command.IndexCount, 1, command.IndexOffset, static_cast<int32_t>(command.VertexOffset), 0);
        }
    }

    void RendererVulkan::RecordSurface(Surface& surface)
    {
        const VkImage image = surface.Images[surface.ImageIndex];
        const VkImageMemoryBarrier2 toTarget = ImageBarrier(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
        PipelineBarrier(_frameCommands, &toTarget, 1);

        VkRenderingAttachmentInfo color = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        color.imageView = surface.Views[surface.ImageIndex];
        color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        std::memcpy(color.clearValue.color.float32, _clearColor, sizeof(_clearColor));

        VkRenderingInfo rendering = { VK_STRUCTURE_TYPE_RENDERING_INFO };
        rendering.renderArea = { { 0, 0 }, { surface.Width, surface.Height } };
        rendering.layerCount = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments = &color;
        vkCmdBeginRendering(_frameCommands, &rendering);
        RecordUI(surface);
        vkCmdEndRendering(_frameCommands);

        const VkImageMemoryBarrier2 toPresent = ImageBarrier(image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_NONE, 0);
        PipelineBarrier(_frameCommands, &toPresent, 1);
    }

    bool RendererVulkan::DrawSurface(Surface& surface, bool withRenderViews)
    {
        if (!ResizeIfNeeded(surface, surface.Skip))
            return false;
        if (surface.Skip)
            return true;

        VkResult result = vkAcquireNextImageKHR(_device, surface.Swapchain, UINT64_MAX, surface.Acquired, VK_NULL_HANDLE, &surface.ImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            surface.Stale = true;
            return true;
        }
        if (result == VK_SUBOPTIMAL_KHR)
            surface.Stale = true;
        else if (!Check(result, "vkAcquireNextImageKHR"))
            return false;

        VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (!Check(vkResetCommandBuffer(_frameCommands, 0), "vkResetCommandBuffer")
            || !Check(vkBeginCommandBuffer(_frameCommands, &begin), "vkBeginCommandBuffer"))
            return false;

        if (withRenderViews)
            RecordRenderViews();
        RecordSurface(surface);

        if (!Check(vkEndCommandBuffer(_frameCommands), "vkEndCommandBuffer"))
            return false;

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSemaphore finished = surface.RenderFinished[surface.ImageIndex];
        VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &surface.Acquired;
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &_frameCommands;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &finished;
        if (!Check(vkResetFences(_device, 1, &_fence), "vkResetFences")
            || !Check(vkQueueSubmit(_queue, 1, &submit, _fence), "vkQueueSubmit"))
            return false;

        VkPresentInfoKHR present = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &finished;
        present.swapchainCount = 1;
        present.pSwapchains = &surface.Swapchain;
        present.pImageIndices = &surface.ImageIndex;
        result = vkQueuePresentKHR(_queue, &present);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
            surface.Stale = true;
        else if (!Check(result, "vkQueuePresentKHR"))
            return false;

        return Check(vkWaitForFences(_device, 1, &_fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    }

    void RendererVulkan::Render()
    {
        if (_failed)
            return;
        if (!DrawSurface(_main, true))
            _failed = true;
    }

    void* RendererVulkan::GetNativeHandle() const
    {
        return _device;
    }

    RendererVulkan::UITexture* RendererVulkan::FindTexture(uint64_t id)
    {
        if (id == 0 || id > _textures.size())
            return nullptr;
        UITexture& texture = _textures[static_cast<size_t>(id - 1)];
        return texture.Target.Handle ? &texture : nullptr;
    }

    void RendererVulkan::WriteTextureDescriptor(uint32_t slot, VkImageView view)
    {
        VkDescriptorImageInfo image = {};
        image.imageView = view;
        image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet = _textureSets[slot];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &image;
        vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);
    }

    bool RendererVulkan::UploadTextureRegion(VkImage image, int32_t x, int32_t y, int32_t width, int32_t height,
        const void* rgba, int32_t pitch, bool firstUpload)
    {
        const size_t rowBytes = static_cast<size_t>(width) * 4;
        Buffer staging;
        if (!CreateBuffer(rowBytes * height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging))
            return false;
        const uint8_t* source = static_cast<const uint8_t*>(rgba);
        for (int32_t row = 0; row < height; ++row)
            std::memcpy(static_cast<uint8_t*>(staging.Mapped) + static_cast<size_t>(row) * rowBytes, source + static_cast<size_t>(row) * pitch, rowBytes);

        const VkCommandBuffer commands = BeginUpload();
        if (!commands)
        {
            DestroyBuffer(staging);
            return false;
        }

        const VkImageMemoryBarrier2 toCopy = ImageBarrier(image,
            firstUpload ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            firstUpload ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 0,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        PipelineBarrier(commands, &toCopy, 1);

        VkBufferImageCopy region = {};
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageOffset = { x, y, 0 };
        region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
        vkCmdCopyBufferToImage(commands, staging.Handle, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        const VkImageMemoryBarrier2 toShader = ImageBarrier(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        PipelineBarrier(commands, &toShader, 1);

        const bool ok = EndUpload();
        DestroyBuffer(staging);
        return ok;
    }

    uint64_t RendererVulkan::CreateUITexture(int32_t width, int32_t height, const void* rgba)
    {
        if (_failed || width <= 0 || height <= 0 || !rgba || _freeTextureSlots.empty())
            return 0;

        Image image;
        if (!CreateImage(static_cast<uint32_t>(width), static_cast<uint32_t>(height), TextureFormat,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, image))
            return 0;
        if (!UploadTextureRegion(image.Handle, 0, 0, width, height, rgba, width * 4, true))
        {
            DestroyImage(image);
            return 0;
        }

        const uint32_t slot = _freeTextureSlots.back();
        _freeTextureSlots.pop_back();
        WriteTextureDescriptor(slot, image.View);

        UITexture& texture = _textures[slot];
        texture.Target = image;
        texture.Width = width;
        texture.Height = height;
        return static_cast<uint64_t>(slot) + 1;
    }

    void RendererVulkan::UpdateUITexture(uint64_t id, int32_t x, int32_t y, int32_t width, int32_t height, const void* rgba, int32_t pitch)
    {
        UITexture* texture = FindTexture(id);
        if (!texture || !rgba || width <= 0 || height <= 0 || FindRenderView(id))
            return;
        if (x < 0 || y < 0 || x + width > texture->Width || y + height > texture->Height)
            return;
        UploadTextureRegion(texture->Target.Handle, x, y, width, height, rgba, pitch, false);
    }

    void RendererVulkan::DestroyUITexture(uint64_t id)
    {
        UITexture* texture = FindTexture(id);
        if (!texture || FindRenderView(id))
            return;
        vkDeviceWaitIdle(_device);
        DestroyImage(texture->Target);
        texture->Width = 0;
        texture->Height = 0;
        _freeTextureSlots.push_back(static_cast<uint32_t>(id - 1));
    }

    void RendererVulkan::StoreDrawList(Surface& surface, const UIDrawList* list)
    {
        surface.Vertices.clear();
        surface.Indices.clear();
        surface.Commands.clear();
        if (!list)
            return;

        if (list->Vertices && list->VertexCount)
            surface.Vertices.assign(list->Vertices, list->Vertices + list->VertexCount);
        if (list->Indices && list->IndexCount)
            surface.Indices.assign(list->Indices, list->Indices + list->IndexCount);
        if (list->Commands && list->CommandCount)
            surface.Commands.assign(list->Commands, list->Commands + list->CommandCount);

        surface.Display[0] = list->DisplayPos.x;
        surface.Display[1] = list->DisplayPos.y;
        surface.Display[2] = list->DisplaySize.x;
        surface.Display[3] = list->DisplaySize.y;
        surface.Scale[0] = list->FramebufferScale.x > 0.0f ? list->FramebufferScale.x : 1.0f;
        surface.Scale[1] = list->FramebufferScale.y > 0.0f ? list->FramebufferScale.y : 1.0f;
    }

    void RendererVulkan::SubmitUI(const UIDrawList* list)
    {
        StoreDrawList(_main, list);
    }

    RendererVulkan::Surface* RendererVulkan::FindSurface(uint64_t id)
    {
        for (auto& surface : _surfaces)
            if (surface->Id == id)
                return surface.get();
        return nullptr;
    }

    uint64_t RendererVulkan::CreateUISurface(Window& window)
    {
        if (_failed)
            return 0;
        auto surface = std::make_unique<Surface>();
        surface->Hwnd = static_cast<HWND>(window.GetNativeHandle());
        surface->Immediate = true;
        if (!surface->Hwnd || !CreateSurface(*surface))
        {
            DestroySurface(*surface);
            return 0;
        }
        surface->Id = _nextSurfaceId++;
        const uint64_t id = surface->Id;
        _surfaces.push_back(std::move(surface));
        return id;
    }

    void RendererVulkan::DestroyUISurface(uint64_t surface)
    {
        for (auto it = _surfaces.begin(); it != _surfaces.end(); ++it)
        {
            if ((*it)->Id != surface)
                continue;
            vkDeviceWaitIdle(_device);
            DestroySurface(**it);
            _surfaces.erase(it);
            return;
        }
    }

    void RendererVulkan::SubmitUISurface(uint64_t surface, const UIDrawList* list)
    {
        if (_failed)
            return;
        if (Surface* target = FindSurface(surface))
        {
            StoreDrawList(*target, list);
            DrawSurface(*target, false);
        }
    }

    void RendererVulkan::SetClearColor(const Color& color)
    {
        _clearColor[0] = color.r;
        _clearColor[1] = color.g;
        _clearColor[2] = color.b;
        _clearColor[3] = color.a;
    }

}
