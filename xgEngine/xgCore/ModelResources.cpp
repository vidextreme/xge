// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ModelResources.h"
#include <cstring>
#include "xgChunkFile.h"
#include "xgLog.h"

namespace xg
{
    namespace
    {
        constexpr uint32_t TagVertexHeader = MakeChunkTag('V', 'H', 'D', 'R');
        constexpr uint32_t TagVertices = MakeChunkTag('V', 'T', 'X', 'S');
        constexpr uint32_t TagIndices16 = MakeChunkTag('I', 'X', '1', '6');
        constexpr uint32_t TagIndices32 = MakeChunkTag('I', 'X', '3', '2');
        constexpr uint32_t TagNodes = MakeChunkTag('N', 'O', 'D', 'E');
        constexpr int32_t VertexStride = 52;
        constexpr int32_t VertexLayoutVersion = 1;
        constexpr uint64_t ProcessedNodeSize = 208;

        struct ModelContext
        {
            ResourceSystem* Resources = nullptr;
            Renderer* Target = nullptr;
        };

        struct LoadedPart
        {
            const uint8_t* Vertices = nullptr;
            int32_t VertexCount = 0;
            const uint8_t* Indices = nullptr;
            int32_t IndexCount = 0;
            IndexFormat Format = IndexFormat::UInt16;
            int32_t MeshIndex = -1;
            int32_t MaterialIndex = 0;
            float BoundsMin[3] = {};
            float BoundsMax[3] = {};
        };

        struct LoadedInstance
        {
            size_t Part = 0;
            float World[16] = {};
        };

        struct LoadedModel
        {
            ResourceBlob Meshes = {};
            std::vector<LoadedPart> Parts;
            std::vector<LoadedInstance> Instances;
            int32_t NodeCount = 0;
        };

        struct ModelData
        {
            ModelResourceInfo Info = {};
            std::vector<ModelPartInfo> Parts;
            std::vector<uint64_t> Meshes;
        };

        ModelContext _context;

        int32_t ReadI32(const uint8_t* data, uint64_t offset)
        {
            int32_t value;
            std::memcpy(&value, data + offset, sizeof(value));
            return value;
        }

        void ReadFloats(const uint8_t* data, uint64_t offset, float* out, int count)
        {
            std::memcpy(out, data + offset, sizeof(float) * static_cast<size_t>(count));
        }

        void Multiply(const float* a, const float* b, float* out)
        {
            for (int row = 0; row < 4; ++row)
                for (int column = 0; column < 4; ++column)
                {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; ++k)
                        sum += a[row * 4 + k] * b[k * 4 + column];
                    out[row * 4 + column] = sum;
                }
        }

        void Identity(float* out)
        {
            for (int i = 0; i < 16; ++i)
                out[i] = i % 5 == 0 ? 1.0f : 0.0f;
        }

        // Walks the items of a validated chunk file, calling visit(item view) for each.
        template <typename Visit>
        bool ForEachItem(const uint8_t* data, uint64_t size, Visit visit)
        {
            uint64_t cursor = sizeof(ChunkFileHeader);
            while (cursor < size)
            {
                ChunkView item;
                if (!chunkfile::ReadChunk(data, size, cursor, item))
                    return false;
                if (item.Type == ChunkTagItem && !visit(item))
                    return false;
            }
            return true;
        }

        template <typename Visit>
        bool ForEachChild(const uint8_t* data, const ChunkView& item, Visit visit)
        {
            uint64_t inner = item.Offset + sizeof(ResourceGUID);
            const uint64_t end = item.Offset + item.Size;
            while (inner < end)
            {
                ChunkView child;
                if (!chunkfile::ReadChunk(data, end, inner, child))
                    return false;
                visit(child);
            }
            return true;
        }

