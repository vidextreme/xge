// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// Offscreen render views: color + depth targets that share the UI texture table, so a panel can draw them.
// The scene content is a textured test cube and ground grid until a real scene renderer exists. Views render in
// linear color through an sRGB target.
#include "pch.h"
#include "RendererDX12.h"
#include "xgLog.h"
#include "dxil.scene.textured.h"
#include "dxil.scene.unlit.h"
#include "xgShaderTypes.h"
#include "xgMat4x4.h"
#include <cmath>
#include <cstring>

namespace xg {

    namespace
    {
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

        float ToLinear(float c)
        {
            return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
        }

        void BuildTestLines(std::vector<SceneVertex>& vertices, UINT& lineVertexCount)
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
            lineVertexCount = static_cast<UINT>(vertices.size());
        }

        bool CreateUploadVertices(ID3D12Device* device, const void* data, size_t size, Microsoft::WRL::ComPtr<ID3D12Resource>& buffer)
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
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, IID_PPV_ARGS(&buffer))))
                return false;
            void* mapped = nullptr;
            D3D12_RANGE noRead = { 0, 0 };
            if (FAILED(buffer->Map(0, &noRead, &mapped)))
                return false;
            std::memcpy(mapped, data, size);
            buffer->Unmap(0, nullptr);
            return true;
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

        D3D12_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SceneVertex, X),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(SceneVertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
        pso.pRootSignature = _sceneRootSignature.Get();
        pso.VS = { shaders::dxil::_sceneUnlitVS, sizeof(shaders::dxil::_sceneUnlitVS) };
        pso.PS = { shaders::dxil::_sceneUnlitPS, sizeof(shaders::dxil::_sceneUnlitPS) };
        pso.InputLayout = { layout, 2 };
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = ViewColorTarget;
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

        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        if (!Check(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_sceneLinePipeline)), "CreateGraphicsPipelineState (scene lines)"))
            return false;

        std::vector<SceneVertex> vertices;
        BuildTestLines(vertices, _sceneLineVertexCount);
        if (!CreateUploadVertices(_device.Get(), vertices.data(), vertices.size() * sizeof(SceneVertex), _sceneVertexBuffer))
            return Check(E_FAIL, "scene vertices");

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
        return CreateTexturedPipeline();
    }

    bool RendererDX12::CreateTexturedPipeline()
    {
        D3D12_DESCRIPTOR_RANGE range = {};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = 0;
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER params[2] = {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[0].Constants.ShaderRegister = 0;
        params[0].Constants.Num32BitValues = sizeof(SceneTexturedConstants) / 4;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable.NumDescriptorRanges = 1;
        params[1].DescriptorTable.pDescriptorRanges = &range;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
        rootDesc.NumParameters = 2;
        rootDesc.pParameters = params;
        rootDesc.NumStaticSamplers = 1;
        rootDesc.pStaticSamplers = &sampler;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
        {
            xg::Log(xg::MessageType::Error, "RendererDX12: textured root signature: %s",
                error ? static_cast<const char*>(error->GetBufferPointer()) : "unknown error");
            return false;
        }
        if (!Check(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&_texturedRootSignature)), "CreateRootSignature (textured)"))
            return false;

        D3D12_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SceneTexturedVertex, X),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(SceneTexturedVertex, U),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(SceneTexturedVertex, Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
        pso.pRootSignature = _texturedRootSignature.Get();
        pso.VS = { shaders::dxil::_sceneTexturedVS, sizeof(shaders::dxil::_sceneTexturedVS) };
        pso.PS = { shaders::dxil::_sceneTexturedPS, sizeof(shaders::dxil::_sceneTexturedPS) };
        pso.InputLayout = { layout, 3 };
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = ViewColorTarget;
        pso.DSVFormat = DepthFormat;
        pso.SampleMask = UINT_MAX;
        pso.SampleDesc.Count = 1;
        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso.RasterizerState.DepthClipEnable = TRUE;
        pso.DepthStencilState.DepthEnable = TRUE;
        pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        if (!Check(_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&_texturedPipeline)), "CreateGraphicsPipelineState (scene textured)"))
            return false;

        std::vector<SceneTexturedVertex> cube;
        BuildTestCube(cube);
        _cubeVertexCount = static_cast<UINT>(cube.size());
        if (!CreateUploadVertices(_device.Get(), cube.data(), cube.size() * sizeof(SceneTexturedVertex), _cubeVertexBuffer))
            return Check(E_FAIL, "cube vertices");

        const uint8_t white[4] = { 255, 255, 255, 255 };
        const TextureMip mip{ white, sizeof(white), 4 };
        const TextureDesc desc{ 1, 1, 1, TextureFormat::RGBA8, TextureFlags::None };
        _whiteTexture = CreateTexture(&desc, &mip);
        return _whiteTexture != 0;
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
        colorDesc.Format = ViewColorStorage;
        colorDesc.SampleDesc.Count = 1;
        colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE colorClear = {};
        colorClear.Format = ViewColorTarget;
        const Color& clear = view.Camera.ClearColor;
        colorClear.Color[0] = ToLinear(clear.r);
        colorClear.Color[1] = ToLinear(clear.g);
        colorClear.Color[2] = ToLinear(clear.b);
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
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = ViewColorTarget;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        _device->CreateRenderTargetView(color.Get(), &rtvDesc, rtv);

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
        if (_failed || !_texturedPipeline || _freeTextureSlots.empty() || _freeViewTargets.empty())
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

    void RendererDX12::SetRenderViewTexture(uint64_t viewId, uint64_t textureId, RenderViewTextureMode mode)
    {
        RenderView* view = FindRenderView(viewId);
        if (!view)
            return;
        view->Texture = textureId != viewId && FindTexture(textureId) ? textureId : 0;
        view->Mode = mode;
    }

    void RendererDX12::RecordRenderViews()
    {
        if (_renderViews.empty())
            return;

        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = _sceneVertexBuffer->GetGPUVirtualAddress();
        vbv.SizeInBytes = _sceneLineVertexCount * static_cast<UINT>(sizeof(SceneVertex));
        vbv.StrideInBytes = sizeof(SceneVertex);
        D3D12_VERTEX_BUFFER_VIEW cubeView = {};
        cubeView.BufferLocation = _cubeVertexBuffer->GetGPUVirtualAddress();
        cubeView.SizeInBytes = _cubeVertexCount * static_cast<UINT>(sizeof(SceneTexturedVertex));
        cubeView.StrideInBytes = sizeof(SceneTexturedVertex);

        ID3D12DescriptorHeap* heaps[] = { _srvHeap.Get() };
        _commandList->SetDescriptorHeaps(1, heaps);
        const D3D12_GPU_DESCRIPTOR_HANDLE heapStart = _srvHeap->GetGPUDescriptorHandleForHeapStart();

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
            const float clearColor[4] = { ToLinear(view.Camera.ClearColor.r), ToLinear(view.Camera.ClearColor.g), ToLinear(view.Camera.ClearColor.b), view.Camera.ClearColor.a };
            _commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
            _commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(view.Width), static_cast<float>(view.Height), 0.0f, 1.0f };
            const D3D12_RECT scissor = { 0, 0, static_cast<LONG>(view.Width), static_cast<LONG>(view.Height) };
            _commandList->RSSetViewports(1, &viewport);
            _commandList->RSSetScissorRects(1, &scissor);

            SceneTexturedConstants constants = {};
            ViewProjectionLH(view.Camera, static_cast<float>(view.Width) / static_cast<float>(view.Height), constants.ViewProjection);
            constants.Mode = static_cast<uint32_t>(view.Mode);

            const uint64_t texture = view.Texture != 0 && FindTexture(view.Texture) ? view.Texture : _whiteTexture;
            D3D12_GPU_DESCRIPTOR_HANDLE srv = heapStart;
            srv.ptr += static_cast<UINT64>(texture - 1) * _srvDescriptorSize;
            _commandList->SetGraphicsRootSignature(_texturedRootSignature.Get());
            _commandList->SetGraphicsRoot32BitConstants(0, sizeof(SceneTexturedConstants) / 4, &constants, 0);
            _commandList->SetGraphicsRootDescriptorTable(1, srv);
            _commandList->SetPipelineState(_texturedPipeline.Get());
            _commandList->IASetVertexBuffers(0, 1, &cubeView);
            _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            _commandList->DrawInstanced(_cubeVertexCount, 1, 0, 0);

            _commandList->SetGraphicsRootSignature(_sceneRootSignature.Get());
            _commandList->SetGraphicsRoot32BitConstants(0, 16, constants.ViewProjection, 0);
            _commandList->SetPipelineState(_sceneLinePipeline.Get());
            _commandList->IASetVertexBuffers(0, 1, &vbv);
            _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
            _commandList->DrawInstanced(_sceneLineVertexCount, 1, 0, 0);

            D3D12_RESOURCE_BARRIER toShader = ViewTransition(color, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            _commandList->ResourceBarrier(1, &toShader);
        }
    }

}
