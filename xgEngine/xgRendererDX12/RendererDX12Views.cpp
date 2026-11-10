// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// Offscreen render views: color + depth targets that share the UI texture table, so a panel can draw them.
// The scene content is a test cube and ground grid until a real scene renderer exists.
#include "pch.h"
#include "RendererDX12.h"
#include "xgLog.h"
#include <d3dcompiler.h>
#include "xgMat4x4.h"
#include <cmath>
#include <cstring>

namespace xg {

    namespace
    {
        const char* s_sceneShaderSource = R"(
cbuffer Constants : register(b0)
{
    row_major float4x4 ViewProjection;
};

struct VSInput
{
    float3 pos : POSITION;
    float4 col : COLOR0;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.pos = mul(float4(input.pos, 1.0f), ViewProjection);
    output.col = input.col;
    return output;
}

float4 PSMain(PSInput input) : SV_Target
{
    return input.col;
}
)";

        struct SceneVertex
        {
            float X, Y, Z;
            Color32 Color;
        };

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

        void BuildTestScene(std::vector<SceneVertex>& vertices, UINT& triangleVertexCount, UINT& lineVertexCount)
        {
            static const float corners[8][3] =
            {
                { -0.5f, 0.0f, -0.5f }, { 0.5f, 0.0f, -0.5f }, { 0.5f, 1.0f, -0.5f }, { -0.5f, 1.0f, -0.5f },
                { -0.5f, 0.0f,  0.5f }, { 0.5f, 0.0f,  0.5f }, { 0.5f, 1.0f,  0.5f }, { -0.5f, 1.0f,  0.5f },
            };
            static const int faces[6][4] = { { 0, 3, 2, 1 }, { 4, 5, 6, 7 }, { 0, 4, 7, 3 }, { 1, 2, 6, 5 }, { 3, 7, 6, 2 }, { 0, 1, 5, 4 } };
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
                    vertices.push_back({ c[0], c[1], c[2], shades[f] });
                }
            }
            triangleVertexCount = static_cast<UINT>(vertices.size());

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
            lineVertexCount = static_cast<UINT>(vertices.size()) - triangleVertexCount;
        }

        D3D12_RESOURCE_BARRIER ViewTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            return barrier;
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
    }

    bool RendererDX12::CreateScenePipeline()
    {
        D3D12_ROOT_PARAMETER param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param.Constants.ShaderRegister = 0;
        param.Constants.Num32BitValues = 16;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.NumParameters = 1;
        rootDesc.pParameters = &param;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
        {
            xg::Log(xg::MessageType::Error, "RendererDX12: scene root signature: %s",
                error ? static_cast<const char*>(error->GetBufferPointer()) : "unknown error");
            return false;
        }
        if (!Check(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_sceneRootSignature)), "CreateRootSignature (scene)"))
            return false;

        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;
        const size_t sourceLength = std::strlen(s_sceneShaderSource);
        if (FAILED(D3DCompile(s_sceneShaderSource, sourceLength, "xgScene", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vertexShader, &error))
            || FAILED(D3DCompile(s_sceneShaderSource, sourceLength, "xgScene", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &pixelShader, &error)))
        {
            xg::Log(xg::MessageType::Error, "RendererDX12: scene shader: %s",
                error ? static_cast<const char*>(error->GetBufferPointer()) : "unknown error");
            return false;
        }

        D3D12_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SceneVertex, X),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(SceneVertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
        pso.pRootSignature = _sceneRootSignature.Get();
        pso.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        pso.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
        pso.InputLayout = { layout, 2 };
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = BackBufferFormat;
        pso.DSVFormat = DepthFormat;
        pso.SampleMask = UINT_MAX;
        pso.SampleDesc.Count = 1;
        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso.RasterizerState.DepthClipEnable = TRUE;
        pso.DepthStencilState.DepthEnable = TRUE;
        pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        D3D12_RENDER_TARGET_BLEND_DESC& blend = pso.BlendState.RenderTarget[0];
        blend.BlendEnable = TRUE;
        blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        if (!Check(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_sceneTrianglePipeline)), "CreateGraphicsPipelineState (scene triangles)"))
            return false;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        if (!Check(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_sceneLinePipeline)), "CreateGraphicsPipelineState (scene lines)"))
            return false;

        std::vector<SceneVertex> vertices;
        BuildTestScene(vertices, _sceneTriangleCount, _sceneLineVertexCount);

        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = vertices.size() * sizeof(SceneVertex);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (!Check(_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&_sceneVertexBuffer)), "CreateCommittedResource (scene vertices)"))
            return false;
        void* mapped = nullptr;
        D3D12_RANGE noRead = { 0, 0 };
        if (!Check(_sceneVertexBuffer->Map(0, &noRead, &mapped), "scene vertices Map"))
            return false;
        std::memcpy(mapped, vertices.data(), vertices.size() * sizeof(SceneVertex));
        _sceneVertexBuffer->Unmap(0, nullptr);

        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
        rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.NumDescriptors = MaxRenderViews;
        D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
        dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvDesc.NumDescriptors = MaxRenderViews;
        if (!Check(_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&_viewRtvHeap)), "CreateDescriptorHeap (view RTV)")
            || !Check(_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&_viewDsvHeap)), "CreateDescriptorHeap (view DSV)"))
            return false;
        _dsvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        _freeViewTargets.reserve(MaxRenderViews);
        for (UINT i = MaxRenderViews; i > 0; --i)
            _freeViewTargets.push_back(i - 1);
        return true;
    }

    bool RendererDX12::CreateRenderViewTargets(RenderView& view, UINT width, UINT height)
    {
        const UINT slot = static_cast<UINT>(view.Id - 1);

        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC colorDesc = {};
        colorDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        colorDesc.Width = width;
        colorDesc.Height = height;
        colorDesc.DepthOrArraySize = 1;
        colorDesc.MipLevels = 1;
        colorDesc.Format = BackBufferFormat;
        colorDesc.SampleDesc.Count = 1;
        colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE colorClear = {};
        colorClear.Format = BackBufferFormat;
        const Color& clear = view.Camera.ClearColor;
        colorClear.Color[0] = clear.r;
        colorClear.Color[1] = clear.g;
        colorClear.Color[2] = clear.b;
        colorClear.Color[3] = clear.a;

        ComPtr<ID3D12Resource> color;
        if (!Check(_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &colorDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &colorClear, IID_PPV_ARGS(&color)), "CreateCommittedResource (view color)"))
            return false;

        D3D12_RESOURCE_DESC depthDesc = colorDesc;
        depthDesc.Format = DepthFormat;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        D3D12_CLEAR_VALUE depthClear = {};
        depthClear.Format = DepthFormat;
        depthClear.DepthStencil.Depth = 1.0f;

        ComPtr<ID3D12Resource> depth;
        if (!Check(_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear, IID_PPV_ARGS(&depth)), "CreateCommittedResource (view depth)"))
            return false;

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = _viewRtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += static_cast<SIZE_T>(view.TargetIndex) * _rtvDescriptorSize;
        _device->CreateRenderTargetView(color.Get(), nullptr, rtv);

        D3D12_CPU_DESCRIPTOR_HANDLE dsv = _viewDsvHeap->GetCPUDescriptorHandleForHeapStart();
        dsv.ptr += static_cast<SIZE_T>(view.TargetIndex) * _dsvDescriptorSize;
        _device->CreateDepthStencilView(depth.Get(), nullptr, dsv);

        D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = BackBufferFormat;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MipLevels = 1;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu = _srvHeap->GetCPUDescriptorHandleForHeapStart();
        cpu.ptr += static_cast<SIZE_T>(slot) * _srvDescriptorSize;
        _device->CreateShaderResourceView(color.Get(), &srv, cpu);

        UITexture& texture = _textures[slot];
        texture.Resource = color;
        texture.Width = static_cast<int32_t>(width);
        texture.Height = static_cast<int32_t>(height);
        view.Depth = depth;
        view.Width = width;
        view.Height = height;
        return true;
    }

    RendererDX12::RenderView* RendererDX12::FindRenderView(uint64_t id)
    {
        for (RenderView& view : _renderViews)
            if (view.Id == id)
                return &view;
        return nullptr;
    }

    uint64_t RendererDX12::CreateRenderView(int32_t width, int32_t height)
    {
        if (_failed || !_sceneTrianglePipeline || _freeTextureSlots.empty() || _freeViewTargets.empty())
            return 0;

        RenderView view;
        view.Id = static_cast<uint64_t>(_freeTextureSlots.back()) + 1;
        view.TargetIndex = _freeViewTargets.back();
        view.Camera = DefaultCamera();
        if (!CreateRenderViewTargets(view, static_cast<UINT>((std::max)(width, 1)), static_cast<UINT>((std::max)(height, 1))))
            return 0;

        _freeTextureSlots.pop_back();
        _freeViewTargets.pop_back();
        _renderViews.push_back(view);
        return view.Id;
    }

    bool RendererDX12::ResizeRenderView(uint64_t id, int32_t width, int32_t height)
    {
        RenderView* view = FindRenderView(id);
        if (!view)
            return false;
        const UINT w = static_cast<UINT>((std::max)(width, 1));
        const UINT h = static_cast<UINT>((std::max)(height, 1));
        if (w == view->Width && h == view->Height)
            return true;
        WaitForGPU();
        return CreateRenderViewTargets(*view, w, h);
    }

    void RendererDX12::DestroyRenderView(uint64_t id)
    {
        for (auto it = _renderViews.begin(); it != _renderViews.end(); ++it)
        {
            if (it->Id != id)
                continue;
            WaitForGPU();
            UITexture& texture = _textures[static_cast<size_t>(id - 1)];
            texture.Resource.Reset();
            texture.Width = 0;
            texture.Height = 0;
            _freeTextureSlots.push_back(static_cast<UINT>(id - 1));
            _freeViewTargets.push_back(it->TargetIndex);
            _renderViews.erase(it);
            return;
        }
    }

    void RendererDX12::SetRenderViewCamera(uint64_t id, const RenderViewCamera* camera)
    {
        if (RenderView* view = FindRenderView(id); view && camera)
            view->Camera = *camera;
    }

    void RendererDX12::RecordRenderViews()
    {
        if (_renderViews.empty())
            return;

        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = _sceneVertexBuffer->GetGPUVirtualAddress();
        vbv.SizeInBytes = (_sceneTriangleCount + _sceneLineVertexCount) * static_cast<UINT>(sizeof(SceneVertex));
        vbv.StrideInBytes = sizeof(SceneVertex);

        for (const RenderView& view : _renderViews)
        {
            ID3D12Resource* color = _textures[static_cast<size_t>(view.Id - 1)].Resource.Get();
            D3D12_RESOURCE_BARRIER toTarget = ViewTransition(color, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            _commandList->ResourceBarrier(1, &toTarget);

            D3D12_CPU_DESCRIPTOR_HANDLE rtv = _viewRtvHeap->GetCPUDescriptorHandleForHeapStart();
            rtv.ptr += static_cast<SIZE_T>(view.TargetIndex) * _rtvDescriptorSize;
            D3D12_CPU_DESCRIPTOR_HANDLE dsv = _viewDsvHeap->GetCPUDescriptorHandleForHeapStart();
            dsv.ptr += static_cast<SIZE_T>(view.TargetIndex) * _dsvDescriptorSize;
            _commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
            const float clearColor[4] = { view.Camera.ClearColor.r, view.Camera.ClearColor.g, view.Camera.ClearColor.b, view.Camera.ClearColor.a };
            _commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
            _commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(view.Width), static_cast<float>(view.Height), 0.0f, 1.0f };
            const D3D12_RECT scissor = { 0, 0, static_cast<LONG>(view.Width), static_cast<LONG>(view.Height) };
            _commandList->RSSetViewports(1, &viewport);
            _commandList->RSSetScissorRects(1, &scissor);

            float viewProjection[16];
            ViewProjectionLH(view.Camera, static_cast<float>(view.Width) / static_cast<float>(view.Height), viewProjection);
            _commandList->SetGraphicsRootSignature(_sceneRootSignature.Get());
            _commandList->SetGraphicsRoot32BitConstants(0, 16, viewProjection, 0);
            _commandList->IASetVertexBuffers(0, 1, &vbv);

            _commandList->SetPipelineState(_sceneTrianglePipeline.Get());
            _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            _commandList->DrawInstanced(_sceneTriangleCount, 1, 0, 0);

            _commandList->SetPipelineState(_sceneLinePipeline.Get());
            _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
            _commandList->DrawInstanced(_sceneLineVertexCount, 1, _sceneTriangleCount, 0);

            D3D12_RESOURCE_BARRIER toShader = ViewTransition(color, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            _commandList->ResourceBarrier(1, &toShader);
        }
    }

}
