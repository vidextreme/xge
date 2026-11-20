// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "xgBase.h"
#include "../3rdparty/ufbx/ufbx.h"

// FBX reading for the editor's mesh importer (xgEditorKit.Managed MeshAssets): ufbx loads the file converted to the
// engine's space (left-handed, Y up, meters), and every material part of every mesh comes out triangulated, with
// de-duplicated vertices. Matrices are row-major for row vectors (v * M): rows 0-2 are the axes, row 3 the translation.

namespace
{
    constexpr uint32_t _channelNormals = 1u << 0;
    constexpr uint32_t _channelTangents = 1u << 1;
    constexpr uint32_t _channelUV0 = 1u << 2;
    constexpr uint32_t _channelColors = 1u << 3;

    struct PartVertex
    {
        float Position[3];
        float Normal[3];
        float Tangent[4];
        float UV[2];
        uint8_t Color[4];
    };
    static_assert(sizeof(PartVertex) == 52, "PartVertex must stay packed");

    struct FbxFile
    {
        ufbx_scene* Scene = nullptr;
        int32_t CachedMesh = -1;
        int32_t CachedPart = -1;
        std::vector<PartVertex> Vertices;
        std::vector<uint32_t> Indices;
        uint32_t Channels = 0;
        float BoundsMin[3] = {};
        float BoundsMax[3] = {};
        std::string MaterialName;
    };

    void ToRowMatrix(const ufbx_matrix& m, float out[16])
    {
        for (int row = 0; row < 4; ++row)
        {
            out[row * 4 + 0] = static_cast<float>(m.cols[row].x);
            out[row * 4 + 1] = static_cast<float>(m.cols[row].y);
            out[row * 4 + 2] = static_cast<float>(m.cols[row].z);
            out[row * 4 + 3] = row == 3 ? 1.0f : 0.0f;
        }
    }

    void CopyError(const ufbx_error& error, char* buffer, int32_t capacity)
    {
        if (!buffer || capacity <= 0)
            return;
        char text[512] = {};
        ufbx_format_error(text, sizeof(text), &error);
        const size_t length = (std::min)(std::strlen(text), static_cast<size_t>(capacity - 1));
        std::memcpy(buffer, text, length);
        buffer[length] = '\0';
    }

    uint8_t ToByte(ufbx_real value)
    {
        const double clamped = (std::min)((std::max)(static_cast<double>(value), 0.0), 1.0);
        return static_cast<uint8_t>(std::lround(clamped * 255.0));
    }

    const ufbx_mesh* MeshAt(const FbxFile* file, int32_t mesh)
    {
        if (!file || !file->Scene || mesh < 0 || static_cast<size_t>(mesh) >= file->Scene->meshes.count)
            return nullptr;
        return file->Scene->meshes.data[mesh];
    }

