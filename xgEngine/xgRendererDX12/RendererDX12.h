#pragma once
#include "xgRenderer.h"
#include <d3d12.h>
#include <dxgi1_6.h>

namespace xg {

    class RendererDX12 : public Renderer {
    public:
        explicit RendererDX12(Window& window);
        ~RendererDX12() override;

        void Render() override;

    private:
        void WaitForGPU();
        void CreateDeviceAndSwapChain(Window& window);
        void CreateRenderTargets();
        void CreateCommandObjects();
        void CreateFenceObjects();

    private:
        static constexpr UINT FrameCount = 2;

        ID3D12Device* device = nullptr;
        IDXGISwapChain3* swapChain = nullptr;
        ID3D12CommandQueue* commandQueue = nullptr;
        ID3D12CommandAllocator* commandAllocator = nullptr;
        ID3D12GraphicsCommandList* commandList = nullptr;

        ID3D12DescriptorHeap* rtvHeap = nullptr;
        UINT rtvDescriptorSize = 0;
        ID3D12Resource* renderTargets[FrameCount] = {};
        UINT frameIndex = 0;

        ID3D12Fence* fence = nullptr;
        UINT64 fenceValue = 0;
        HANDLE fenceEvent = nullptr;
    };

}
