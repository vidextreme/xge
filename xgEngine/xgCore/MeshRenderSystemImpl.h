// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <vector>
#include "xgMeshRenderer.h"
#include "xgRenderer.h"

namespace xg
{
    class TransformSystem;

    class MeshRenderSystemImpl : public MeshRenderSystem
    {
        XG_DECLARE_SUBSYSTEM(MeshRenderSystemImpl, MeshRenderSystem)
    public:
        MeshRenderSystemImpl(ComponentDataRegistry* registry, ResourceSystem* resources, TransformSystem* transforms, uint32_t ownerModuleId);
        ~MeshRenderSystemImpl() override;

        void Shutdown() override;
        // Releases every resource and stops listening (the engine calls it before subsystems shut down in any order).
        void Detach();

        DataTypeID GetDataType() const override { return _type; }
        int32_t GetDrawCount() const override { return _drawCount; }
        int32_t GetMaterialDrawCount() const override { return _materialDrawCount; }

        // Builds and submits the frame's scene draws (the engine calls it after the transform update).
        void Update(Renderer* renderer);

    private:
        struct SlotState
        {
            ResourceGUID ModelGuid = {};
            ResourceGUID MaterialGuid = {};
            ResourceHandle Model = 0;
            ResourceHandle Material = 0;
        };

        static void OnRemoved(void* user, ActorID actor, uint32_t slot);
        static void OnMoved(void* user, ActorID actor, uint32_t from, uint32_t to);

        void ReleaseState(SlotState& state);
        void Track(SlotState& state, const MeshRendererData& data);
        void ResetAll();

        ComponentDataRegistry* _registry = nullptr;
        ResourceSystem* _resources = nullptr;
        TransformSystem* _transforms = nullptr;
        uint32_t _ownerModuleId = 0;
        DataTypeID _type = 0;
        DataPoolListener _listener = {};
        Renderer* _renderer = nullptr;
        ResourceTypeID _modelType = 0;
        ResourceTypeID _materialType = 0;
        std::vector<SlotState> _slots;
        std::vector<SceneDraw> _draws;
        int32_t _drawCount = 0;
        int32_t _materialDrawCount = 0;
    };
}
