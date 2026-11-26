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

        bool SupportsTextureFormat(TextureFormat format) const override;
        uint64_t CreateTexture(const TextureDesc* desc, const TextureMip* mips) override;
        void DestroyTexture(uint64_t id) override;
        void SetRenderViewTexture(uint64_t viewId, uint64_t textureId, RenderViewTextureMode mode) override;
        uint64_t CreateMesh(const MeshDesc* desc) override;
        void DestroyMesh(uint64_t id) override;
        void SetRenderViewDraws(uint64_t viewId, const RenderViewDraw* draws, uint32_t count) override;
        ShaderFormat GetShaderFormat() const override { return ShaderFormat::DXIL; }
        uint64_t CreateShader(const ShaderDesc* desc) override;
        void DestroyShader(uint64_t id) override;
        uint64_t CreateMaterial(const MaterialDesc* desc) override;
        bool UpdateMaterial(uint64_t id, const MaterialDesc* desc) override;
        void DestroyMaterial(uint64_t id) override;
        void SetSceneDraws(const SceneDraw* draws, uint32_t count) override;
        void SetRenderViewMaterialDraws(uint64_t viewId, const SceneDraw* draws, uint32_t count) override;
        void SetRenderViewFlags(uint64_t viewId, RenderViewFlags flags) override;
        bool ReadRenderView(uint64_t viewId, void* rgba, uint64_t capacity) override;

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
            // Created by CreateTexture (any format, mips): not updatable with UpdateUITexture.
            bool Sampled = false;
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
            uint64_t Texture = 0;
            RenderViewTextureMode Mode = RenderViewTextureMode::Color;
            std::vector<RenderViewDraw> Draws;
            std::vector<SceneDraw> MaterialDraws;
            RenderViewFlags Flags = RenderViewFlags::None;
        };

        struct GpuShader
        {
            ComPtr<ID3D12PipelineState> Pipeline;
        };

        // Constants: a 256-byte upload buffer holding XGMaterial (float4 Params[4]), mapped while the material lives.
        struct GpuMaterial
        {
            uint64_t Shader = 0;
            uint64_t Textures[MaterialTextureSlots] = {};
            ComPtr<ID3D12Resource> Constants;
            void* Mapped = nullptr;
        };

        struct GpuMesh
        {
            ComPtr<ID3D12Resource> Vertices;
            ComPtr<ID3D12Resource> Indices;
            UINT VertexBytes = 0;
            UINT Stride = 0;
            UINT IndexCount = 0;
            DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
        };

        bool CreateScenePipeline();
        bool CreateTexturedPipeline();
        bool CreateLitPipeline();
        bool CreateSurfaceRootSignature();
        GpuMesh* FindMesh(uint64_t id);
        GpuShader* FindShader(uint64_t id);
        GpuMaterial* FindMaterial(uint64_t id);
        bool WriteMaterial(GpuMaterial& material, const MaterialDesc& desc);
        void RecordLitMesh(const GpuMesh& mesh, const float* world, const float* viewProjection);
        void RecordSceneDraws(const std::vector<SceneDraw>& draws, const float* viewProjection, D3D12_GPU_DESCRIPTOR_HANDLE heapStart, bool skipEditorHidden);
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
        // Render view color: typeless storage, written through an sRGB RTV (shaders output linear color) and read by
        // the UI through a UNORM SRV.
        static constexpr DXGI_FORMAT ViewColorStorage = DXGI_FORMAT_R8G8B8A8_TYPELESS;
        static constexpr DXGI_FORMAT ViewColorTarget = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        std::vector<RenderView> _renderViews;
        std::vector<UINT> _freeViewTargets;
        ComPtr<ID3D12DescriptorHeap> _viewRtvHeap;
        ComPtr<ID3D12DescriptorHeap> _viewDsvHeap;
        UINT _dsvDescriptorSize = 0;
        ComPtr<ID3D12RootSignature> _sceneRootSignature;
        ComPtr<ID3D12PipelineState> _sceneLinePipeline;
        ComPtr<ID3D12Resource> _sceneVertexBuffer;
        UINT _sceneLineVertexCount = 0;
        ComPtr<ID3D12RootSignature> _texturedRootSignature;
        ComPtr<ID3D12PipelineState> _texturedPipeline;
        ComPtr<ID3D12Resource> _cubeVertexBuffer;
        UINT _cubeVertexCount = 0;
        uint64_t _whiteTexture = 0;
        ComPtr<ID3D12RootSignature> _litRootSignature;
        ComPtr<ID3D12PipelineState> _litPipeline;
        std::vector<GpuMesh> _meshes;
        std::vector<UINT> _freeMeshes;
        // Surface shaders: root constants b0, root CBV b1, one table per texture t0..t3, static sampler s0.
        ComPtr<ID3D12RootSignature> _surfaceRootSignature;
        std::vector<GpuShader> _shaders;
        std::vector<UINT> _freeShaders;
        std::vector<GpuMaterial> _materials;
        std::vector<UINT> _freeMaterials;
        std::vector<SceneDraw> _sceneDraws;
    };

}
