// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgUIDraw.h"
#include <cstdint>
#include <unordered_map>

struct SDL_Window;
struct SDL_GPUDevice;
struct SDL_GPUGraphicsPipeline;
struct SDL_GPUSampler;
struct SDL_GPUTexture;
struct SDL_GPUBuffer;
struct SDL_GPUTransferBuffer;

namespace xg {

    class UIRenderTargetSDLGPU : public UIRenderTarget {
    public:
        explicit UIRenderTargetSDLGPU(SDL_Window* window);
        ~UIRenderTargetSDLGPU() override;

        bool IsValid() const noexcept { return _pipeline != nullptr; }

        uint64_t CreateUITexture(int32_t width, int32_t height, const void* rgba) override;
        void UpdateUITexture(uint64_t id, int32_t x, int32_t y, int32_t width, int32_t height, const void* rgba, int32_t pitch) override;
        void DestroyUITexture(uint64_t id) override;
        void SubmitUI(const UIDrawList* list) override;
        void SetClearColor(const Color& color) override;

        bool SupportsSurfaces() const override { return _pipeline != nullptr; }
        uint64_t CreateUISurface(Window& window) override;
        void DestroyUISurface(uint64_t surface) override;
        void SubmitUISurface(uint64_t surface, const UIDrawList* list) override;

    private:
        struct Surface
        {
            SDL_Window* Window = nullptr;
            SDL_GPUBuffer* VertexBuffer = nullptr;
            SDL_GPUBuffer* IndexBuffer = nullptr;
            SDL_GPUTransferBuffer* Transfer = nullptr;
            uint32_t VertexCapacity = 0;
            uint32_t IndexCapacity = 0;
        };

        bool CreatePipeline();
        bool EnsureBuffers(Surface& surface, uint32_t vertexBytes, uint32_t indexBytes);
        void ReleaseBuffers(Surface& surface);
        void Present(Surface& surface, const UIDrawList* list);
        void Upload(SDL_GPUTexture* texture, int32_t x, int32_t y, int32_t width, int32_t height, const void* rgba, int32_t pitch);

        SDL_GPUDevice* _device = nullptr;
        SDL_GPUGraphicsPipeline* _pipeline = nullptr;
        SDL_GPUSampler* _sampler = nullptr;
        int _swapchainFormat = 0;

        Surface _main;
        std::unordered_map<uint64_t, Surface> _surfaces;
        uint64_t _nextSurfaceId = 1;

        std::unordered_map<uint64_t, SDL_GPUTexture*> _textures;
        uint64_t _nextTextureId = 1;

        Color _clear = Color(0.0f, 0.0f, 0.0f, 1.0f);
    };

} // namespace xg
