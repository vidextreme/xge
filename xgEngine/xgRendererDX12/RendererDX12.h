// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgRenderer.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

namespace xg {

    class RendererDX12 : public Renderer {
    public:
        RendererDX12() = default;
        ~RendererDX12() override;

        bool Initialize(Window& window);

        void Render() override;
        void* GetNativeHandle() const override;

        uint64_t CreateUITexture(int32_t width, int32_t height, const void* rgba) override;
        void UpdateUITexture(uint64_t id, int32_t x, int32_t y, int32_t width, int32_t height, const void* rgba, int32_t pitch) override;
        void DestroyUITexture(uint64_t id) override;
        void SubmitUI(const UIDrawList* list) override;
        void SetClearColor(const Color& color) override;

        uint64_t CreateRenderView(int32_t width, int32_t height) override;
        bool ResizeRenderView(uint64_t id, int32_t width, int32_t height) override;
        void DestroyRenderView(uint64_t id) override;
        void SetRenderViewCamera(uint64_t id, const RenderViewCamera* camera) override;

        bool SupportsSurfaces() const override { return !_failed; }
        uint64_t CreateUISurface(Window& window) override;
        void DestroyUISurface(uint64_t surface) override;
        void SubmitUISurface(uint64_t surface, const UIDrawList* list) override;

    private:
        template<typename T>
        using ComPtr = Microsoft::WRL::ComPtr<T>;

        struct UITexture
        {
            ComPtr<ID3D12Resource> Resource;
            int32_t Width = 0;
            int32_t Height = 0;
        };

        static constexpr UINT FrameCount = 2;

        // One swapchain-backed presentation target: the main window, or a secondary editor viewport window.
        struct Surface
        {
            uint64_t Id = 0;
            HWND Hwnd = nullptr;
            UINT Width = 0;
            UINT Height = 0;
            ComPtr<IDXGISwapChain3> SwapChain;
            ComPtr<ID3D12DescriptorHeap> RtvHeap;
            ComPtr<ID3D12Resource> RenderTargets[FrameCount];
            UINT FrameIndex = 0;
            ComPtr<ID3D12Resource> VertexBuffer;
            ComPtr<ID3D12Resource> IndexBuffer;
            size_t VertexCapacity = 0;
            size_t IndexCapacity = 0;
            std::vector<UIVertex> Vertices;
            std::vector<uint16_t> Indices;
            std::vector<UIDrawCommand> Commands;
            float Display[4] = {};
            float Scale[2] = { 1.0f, 1.0f };
            bool Skip = false;
        };

        bool CreateDevice();
        bool CreateSwapChain(Surface& surface);
        bool CreateRenderTargets(Surface& surface);
        void ReleaseRenderTargets(Surface& surface);
        bool CreateCommandObjects();
        bool CreateFenceObjects();
        bool CreateUIPipeline();
        bool ResizeIfNeeded(Surface& surface, bool& skipFrame);
        bool WaitForGPU();
        bool EnsureUIBuffers(Surface& surface, size_t vertexCount, size_t indexCount);
        bool UploadTextureRegion(ID3D12Resource* texture, int32_t x, int32_t y, int32_t width, int32_t height,
            const void* rgba, int32_t pitch, bool firstUpload);
        void RecordUI(Surface& surface);
        void RecordSurface(Surface& surface);
        // Secondary windows present as soon as ImGui submits them, right after it moved them (avoids stale content while dragging).
        void PresentSurfaceNow(Surface& surface);
        void StoreDrawList(Surface& surface, const UIDrawList* list);
        Surface* FindSurface(uint64_t id);
        bool Check(HRESULT hr, const char* what);
        UITexture* FindTexture(uint64_t id);

        struct RenderView
        {
            uint64_t Id = 0;
            UINT TargetIndex = 0;
            ComPtr<ID3D12Resource> Depth;
            UINT Width = 0;
            UINT Height = 0;
            RenderViewCamera Camera = {};
        };

        bool CreateScenePipeline();
        bool CreateRenderViewTargets(RenderView& view, UINT width, UINT height);
        void RecordRenderViews();
        RenderView* FindRenderView(uint64_t id);

        static constexpr UINT SrvHeapSize = 256;
        static constexpr DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        Surface _main;
        std::vector<std::unique_ptr<Surface>> _surfaces;
        uint64_t _nextSurfaceId = 1;

        ComPtr<IDXGIFactory6> _factory;
        ComPtr<ID3D12Device> _device;
        ComPtr<ID3D12CommandQueue> _commandQueue;
        ComPtr<ID3D12CommandAllocator> _commandAllocator;
        ComPtr<ID3D12GraphicsCommandList> _commandList;
        ComPtr<ID3D12CommandAllocator> _uploadAllocator;
        ComPtr<ID3D12GraphicsCommandList> _uploadList;

        UINT _rtvDescriptorSize = 0;

        ComPtr<ID3D12Fence> _fence;
        UINT64 _fenceValue = 0;
        HANDLE _fenceEvent = nullptr;

        float _clearColor[4] = { 0.1f, 0.2f, 0.4f, 1.0f };
        bool _failed = false;

        ComPtr<ID3D12RootSignature> _uiRootSignature;
        ComPtr<ID3D12PipelineState> _uiPipeline;
        ComPtr<ID3D12DescriptorHeap> _srvHeap;
        UINT _srvDescriptorSize = 0;
        std::vector<UITexture> _textures;
        std::vector<UINT> _freeTextureSlots;


        static constexpr UINT MaxRenderViews = 16;
        static constexpr DXGI_FORMAT DepthFormat = DXGI_FORMAT_D32_FLOAT;
        std::vector<RenderView> _renderViews;
        std::vector<UINT> _freeViewTargets;
        ComPtr<ID3D12DescriptorHeap> _viewRtvHeap;
        ComPtr<ID3D12DescriptorHeap> _viewDsvHeap;
        UINT _dsvDescriptorSize = 0;
        ComPtr<ID3D12RootSignature> _sceneRootSignature;
        ComPtr<ID3D12PipelineState> _sceneTrianglePipeline;
        ComPtr<ID3D12PipelineState> _sceneLinePipeline;
        ComPtr<ID3D12Resource> _sceneVertexBuffer;
        UINT _sceneTriangleCount = 0;
        UINT _sceneLineVertexCount = 0;
    };

}
