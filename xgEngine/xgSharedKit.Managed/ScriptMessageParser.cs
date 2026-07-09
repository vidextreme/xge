// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
﻿using System;
using System.Runtime.InteropServices;
using System.Text;
using xg;

namespace xg
{
    public unsafe class ScriptMessageParser
    {
        private byte* _ptr;
        private byte* _end;

        public ScriptMessageParser(ScriptMessageInterop msg)
        {
            _ptr = msg.Payload;
            _end = msg.Payload + msg.PayloadSize;
        }

        // Reads a primitive directly out of the binary stream (Zero-Allocation)
        public T ReadPrimitive<T>() where T : unmanaged
        {
            int size = sizeof(T);
            if (_ptr + size > _end)
                throw new IndexOutOfRangeException("ScriptMessage buffer underflow!");

            T value = *(T*)_ptr;
            _ptr += size; // Advance stream pointer
            return value;
        }

        // Reads variable length strings matching your xgCodecBinary.cpp logic
        public string ReadString()
        {
            // 1. Read the 4-byte length prefix
            uint len = ReadPrimitive<uint>();
            if (len == 0) return string.Empty;

            if (_ptr + len > _end)
                throw new IndexOutOfRangeException("ScriptMessage string parsing overflow!");

            // 2. Extract string without copying if possible, or convert to C# string
            string result = Encoding.UTF8.GetString(_ptr, (int)len);
            _ptr += len;
            return result;
        }
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Route
    {
        public RouteType Type;

        // Marshaled as UTF‑8 const char*
        [MarshalAs(UnmanagedType.LPUTF8Str)]
        public string Id;

        public int DepthLimit;

        // Always null for managed → native
        public IntPtr Predicate;
    }

}
