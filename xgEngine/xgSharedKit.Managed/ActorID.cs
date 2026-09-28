// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;

public readonly struct ActorID
{
    public readonly xgID Value;

    public ActorID(xgID value)
    {
        Value = value;
    }


    // Implicit conversion FROM ulong → xgID
    public static implicit operator ActorID(ulong value)
        => new ActorID(value);

    // Implicit conversion FROM xgID → ulong
    public static implicit operator ulong(ActorID id)
        => id.Value;

    // Implicit conversion FROM ulong → xgID
    public static implicit operator ActorID(xgID value)
        => new ActorID(value);

    // Implicit conversion FROM xgID → ulong
    public static implicit operator xgID(ActorID id)
        => id.Value;

    public override string ToString() => Value.ToString();
}
