// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;

namespace xg.editor
{
    // Draws one data struct of a data component in place of the default field list (the Camera card). Gets the boxed
    // element and returns the element to store (the same box when nothing changed).
    public interface IComponentDataEditor
    {
        // Full name of the data struct it draws ("xg.CameraData").
        string DataType { get; }
        object OnGUI(ActorID actor, object data);
    }

    // An extra card under a component's card in the Inspector (the camera preview).
    public interface IComponentPreview
    {
        // Full name of the component type it previews ("xg.Camera").
        string ComponentType { get; }
        string Title { get; }
        Icon Icon => Icon.Eye;
        void OnGUI(ActorID actor, object component);
        // The Inspector stopped showing it (free render views and the like).
        void Release() { }
    }

    // The registered data editors and previews, matched by type full name so copies of a type in other load contexts
    // find them too. The editor registers its own at startup; editor scripts can add more.
    public static class ComponentEditors
    {
        private static readonly Dictionary<string, IComponentDataEditor> _data = new(StringComparer.Ordinal);
        private static readonly Dictionary<string, IComponentPreview> _previews = new(StringComparer.Ordinal);

        public static void Register(IComponentDataEditor editor) => _data[editor.DataType] = editor;

        public static void Register(IComponentPreview preview) => _previews[preview.ComponentType] = preview;

        public static IComponentDataEditor? DataEditor(Type dataType) =>
            dataType.FullName is string name && _data.TryGetValue(name, out IComponentDataEditor? editor) ? editor : null;

        public static IComponentPreview? Preview(Type componentType) =>
            componentType.FullName is string name && _previews.TryGetValue(name, out IComponentPreview? preview) ? preview : null;
    }
}
