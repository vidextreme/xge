// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "UIRenderTargetSDLGPU.h"
#include "xgWindow.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include "shaders/UIShaderBlobs.h"

namespace xg {

    namespace
    {
        struct UIConstants
        {
            float Scale[2];
            float Translate[2];
        };

        uint32_t AlignUp(uint32_t value, uint32_t alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }
    }

    UIRenderTargetSDLGPU::UIRenderTargetSDLGPU(SDL_Window* window)
    {
        _main.Window = window;
        if (!_main.Window)
            return;

        _device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_DXBC, false, nullptr);
        if (!_device)
        {
            SDL_Log("xgPlatform: SDL_CreateGPUDevice failed: %s", SDL_GetError());
            return;
        }
        if (!SDL_ClaimWindowForGPUDevice(_device, _main.Window))
        {
            SDL_Log("xgPlatform: SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
            SDL_DestroyGPUDevice(_device);
            _device = nullptr;
            return;
        }
        SDL_SetGPUSwapchainParameters(_device, _main.Window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);

        if (!CreatePipeline())
            SDL_Log("xgPlatform: UI pipeline creation failed: %s", SDL_GetError());
    }

    UIRenderTargetSDLGPU::~UIRenderTargetSDLGPU()
    {
        if (!_device)
            return;

        SDL_WaitForGPUIdle(_device);
        for (auto& entry : _textures)
            SDL_ReleaseGPUTexture(_device, entry.second);
        _textures.clear();

        for (auto& entry : _surfaces)
        {
            ReleaseBuffers(entry.second);
            SDL_ReleaseWindowFromGPUDevice(_device, entry.second.Window);
        }
        _surfaces.clear();
        ReleaseBuffers(_main);
        if (_sampler) SDL_ReleaseGPUSampler(_device, _sampler);
        if (_pipeline) SDL_ReleaseGPUGraphicsPipeline(_device, _pipeline);

        SDL_ReleaseWindowFromGPUDevice(_device, _main.Window);
        SDL_DestroyGPUDevice(_device);
        _device = nullptr;
    }

    bool UIRenderTargetSDLGPU::CreatePipeline()
    {
        const bool dxil = (SDL_GetGPUShaderFormats(_device) & SDL_GPU_SHADERFORMAT_DXIL) != 0;

        SDL_GPUShaderCreateInfo vertexInfo = {};
        vertexInfo.code = dxil ? UIVertexDXIL : UIVertexDXBC;
        vertexInfo.code_size = dxil ? sizeof(UIVertexDXIL) : sizeof(UIVertexDXBC);
        vertexInfo.entrypoint = "VSMain";
        vertexInfo.format = dxil ? SDL_GPU_SHADERFORMAT_DXIL : SDL_GPU_SHADERFORMAT_DXBC;
        vertexInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vertexInfo.num_uniform_buffers = 1;

        SDL_GPUShaderCreateInfo fragmentInfo = {};
        fragmentInfo.code = dxil ? UIFragmentDXIL : UIFragmentDXBC;
        fragmentInfo.code_size = dxil ? sizeof(UIFragmentDXIL) : sizeof(UIFragmentDXBC);
        fragmentInfo.entrypoint = "PSMain";
        fragmentInfo.format = vertexInfo.format;
        fragmentInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fragmentInfo.num_samplers = 1;

        SDL_GPUShader* vertexShader = SDL_CreateGPUShader(_device, &vertexInfo);
        SDL_GPUShader* fragmentShader = SDL_CreateGPUShader(_device, &fragmentInfo);
        if (!vertexShader || !fragmentShader)
        {
            if (vertexShader) SDL_ReleaseGPUShader(_device, vertexShader);
            if (fragmentShader) SDL_ReleaseGPUShader(_device, fragmentShader);
            return false;
        }

        SDL_GPUVertexBufferDescription bufferDesc = {};
        bufferDesc.slot = 0;
        bufferDesc.pitch = sizeof(UIVertex);
        bufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

        SDL_GPUVertexAttribute attributes[3] = {};
        attributes[0].location = 0;
        attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[0].offset = offsetof(UIVertex, Position);
        attributes[1].location = 1;
        attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attributes[1].offset = offsetof(UIVertex, UV);
        attributes[2].location = 2;
        attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
        attributes[2].offset = offsetof(UIVertex, Color);

        SDL_GPUColorTargetDescription colorTarget = {};
        colorTarget.format = SDL_GetGPUSwapchainTextureFormat(_device, _main.Window);
        _swapchainFormat = static_cast<int>(colorTarget.format);
        colorTarget.blend_state.enable_blend = true;
        colorTarget.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colorTarget.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorTarget.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        colorTarget.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colorTarget.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorTarget.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertexShader;
        pipelineInfo.fragment_shader = fragmentShader;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &bufferDesc;
        pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
        pipelineInfo.vertex_input_state.vertex_attributes = attributes;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 3;
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pipelineInfo.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        pipelineInfo.target_info.color_target_descriptions = &colorTarget;
        pipelineInfo.target_info.num_color_targets = 1;

        _pipeline = SDL_CreateGPUGraphicsPipeline(_device, &pipelineInfo);
        SDL_ReleaseGPUShader(_device, vertexShader);
        SDL_ReleaseGPUShader(_device, fragmentShader);
        if (!_pipeline)
            return false;

        SDL_GPUSamplerCreateInfo samplerInfo = {};
        samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        _sampler = SDL_CreateGPUSampler(_device, &samplerInfo);
        return _sampler != nullptr;
    }

