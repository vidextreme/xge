// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Editor-only bindings to xgEditorKernel's render view exports (EditorSceneViewExports.h).
    internal static unsafe partial class SceneViews
    {
        private const string DLL = "xgEditorKernel";

        // Mirrors xg::RenderViewCamera (xgCore/public/xgRenderer.h).
        [StructLayout(LayoutKind.Explicit, Size = 80)]
        public struct Camera
        {
            [FieldOffset(0)] public Vector3 Eye;
            [FieldOffset(16)] public Vector3 Target;
            [FieldOffset(32)] public Vector3 Up;
            [FieldOffset(48)] public float FovY;
            [FieldOffset(52)] public float NearZ;
            [FieldOffset(56)] public float FarZ;
            [FieldOffset(64)] public Color ClearColor;
        }

        [LibraryImport(DLL, EntryPoint = "XG_EditorHasSceneRenderer")]
        public static partial int HasRenderer();

        [LibraryImport(DLL, EntryPoint = "XG_EditorCreateSceneView")]
        public static partial ulong Create(int width, int height);

        [LibraryImport(DLL, EntryPoint = "XG_EditorResizeSceneView")]
        public static partial int Resize(ulong id, int width, int height);

        [LibraryImport(DLL, EntryPoint = "XG_EditorDestroySceneView")]
        public static partial void Destroy(ulong id);

        [LibraryImport(DLL, EntryPoint = "XG_EditorSetSceneViewCamera")]
        public static partial void SetCamera(ulong id, Camera* camera);

        // Mirrors xg::RenderViewDraw: a GPU mesh drawn lit with a texture (0 = white) at a world matrix (row-major, row vectors).
        [StructLayout(LayoutKind.Sequential)]
        public struct Draw
        {
            public ulong Mesh;
            public ulong Texture;
            public fixed float World[16];
        }

        [LibraryImport(DLL, EntryPoint = "XG_EditorSetSceneViewDraws")]
        public static partial void SetDraws(ulong id, Draw* draws, uint count);

        // Mirrors xg::SceneDraw: a GPU mesh drawn with a renderer material (0 = the built-in look).
        [StructLayout(LayoutKind.Sequential)]
        public struct MaterialDraw
        {
            public ulong Mesh;
            public ulong Material;
            public fixed float World[16];
        }

        // Mirrors xg::MeshDesc: Stride 52 (xg::shaders::SceneLitVertex), Format 0 = 16-bit indices, 1 = 32-bit.
        [StructLayout(LayoutKind.Sequential)]
        public struct MeshDesc
        {
            public int VertexCount;
            public int Stride;
            public void* Vertices;
            public int IndexCount;
            public uint Format;
            public void* Indices;
        }

        // Mirrors xg::MaterialDesc.
        [StructLayout(LayoutKind.Sequential)]
        public struct MaterialDesc
        {
            public ulong Shader;
            public fixed ulong Textures[4];
            public fixed float Params[16];
        }

        public const uint FlagHideTestScene = 1;
        public const uint FlagHideSceneDraws = 2;

        [LibraryImport(DLL, EntryPoint = "XG_EditorSetSceneViewMaterialDraws")]
        public static partial void SetMaterialDraws(ulong id, MaterialDraw* draws, uint count);

        [LibraryImport(DLL, EntryPoint = "XG_EditorSetSceneViewFlags")]
        public static partial void SetFlags(ulong id, uint flags);

        [LibraryImport(DLL, EntryPoint = "XG_EditorCreateMesh")]
        public static partial ulong CreateMesh(MeshDesc* desc);

        [LibraryImport(DLL, EntryPoint = "XG_EditorDestroyMesh")]
        public static partial void DestroyMesh(ulong id);

        [LibraryImport(DLL, EntryPoint = "XG_EditorCreateMaterial")]
        public static partial ulong CreateMaterial(MaterialDesc* desc);

        [LibraryImport(DLL, EntryPoint = "XG_EditorUpdateMaterial")]
        public static partial int UpdateMaterial(ulong id, MaterialDesc* desc);

        [LibraryImport(DLL, EntryPoint = "XG_EditorDestroyMaterial")]
        public static partial void DestroyMaterial(ulong id);

        // mode: 0 color, 1 tangent-space normal (BC5), 2 luminance (BC4). textureId 0 = untextured.
        [LibraryImport(DLL, EntryPoint = "XG_EditorSetSceneViewTexture")]
        public static partial void SetTexture(ulong id, ulong textureId, int mode);
    }
}
