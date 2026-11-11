// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace xg
{
    // Native OS open dialogs, modal to the engine's main window.
    // The callback receives the chosen path, or null when cancelled. It can run on another thread, so hand the
    // result back to the UI thread (for example through a ConcurrentQueue) before touching UI state.
    public static unsafe partial class FileDialog
    {
        private const string NativeLib = "xgScriptNative";

        public static void OpenFolder(Action<string?> callback, string? defaultLocation = null)
        {
            ArgumentNullException.ThrowIfNull(callback);
            nint user = GCHandle.ToIntPtr(GCHandle.Alloc(callback));
            XG_ShowOpenFolderDialog(ScriptRuntime._enginePtr, defaultLocation, &OnResult, user);
        }

        // pattern lists extensions separated by ';' without dots ("png;jpg"); null shows all files.
        public static void OpenFile(Action<string?> callback, string? filterName = null, string? pattern = null, string? defaultLocation = null)
        {
            ArgumentNullException.ThrowIfNull(callback);
            nint user = GCHandle.ToIntPtr(GCHandle.Alloc(callback));
            XG_ShowOpenFileDialog(ScriptRuntime._enginePtr, filterName, pattern, defaultLocation, &OnResult, user);
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        private static void OnResult(nint user, byte* path)
        {
            GCHandle handle = GCHandle.FromIntPtr(user);
            Action<string?>? callback = handle.Target as Action<string?>;
            handle.Free();
            try
            {
                callback?.Invoke(path == null ? null : Marshal.PtrToStringUTF8((nint)path));
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }

        [LibraryImport(NativeLib, EntryPoint = "XG_ShowOpenFolderDialog", StringMarshalling = StringMarshalling.Utf8)]
        private static partial void XG_ShowOpenFolderDialog(nint engine, string? defaultLocation,
            delegate* unmanaged[Cdecl]<nint, byte*, void> callback, nint user);

        [LibraryImport(NativeLib, EntryPoint = "XG_ShowOpenFileDialog", StringMarshalling = StringMarshalling.Utf8)]
        private static partial void XG_ShowOpenFileDialog(nint engine, string? filterName, string? filterPattern, string? defaultLocation,
            delegate* unmanaged[Cdecl]<nint, byte*, void> callback, nint user);
    }

    // Image files decoded by the platform layer (BMP and PNG) into tightly packed RGBA8 pixels.
    public static unsafe partial class ImageFile
    {
        private const string NativeLib = "xgScriptNative";

        public static byte[]? LoadRGBA(string path, out int width, out int height)
        {
            int w = 0, h = 0;
            byte* pixels = XG_LoadImageRGBA(path, &w, &h);
            width = w;
            height = h;
            if (pixels == null)
                return null;
            try
            {
                return new ReadOnlySpan<byte>(pixels, checked(w * h * 4)).ToArray();
            }
            finally
            {
                XG_FreeImageRGBA(pixels);
            }
        }

        [LibraryImport(NativeLib, EntryPoint = "XG_LoadImageRGBA", StringMarshalling = StringMarshalling.Utf8)]
        private static partial byte* XG_LoadImageRGBA(string path, int* width, int* height);

        [LibraryImport(NativeLib, EntryPoint = "XG_FreeImageRGBA")]
        private static partial void XG_FreeImageRGBA(byte* pixels);
    }
}