    bool BuildPart(FbxFile* file, int32_t meshIndex, int32_t partIndex)
    {
        if (file->CachedMesh == meshIndex && file->CachedPart == partIndex)
            return true;
        const ufbx_mesh* mesh = MeshAt(file, meshIndex);
        if (!mesh || partIndex < 0 || static_cast<size_t>(partIndex) >= mesh->material_parts.count)
            return false;
        const ufbx_mesh_part& part = mesh->material_parts.data[partIndex];

        file->Vertices.clear();
        file->Indices.clear();
        file->Channels = 0;
        if (mesh->vertex_normal.exists)
            file->Channels |= _channelNormals;
        if (mesh->vertex_tangent.exists && mesh->vertex_bitangent.exists && mesh->vertex_normal.exists)
            file->Channels |= _channelTangents;
        if (mesh->vertex_uv.exists)
            file->Channels |= _channelUV0;
        if (mesh->vertex_color.exists)
            file->Channels |= _channelColors;

        std::vector<uint32_t> corners(mesh->max_face_triangles * 3);
        file->Vertices.reserve(part.num_triangles * 3);
        for (size_t f = 0; f < part.face_indices.count; ++f)
        {
            const ufbx_face face = mesh->faces.data[part.face_indices.data[f]];
            const uint32_t triangles = ufbx_triangulate_face(corners.data(), corners.size(), mesh, face);
            for (uint32_t c = 0; c < triangles * 3; ++c)
            {
                const uint32_t index = corners[c];
                PartVertex vertex = {};
                const ufbx_vec3 position = ufbx_get_vertex_vec3(&mesh->vertex_position, index);
                vertex.Position[0] = static_cast<float>(position.x);
                vertex.Position[1] = static_cast<float>(position.y);
                vertex.Position[2] = static_cast<float>(position.z);
                if (file->Channels & _channelNormals)
                {
                    const ufbx_vec3 normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
                    vertex.Normal[0] = static_cast<float>(normal.x);
                    vertex.Normal[1] = static_cast<float>(normal.y);
                    vertex.Normal[2] = static_cast<float>(normal.z);
                }
                if (file->Channels & _channelTangents)
                {
                    const ufbx_vec3 tangent = ufbx_get_vertex_vec3(&mesh->vertex_tangent, index);
                    const ufbx_vec3 bitangent = ufbx_get_vertex_vec3(&mesh->vertex_bitangent, index);
                    const ufbx_vec3 normal = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
                    const double cx = normal.y * tangent.z - normal.z * tangent.y;
                    const double cy = normal.z * tangent.x - normal.x * tangent.z;
                    const double cz = normal.x * tangent.y - normal.y * tangent.x;
                    vertex.Tangent[0] = static_cast<float>(tangent.x);
                    vertex.Tangent[1] = static_cast<float>(tangent.y);
                    vertex.Tangent[2] = static_cast<float>(tangent.z);
                    vertex.Tangent[3] = cx * bitangent.x + cy * bitangent.y + cz * bitangent.z < 0.0 ? -1.0f : 1.0f;
                }
                if (file->Channels & _channelUV0)
                {
                    const ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);
                    vertex.UV[0] = static_cast<float>(uv.x);
                    vertex.UV[1] = 1.0f - static_cast<float>(uv.y);
                }
                if (file->Channels & _channelColors)
                {
                    const ufbx_vec4 color = ufbx_get_vertex_vec4(&mesh->vertex_color, index);
                    vertex.Color[0] = ToByte(color.x);
                    vertex.Color[1] = ToByte(color.y);
                    vertex.Color[2] = ToByte(color.z);
                    vertex.Color[3] = ToByte(color.w);
                }
                file->Vertices.push_back(vertex);
            }
        }

        file->Indices.resize(file->Vertices.size());
        if (!file->Vertices.empty())
        {
            ufbx_vertex_stream stream = { file->Vertices.data(), file->Vertices.size(), sizeof(PartVertex) };
            ufbx_error error = {};
            const size_t unique = ufbx_generate_indices(&stream, 1, file->Indices.data(), file->Indices.size(), nullptr, &error);
            if (error.type != UFBX_ERROR_NONE)
                return false;
            file->Vertices.resize(unique);
        }

        for (int axis = 0; axis < 3; ++axis)
        {
            file->BoundsMin[axis] = file->Vertices.empty() ? 0.0f : file->Vertices[0].Position[axis];
            file->BoundsMax[axis] = file->BoundsMin[axis];
        }
        for (const PartVertex& vertex : file->Vertices)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                file->BoundsMin[axis] = (std::min)(file->BoundsMin[axis], vertex.Position[axis]);
                file->BoundsMax[axis] = (std::max)(file->BoundsMax[axis], vertex.Position[axis]);
            }
        }

        const ufbx_material* material = part.index < mesh->materials.count ? mesh->materials.data[part.index] : nullptr;
        file->MaterialName = material ? std::string(material->name.data, material->name.length) : std::string();
        file->CachedMesh = meshIndex;
        file->CachedPart = partIndex;
        return true;
    }
}

struct XG_FbxImportOptions
{
    float Scale;
    uint32_t Flags;
};

struct XG_FbxCounts
{
    int32_t Nodes;
    int32_t Meshes;
    int32_t Materials;
};

struct XG_FbxNode
{
    const char* Name;
    int32_t Parent;
    int32_t Mesh;
    float Local[16];
    float Geometry[16];
};

struct XG_FbxPartInfo
{
    const char* MeshName;
    const char* MaterialName;
    int32_t MaterialIndex;
    int32_t VertexCount;
    int32_t IndexCount;
    uint32_t Channels;
    float BoundsMin[3];
    float BoundsMax[3];
};

// Loads an FBX file. Returns null on failure, with the error text in error.
XG_MODULE_EXPORT void* XG_EditorFbxOpen(const char* path, const XG_FbxImportOptions* options, char* error, int32_t capacity)
{
    if (!path)
        return nullptr;
    const float scale = options && options->Scale > 0.0f ? options->Scale : 1.0f;
    ufbx_load_opts opts = {};
    opts.target_axes = ufbx_axes_left_handed_y_up;
    opts.target_unit_meters = 1.0f / scale;
    opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
    opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
    opts.ignore_embedded = true;
    opts.ignore_animation = true;
    opts.generate_missing_normals = true;
    opts.load_external_files = false;
    ufbx_error loadError = {};
    ufbx_scene* scene = ufbx_load_file(path, &opts, &loadError);
    if (!scene)
    {
        CopyError(loadError, error, capacity);
        return nullptr;
    }
    auto* file = new FbxFile();
    file->Scene = scene;
    return file;
}

