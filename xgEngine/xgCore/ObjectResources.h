// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "xgObjectDriver.h"
#include "xgTransforms.h"
#include "xgResources.h"

namespace xg
{
    enum class ObjectNodeKind : uint32_t
    {
        Mesh = 0,
        Proxy = 1,
    };

    enum class ObjectNodeExpose : uint32_t
    {
        None = 0,
        Actor = 1,
        Proxy = 2,
    };

    struct ObjectNode
    {
        ResourceGUID Id = {};
        int32_t Parent = -1;
        ObjectNodeKind Kind = ObjectNodeKind::Mesh;
        ObjectNodeExpose Expose = ObjectNodeExpose::None;
        uint32_t NameHash = 0;
        std::string Name;
        // Local-to-root matrix of the node (row-major for row vectors), baked at build time.
        float Baked[16] = {};
        // Nearest ancestor exposed as an actor (-1 = the driver's actor) and the node's pose relative to it.
        int32_t ActorParent = -1;
        TransformData ActorPose = {};
        // Authored components of an actor node (a JSON array, empty for none).
        std::string Components;
    };

    // One drawn submesh: the node it hangs on, the model, its submesh item (zero = every part) and its material.
    struct ObjectPart
    {
        int32_t Node = 0;
        ResourceGUID Model = {};
        ResourceGUID Submesh = {};
        ResourceGUID Material = {};
    };

    // The data of a Ready "object" resource (ResourceSystem::GetData), loaded from <guid>.xdo by the asset GUID alone.
    struct ObjectResourceData
    {
        std::vector<ObjectNode> Nodes;
        std::vector<ObjectPart> Parts;
    };

    // Registers the "object" resource type (Async): a job worker reads <guid>.xdo (nodes parents first, parts, names).
    ResourceTypeID RegisterObjectResources(ResourceSystem& resources, uint32_t moduleId);

    // FNV-1a over the UTF-8 bytes, as the .xdo stores node names.
    uint32_t HashObjectName(const char* name);
}
