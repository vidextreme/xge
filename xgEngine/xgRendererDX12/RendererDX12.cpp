// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "RendererDX12.h"
#include "xgWindow.h"
#include "xgLog.h"
#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3dcompiler")

namespace xg {

    namespace
    {
        const char* _uiShaderSource = R"(
cbuffer Constants : register(b0)
{
    float4x4 ProjectionMatrix;
};

struct VSInput
{
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.0f, 1.0f));
    output.col = input.col;
    output.uv = input.uv;
    return output;
}

SamplerState Sampler0 : register(s0);
Texture2D Texture0 : register(t0);

float4 PSMain(PSInput input) : SV_Target
{
    return input.col * Texture0.Sample(Sampler0, input.uv);
}
)";

        D3D12_RESOURCE_BARRIER Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            return barrier;
        }

        bool CreateUploadBuffer(ID3D12Device* device, size_t size, Microsoft::WRL::ComPtr<ID3D12Resource>& buffer)
        {
            D3D12_HEAP_PROPERTIES heap = {};
            heap.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = size;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            buffer.Reset();
            return SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)));
        }
    }

    RendererDX12::~RendererDX12()
    {
        if (_commandQueue && _fence && _fenceEvent)
            WaitForGPU();

        _surfaces.clear();
        _renderViews.clear();
        _textures.clear();
        ReleaseRenderTargets(_main);

        if (_fenceEvent)
        {
            CloseHandle(_fenceEvent);
            _fenceEvent = nullptr;
        }
    }

    bool RendererDX12::Check(HRESULT hr, const char* what)
    {
        if (SUCCEEDED(hr))
            return true;
        xg::Log(xg::MessageType::Error, "RendererDX12: %s failed (hr=0x%08X)", what, static_cast<unsigned>(hr));
        return false;
    }

    bool RendererDX12::Initialize(Window& window)
    {
        _main.Hwnd = static_cast<HWND>(window.GetNativeHandle());
        if (!_main.Hwnd)
        {
            xg::Log(xg::MessageType::Error, "RendererDX12: window has no native handle");
            return false;
        }

        if (!CreateDevice() || !CreateSwapChain(_main) || !CreateCommandObjects()
            || !CreateFenceObjects() || !CreateUIPipeline())
            return false;
        if (!CreateScenePipeline())
            xg::Log(xg::MessageType::Warning, "RendererDX12: scene pipeline unavailable, render views disabled");

        xg::Log(xg::MessageType::Success, "RendererDX12: initialized (%ux%u)", _main.Width, _main.Height);
        return true;
    }

    bool RendererDX12::CreateDevice()
    {
        UINT dxgiFlags = 0;
#ifdef _DEBUG
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
        {
            debug->EnableDebugLayer();
            dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif
        if (!Check(CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&_factory)), "CreateDXGIFactory2"))
            return false;

        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; _factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
        {
            DXGI_ADAPTER_DESC1 desc = {};
            adapter->GetDesc1(&desc);
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0
                && SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device))))
                break;
            adapter.Reset();
        }
        if (!_device && !Check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device)), "D3D12CreateDevice"))
            return false;

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (!Check(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_commandQueue)), "CreateCommandQueue"))
            return false;

        _rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        return true;
    }

    bool RendererDX12::CreateSwapChain(Surface& surface)
    {
        RECT rect = {};
        GetClientRect(surface.Hwnd, &rect);
        surface.Width = static_cast<UINT>((std::max)(1L, rect.right - rect.left));
        surface.Height = static_cast<UINT>((std::max)(1L, rect.bottom - rect.top));

        DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
        swapDesc.BufferCount = FrameCount;
        swapDesc.Width = surface.Width;
        swapDesc.Height = surface.Height;
        swapDesc.Format = BackBufferFormat;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapDesc.SampleDesc.Count = 1;

        ComPtr<IDXGISwapChain1> swapChain1;
        if (!Check(_factory->CreateSwapChainForHwnd(_commandQueue.Get(), surface.Hwnd, &swapDesc, nullptr, nullptr, &swapChain1), "CreateSwapChainForHwnd"))
            return false;
        _factory->MakeWindowAssociation(surface.Hwnd, DXGI_MWA_NO_ALT_ENTER);

        if (!Check(swapChain1.As(&surface.SwapChain), "IDXGISwapChain3 query"))
            return false;
        surface.FrameIndex = surface.SwapChain->GetCurrentBackBufferIndex();
        return CreateRenderTargets(surface);
    }

    bool RendererDX12::CreateRenderTargets(Surface& surface)
    {
        if (!surface.RtvHeap)
        {
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = FrameCount;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            if (!Check(_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&surface.RtvHeap)), "CreateDescriptorHeap (RTV)"))
                return false;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = surface.RtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < FrameCount; ++i)
        {
            if (!Check(surface.SwapChain->GetBuffer(i, IID_PPV_ARGS(&surface.RenderTargets[i])), "GetBuffer"))
                return false;
            _device->CreateRenderTargetView(surface.RenderTargets[i].Get(), nullptr, rtvHandle);
            rtvHandle.ptr += _rtvDescriptorSize;
        }
        return true;
    }

    void RendererDX12::ReleaseRenderTargets(Surface& surface)
    {
        for (UINT i = 0; i < FrameCount; ++i)
            surface.RenderTargets[i].Reset();
    }

    bool RendererDX12::CreateCommandObjects()
    {
        if (!Check(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator)), "CreateCommandAllocator")
            || !Check(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.Get(), nullptr, IID_PPV_ARGS(&_commandList)), "CreateCommandList")
            || !Check(_commandList->Close(), "CommandList Close"))
            return false;

        if (!Check(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_uploadAllocator)), "CreateCommandAllocator (upload)")
            || !Check(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _uploadAllocator.Get(), nullptr, IID_PPV_ARGS(&_uploadList)), "CreateCommandList (upload)")
            || !Check(_uploadList->Close(), "CommandList Close (upload)"))
            return false;
        return true;
    }

    bool RendererDX12::CreateFenceObjects()
    {
        if (!Check(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)), "CreateFence"))
            return false;
        _fenceValue = 1;
        _fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!_fenceEvent)
        {
            xg::Log(xg::MessageType::Error, "RendererDX12: CreateEvent failed");
            return false;
        }
        return true;
    }

    bool RendererDX12::CreateUIPipeline()
    {
        D3D12_DESCRIPTOR_RANGE range = {};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = 0;
        range.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER params[2] = {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[0].Constants.ShaderRegister = 0;
        params[0].Constants.Num32BitValues = 16;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable.NumDescriptorRanges = 1;
        params[1].DescriptorTable.pDescriptorRanges = &range;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.NumParameters = 2;
        rootDesc.pParameters = params;
        rootDesc.NumStaticSamplers = 1;
        rootDesc.pStaticSamplers = &sampler;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
            | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
        {
            xg::Log(xg::MessageType::Error, "RendererDX12: root signature: %s",
                error ? static_cast<const char*>(error->GetBufferPointer()) : "unknown error");
            return false;
        }
        if (!Check(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_uiRootSignature)), "CreateRootSignature"))
            return false;

        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;
        const size_t sourceLength = std::strlen(_uiShaderSource);
        if (FAILED(D3DCompile(_uiShaderSource, sourceLength, "xgUI", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vertexShader, &error)))
        {
            xg::Log(xg::MessageType::Error, "RendererDX12: UI vertex shader: %s",
                error ? static_cast<const char*>(error->GetBufferPointer()) : "unknown error");
            return false;
        }
        if (FAILED(D3DCompile(_uiShaderSource, sourceLength, "xgUI", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &pixelShader, &error)))
        {
            xg::Log(xg::MessageType::Error, "RendererDX12: UI pixel shader: %s",
                error ? static_cast<const char*>(error->GetBufferPointer()) : "unknown error");
            return false;
        }

        static_assert(sizeof(UIVertex) == 20, "UIVertex layout must match the UI input layout");
        D3D12_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(UIVertex, Position),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, offsetof(UIVertex, UV),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(UIVertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
        pso.pRootSignature = _uiRootSignature.Get();
        pso.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        pso.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
        pso.InputLayout = { layout, 3 };
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = BackBufferFormat;
        pso.SampleMask = UINT_MAX;
        pso.SampleDesc.Count = 1;

        D3D12_RENDER_TARGET_BLEND_DESC& blend = pso.BlendState.RenderTarget[0];
        blend.BlendEnable = TRUE;
        blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso.RasterizerState.DepthClipEnable = TRUE;
        pso.DepthStencilState.DepthEnable = FALSE;
        pso.DepthStencilState.StencilEnable = FALSE;

        if (!Check(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_uiPipeline)), "CreateGraphicsPipelineState (UI)"))
            return false;

        D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
        srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvDesc.NumDescriptors = SrvHeapSize;
        srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (!Check(_device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&_srvHeap)), "CreateDescriptorHeap (SRV)"))
            return false;
        _srvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        _textures.resize(SrvHeapSize);
        _freeTextureSlots.reserve(SrvHeapSize);
        for (UINT i = SrvHeapSize; i > 0; --i)
            _freeTextureSlots.push_back(i - 1);
        return true;
    }

    bool RendererDX12::WaitForGPU()
    {
        const UINT64 fenceToWait = _fenceValue;
        if (FAILED(_commandQueue->Signal(_fence.Get(), fenceToWait)))
            return false;
        _fenceValue++;

        if (_fence->GetCompletedValue() < fenceToWait)
        {
            if (FAILED(_fence->SetEventOnCompletion(fenceToWait, _fenceEvent)))
                return false;
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
        return true;
    }

    bool RendererDX12::ResizeIfNeeded(Surface& surface, bool& skipFrame)
    {
        skipFrame = false;
        RECT rect = {};
        if (IsIconic(surface.Hwnd) || !GetClientRect(surface.Hwnd, &rect)
            || rect.right <= rect.left || rect.bottom <= rect.top)
        {
            skipFrame = true;
            return true;
        }

        const UINT width = static_cast<UINT>(rect.right - rect.left);
        const UINT height = static_cast<UINT>(rect.bottom - rect.top);
        if (width == surface.Width && height == surface.Height)
            return true;

        if (!WaitForGPU())
            return false;
        ReleaseRenderTargets(surface);
        if (!Check(surface.SwapChain->ResizeBuffers(FrameCount, width, height, BackBufferFormat, 0), "ResizeBuffers"))
            return false;
        surface.Width = width;
        surface.Height = height;
        surface.FrameIndex = surface.SwapChain->GetCurrentBackBufferIndex();
        return CreateRenderTargets(surface);
    }

    bool RendererDX12::EnsureUIBuffers(Surface& surface, size_t vertexCount, size_t indexCount)
    {
        if (vertexCount > surface.VertexCapacity)
        {
            const size_t capacity = vertexCount + 5000;
            if (!CreateUploadBuffer(_device.Get(), capacity * sizeof(UIVertex), surface.VertexBuffer))
                return Check(E_OUTOFMEMORY, "UI vertex buffer");
            surface.VertexCapacity = capacity;
        }
        if (indexCount > surface.IndexCapacity)
        {
            const size_t capacity = indexCount + 10000;
            if (!CreateUploadBuffer(_device.Get(), capacity * sizeof(uint16_t), surface.IndexBuffer))
                return Check(E_OUTOFMEMORY, "UI index buffer");
            surface.IndexCapacity = capacity;
        }
        return true;
    }

    void RendererDX12::RecordUI(Surface& surface)
    {
        if (surface.Commands.empty() || surface.Vertices.empty() || surface.Indices.empty())
            return;
        if (surface.Display[2] <= 0.0f || surface.Display[3] <= 0.0f)
            return;
        if (!EnsureUIBuffers(surface, surface.Vertices.size(), surface.Indices.size()))
            return;

        void* mapped = nullptr;
        D3D12_RANGE noRead = { 0, 0 };
        if (FAILED(surface.VertexBuffer->Map(0, &noRead, &mapped)))
            return;
        std::memcpy(mapped, surface.Vertices.data(), surface.Vertices.size() * sizeof(UIVertex));
        surface.VertexBuffer->Unmap(0, nullptr);

        if (FAILED(surface.IndexBuffer->Map(0, &noRead, &mapped)))
            return;
        std::memcpy(mapped, surface.Indices.data(), surface.Indices.size() * sizeof(uint16_t));
        surface.IndexBuffer->Unmap(0, nullptr);

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

        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, surface.Display[2] * surface.Scale[0], surface.Display[3] * surface.Scale[1], 0.0f, 1.0f };
        _commandList->RSSetViewports(1, &viewport);

        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = surface.VertexBuffer->GetGPUVirtualAddress();
        vbv.SizeInBytes = static_cast<UINT>(surface.Vertices.size() * sizeof(UIVertex));
        vbv.StrideInBytes = sizeof(UIVertex);
        _commandList->IASetVertexBuffers(0, 1, &vbv);

        D3D12_INDEX_BUFFER_VIEW ibv = {};
        ibv.BufferLocation = surface.IndexBuffer->GetGPUVirtualAddress();
        ibv.SizeInBytes = static_cast<UINT>(surface.Indices.size() * sizeof(uint16_t));
        ibv.Format = DXGI_FORMAT_R16_UINT;
        _commandList->IASetIndexBuffer(&ibv);
        _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        _commandList->SetPipelineState(_uiPipeline.Get());
        _commandList->SetGraphicsRootSignature(_uiRootSignature.Get());
        _commandList->SetGraphicsRoot32BitConstants(0, 16, projection, 0);

        ID3D12DescriptorHeap* heaps[] = { _srvHeap.Get() };
        _commandList->SetDescriptorHeaps(1, heaps);

        const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        _commandList->OMSetBlendFactor(blendFactor);

        const D3D12_GPU_DESCRIPTOR_HANDLE heapStart = _srvHeap->GetGPUDescriptorHandleForHeapStart();
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

            const D3D12_RECT scissor = { static_cast<LONG>(minX), static_cast<LONG>(minY), static_cast<LONG>(maxX), static_cast<LONG>(maxY) };
            _commandList->RSSetScissorRects(1, &scissor);

            D3D12_GPU_DESCRIPTOR_HANDLE srv = heapStart;
            srv.ptr += static_cast<UINT64>(command.TextureId - 1) * _srvDescriptorSize;
            _commandList->SetGraphicsRootDescriptorTable(1, srv);
            _commandList->DrawIndexedInstanced(command.IndexCount, 1, command.IndexOffset, static_cast<INT>(command.VertexOffset), 0);
        }
    }

    void RendererDX12::RecordSurface(Surface& surface)
    {
        ID3D12Resource* backBuffer = surface.RenderTargets[surface.FrameIndex].Get();
        D3D12_RESOURCE_BARRIER toTarget = Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        _commandList->ResourceBarrier(1, &toTarget);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = surface.RtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += static_cast<SIZE_T>(surface.FrameIndex) * _rtvDescriptorSize;
        _commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
        _commandList->ClearRenderTargetView(rtvHandle, _clearColor, 0, nullptr);

        RecordUI(surface);

        D3D12_RESOURCE_BARRIER toPresent = Transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        _commandList->ResourceBarrier(1, &toPresent);
    }

    void RendererDX12::Render()
    {
        if (_failed)
            return;

        if (!ResizeIfNeeded(_main, _main.Skip))
        {
            _failed = true;
            return;
        }
        if (_main.Skip)
            return;

        if (!Check(_commandAllocator->Reset(), "CommandAllocator Reset")
            || !Check(_commandList->Reset(_commandAllocator.Get(), nullptr), "CommandList Reset"))
        {
            _failed = true;
            return;
        }

        RecordRenderViews();
        RecordSurface(_main);

        if (!Check(_commandList->Close(), "CommandList Close"))
        {
            _failed = true;
            return;
        }

        ID3D12CommandList* lists[] = { _commandList.Get() };
        _commandQueue->ExecuteCommandLists(1, lists);

        const HRESULT presentResult = _main.SwapChain->Present(1, 0);
        if (!Check(presentResult, "Present") || !WaitForGPU())
        {
            _failed = true;
            return;
        }
        _main.FrameIndex = _main.SwapChain->GetCurrentBackBufferIndex();
    }

    void RendererDX12::PresentSurfaceNow(Surface& surface)
    {
        if (_failed)
            return;
        bool skip = false;
        if (!ResizeIfNeeded(surface, skip) || skip)
            return;

        if (!Check(_commandAllocator->Reset(), "CommandAllocator Reset (surface)")
            || !Check(_commandList->Reset(_commandAllocator.Get(), nullptr), "CommandList Reset (surface)"))
            return;
        RecordSurface(surface);
        if (!Check(_commandList->Close(), "CommandList Close (surface)"))
            return;

        ID3D12CommandList* lists[] = { _commandList.Get() };
        _commandQueue->ExecuteCommandLists(1, lists);
        surface.SwapChain->Present(0, 0);
        WaitForGPU();
        surface.FrameIndex = surface.SwapChain->GetCurrentBackBufferIndex();
    }

    void* RendererDX12::GetNativeHandle() const
    {
        return _device.Get();
    }

    RendererDX12::UITexture* RendererDX12::FindTexture(uint64_t id)
    {
        if (id == 0 || id > _textures.size())
            return nullptr;
        UITexture& texture = _textures[static_cast<size_t>(id - 1)];
        return texture.Resource ? &texture : nullptr;
    }

    bool RendererDX12::UploadTextureRegion(ID3D12Resource* texture, int32_t x, int32_t y, int32_t width, int32_t height,
        const void* rgba, int32_t pitch, bool firstUpload)
    {
        const UINT rowBytes = static_cast<UINT>(width) * 4;
        const UINT rowPitch = (rowBytes + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);

        ComPtr<ID3D12Resource> upload;
        if (!CreateUploadBuffer(_device.Get(), static_cast<size_t>(rowPitch) * height, upload))
            return Check(E_OUTOFMEMORY, "texture upload buffer");

        void* mapped = nullptr;
        D3D12_RANGE noRead = { 0, 0 };
        if (!Check(upload->Map(0, &noRead, &mapped), "texture upload Map"))
            return false;
        const uint8_t* source = static_cast<const uint8_t*>(rgba);
        for (int32_t row = 0; row < height; ++row)
            std::memcpy(static_cast<uint8_t*>(mapped) + static_cast<size_t>(row) * rowPitch, source + static_cast<size_t>(row) * pitch, rowBytes);
        upload->Unmap(0, nullptr);

        if (!Check(_uploadAllocator->Reset(), "upload allocator Reset")
            || !Check(_uploadList->Reset(_uploadAllocator.Get(), nullptr), "upload list Reset"))
            return false;

        if (!firstUpload)
        {
            D3D12_RESOURCE_BARRIER toCopy = Transition(texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
            _uploadList->ResourceBarrier(1, &toCopy);
        }

        D3D12_TEXTURE_COPY_LOCATION destination = {};
        destination.pResource = texture;
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION sourceLocation = {};
        sourceLocation.pResource = upload.Get();
        sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        sourceLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sourceLocation.PlacedFootprint.Footprint.Width = static_cast<UINT>(width);
        sourceLocation.PlacedFootprint.Footprint.Height = static_cast<UINT>(height);
        sourceLocation.PlacedFootprint.Footprint.Depth = 1;
        sourceLocation.PlacedFootprint.Footprint.RowPitch = rowPitch;

        _uploadList->CopyTextureRegion(&destination, static_cast<UINT>(x), static_cast<UINT>(y), 0, &sourceLocation, nullptr);

        D3D12_RESOURCE_BARRIER toShader = Transition(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        _uploadList->ResourceBarrier(1, &toShader);

        if (!Check(_uploadList->Close(), "upload list Close"))
            return false;
        ID3D12CommandList* lists[] = { _uploadList.Get() };
        _commandQueue->ExecuteCommandLists(1, lists);
        return WaitForGPU();
    }

    uint64_t RendererDX12::CreateUITexture(int32_t width, int32_t height, const void* rgba)
    {
        if (width <= 0 || height <= 0 || !rgba || _freeTextureSlots.empty())
            return 0;

        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = static_cast<UINT64>(width);
        desc.Height = static_cast<UINT>(height);
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        ComPtr<ID3D12Resource> resource;
        if (!Check(_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource)), "CreateCommittedResource (UI texture)"))
            return 0;

        if (!UploadTextureRegion(resource.Get(), 0, 0, width, height, rgba, width * 4, true))
            return 0;

        const UINT slot = _freeTextureSlots.back();
        _freeTextureSlots.pop_back();

        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MipLevels = 1;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        D3D12_CPU_DESCRIPTOR_HANDLE cpu = _srvHeap->GetCPUDescriptorHandleForHeapStart();
        cpu.ptr += static_cast<SIZE_T>(slot) * _srvDescriptorSize;
        _device->CreateShaderResourceView(resource.Get(), &srv, cpu);

        UITexture& texture = _textures[slot];
        texture.Resource = resource;
        texture.Width = width;
        texture.Height = height;
        return static_cast<uint64_t>(slot) + 1;
    }

    void RendererDX12::UpdateUITexture(uint64_t id, int32_t x, int32_t y, int32_t width, int32_t height, const void* rgba, int32_t pitch)
    {
        UITexture* texture = FindTexture(id);
        if (!texture || !rgba || width <= 0 || height <= 0)
            return;
        if (x < 0 || y < 0 || x + width > texture->Width || y + height > texture->Height)
            return;
        UploadTextureRegion(texture->Resource.Get(), x, y, width, height, rgba, pitch, false);
    }

    void RendererDX12::DestroyUITexture(uint64_t id)
    {
        UITexture* texture = FindTexture(id);
        if (!texture || FindRenderView(id))
            return;
        WaitForGPU();
        texture->Resource.Reset();
        texture->Width = 0;
        texture->Height = 0;
        _freeTextureSlots.push_back(static_cast<UINT>(id - 1));
    }

    void RendererDX12::StoreDrawList(Surface& surface, const UIDrawList* list)
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

    void RendererDX12::SubmitUI(const UIDrawList* list)
    {
        StoreDrawList(_main, list);
    }

    RendererDX12::Surface* RendererDX12::FindSurface(uint64_t id)
    {
        for (auto& surface : _surfaces)
            if (surface->Id == id)
                return surface.get();
        return nullptr;
    }

    uint64_t RendererDX12::CreateUISurface(Window& window)
    {
        if (_failed)
            return 0;
        auto surface = std::make_unique<Surface>();
        surface->Hwnd = static_cast<HWND>(window.GetNativeHandle());
        if (!surface->Hwnd || !CreateSwapChain(*surface))
            return 0;
        surface->Id = _nextSurfaceId++;
        const uint64_t id = surface->Id;
        _surfaces.push_back(std::move(surface));
        return id;
    }

    void RendererDX12::DestroyUISurface(uint64_t surface)
    {
        for (auto it = _surfaces.begin(); it != _surfaces.end(); ++it)
        {
            if ((*it)->Id != surface)
                continue;
            WaitForGPU();
            _surfaces.erase(it);
            return;
        }
    }

    void RendererDX12::SubmitUISurface(uint64_t surface, const UIDrawList* list)
    {
        if (Surface* target = FindSurface(surface))
        {
            StoreDrawList(*target, list);
            PresentSurfaceNow(*target);
        }
    }

    void RendererDX12::SetClearColor(const Color& color)
    {
        _clearColor[0] = color.r;
        _clearColor[1] = color.g;
        _clearColor[2] = color.b;
        _clearColor[3] = color.a;
    }

}
