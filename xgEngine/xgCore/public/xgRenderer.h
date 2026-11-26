// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgHandles.h"
#include "xgModules.h"
#include "xgUIDraw.h"
#include "xgVec3.h"

namespace xg {

    class Window;
    class Logger;

    // Camera for an offscreen render view. Y is up; FovY is in radians.
    // Crosses into managed code (xgEditorKernel scene view exports): 80 bytes, Vec3 and Color are 16-byte aligned.
    struct RenderViewCamera
    {
        Vec3 Eye;
        Vec3 Target;
        Vec3 Up;
        float FovY;
        float NearZ;
        float FarZ;
        Color ClearColor;
    };
    static_assert(sizeof(RenderViewCamera) == 80, "RenderViewCamera layout is mirrored in managed code");

    // GPU texture formats. The values match the editor's TextureHardwareFormat and a .xdt TXPX chunk's Extra.
    enum class TextureFormat : uint32_t
    {
        RGBA8 = 1,
        BC1 = 2,
        BC3 = 3,
        BC4 = 4,
        BC5 = 5,
    };

    enum class TextureFlags : uint32_t
    {
        None = 0,
        // Color data stored in sRGB: sampled through an sRGB view, so shaders read linear values.
        SRGB = 1u << 0,
    };

    struct TextureDesc
    {
        int32_t Width;
        int32_t Height;
        int32_t MipCount;
        TextureFormat Format;
        TextureFlags Flags;
    };

    // One mip level, tightly packed rows (RowPitch = bytes per row of pixels or of 4x4 blocks).
    struct TextureMip
    {
        const void* Data;
        uint64_t Size;
        uint32_t RowPitch;
    };

    // The data of a Ready "texture" resource (ResourceSystem::GetData), loaded from a .xdt by breadcrumb
    // <file-guid>/<texture item guid>. Usage is the editor's TextureUsage (0 color, 1 normal, 2 luminance).
    struct TextureResourceInfo
    {
        uint64_t TextureId;
        int32_t Width;
        int32_t Height;
        int32_t MipCount;
        TextureFormat Format;
        uint32_t Usage;
        TextureFlags Flags;
    };

    inline constexpr const char* TextureResourceType = "texture";

    enum class IndexFormat : uint32_t
    {
        UInt16 = 0,
        UInt32 = 1,
    };

    // Static triangle-list geometry. Vertices use the scene.lit layout (shaders::SceneLitVertex, Stride 52).
    struct MeshDesc
    {
        int32_t VertexCount;
        int32_t Stride;
        const void* Vertices;
        int32_t IndexCount;
        IndexFormat Format;
        const void* Indices;
    };

    // One mesh drawn in a render view: world is row-major for row vectors (v * M); texture 0 = white.
    struct RenderViewDraw
    {
        uint64_t Mesh;
        uint64_t Texture;
        float World[16];
    };

    // One drawn part of a Ready "model" resource: a GPU mesh and where it sits (node world times geometry offset).
    struct ModelPartInfo
    {
        uint64_t Mesh;
        int32_t MaterialIndex;
        int32_t TriangleCount;
        float World[16];
    };

    // The data of a Ready "model" resource (ResourceSystem::GetData), loaded from <guid>.xdv and <guid>.xdh by the
    // asset GUID alone. Bounds are in model space, over every drawn part. Parts stays valid while the resource lives.
    struct ModelResourceInfo
    {
        int32_t PartCount;
        int32_t NodeCount;
        float BoundsMin[3];
        float BoundsMax[3];
        const ModelPartInfo* Parts;
    };

    inline constexpr const char* ModelResourceType = "model";

    // How a render view's test cube reads its texture.
    enum class RenderViewTextureMode : int32_t
    {
        Color = 0,
        // Tangent-space normal in RG (BC5): Z is rebuilt and the normal is shown as a color.
        Normal = 1,
        // One channel in R (BC4), shown as gray.
        Luminance = 2,
    };

    // Bytecode a renderer consumes: DX12 takes DXIL, Vulkan SPIR-V.
    enum class ShaderFormat : uint32_t
    {
        None = 0,
        DXIL = 1,
        SPIRV = 2,
    };

    // A surface shader (surface.common.hlsli contract): vertex and pixel bytecode in the renderer's ShaderFormat.
    struct ShaderDesc
    {
        const void* VS;
        uint64_t VSSize;
        const void* PS;
        uint64_t PSSize;
    };

    inline constexpr int32_t MaterialTextureSlots = 4;
    inline constexpr int32_t MaterialParamFloats = 16;
    inline constexpr int32_t MaterialParamSlots = MaterialParamFloats / 4;

    // A shader with its XGMaterial values (Params = float4 Params[4]) and textures t0..t3 (0 = white).
    struct MaterialDesc
    {
        uint64_t Shader;
        uint64_t Textures[MaterialTextureSlots];
        float Params[MaterialParamFloats];
    };

    enum class SceneDrawFlags : uint32_t
    {
        None = 0,
        // Hidden in the editor: skipped by views with RenderViewFlags::HideEditorHidden (the Scene view).
        EditorHidden = 1u << 0,
    };

    // One mesh drawn with a material (0 or a destroyed material = the built-in lit look, white). World as RenderViewDraw.
    struct SceneDraw
    {
        uint64_t Mesh;
        uint64_t Material;
        float World[16];
        SceneDrawFlags Flags;
        uint32_t Reserved;
    };

    enum class RenderViewFlags : uint32_t
    {
        None = 0,
        // No test cube and grid (material previews).
        HideTestScene = 1u << 0,
        // The global scene draws (SetSceneDraws) aren't drawn in this view.
        HideSceneDraws = 1u << 1,
        // Scene draws flagged SceneDrawFlags::EditorHidden aren't drawn in this view.
        HideEditorHidden = 1u << 2,
    };

