// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// Offscreen render views: color + depth targets that share the UI texture table, so a panel can draw them.
// The scene content is a textured test cube and ground grid until a real scene renderer exists. Views render in
// linear color through an sRGB attachment view.
#include "pch.h"
#include "RendererVulkan.h"
#include "xgLog.h"
#include "xgShaderTypes.h"
#include "spv.scene.lit.h"
#include "spv.scene.textured.h"
#include "spv.scene.unlit.h"
#include "xgMat4x4.h"
#include <cmath>
#include <cstring>

namespace xg {

    namespace
    {
        using shaders::SceneLitConstants;
        using shaders::SceneLitVertex;
        using shaders::SceneTexturedConstants;
        using shaders::SceneTexturedVertex;
        using shaders::SceneVertex;

        constexpr Color32 Rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        {
            return Color32(r, g, b, a);
        }

        void ViewProjectionLH(const RenderViewCamera& camera, float aspect, float out[16])
        {
            const float nearZ = camera.NearZ > 0.0f ? camera.NearZ : 0.1f;
            const float farZ = camera.FarZ > nearZ ? camera.FarZ : 100.0f;
            const float fovY = camera.FovY > 0.0f ? camera.FovY : 1.0471976f;
            const Mat4x4 viewProjection = Mat4x4::LookAtLH(camera.Eye, camera.Target, camera.Up)
                * Mat4x4::PerspectiveFovLH(fovY, aspect > 0.0f ? aspect : 1.0f, nearZ, farZ);
            std::memcpy(out, &viewProjection, sizeof(float) * 16);
        }

        // The cube's faces: corners in order bottom-left, top-left, top-right, bottom-right as seen from outside, so every
        // face shows the texture upright. Each face keeps its own gray shade (multiplied with the texture).
        void BuildTestCube(std::vector<SceneTexturedVertex>& vertices)
        {
            static const float corners[8][3] =
            {
                { -0.5f, 0.0f, -0.5f }, { 0.5f, 0.0f, -0.5f }, { 0.5f, 1.0f, -0.5f }, { -0.5f, 1.0f, -0.5f },
                { -0.5f, 0.0f,  0.5f }, { 0.5f, 0.0f,  0.5f }, { 0.5f, 1.0f,  0.5f }, { -0.5f, 1.0f,  0.5f },
            };
            static const int faces[6][4] = { { 0, 3, 2, 1 }, { 5, 6, 7, 4 }, { 4, 7, 3, 0 }, { 1, 2, 6, 5 }, { 3, 7, 6, 2 }, { 4, 0, 1, 5 } };
            static const float uvs[4][2] = { { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f } };
            static const Color32 shades[6] =
            {
                Rgba(150, 150, 156), Rgba(120, 120, 126), Rgba(135, 135, 141), Rgba(165, 165, 171), Rgba(200, 200, 206), Rgba(90, 90, 96),
            };

            vertices.clear();
            for (int f = 0; f < 6; ++f)
            {
                const int order[6] = { 0, 1, 2, 0, 2, 3 };
                for (int i : order)
                {
                    const float* c = corners[faces[f][i]];
                    vertices.push_back({ c[0], c[1], c[2], uvs[i][0], uvs[i][1], shades[f] });
                }
            }
        }

        void Multiply4x4(const float* a, const float* b, float* out)
        {
            for (int row = 0; row < 4; ++row)
                for (int column = 0; column < 4; ++column)
                {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; ++k)
                        sum += a[row * 4 + k] * b[k * 4 + column];
                    out[row * 4 + column] = sum;
                }
        }

        float ToLinear(float c)
        {
            return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }

        void BuildTestLines(std::vector<SceneVertex>& vertices, uint32_t& lineVertexCount)
        {
            vertices.clear();
            const float extent = 10.0f;
            for (int i = -10; i <= 10; ++i)
            {
                const float t = static_cast<float>(i);
                const Color32 lineX = i == 0 ? Rgba(208, 72, 72) : Rgba(110, 110, 110, 160);
                const Color32 lineZ = i == 0 ? Rgba(79, 135, 214) : Rgba(110, 110, 110, 160);
                vertices.push_back({ -extent, 0.0f, t, lineX });
                vertices.push_back({ extent, 0.0f, t, lineX });
                vertices.push_back({ t, 0.0f, -extent, lineZ });
                vertices.push_back({ t, 0.0f, extent, lineZ });
            }
            vertices.push_back({ 0.0f, 0.0f, 0.0f, Rgba(111, 160, 74) });
            vertices.push_back({ 0.0f, 2.0f, 0.0f, Rgba(111, 160, 74) });
            lineVertexCount = static_cast<uint32_t>(vertices.size());
        }

