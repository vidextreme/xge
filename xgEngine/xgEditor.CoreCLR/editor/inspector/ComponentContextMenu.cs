// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The Inspector's component menu (right-click a component card header or its tab): Remove Component, after a Yes/No
    // confirmation. Targets are the actor and the live component.
    public sealed class ComponentContextMenu : IContextMenuTool
    {
        public const string Source = "inspector.component";

        public string ContextMenuGroup => "Component";

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;

        public ToolUsability OnContextMenu(ContextMenuContext context) =>
            context.Source == Source && context.Targets.Length >= 2 && context.Targets[0] is ActorID ? ToolUsability.Usable : ToolUsability.NotApplicable;

        public void OnContextMenuGUI(ContextMenu menu, ContextMenuContext context)
        {
            ActorID actor = (ActorID)context.Targets[0];
            object component = context.Targets[1];
            bool composed = ArchetypeDriver.IsCompositionActor(actor);
            menu.Item("Remove Component", Icon.Trash, () => ConfirmRemove(actor, component), enabled: !composed,
                tooltip: composed ? "Composition Actors keep their object's components; edit the object (.xco)" : null);
        }

        public static void ConfirmRemove(ActorID actor, object component)
        {
            Type type = TypeBridge.ToEditor(component.GetType());
            string name = SerializedProperty.Nicify(type.Name);
            string actorName = ActorTree.GetName(actor);
            ComponentIcon icon = ComponentIcons.Get(type);
            ConfirmDialog.Open(new ConfirmRequest
            {
                Title = "Remove Component",
                Message = $"Remove {name} from {actorName}?",
                Detail = PlayMode.IsActive ? "Play mode puts it back at Stop." : "Its field values are lost.",
                DrawPreview = (draw, rect) => icon.Draw(draw, rect.Center, 26.0f),
                Confirm = () =>
                {
                    bool removed = ComponentsDriver.Remove(component);
                    if (removed)
                        Debug.Log($"[Debug] Inspector removed {name} from {actorName}");
                    else
                        Debug.LogWarning($"[Inspector] Could not remove {name} from {actorName}");
                },
            });
        }
    }
}