XG_MODULE_EXPORT void XG_EditorFbxClose(void* handle)
{
    auto* file = static_cast<FbxFile*>(handle);
    if (!file)
        return;
    ufbx_free_scene(file->Scene);
    delete file;
}

XG_MODULE_EXPORT bool XG_EditorFbxGetCounts(void* handle, XG_FbxCounts* out)
{
    auto* file = static_cast<FbxFile*>(handle);
    if (!file || !out)
        return false;
    out->Nodes = static_cast<int32_t>(file->Scene->nodes.count);
    out->Meshes = static_cast<int32_t>(file->Scene->meshes.count);
    out->Materials = static_cast<int32_t>(file->Scene->materials.count);
    return true;
}

// Node i in parent-first order. The name stays valid until Close.
XG_MODULE_EXPORT bool XG_EditorFbxGetNode(void* handle, int32_t index, XG_FbxNode* out)
{
    auto* file = static_cast<FbxFile*>(handle);
    if (!file || !out || index < 0 || static_cast<size_t>(index) >= file->Scene->nodes.count)
        return false;
    const ufbx_node* node = file->Scene->nodes.data[index];
    out->Name = node->name.data;
    out->Parent = node->parent ? static_cast<int32_t>(node->parent->typed_id) : -1;
    out->Mesh = node->mesh ? static_cast<int32_t>(node->mesh->typed_id) : -1;
    ToRowMatrix(node->node_to_parent, out->Local);
    ToRowMatrix(node->geometry_to_node, out->Geometry);
    return true;
}

XG_MODULE_EXPORT int32_t XG_EditorFbxGetPartCount(void* handle, int32_t mesh)
{
    const ufbx_mesh* found = MeshAt(static_cast<FbxFile*>(handle), mesh);
    return found ? static_cast<int32_t>(found->material_parts.count) : 0;
}

// Triangulates and de-duplicates the part (kept until another part is asked for) and describes it. Names stay
// valid until the next GetPart or Close.
XG_MODULE_EXPORT bool XG_EditorFbxGetPart(void* handle, int32_t mesh, int32_t part, XG_FbxPartInfo* out)
{
    auto* file = static_cast<FbxFile*>(handle);
    if (!file || !out || !BuildPart(file, mesh, part))
        return false;
    const ufbx_mesh* found = MeshAt(file, mesh);
    out->MeshName = found->name.data;
    out->MaterialName = file->MaterialName.c_str();
    out->MaterialIndex = static_cast<int32_t>(found->material_parts.data[part].index);
    out->VertexCount = static_cast<int32_t>(file->Vertices.size());
    out->IndexCount = static_cast<int32_t>(file->Indices.size());
    out->Channels = file->Channels;
    std::memcpy(out->BoundsMin, file->BoundsMin, sizeof(out->BoundsMin));
    std::memcpy(out->BoundsMax, file->BoundsMax, sizeof(out->BoundsMax));
    return true;
}

// Copies the part's channels into the caller's buffers (any may be null): positions and normals float3, tangents
// float4 (w = handedness), uv0 float2 (V flipped to top-left origin), colors RGBA8, indices uint32.
XG_MODULE_EXPORT bool XG_EditorFbxReadPart(void* handle, int32_t mesh, int32_t part, float* positions, float* normals, float* tangents,
    float* uv0, uint8_t* colors, uint32_t* indices)
{
    auto* file = static_cast<FbxFile*>(handle);
    if (!file || !BuildPart(file, mesh, part))
        return false;
    for (size_t i = 0; i < file->Vertices.size(); ++i)
    {
        const PartVertex& vertex = file->Vertices[i];
        if (positions)
            std::memcpy(positions + i * 3, vertex.Position, sizeof(vertex.Position));
        if (normals)
            std::memcpy(normals + i * 3, vertex.Normal, sizeof(vertex.Normal));
        if (tangents)
            std::memcpy(tangents + i * 4, vertex.Tangent, sizeof(vertex.Tangent));
        if (uv0)
            std::memcpy(uv0 + i * 2, vertex.UV, sizeof(vertex.UV));
        if (colors)
            std::memcpy(colors + i * 4, vertex.Color, sizeof(vertex.Color));
    }
    if (indices && !file->Indices.empty())
        std::memcpy(indices, file->Indices.data(), file->Indices.size() * sizeof(uint32_t));
    return true;
}
