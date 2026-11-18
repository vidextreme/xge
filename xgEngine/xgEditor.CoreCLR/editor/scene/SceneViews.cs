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
    }
}
