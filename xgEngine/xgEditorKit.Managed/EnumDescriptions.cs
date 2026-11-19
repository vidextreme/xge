// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.ComponentModel;
using System.Reflection;

namespace xg.editor
{
    // Display labels of an enum's members for combos and option buttons: each member's [Description], or its name when
    // it has none. Values and Labels are in value order and cached per type.
    public static class EnumDescriptions<T> where T : struct, Enum
    {
        public static readonly T[] Values = Enum.GetValues<T>();

        public static readonly string[] Labels = Values.Select(Label).ToArray();

        public static int IndexOf(T value) => Math.Max(Array.IndexOf(Values, value), 0);

        public static string Of(T value) => Labels[IndexOf(value)];

        private static string Label(T value)
        {
            string name = value.ToString();
            return typeof(T).GetField(name)?.GetCustomAttribute<DescriptionAttribute>()?.Description ?? name;
        }
    }
}
