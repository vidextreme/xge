// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using XFG;
using EditorActorEvent = XFG.Core.EventX<xg.editor.EditorEventActor, xg.ActorID>;

namespace xg.editor
{
    public enum EditorEventActor
    {
        OnSelectActor,
        OnDeselectActor,
    }

    // Editor-wide actor events (selection), raised by the Hierarchy. They reach handlers in the editor module and in a
    // project's editor scripts (same load context), not game runtime scripts.
    public static partial class Editor
    {
        public static Core.Subscription<EditorEventActor, Action<ActorID>> SubscribeEvent(EditorEventActor eventId, Action<ActorID> handler) =>
            EditorActorEvent.Subscribe(eventId, handler);

        public static bool UnsubscribeEvent(EditorEventActor eventId, Action<ActorID> handler) =>
            EditorActorEvent.Unsubscribe(eventId, handler);

        public static void BroadcastEvent(EditorEventActor eventId, ActorID actor) =>
            EditorActorEvent.Broadcast(eventId, actor);

        public static void ClearAllEvents() => EditorActorEvent.ClearAll();
    }
}
