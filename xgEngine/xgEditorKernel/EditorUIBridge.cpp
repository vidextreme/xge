// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "EditorUIBridge.h"
#include "imgui.h"
#include "xgRenderer.h"
#include <cstddef>
#include <vector>

static_assert(sizeof(ImDrawVert) == sizeof(xg::UIVertex), "ImDrawVert must match xg::UIVertex");
static_assert(offsetof(ImDrawVert, pos) == offsetof(xg::UIVertex, X), "ImDrawVert::pos must match xg::UIVertex::X");
static_assert(offsetof(ImDrawVert, uv) == offsetof(xg::UIVertex, U), "ImDrawVert::uv must match xg::UIVertex::U");
static_assert(offsetof(ImDrawVert, col) == offsetof(xg::UIVertex, Color), "ImDrawVert::col must match xg::UIVertex::Color");
static_assert(sizeof(ImDrawIdx) == sizeof(uint16_t), "the engine UI draw list uses 16-bit indices");

namespace xg::ui_bridge
{
    namespace
    {
        std::vector<UIVertex> s_vertices;
        std::vector<uint16_t> s_indices;
        std::vector<UIDrawCommand> s_commands;
        std::vector<uint8_t> s_scratch;

        const void* ToRGBA(ImTextureData* tex, int x, int y, int width, int height, int& pitch)
        {
            if (tex->Format == ImTextureFormat_RGBA32)
            {
                pitch = tex->GetPitch();
                return tex->GetPixelsAt(x, y);
            }

            s_scratch.resize(static_cast<size_t>(width) * height * 4);
            for (int row = 0; row < height; ++row)
            {
                const uint8_t* source = static_cast<const uint8_t*>(tex->GetPixelsAt(x, y + row));
                uint8_t* destination = s_scratch.data() + static_cast<size_t>(row) * width * 4;
                for (int column = 0; column < width; ++column)
                {
                    destination[column * 4 + 0] = 255;
                    destination[column * 4 + 1] = 255;
                    destination[column * 4 + 2] = 255;
                    destination[column * 4 + 3] = source[column];
                }
            }
            pitch = width * 4;
            return s_scratch.data();
        }

        void DestroyTexture(Renderer& renderer, ImTextureData* tex)
        {
            const uint64_t id = static_cast<uint64_t>(tex->GetTexID());
            if (id != 0)
                renderer.DestroyUITexture(id);
            tex->SetTexID(ImTextureID_Invalid);
            tex->SetStatus(ImTextureStatus_Destroyed);
        }

        void UpdateTexture(Renderer& renderer, ImTextureData* tex)
        {
            if (tex->Status == ImTextureStatus_WantCreate)
            {
                int pitch = 0;
                const void* pixels = ToRGBA(tex, 0, 0, tex->Width, tex->Height, pitch);
                if (pitch != tex->Width * 4)
                    return;
                const uint64_t id = renderer.CreateUITexture(tex->Width, tex->Height, pixels);
                if (id == 0)
                    return;
                tex->SetTexID(static_cast<ImTextureID>(id));
                tex->SetStatus(ImTextureStatus_OK);
            }
            else if (tex->Status == ImTextureStatus_WantUpdates)
            {
                const uint64_t id = static_cast<uint64_t>(tex->GetTexID());
                for (const ImTextureRect& rect : tex->Updates)
                {
                    int pitch = 0;
                    const void* pixels = ToRGBA(tex, rect.x, rect.y, rect.w, rect.h, pitch);
                    renderer.UpdateUITexture(id, rect.x, rect.y, rect.w, rect.h, pixels, pitch);
                }
                tex->SetStatus(ImTextureStatus_OK);
            }
            else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0)
            {
                DestroyTexture(renderer, tex);
            }
        }
    }

    void ProcessTextures(Renderer& renderer, ImDrawData* drawData)
    {
        if (!drawData || !drawData->Textures)
            return;
        for (ImTextureData* tex : *drawData->Textures)
            if (tex->Status != ImTextureStatus_OK)
                UpdateTexture(renderer, tex);
    }

    void SubmitDrawData(Renderer& renderer, ImDrawData* drawData)
    {
        s_vertices.clear();
        s_indices.clear();
        s_commands.clear();

        UIDrawList list = {};
        if (drawData && drawData->Valid && drawData->TotalVtxCount > 0)
        {
            s_vertices.reserve(static_cast<size_t>(drawData->TotalVtxCount));
            s_indices.reserve(static_cast<size_t>(drawData->TotalIdxCount));

            for (const ImDrawList* drawList : drawData->CmdLists)
            {
                const uint32_t vertexBase = static_cast<uint32_t>(s_vertices.size());
                const uint32_t indexBase = static_cast<uint32_t>(s_indices.size());
                const UIVertex* vertices = reinterpret_cast<const UIVertex*>(drawList->VtxBuffer.Data);
                s_vertices.insert(s_vertices.end(), vertices, vertices + drawList->VtxBuffer.Size);
                s_indices.insert(s_indices.end(), drawList->IdxBuffer.Data, drawList->IdxBuffer.Data + drawList->IdxBuffer.Size);

                for (const ImDrawCmd& cmd : drawList->CmdBuffer)
                {
                    if (cmd.UserCallback != nullptr || cmd.ElemCount == 0)
                        continue;
                    UIDrawCommand command = {};
                    command.ClipMinX = cmd.ClipRect.x;
                    command.ClipMinY = cmd.ClipRect.y;
                    command.ClipMaxX = cmd.ClipRect.z;
                    command.ClipMaxY = cmd.ClipRect.w;
                    command.TextureId = static_cast<uint64_t>(cmd.GetTexID());
                    command.IndexOffset = indexBase + cmd.IdxOffset;
                    command.VertexOffset = vertexBase + cmd.VtxOffset;
                    command.IndexCount = cmd.ElemCount;
                    s_commands.push_back(command);
                }
            }

            list.Vertices = s_vertices.data();
            list.VertexCount = static_cast<uint32_t>(s_vertices.size());
            list.Indices = s_indices.data();
            list.IndexCount = static_cast<uint32_t>(s_indices.size());
            list.Commands = s_commands.data();
            list.CommandCount = static_cast<uint32_t>(s_commands.size());
            list.DisplayX = drawData->DisplayPos.x;
            list.DisplayY = drawData->DisplayPos.y;
            list.DisplayWidth = drawData->DisplaySize.x;
            list.DisplayHeight = drawData->DisplaySize.y;
            list.ScaleX = drawData->FramebufferScale.x;
            list.ScaleY = drawData->FramebufferScale.y;
        }
        renderer.SubmitUI(&list);
    }

    void DestroyAllTextures(Renderer& renderer)
    {
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
            if (tex->RefCount == 1 && tex->Status != ImTextureStatus_Destroyed)
                DestroyTexture(renderer, tex);
    }
}
