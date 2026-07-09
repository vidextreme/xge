// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
﻿using System;
using System.Runtime.InteropServices;

namespace xg
{    
    [StructLayout(LayoutKind.Sequential)]
    public unsafe struct ScriptMessageInterop
    {
        [MarshalAs(UnmanagedType.LPUTF8Str)]
        public string TypeName;      
        public int PayloadSize;      // Maps to native 'int' / 'uint32_t'
        public byte* Payload;        // Maps to native 'void*' / 'uint8_t*'
        public uint SourceID;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ScriptModuleContext
    {
        public uint InstanceId;
        public IntPtr NativeObjectPtr;
    }
}
