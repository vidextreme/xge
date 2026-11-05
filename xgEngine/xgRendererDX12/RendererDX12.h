// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgRenderer.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
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
        void SetClearColor(float r, float g, float b, float a) override;

    private:
        template<typename T>
        using ComPtr = Microsoft::WRL::ComPtr<T>;

        struct UITexture
        {
            ComPtr<ID3D12Resource> Resource;
            int32_t Width = 0;
            int32_t Height = 0;
        };

        bool CreateDeviceAndSwapChain();
        bool CreateRenderTargets();
        void ReleaseRenderTargets();
        bool CreateCommandObjects();
        bool CreateFenceObjects();
        bool CreateUIPipeline();
        bool ResizeIfNeeded(bool& skipFrame);
        bool WaitForGPU();
        bool EnsureUIBuffers(size_t vertexCount, size_t indexCount);
        bool UploadTextureRegion(ID3D12Resource* texture, int32_t x, int32_t y, int32_t width, int32_t height,
            const void* rgba, int32_t pitch, bool firstUpload);
        void RecordUI();
        bool Check(HRESULT hr, const char* what);
        UITexture* FindTexture(uint64_t id);

        static constexpr UINT FrameCount = 2;
        static constexpr UINT SrvHeapSize = 256;
        static constexpr DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        HWND _hwnd = nullptr;
        UINT _width = 0;
        UINT _height = 0;

        ComPtr<IDXGIFactory6> _factory;
        ComPtr<ID3D12Device> _device;
        ComPtr<IDXGISwapChain3> _swapChain;
        ComPtr<ID3D12CommandQueue> _commandQueue;
        ComPtr<ID3D12CommandAllocator> _commandAllocator;
        ComPtr<ID3D12GraphicsCommandList> _commandList;
        ComPtr<ID3D12CommandAllocator> _uploadAllocator;
        ComPtr<ID3D12GraphicsCommandList> _uploadList;

        ComPtr<ID3D12DescriptorHeap> _rtvHeap;
        UINT _rtvDescriptorSize = 0;
        ComPtr<ID3D12Resource> _renderTargets[FrameCount];
        UINT _frameIndex = 0;

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

        ComPtr<ID3D12Resource> _vertexBuffer;
        ComPtr<ID3D12Resource> _indexBuffer;
        size_t _vertexCapacity = 0;
        size_t _indexCapacity = 0;

        std::vector<UIVertex> _uiVertices;
        std::vector<uint16_t> _uiIndices;
        std::vector<UIDrawCommand> _uiCommands;
        float _uiDisplay[4] = {};
        float _uiScale[2] = { 1.0f, 1.0f };
    };

}
