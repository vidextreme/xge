// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // Event interfaces a tool can implement. They're delivered to active tools only, in activation order.

    // Immediate GUI drawn in the Tools ribbon while the tool is active (placement: [ToolGUI]).
    public interface IEditorGUIDraw
    {
        void OnGUI();
    }

    // Overlay drawing over a scene view (gizmos and handles). Declared now; the scene view is wired later.
    public interface IEditorSceneDraw
    {
        void OnSceneGUI(SceneViewContext view);
    }

    public interface IPointerDown { void OnPointerDown(PointerEventData data); }
    public interface IPointerUp { void OnPointerUp(PointerEventData data); }
    public interface IPointerClick { void OnPointerClick(PointerEventData data); }
    public interface IPointerMove { void OnPointerMove(PointerEventData data); }
    public interface IBeginDrag { void OnBeginDrag(PointerEventData data); }
    public interface IDrag { void OnDrag(PointerEventData data); }
    public interface IEndDrag { void OnEndDrag(PointerEventData data); }
    public interface IScroll { void OnScroll(PointerEventData data); }

    // Raised when the selection contains one asset or several.
    public interface ISelectAssetItem
    {
        void OnSelectAssetItem(AssetRef item);
    }

    public interface ISelectAssetItems
    {
        void OnSelectAssetItems(IReadOnlyList<AssetRef> items);
    }
}
