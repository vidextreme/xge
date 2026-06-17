#include "pch.h"
#include "RendererDX12.h"
#include "xgWindow.h"
#include <wrl/client.h>
#include <stdexcept>
#include <cassert>

using Microsoft::WRL::ComPtr;

namespace xg {

    static void ThrowIfFailed(HRESULT hr) {
        if (FAILED(hr)) {
            throw std::runtime_error("DX12 call failed");
        }
    }

    RendererDX12::RendererDX12(Window& window) {
        CreateDeviceAndSwapChain(window);
        CreateRenderTargets();
        CreateCommandObjects();
        CreateFenceObjects();
    }

    RendererDX12::~RendererDX12() {
        WaitForGPU();

        if (fenceEvent) {
            CloseHandle(fenceEvent);
            fenceEvent = nullptr;
        }

        for (UINT i = 0; i < FrameCount; ++i) {
            if (renderTargets[i]) {
                renderTargets[i]->Release();
                renderTargets[i] = nullptr;
            }
        }

        if (rtvHeap)          rtvHeap->Release();
        if (commandList)      commandList->Release();
        if (commandAllocator) commandAllocator->Release();
        if (commandQueue)     commandQueue->Release();
        if (swapChain)        swapChain->Release();
        if (fence)            fence->Release();
        if (device)           device->Release();
    }

    void RendererDX12::CreateDeviceAndSwapChain(Window& window) {
        UINT dxgiFlags = 0;

        ComPtr<IDXGIFactory6> factory;
        ThrowIfFailed(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&factory)));

        ComPtr<IDXGIAdapter1> adapter;
        ThrowIfFailed(factory->EnumAdapters1(0, &adapter));

        ThrowIfFailed(D3D12CreateDevice(
            adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device)
        ));

        D3D12_COMMAND_QUEUE_DESC qDesc = {};
        qDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ThrowIfFailed(device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&commandQueue)));

        DXGI_SWAP_CHAIN_DESC1 scDesc = {};
        scDesc.BufferCount = FrameCount;
        scDesc.Width = window.GetWidth();
        scDesc.Height = window.GetHeight();
        scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scDesc.SampleDesc.Count = 1;

        HWND hwnd = static_cast<HWND>(window.GetNativeHandle());
        assert(hwnd != nullptr);

        ComPtr<IDXGISwapChain1> tempSwapChain;
        ThrowIfFailed(factory->CreateSwapChainForHwnd(
            commandQueue,
            hwnd,
            &scDesc,
            nullptr,
            nullptr,
            &tempSwapChain
        ));

        ComPtr<IDXGISwapChain3> swapChain3;
        ThrowIfFailed(tempSwapChain.As(&swapChain3));

        swapChain = swapChain3.Detach();
        frameIndex = swapChain->GetCurrentBackBufferIndex();
    }

    void RendererDX12::CreateRenderTargets() {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = FrameCount;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap)));

        rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < FrameCount; ++i) {
            ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])));
            device->CreateRenderTargetView(renderTargets[i], nullptr, rtvHandle);
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    void RendererDX12::CreateCommandObjects() {
        ThrowIfFailed(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&commandAllocator)
        ));

        ThrowIfFailed(device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            commandAllocator,
            nullptr,
            IID_PPV_ARGS(&commandList)
        ));

        ThrowIfFailed(commandList->Close());
    }

    void RendererDX12::CreateFenceObjects() {
        ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        fenceValue = 1;
        fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    void RendererDX12::WaitForGPU() {
        const UINT64 fenceToWait = fenceValue;
        ThrowIfFailed(commandQueue->Signal(fence, fenceToWait));
        fenceValue++;

        if (fence->GetCompletedValue() < fenceToWait) {
            ThrowIfFailed(fence->SetEventOnCompletion(fenceToWait, fenceEvent));
            WaitForSingleObject(fenceEvent, INFINITE);
        }
    }

    void RendererDX12::Render() {
        ThrowIfFailed(commandAllocator->Reset());
        ThrowIfFailed(commandList->Reset(commandAllocator, nullptr));

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = renderTargets[frameIndex];
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList->ResourceBarrier(1, &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += frameIndex * rtvDescriptorSize;

        const float clearColor[4] = { 0.1f, 0.2f, 0.4f, 1.0f };
        commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &barrier);

        ThrowIfFailed(commandList->Close());

        ID3D12CommandList* lists[] = { commandList };
        commandQueue->ExecuteCommandLists(1, lists);

        ThrowIfFailed(swapChain->Present(1, 0));

        WaitForGPU();
        frameIndex = swapChain->GetCurrentBackBufferIndex();
    }

    void* RendererDX12::GetNativeHandle() const {
        return device;
    }

}
