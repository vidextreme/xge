// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;

namespace xg.editor
{
    // Inspector drawing on top of xg.imui.Gui: PropertyField picks the field's drawer and adds [Header] and [Tooltip].
    public static class EditorGUI
    {
        private static readonly HashSet<string> _reported = new();

        public static void PropertyField(SerializedProperty property, string? label = null)
        {
            foreach (object attribute in property.FieldInfo.GetCustomAttributes(typeof(HeaderAttribute), true))
            {
                Gui.Space(4.0f);
                using (Gui.Font(FontStyle.Bold))
                    Gui.Label(((HeaderAttribute)attribute).Header);
            }

            string text = label ?? property.DisplayName;
            using (Gui.Id(property.PropertyPath))
            {
                try
                {
                    DrawerRegistry.GetDrawer(property).OnGUI(property, text);
                }
                catch (Exception e)
                {
                    Gui.PropertyLabel(text, $"<{e.GetType().Name}>");
                    if (_reported.Add(property.PropertyPath + e.GetType().FullName))
                        Debug.LogError($"[Inspector] Drawing {property.PropertyPath} threw (reported once): {e}");
                }
            }

            if (property.GetAttribute<TooltipAttribute>() is TooltipAttribute tooltip)
                Gui.Tooltip(tooltip.Tooltip);
        }

        // Draws every visible field of the object.
        public static void ObjectFields(SerializedObject target)
        {
            foreach (SerializedProperty property in target.Properties)
                PropertyField(property);
        }
    }
}