        bool ReadParts(const uint8_t* data, uint64_t size, LoadedModel& model)
        {
            return ForEachItem(data, size, [&](const ChunkView& item)
            {
                LoadedPart part;
                bool header = false;
                bool vertices = false;
                bool indices = false;
                uint64_t vertexBytes = 0;
                uint64_t indexBytes = 0;
                ForEachChild(data, item, [&](const ChunkView& child)
                {
                    if (child.Type == TagVertexHeader && child.Size >= 52)
                    {
                        part.VertexCount = ReadI32(data, child.Offset);
                        part.IndexCount = ReadI32(data, child.Offset + 4);
                        header = ReadI32(data, child.Offset + 8) == VertexStride && ReadI32(data, child.Offset + 12) == VertexLayoutVersion;
                        part.MeshIndex = ReadI32(data, child.Offset + 16);
                        part.MaterialIndex = ReadI32(data, child.Offset + 20);
                        ReadFloats(data, child.Offset + 28, part.BoundsMin, 3);
                        ReadFloats(data, child.Offset + 40, part.BoundsMax, 3);
                    }
                    else if (child.Type == TagVertices)
                    {
                        part.Vertices = data + child.Offset;
                        vertexBytes = child.Size;
                        vertices = true;
                    }
                    else if (child.Type == TagIndices16 || child.Type == TagIndices32)
                    {
                        part.Indices = data + child.Offset;
                        part.Format = child.Type == TagIndices32 ? IndexFormat::UInt32 : IndexFormat::UInt16;
                        indexBytes = child.Size;
                        indices = true;
                    }
                });
                const uint64_t indexSize = part.Format == IndexFormat::UInt32 ? 4 : 2;
                if (!header || !vertices || !indices || part.VertexCount <= 0 || part.IndexCount <= 0
                    || vertexBytes < static_cast<uint64_t>(part.VertexCount) * VertexStride
                    || indexBytes < static_cast<uint64_t>(part.IndexCount) * indexSize)
                    return false;
                model.Parts.push_back(part);
                return true;
            });
        }

        // Places every part: once per node whose mesh index matches (geometry offset times node world), or once at
        // the origin when no node uses its mesh.
        bool PlaceParts(const uint8_t* data, uint64_t size, LoadedModel& model)
        {
            const uint8_t* nodes = nullptr;
            int32_t count = 0;
            ForEachItem(data, size, [&](const ChunkView& item)
            {
                ForEachChild(data, item, [&](const ChunkView& child)
                {
                    if (child.Type == TagNodes && child.Size >= 8 && !nodes)
                    {
                        count = ReadI32(data, child.Offset);
                        if (count >= 0 && child.Size >= 8 + static_cast<uint64_t>(count) * ProcessedNodeSize)
                            nodes = data + child.Offset + 8;
                    }
                });
                return true;
            });
            model.NodeCount = nodes ? count : 0;
            for (size_t p = 0; p < model.Parts.size(); ++p)
            {
                bool placed = false;
                for (int32_t n = 0; nodes && n < count; ++n)
                {
                    const uint8_t* node = nodes + static_cast<uint64_t>(n) * ProcessedNodeSize;
                    if (ReadI32(node, 4) != model.Parts[p].MeshIndex)
                        continue;
                    float geometry[16];
                    float world[16];
                    ReadFloats(node, 80, geometry, 16);
                    ReadFloats(node, 144, world, 16);
                    LoadedInstance instance;
                    instance.Part = p;
                    Multiply(geometry, world, instance.World);
                    model.Instances.push_back(instance);
                    placed = true;
                }
                if (!placed)
                {
                    LoadedInstance instance;
                    instance.Part = p;
                    Identity(instance.World);
                    model.Instances.push_back(instance);
                }
            }
            return true;
        }

