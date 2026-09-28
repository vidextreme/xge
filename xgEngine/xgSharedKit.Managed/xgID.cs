// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    public readonly struct xgID
    {
        public readonly ulong Value;

        public xgID(ulong value)
        {
            Value = value;
        }

        // Implicit conversion FROM ulong → xgID
        public static implicit operator xgID(ulong value)
            => new xgID(value);

        // Implicit conversion FROM xgID → ulong
        public static implicit operator ulong(xgID id)
            => id.Value;
        public override string ToString() => Value.ToString();
    }
}
