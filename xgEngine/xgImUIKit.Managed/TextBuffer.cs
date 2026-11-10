// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text;

namespace xg.imui
{
    // Fixed-capacity, null-terminated UTF-8 buffer edited in place by native text widgets (Gui.SearchField).
    public sealed class TextBuffer
    {
        private string? _text;

        public TextBuffer(int capacity = 256, string? text = null)
        {
            Buffer = new byte[Math.Max(capacity, 2)];
            if (!string.IsNullOrEmpty(text))
                Text = text;
            else
                _text = string.Empty;
        }

        internal byte[] Buffer { get; }

        public int Capacity => Buffer.Length;

        public string Text
        {
            get
            {
                if (_text is null)
                {
                    int length = Array.IndexOf(Buffer, (byte)0);
                    _text = Encoding.UTF8.GetString(Buffer, 0, length < 0 ? Buffer.Length : length);
                }
                return _text;
            }
            set
            {
                Array.Clear(Buffer);
                string source = value ?? string.Empty;
                int written = 0;
                Encoder encoder = Encoding.UTF8.GetEncoder();
                encoder.Convert(source.AsSpan(), Buffer.AsSpan(0, Buffer.Length - 1), true, out _, out written, out _);
                _text = Encoding.UTF8.GetString(Buffer, 0, written);
            }
        }

        public bool IsEmpty => Buffer[0] == 0;

        internal void MarkEdited() => _text = null;

        public override string ToString() => Text;
    }
}