        bool LoadModel(const ResourceID* id, void* user, void** outData)
        {
            auto* context = static_cast<ModelContext*>(user);
            char text[ResourceIDTextCapacity] = {};
            FormatResourceID(id, text, ResourceIDTextCapacity);
            auto* model = new LoadedModel();
            ResourceBlob hierarchy = {};
            bool valid = context->Resources->ReadFile(&id->File, "xdv", &model->Meshes)
                && context->Resources->ReadFile(&id->File, "xdh", &hierarchy);
            if (valid)
            {
                const auto* meshes = static_cast<const uint8_t*>(model->Meshes.Data);
                const auto* nodes = static_cast<const uint8_t*>(hierarchy.Data);
                valid = ValidateChunkFile(meshes, model->Meshes.Size) == ChunkFileStatus::Valid
                    && ValidateChunkFile(nodes, hierarchy.Size) == ChunkFileStatus::Valid
                    && ReadParts(meshes, model->Meshes.Size, *model) && !model->Parts.empty()
                    && PlaceParts(nodes, hierarchy.Size, *model);
            }
            context->Resources->FreeBlob(&hierarchy);
            if (!valid)
            {
                xg::Log(xg::MessageType::Warning, "[Models] %s: no valid .xdv and .xdh in the resource sources", text);
                context->Resources->FreeBlob(&model->Meshes);
                delete model;
                return false;
            }
            *outData = model;
            return true;
        }

        void TransformBounds(const LoadedPart& part, const float* world, float* min, float* max, bool& first)
        {
            for (int corner = 0; corner < 8; ++corner)
            {
                const float p[3] = {
                    corner & 1 ? part.BoundsMax[0] : part.BoundsMin[0],
                    corner & 2 ? part.BoundsMax[1] : part.BoundsMin[1],
                    corner & 4 ? part.BoundsMax[2] : part.BoundsMin[2] };
                for (int axis = 0; axis < 3; ++axis)
                {
                    const float v = p[0] * world[axis] + p[1] * world[4 + axis] + p[2] * world[8 + axis] + world[12 + axis];
                    min[axis] = first ? v : (std::min)(min[axis], v);
                    max[axis] = first ? v : (std::max)(max[axis], v);
                }
                first = false;
            }
        }

        bool FinalizeModel(void* data, void* user, void** outData)
        {
            auto* context = static_cast<ModelContext*>(user);
            auto* model = static_cast<LoadedModel*>(data);
            auto* result = new ModelData();
            bool ok = true;
            for (const LoadedPart& part : model->Parts)
            {
                const MeshDesc desc{ part.VertexCount, VertexStride, part.Vertices, part.IndexCount, part.Format, part.Indices };
                const uint64_t mesh = context->Target->CreateMesh(&desc);
                if (mesh == 0)
                {
                    ok = false;
                    break;
                }
                result->Meshes.push_back(mesh);
            }
            if (ok)
            {
                bool first = true;
                for (const LoadedInstance& instance : model->Instances)
                {
                    const LoadedPart& part = model->Parts[instance.Part];
                    ModelPartInfo info = {};
                    info.Mesh = result->Meshes[instance.Part];
                    info.MaterialIndex = part.MaterialIndex;
                    info.TriangleCount = part.IndexCount / 3;
                    std::memcpy(info.World, instance.World, sizeof(info.World));
                    result->Parts.push_back(info);
                    TransformBounds(part, instance.World, result->Info.BoundsMin, result->Info.BoundsMax, first);
                }
                result->Info.PartCount = static_cast<int32_t>(result->Parts.size());
                result->Info.NodeCount = model->NodeCount;
                result->Info.Parts = result->Parts.data();
            }
            else
            {
                for (uint64_t mesh : result->Meshes)
                    context->Target->DestroyMesh(mesh);
                delete result;
                result = nullptr;
            }
            context->Resources->FreeBlob(&model->Meshes);
            delete model;
            *outData = result;
            return result != nullptr;
        }

        void UnloadModel(void* data, void* user)
        {
            auto* context = static_cast<ModelContext*>(user);
            auto* model = static_cast<ModelData*>(data);
            if (context->Target)
                for (uint64_t mesh : model->Meshes)
                    context->Target->DestroyMesh(mesh);
            delete model;
        }
    }

    ResourceTypeID RegisterModelResources(ResourceSystem& resources, uint32_t moduleId, Renderer* renderer)
    {
        if (!renderer)
            return 0;
        _context.Resources = &resources;
        _context.Target = renderer;
        const ResourceTypeDesc desc{ ModelResourceType, &LoadModel, &UnloadModel, &_context, &FinalizeModel, ResourceTypeFlags::Async };
        return resources.RegisterType(moduleId, &desc);
    }
}