        RenderViewCamera DefaultCamera()
        {
            RenderViewCamera camera = {};
            camera.Eye = Vec3(3.0f, 2.5f, -4.0f);
            camera.Target = Vec3(0.0f, 0.5f, 0.0f);
            camera.Up = Vec3(0.0f, 1.0f, 0.0f);
            camera.FovY = 1.0471976f;
            camera.NearZ = 0.1f;
            camera.FarZ = 100.0f;
            camera.ClearColor = Color(0.19f, 0.19f, 0.19f, 1.0f);
            return camera;
        }

        VkImageMemoryBarrier2 ViewBarrier(VkImage image, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout,
            VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess)
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

        void ViewBarriers(VkCommandBuffer commands, const VkImageMemoryBarrier2* barriers, uint32_t count)
        {
            VkDependencyInfo dependency = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
            dependency.imageMemoryBarrierCount = count;
            dependency.pImageMemoryBarriers = barriers;
            vkCmdPipelineBarrier2(commands, &dependency);
        }
    }

    bool RendererVulkan::CreateScenePipeline()
    {
        VkPushConstantRange push = { VK_SHADER_STAGE_VERTEX_BIT, 0, shaders::ConstantsSize };
        VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &push;
        if (!Check(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_scenePipelineLayout), "vkCreatePipelineLayout (scene)"))
            return false;

        const VkShaderModule vertexShader = CreateShaderModule(shaders::spirv::_sceneUnlitVS, sizeof(shaders::spirv::_sceneUnlitVS));
        const VkShaderModule pixelShader = CreateShaderModule(shaders::spirv::_sceneUnlitPS, sizeof(shaders::spirv::_sceneUnlitPS));
        if (!vertexShader || !pixelShader)
        {
            if (vertexShader) vkDestroyShaderModule(_device, vertexShader, nullptr);
            if (pixelShader) vkDestroyShaderModule(_device, pixelShader, nullptr);
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertexShader;
        stages[0].pName = "VSMain";
        stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = pixelShader;
        stages[1].pName = "PSMain";

        VkVertexInputBindingDescription binding = { 0, sizeof(SceneVertex), VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attributes[2] = {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(SceneVertex, X)) },
            { 1, 0, VK_FORMAT_R8G8B8A8_UNORM,   static_cast<uint32_t>(offsetof(SceneVertex, Color)) },
        };
        VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = 2;
        vertexInput.pVertexAttributeDescriptions = attributes;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

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
        depth.depthTestEnable = VK_TRUE;
        depth.depthWriteEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendAttachmentState blend = {};
        blend.blendEnable = VK_TRUE;
        blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp = VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend.alphaBlendOp = VK_BLEND_OP_ADD;
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &blend;

        const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates;

        const VkFormat colorFormat = ViewColorTarget;
        VkPipelineRenderingCreateInfo rendering = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &colorFormat;
        rendering.depthAttachmentFormat = DepthFormat;

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
        info.layout = _scenePipelineLayout;

        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        const bool ok = Check(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &info, nullptr, &_sceneLinePipeline), "vkCreateGraphicsPipelines (scene lines)");
        vkDestroyShaderModule(_device, vertexShader, nullptr);
        vkDestroyShaderModule(_device, pixelShader, nullptr);
        if (!ok)
            return false;

        std::vector<SceneVertex> vertices;
        BuildTestLines(vertices, _sceneLineVertexCount);
        if (!CreateBuffer(vertices.size() * sizeof(SceneVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, _sceneVertexBuffer))
            return false;
        std::memcpy(_sceneVertexBuffer.Mapped, vertices.data(), vertices.size() * sizeof(SceneVertex));

        if (!CreateTexturedPipeline())
            return false;
        std::vector<SceneTexturedVertex> cube;
        BuildTestCube(cube);
        _cubeVertexCount = static_cast<uint32_t>(cube.size());
        if (!CreateBuffer(cube.size() * sizeof(SceneTexturedVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, _cubeVertexBuffer))
            return false;
        std::memcpy(_cubeVertexBuffer.Mapped, cube.data(), cube.size() * sizeof(SceneTexturedVertex));

        const uint8_t white[4] = { 255, 255, 255, 255 };
        const TextureMip mip{ white, sizeof(white), 4 };
        const TextureDesc desc{ 1, 1, 1, TextureFormat::RGBA8, TextureFlags::None };
        _whiteTexture = CreateTexture(&desc, &mip);
        return _whiteTexture != 0 && CreateLitPipeline();
    }

    bool RendererVulkan::CreateLitPipeline()
    {
        VkPushConstantRange push = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SceneLitConstants) };
        VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &_uiSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &push;
        if (!Check(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_litPipelineLayout), "vkCreatePipelineLayout (scene lit)"))
            return false;

        const VkShaderModule vertexShader = CreateShaderModule(shaders::spirv::_sceneLitVS, sizeof(shaders::spirv::_sceneLitVS));
        const VkShaderModule pixelShader = CreateShaderModule(shaders::spirv::_sceneLitPS, sizeof(shaders::spirv::_sceneLitPS));
        if (!vertexShader || !pixelShader)
        {
            if (vertexShader) vkDestroyShaderModule(_device, vertexShader, nullptr);
            if (pixelShader) vkDestroyShaderModule(_device, pixelShader, nullptr);
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertexShader;
        stages[0].pName = "VSMain";
        stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = pixelShader;
        stages[1].pName = "PSMain";

        VkVertexInputBindingDescription binding = { 0, sizeof(SceneLitVertex), VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attributes[5] = {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    static_cast<uint32_t>(offsetof(SceneLitVertex, X)) },
            { 1, 0, VK_FORMAT_R32G32B32_SFLOAT,    static_cast<uint32_t>(offsetof(SceneLitVertex, NX)) },
            { 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(SceneLitVertex, TX)) },
            { 3, 0, VK_FORMAT_R32G32_SFLOAT,       static_cast<uint32_t>(offsetof(SceneLitVertex, U)) },
            { 4, 0, VK_FORMAT_R8G8B8A8_UNORM,      static_cast<uint32_t>(offsetof(SceneLitVertex, Color)) },
        };
        VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = 5;
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
        depth.depthTestEnable = VK_TRUE;
        depth.depthWriteEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        VkPipelineColorBlendAttachmentState blend = {};
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &blend;
        const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates;
        const VkFormat colorFormat = ViewColorTarget;
        VkPipelineRenderingCreateInfo rendering = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &colorFormat;
        rendering.depthAttachmentFormat = DepthFormat;

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
        info.layout = _litPipelineLayout;
        const bool ok = Check(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &info, nullptr, &_litPipeline), "vkCreateGraphicsPipelines (scene lit)");
        vkDestroyShaderModule(_device, vertexShader, nullptr);
        vkDestroyShaderModule(_device, pixelShader, nullptr);
        return ok;
    }

    RendererVulkan::GpuMesh* RendererVulkan::FindMesh(uint64_t id)
    {
        if (id == 0 || id > _meshes.size())
            return nullptr;
        GpuMesh& mesh = _meshes[static_cast<size_t>(id - 1)];
        return mesh.Vertices.Handle ? &mesh : nullptr;
    }

    uint64_t RendererVulkan::CreateMesh(const MeshDesc* desc)
    {
        if (_failed || !desc || !desc->Vertices || !desc->Indices || desc->VertexCount <= 0 || desc->IndexCount <= 0
            || desc->Stride != static_cast<int32_t>(sizeof(SceneLitVertex)))
            return 0;
        const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(desc->VertexCount) * static_cast<VkDeviceSize>(desc->Stride);
        const VkDeviceSize indexBytes = static_cast<VkDeviceSize>(desc->IndexCount) * (desc->Format == IndexFormat::UInt32 ? 4u : 2u);
        GpuMesh mesh;
        if (!CreateBuffer(vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.Vertices)
            || !CreateBuffer(indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.Indices))
        {
            DestroyBuffer(mesh.Vertices);
            DestroyBuffer(mesh.Indices);
            return 0;
        }
        std::memcpy(mesh.Vertices.Mapped, desc->Vertices, static_cast<size_t>(vertexBytes));
        std::memcpy(mesh.Indices.Mapped, desc->Indices, static_cast<size_t>(indexBytes));
        mesh.IndexCount = static_cast<uint32_t>(desc->IndexCount);
        mesh.IndexType = desc->Format == IndexFormat::UInt32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
        uint32_t slot;
        if (!_freeMeshes.empty())
        {
            slot = _freeMeshes.back();
            _freeMeshes.pop_back();
            _meshes[slot] = mesh;
        }
        else
        {
            slot = static_cast<uint32_t>(_meshes.size());
            _meshes.push_back(mesh);
        }
        return static_cast<uint64_t>(slot) + 1;
    }

    void RendererVulkan::DestroyMesh(uint64_t id)
    {
        GpuMesh* mesh = FindMesh(id);
        if (!mesh)
            return;
        vkDeviceWaitIdle(_device);
        DestroyBuffer(mesh->Vertices);
        DestroyBuffer(mesh->Indices);
        *mesh = GpuMesh();
        _freeMeshes.push_back(static_cast<uint32_t>(id - 1));
    }

    void RendererVulkan::SetRenderViewDraws(uint64_t viewId, const RenderViewDraw* draws, uint32_t count)
    {
        RenderView* view = FindRenderView(viewId);
        if (!view)
            return;
        view->Draws.assign(draws, draws ? draws + count : draws);
    }

    bool RendererVulkan::CreateTexturedPipeline()
    {
        VkPushConstantRange push = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SceneTexturedConstants) };
        VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &_uiSetLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &push;
        if (!Check(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_texturedPipelineLayout), "vkCreatePipelineLayout (scene textured)"))
            return false;

        const VkShaderModule vertexShader = CreateShaderModule(shaders::spirv::_sceneTexturedVS, sizeof(shaders::spirv::_sceneTexturedVS));
        const VkShaderModule pixelShader = CreateShaderModule(shaders::spirv::_sceneTexturedPS, sizeof(shaders::spirv::_sceneTexturedPS));
        if (!vertexShader || !pixelShader)
        {
            if (vertexShader) vkDestroyShaderModule(_device, vertexShader, nullptr);
            if (pixelShader) vkDestroyShaderModule(_device, pixelShader, nullptr);
            return false;
        }

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertexShader;
        stages[0].pName = "VSMain";
        stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = pixelShader;
        stages[1].pName = "PSMain";

        VkVertexInputBindingDescription binding = { 0, sizeof(SceneTexturedVertex), VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription attributes[3] = {
            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(SceneTexturedVertex, X)) },
            { 1, 0, VK_FORMAT_R32G32_SFLOAT,    static_cast<uint32_t>(offsetof(SceneTexturedVertex, U)) },
            { 2, 0, VK_FORMAT_R8G8B8A8_UNORM,   static_cast<uint32_t>(offsetof(SceneTexturedVertex, Color)) },
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
        depth.depthTestEnable = VK_TRUE;
        depth.depthWriteEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        VkPipelineColorBlendAttachmentState blend = {};
        blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo colorBlend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlend.attachmentCount = 1;
        colorBlend.pAttachments = &blend;
        const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates;

        const VkFormat colorFormat = ViewColorTarget;
        VkPipelineRenderingCreateInfo rendering = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &colorFormat;
        rendering.depthAttachmentFormat = DepthFormat;

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
        info.layout = _texturedPipelineLayout;
        const bool ok = Check(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &info, nullptr, &_texturedPipeline), "vkCreateGraphicsPipelines (scene textured)");
        vkDestroyShaderModule(_device, vertexShader, nullptr);
        vkDestroyShaderModule(_device, pixelShader, nullptr);
        return ok;
    }

    bool RendererVulkan::CreateRenderViewTargets(RenderView& view, uint32_t width, uint32_t height)
    {
        Image color;
        Image depth;
        if (!CreateImage(width, height, UITextureFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, color,
            1, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT))
            return false;
        VkImageViewCreateInfo targetInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        targetInfo.image = color.Handle;
        targetInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        targetInfo.format = ViewColorTarget;
        targetInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        if (!Check(vkCreateImageView(_device, &targetInfo, nullptr, &color.TargetView), "vkCreateImageView (view target)"))
        {
            DestroyImage(color);
            return false;
        }
        if (!CreateImage(width, height, DepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, depth))
        {
            DestroyImage(color);
            return false;
        }

        const VkCommandBuffer commands = BeginUpload();
        if (!commands)
        {
            DestroyImage(color);
            DestroyImage(depth);
            return false;
        }
        const VkImageMemoryBarrier2 toShader = ViewBarrier(color.Handle, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
        ViewBarriers(commands, &toShader, 1);
        if (!EndUpload())
        {
            DestroyImage(color);
            DestroyImage(depth);
            return false;
        }

        const uint32_t slot = static_cast<uint32_t>(view.Id - 1);
        UITexture& texture = _textures[slot];
        DestroyImage(texture.Target);
        DestroyImage(view.Depth);
        texture.Target = color;
        texture.Width = static_cast<int32_t>(width);
        texture.Height = static_cast<int32_t>(height);
        WriteTextureDescriptor(slot, color.View);
        view.Depth = depth;
        view.Width = width;
        view.Height = height;
        return true;
    }

    RendererVulkan::RenderView* RendererVulkan::FindRenderView(uint64_t id)
    {
        for (RenderView& view : _renderViews)
            if (view.Id == id)
                return &view;
        return nullptr;
    }

    uint64_t RendererVulkan::CreateRenderView(int32_t width, int32_t height)
    {
        if (_failed || !_texturedPipeline || _freeTextureSlots.empty() || _renderViews.size() >= MaxRenderViews)
            return 0;

        RenderView view;
        view.Id = static_cast<uint64_t>(_freeTextureSlots.back()) + 1;
        view.Camera = DefaultCamera();
        if (!CreateRenderViewTargets(view, static_cast<uint32_t>((std::max)(width, 1)), static_cast<uint32_t>((std::max)(height, 1))))
            return 0;

        _freeTextureSlots.pop_back();
        _renderViews.push_back(view);
        return view.Id;
    }

    bool RendererVulkan::ResizeRenderView(uint64_t id, int32_t width, int32_t height)
    {
        RenderView* view = FindRenderView(id);
        if (!view)
            return false;
        const uint32_t w = static_cast<uint32_t>((std::max)(width, 1));
        const uint32_t h = static_cast<uint32_t>((std::max)(height, 1));
        if (w == view->Width && h == view->Height)
            return true;
        vkDeviceWaitIdle(_device);
        return CreateRenderViewTargets(*view, w, h);
    }

    void RendererVulkan::DestroyRenderView(uint64_t id)
    {
        for (auto it = _renderViews.begin(); it != _renderViews.end(); ++it)
        {
            if (it->Id != id)
                continue;
            vkDeviceWaitIdle(_device);
            UITexture& texture = _textures[static_cast<size_t>(id - 1)];
            DestroyImage(texture.Target);
            texture.Width = 0;
            texture.Height = 0;
            DestroyImage(it->Depth);
            _freeTextureSlots.push_back(static_cast<uint32_t>(id - 1));
            _renderViews.erase(it);
            return;
        }
    }

    void RendererVulkan::SetRenderViewCamera(uint64_t id, const RenderViewCamera* camera)
    {
        if (RenderView* view = FindRenderView(id); view && camera)
            view->Camera = *camera;
    }

    void RendererVulkan::SetRenderViewTexture(uint64_t viewId, uint64_t textureId, RenderViewTextureMode mode)
    {
        RenderView* view = FindRenderView(viewId);
        if (!view)
            return;
        view->Texture = textureId != viewId && FindTexture(textureId) ? textureId : 0;
        view->Mode = mode;
    }

    void RendererVulkan::RecordRenderViews()
    {
        if (_renderViews.empty())
            return;

        for (const RenderView& view : _renderViews)
        {
            const Image& color = _textures[static_cast<size_t>(view.Id - 1)].Target;
            const VkImageMemoryBarrier2 toTarget[2] = {
                ViewBarrier(color.Handle, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT),
                ViewBarrier(view.Depth.Handle, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
            };
            ViewBarriers(_frameCommands, toTarget, 2);

            VkRenderingAttachmentInfo colorAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            colorAttachment.imageView = color.TargetView;
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = { { ToLinear(view.Camera.ClearColor.r), ToLinear(view.Camera.ClearColor.g), ToLinear(view.Camera.ClearColor.b), view.Camera.ClearColor.a } };

            VkRenderingAttachmentInfo depthAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            depthAttachment.imageView = view.Depth.View;
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

            VkRenderingInfo rendering = { VK_STRUCTURE_TYPE_RENDERING_INFO };
            rendering.renderArea = { { 0, 0 }, { view.Width, view.Height } };
            rendering.layerCount = 1;
            rendering.colorAttachmentCount = 1;
            rendering.pColorAttachments = &colorAttachment;
            rendering.pDepthAttachment = &depthAttachment;
            vkCmdBeginRendering(_frameCommands, &rendering);

            const float height = static_cast<float>(view.Height);
            const VkViewport viewport = { 0.0f, height, static_cast<float>(view.Width), -height, 0.0f, 1.0f };
            const VkRect2D scissor = { { 0, 0 }, { view.Width, view.Height } };
            vkCmdSetViewport(_frameCommands, 0, 1, &viewport);
            vkCmdSetScissor(_frameCommands, 0, 1, &scissor);

            SceneTexturedConstants constants = {};
            ViewProjectionLH(view.Camera, static_cast<float>(view.Width) / static_cast<float>(view.Height), constants.ViewProjection);
            constants.Mode = static_cast<uint32_t>(view.Mode);
            const uint64_t texture = view.Texture != 0 && FindTexture(view.Texture) ? view.Texture : _whiteTexture;
            const VkDeviceSize offset = 0;

            vkCmdBindPipeline(_frameCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, _texturedPipeline);
            vkCmdBindDescriptorSets(_frameCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, _texturedPipelineLayout, 0, 1,
                &_textureSets[static_cast<size_t>(texture - 1)], 0, nullptr);
            vkCmdPushConstants(_frameCommands, _texturedPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                sizeof(SceneTexturedConstants), &constants);
            vkCmdBindVertexBuffers(_frameCommands, 0, 1, &_cubeVertexBuffer.Handle, &offset);
            vkCmdDraw(_frameCommands, _cubeVertexCount, 1, 0, 0);

            if (!view.Draws.empty() && _litPipeline)
            {
                vkCmdBindPipeline(_frameCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, _litPipeline);
                for (const RenderViewDraw& draw : view.Draws)
                {
                    GpuMesh* mesh = FindMesh(draw.Mesh);
                    if (!mesh)
                        continue;
                    SceneLitConstants lit = {};
                    Multiply4x4(draw.World, constants.ViewProjection, lit.WorldViewProjection);
                    for (int row = 0; row < 3; ++row)
                        for (int k = 0; k < 4; ++k)
                            lit.WorldRows[row * 4 + k] = k < 3 ? draw.World[row * 4 + k] : 0.0f;
                    const uint64_t drawTexture = draw.Texture != 0 && FindTexture(draw.Texture) ? draw.Texture : _whiteTexture;
                    vkCmdBindDescriptorSets(_frameCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, _litPipelineLayout, 0, 1,
                        &_textureSets[static_cast<size_t>(drawTexture - 1)], 0, nullptr);
                    vkCmdPushConstants(_frameCommands, _litPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                        sizeof(SceneLitConstants), &lit);
                    vkCmdBindVertexBuffers(_frameCommands, 0, 1, &mesh->Vertices.Handle, &offset);
                    vkCmdBindIndexBuffer(_frameCommands, mesh->Indices.Handle, 0, mesh->IndexType);
                    vkCmdDrawIndexed(_frameCommands, mesh->IndexCount, 1, 0, 0, 0);
                }
            }

            vkCmdBindPipeline(_frameCommands, VK_PIPELINE_BIND_POINT_GRAPHICS, _sceneLinePipeline);
            vkCmdPushConstants(_frameCommands, _scenePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, shaders::ConstantsSize, constants.ViewProjection);
            vkCmdBindVertexBuffers(_frameCommands, 0, 1, &_sceneVertexBuffer.Handle, &offset);
            vkCmdDraw(_frameCommands, _sceneLineVertexCount, 1, 0, 0);

            vkCmdEndRendering(_frameCommands);

            const VkImageMemoryBarrier2 toShader = ViewBarrier(color.Handle, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
            ViewBarriers(_frameCommands, &toShader, 1);
        }
    }

}
