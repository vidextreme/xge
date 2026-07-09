// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
﻿using System;
using System.Text;
using System.Text.Json;

namespace xg
{
    public static class MessageCodec
    {
        private static readonly Dictionary<string, Func<byte[], object>> _decoders =
            new();

        public static void Register(string typeName, Func<byte[], object> decoder)
            => _decoders[typeName] = decoder;

        public static object Decode(string typeName, byte[] payload)
        {
            if (_decoders.TryGetValue(typeName, out var fn))
                return fn(payload);

            // fallback: treat as JSON
            return JsonSerializer.Deserialize<object>(payload);
        }
    }

}
