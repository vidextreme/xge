// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.imui
{
    // RGBA8 texture usable with Painter.Image. Dispose it (or let ImUI.ShutdownAll) to free the GPU copy.
    public sealed unsafe class Texture : IDisposable
    {
        internal nint Handle { get; private set; }

        public int Width { get; }
        public int Height { get; }

        public Texture(ReadOnlySpan<byte> rgba, int width, int height)
        {
            if (width <= 0 || height <= 0 || rgba.Length < width * height * 4)
                throw new ArgumentException("rgba must hold width * height * 4 bytes.", nameof(rgba));
            Width = width;
            Height = height;
            fixed (byte* p = rgba)
                Handle = Native.CreateTexture(p, width, height);
            if (Handle == 0)
                throw new InvalidOperationException("XG_ImUICreateTexture failed (immediate UI not initialized?).");
            ImUI.Track(this);
        }

        public bool IsValid => Handle != 0;

        public void Dispose()
        {
            if (Handle == 0)
                return;
            Native.DestroyTexture(Handle);
            Handle = 0;
            ImUI.Untrack(this);
        }
    }
}