    void UIRenderTargetSDLGPU::Upload(SDL_GPUTexture* texture, int32_t x, int32_t y, int32_t width, int32_t height, const void* rgba, int32_t pitch)
    {
        const uint32_t rowBytes = static_cast<uint32_t>(width) * 4;
        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = rowBytes * static_cast<uint32_t>(height);
        SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(_device, &transferInfo);
        if (!transfer)
            return;

        uint8_t* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(_device, transfer, false));
        const uint8_t* source = static_cast<const uint8_t*>(rgba);
        for (int32_t row = 0; row < height; ++row)
            std::memcpy(mapped + static_cast<size_t>(row) * rowBytes, source + static_cast<size_t>(row) * pitch, rowBytes);
        SDL_UnmapGPUTransferBuffer(_device, transfer);

        SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(_device);
        SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
        SDL_GPUTextureTransferInfo sourceInfo = {};
        sourceInfo.transfer_buffer = transfer;
        sourceInfo.pixels_per_row = static_cast<uint32_t>(width);
        sourceInfo.rows_per_layer = static_cast<uint32_t>(height);
        SDL_GPUTextureRegion region = {};
        region.texture = texture;
        region.x = static_cast<uint32_t>(x);
        region.y = static_cast<uint32_t>(y);
        region.w = static_cast<uint32_t>(width);
        region.h = static_cast<uint32_t>(height);
        region.d = 1;
        SDL_UploadToGPUTexture(copy, &sourceInfo, &region, false);
        SDL_EndGPUCopyPass(copy);
        SDL_SubmitGPUCommandBuffer(commands);
        SDL_ReleaseGPUTransferBuffer(_device, transfer);
    }

    uint64_t UIRenderTargetSDLGPU::CreateUITexture(int32_t width, int32_t height, const void* rgba)
    {
        if (!_device || width <= 0 || height <= 0)
            return 0;

        SDL_GPUTextureCreateInfo info = {};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = static_cast<uint32_t>(width);
        info.height = static_cast<uint32_t>(height);
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        SDL_GPUTexture* texture = SDL_CreateGPUTexture(_device, &info);
        if (!texture)
            return 0;

        if (rgba)
            Upload(texture, 0, 0, width, height, rgba, width * 4);

        const uint64_t id = _nextTextureId++;
        _textures[id] = texture;
        return id;
    }

    void UIRenderTargetSDLGPU::UpdateUITexture(uint64_t id, int32_t x, int32_t y, int32_t width, int32_t height, const void* rgba, int32_t pitch)
    {
        auto it = _textures.find(id);
        if (it == _textures.end() || !rgba || width <= 0 || height <= 0)
            return;
        Upload(it->second, x, y, width, height, rgba, pitch);
    }

    void UIRenderTargetSDLGPU::DestroyUITexture(uint64_t id)
    {
        auto it = _textures.find(id);
        if (it == _textures.end())
            return;
        SDL_ReleaseGPUTexture(_device, it->second);
        _textures.erase(it);
    }

    bool UIRenderTargetSDLGPU::EnsureBuffers(Surface& surface, uint32_t vertexBytes, uint32_t indexBytes)
    {
        if (vertexBytes > surface.VertexCapacity || indexBytes > surface.IndexCapacity)
        {
            SDL_WaitForGPUIdle(_device);
            ReleaseBuffers(surface);

            surface.VertexCapacity = (std::max)(AlignUp(vertexBytes + vertexBytes / 2, 4), surface.VertexCapacity);
            surface.IndexCapacity = (std::max)(AlignUp(indexBytes + indexBytes / 2, 4), surface.IndexCapacity);

            SDL_GPUBufferCreateInfo vertexInfo = {};
            vertexInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            vertexInfo.size = surface.VertexCapacity;
            surface.VertexBuffer = SDL_CreateGPUBuffer(_device, &vertexInfo);

            SDL_GPUBufferCreateInfo indexInfo = {};
            indexInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
            indexInfo.size = surface.IndexCapacity;
            surface.IndexBuffer = SDL_CreateGPUBuffer(_device, &indexInfo);

            SDL_GPUTransferBufferCreateInfo transferInfo = {};
            transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            transferInfo.size = surface.VertexCapacity + surface.IndexCapacity;
            surface.Transfer = SDL_CreateGPUTransferBuffer(_device, &transferInfo);
        }
        return surface.VertexBuffer && surface.IndexBuffer && surface.Transfer;
    }

    void UIRenderTargetSDLGPU::Present(Surface& surface, const UIDrawList* list)
    {
        if (!_pipeline || !list)
            return;

        const uint32_t vertexBytes = list->VertexCount * static_cast<uint32_t>(sizeof(UIVertex));
        const uint32_t indexBytes = AlignUp(list->IndexCount * static_cast<uint32_t>(sizeof(uint16_t)), 4);
        const bool hasGeometry = list->VertexCount > 0 && list->IndexCount > 0 && list->CommandCount > 0
            && list->DisplaySize.x > 0.0f && list->DisplaySize.y > 0.0f;
        if (hasGeometry && !EnsureBuffers(surface, vertexBytes, indexBytes))
            return;

        SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(_device);
        if (!commands)
            return;

        SDL_GPUTexture* swapchain = nullptr;
        uint32_t swapWidth = 0, swapHeight = 0;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, surface.Window, &swapchain, &swapWidth, &swapHeight) || !swapchain)
        {
            SDL_SubmitGPUCommandBuffer(commands);
            return;
        }

        if (hasGeometry)
        {
            uint8_t* mapped = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(_device, surface.Transfer, true));
            std::memcpy(mapped, list->Vertices, vertexBytes);
            std::memcpy(mapped + surface.VertexCapacity, list->Indices, list->IndexCount * sizeof(uint16_t));
            SDL_UnmapGPUTransferBuffer(_device, surface.Transfer);

            SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(commands);
            SDL_GPUTransferBufferLocation vertexSource = { surface.Transfer, 0 };
            SDL_GPUBufferRegion vertexRegion = { surface.VertexBuffer, 0, vertexBytes };
            SDL_UploadToGPUBuffer(copy, &vertexSource, &vertexRegion, true);
            SDL_GPUTransferBufferLocation indexSource = { surface.Transfer, surface.VertexCapacity };
            SDL_GPUBufferRegion indexRegion = { surface.IndexBuffer, 0, indexBytes };
            SDL_UploadToGPUBuffer(copy, &indexSource, &indexRegion, true);
            SDL_EndGPUCopyPass(copy);
        }

        SDL_GPUColorTargetInfo target = {};
        target.texture = swapchain;
        target.clear_color = SDL_FColor{ _clear.r, _clear.g, _clear.b, _clear.a };
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);

        if (hasGeometry)
        {
            const float framebufferWidth = list->DisplaySize.x * list->FramebufferScale.x;
            const float framebufferHeight = list->DisplaySize.y * list->FramebufferScale.y;

            SDL_BindGPUGraphicsPipeline(pass, _pipeline);
            SDL_GPUBufferBinding vertexBinding = { surface.VertexBuffer, 0 };
            SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
            SDL_GPUBufferBinding indexBinding = { surface.IndexBuffer, 0 };
            SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

            SDL_GPUViewport viewport = { 0.0f, 0.0f, framebufferWidth, framebufferHeight, 0.0f, 1.0f };
            SDL_SetGPUViewport(pass, &viewport);

            UIConstants constants = {};
            constants.Scale[0] = 2.0f / list->DisplaySize.x;
            constants.Scale[1] = 2.0f / list->DisplaySize.y;
            constants.Translate[0] = -1.0f - list->DisplayPos.x * constants.Scale[0];
            constants.Translate[1] = -1.0f - list->DisplayPos.y * constants.Scale[1];
            SDL_PushGPUVertexUniformData(commands, 0, &constants, sizeof(constants));

            for (uint32_t i = 0; i < list->CommandCount; ++i)
            {
                const UIDrawCommand& cmd = list->Commands[i];
                auto texture = _textures.find(cmd.TextureId);
                if (texture == _textures.end())
                    continue;

                const float minX = (std::max)((cmd.Clip.min.x - list->DisplayPos.x) * list->FramebufferScale.x, 0.0f);
                const float minY = (std::max)((cmd.Clip.min.y - list->DisplayPos.y) * list->FramebufferScale.y, 0.0f);
                const float maxX = (std::min)((cmd.Clip.max.x - list->DisplayPos.x) * list->FramebufferScale.x, framebufferWidth);
                const float maxY = (std::min)((cmd.Clip.max.y - list->DisplayPos.y) * list->FramebufferScale.y, framebufferHeight);
                if (maxX <= minX || maxY <= minY)
                    continue;

                SDL_Rect scissor = { static_cast<int>(minX), static_cast<int>(minY),
                    static_cast<int>(maxX - minX), static_cast<int>(maxY - minY) };
                SDL_SetGPUScissor(pass, &scissor);

                SDL_GPUTextureSamplerBinding samplerBinding = { texture->second, _sampler };
                SDL_BindGPUFragmentSamplers(pass, 0, &samplerBinding, 1);
                SDL_DrawGPUIndexedPrimitives(pass, cmd.IndexCount, 1, cmd.IndexOffset, static_cast<int32_t>(cmd.VertexOffset), 0);
            }
        }

        SDL_EndGPURenderPass(pass);
        SDL_SubmitGPUCommandBuffer(commands);
    }

    void UIRenderTargetSDLGPU::ReleaseBuffers(Surface& surface)
    {
        if (surface.Transfer) SDL_ReleaseGPUTransferBuffer(_device, surface.Transfer);
        if (surface.VertexBuffer) SDL_ReleaseGPUBuffer(_device, surface.VertexBuffer);
        if (surface.IndexBuffer) SDL_ReleaseGPUBuffer(_device, surface.IndexBuffer);
        surface.Transfer = nullptr;
        surface.VertexBuffer = nullptr;
        surface.IndexBuffer = nullptr;
    }

    void UIRenderTargetSDLGPU::SubmitUI(const UIDrawList* list)
    {
        Present(_main, list);
    }

    uint64_t UIRenderTargetSDLGPU::CreateUISurface(Window& window)
    {
        SDL_Window* sdlWindow = static_cast<SDL_Window*>(window.GetPlatformWindow());
        if (!_pipeline || !sdlWindow || !SDL_ClaimWindowForGPUDevice(_device, sdlWindow))
            return 0;
        if (static_cast<int>(SDL_GetGPUSwapchainTextureFormat(_device, sdlWindow)) != _swapchainFormat)
        {
            SDL_Log("xgPlatform: secondary window swapchain format differs from the main window, surface refused");
            SDL_ReleaseWindowFromGPUDevice(_device, sdlWindow);
            return 0;
        }
        const SDL_GPUPresentMode mode = SDL_WindowSupportsGPUPresentMode(_device, sdlWindow, SDL_GPU_PRESENTMODE_IMMEDIATE)
            ? SDL_GPU_PRESENTMODE_IMMEDIATE : SDL_GPU_PRESENTMODE_VSYNC;
        SDL_SetGPUSwapchainParameters(_device, sdlWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode);

        const uint64_t id = _nextSurfaceId++;
        Surface surface;
        surface.Window = sdlWindow;
        _surfaces[id] = surface;
        return id;
    }

    void UIRenderTargetSDLGPU::DestroyUISurface(uint64_t surface)
    {
        auto it = _surfaces.find(surface);
        if (it == _surfaces.end())
            return;
        SDL_WaitForGPUIdle(_device);
        ReleaseBuffers(it->second);
        SDL_ReleaseWindowFromGPUDevice(_device, it->second.Window);
        _surfaces.erase(it);
    }

    void UIRenderTargetSDLGPU::SubmitUISurface(uint64_t surface, const UIDrawList* list)
    {
        auto it = _surfaces.find(surface);
        if (it != _surfaces.end())
            Present(it->second, list);
    }

    void UIRenderTargetSDLGPU::SetClearColor(const Color& color)
    {
        _clear = color;
    }

    XG_DECLARE_UNIQUE_TYPE(UIRenderTargetSDLGPU)
    XG_API UIRenderTargetUnique NewUIRenderTarget(Window& window)
    {
        auto* target = new UIRenderTargetSDLGPU(static_cast<SDL_Window*>(window.GetPlatformWindow()));
        if (!target->IsValid())
        {
            delete target;
            return UIRenderTargetUnique(XG_EMPTY_UNIQUE_HANDLE());
        }
        return UIRenderTargetUnique(MakeUIRenderTargetSDLGPUUniqueHandle(target));
    }

} // namespace xg
