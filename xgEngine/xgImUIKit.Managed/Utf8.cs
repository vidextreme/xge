// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text;

namespace xg.imui
{
    // Per-thread, null-terminated UTF-8 scratch buffers for passing text to native calls without per-call allocations.
    // Each slot is valid until the next Encode on the same slot; use different slots for arguments of one call.
    internal static class Utf8
    {
        private const int SlotCount = 4;

        [ThreadStatic] private static byte[]?[]? _slots;

        public static byte[]? Encode(string? text, int slot = 0) => text is null ? null : Encode(text.AsSpan(), slot);

        public static byte[] Encode(ReadOnlySpan<char> text, int slot = 0)
        {
            byte[] buffer = Rent(Encoding.UTF8.GetMaxByteCount(text.Length) + 1, slot);
            int written = Encoding.UTF8.GetBytes(text, buffer);
            buffer[written] = 0;
            return buffer;
        }

        public static byte[] Encode(ReadOnlySpan<byte> utf8, int slot = 0)
        {
            byte[] buffer = Rent(utf8.Length + 1, slot);
            utf8.CopyTo(buffer);
            buffer[utf8.Length] = 0;
            return buffer;
        }

        private static byte[] Rent(int size, int slot)
        {
            _slots ??= new byte[]?[SlotCount];
            byte[]? buffer = _slots[slot];
            if (buffer is null || buffer.Length < size)
            {
                buffer = new byte[Math.Max(size, buffer is null ? 256 : buffer.Length * 2)];
                _slots[slot] = buffer;
            }
            return buffer;
        }
    }
}