    inline constexpr bool HasFlag(RenderViewFlags flags, RenderViewFlags flag)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
    }

    // A //@param of a surface shader: Type is the editor's ShaderParamType (1 float .. 4 float4, 5 color); Slot indexes
    // XGMaterial.Params.
    struct ShaderParamInfo
    {
        char Name[32];
        uint32_t Type;
        int32_t Slot;
        float Default[4];
    };
    static_assert(sizeof(ShaderParamInfo) == 56, "ShaderParamInfo matches the .xds SHPR record");

    // A //@texture of a surface shader: Usage 0 color, 1 normal, 2 luminance; Slot is the t register.
    struct ShaderTextureInfo
    {
        char Name[32];
        uint32_t Usage;
        int32_t Slot;
    };
    static_assert(sizeof(ShaderTextureInfo) == 40, "ShaderTextureInfo matches the .xds SHTX record");

    // The data of a Ready "shader" resource, loaded from <guid>.xds by the asset GUID alone.
    struct ShaderResourceInfo
    {
        uint64_t ShaderId;
        int32_t ParamCount;
        int32_t TextureCount;
        ShaderParamInfo Params[MaterialParamSlots];
        ShaderTextureInfo Textures[MaterialTextureSlots];
    };

    inline constexpr const char* ShaderResourceType = "shader";

    // The data of a Ready "material" resource, loaded from <guid>.xdm by the asset GUID alone. Its renderer material
    // is created once the shader and textures it names are loaded (MaterialId stays 0 until then).
    struct MaterialResourceInfo
    {
        uint64_t MaterialId;
        uint64_t ShaderHi;
        uint64_t ShaderLo;
        int32_t ParamCount;
        int32_t TextureCount;
    };

    inline constexpr const char* MaterialResourceType = "material";

    class Renderer : public UIRenderTarget {
    public:
        virtual void Render() = 0;
        virtual void* GetNativeHandle() const = 0;

        // Offscreen 3D views. The returned id is also a valid UI texture id (draw it in a panel).
        virtual uint64_t CreateRenderView(int32_t width, int32_t height) = 0;
        virtual bool ResizeRenderView(uint64_t id, int32_t width, int32_t height) = 0;
        virtual void DestroyRenderView(uint64_t id) = 0;
        virtual void SetRenderViewCamera(uint64_t id, const RenderViewCamera* camera) = 0;

        // Sampled textures with mips. The id is also a valid UI texture id; destroy it with DestroyTexture.
        // Main thread only. Returns 0 when the format isn't supported or creation fails.
        virtual bool SupportsTextureFormat(TextureFormat format) const = 0;
        virtual uint64_t CreateTexture(const TextureDesc* desc, const TextureMip* mips) = 0;
        virtual void DestroyTexture(uint64_t id) = 0;

        // Texture shown on the view's test cube (0 = untextured).
        virtual void SetRenderViewTexture(uint64_t viewId, uint64_t textureId, RenderViewTextureMode mode) = 0;

        // GPU meshes (main thread). Returns 0 for an invalid description or when creation fails.
        virtual uint64_t CreateMesh(const MeshDesc* desc) = 0;
        virtual void DestroyMesh(uint64_t id) = 0;

        // Meshes the view draws after its test cube, lit (copied; count 0 clears). Destroyed meshes are skipped.
        virtual void SetRenderViewDraws(uint64_t viewId, const RenderViewDraw* draws, uint32_t count) = 0;

        // Surface shaders and materials (main thread). Create returns 0 on failure. UpdateMaterial changes values and
        // textures in place (the shader too); a material whose shader was destroyed draws with the built-in look.
        virtual ShaderFormat GetShaderFormat() const = 0;
        virtual uint64_t CreateShader(const ShaderDesc* desc) = 0;
        virtual void DestroyShader(uint64_t id) = 0;
        virtual uint64_t CreateMaterial(const MaterialDesc* desc) = 0;
        virtual bool UpdateMaterial(uint64_t id, const MaterialDesc* desc) = 0;
        virtual void DestroyMaterial(uint64_t id) = 0;

        // The scene's draws, drawn in every render view after its own draws (copied; count 0 clears).
        virtual void SetSceneDraws(const SceneDraw* draws, uint32_t count) = 0;
        // Material draws of one view, drawn after its RenderViewDraws (copied; count 0 clears).
        virtual void SetRenderViewMaterialDraws(uint64_t viewId, const SceneDraw* draws, uint32_t count) = 0;
        virtual void SetRenderViewFlags(uint64_t viewId, RenderViewFlags flags) = 0;

        // Copies the view's last rendered color into tightly packed RGBA8 (the sRGB bytes the UI shows; alpha as
        // cleared and drawn). capacity must be at least width * height * 4. Runs its own copy and waits for the GPU, so
        // call it between frames (main thread). Returns false for an unknown view or a short buffer.
        virtual bool ReadRenderView(uint64_t viewId, void* rgba, uint64_t capacity) = 0;
    };

    XG_DECLARE_MODULE_FUNCTION(XG_CreateRenderer, Renderer*, Window&);
    XG_DECLARE_MODULE_FUNCTION(XG_DestroyRenderer, void, Renderer*);
    // Optional: renderer DLLs link their own xgBase, so the engine hands them its logger before XG_CreateRenderer.
    XG_DECLARE_MODULE_FUNCTION(XG_RendererSetLogger, void, Logger*);

	XG_DECLARE_UNIQUE(Renderer)

    XG_API RendererUnique CreateRenderer(Window& window);
    XG_API void      DestroyRenderer(Renderer* renderer);

}

XG_DECLARE_MODULE(xg, Renderer)
