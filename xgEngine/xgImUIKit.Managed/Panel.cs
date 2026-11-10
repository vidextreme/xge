// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.InteropServices;
using xg.math;

namespace xg.imui
{
    // Base class for managed panels (dockable, listed in the Window menu). Mirrors native xg::imui::Panel.
    //
    //   sealed class StatsPanel : Panel
    //   {
    //       public override string Title => "Stats";
    //       public override DockSlot Slot => DockSlot.Right;
    //       protected override void OnGUI() { Gui.Label("Hello"); }
    //   }
    //   Panel.Open<StatsPanel>();
    public abstract unsafe class Panel
    {
        private const int MaxReportedErrors = 3;

        private nint _handle;
        private GCHandle _self;
        private int _errorCount;

        public abstract string Title { get; }
        public virtual string? MenuPath => null;
        public virtual DockSlot Slot => DockSlot.Floating;
        public virtual Icon PanelIcon => Icon.None;
        public virtual PanelFlags Flags => PanelFlags.None;
        // Initial position and size for floating panels, relative to the main window. Null keeps the default.
        public virtual Rect? InitialRect => null;

        public bool IsRegistered => _handle != 0;
        public bool IsOpen => _handle != 0 && Native.IsPanelOpen(_handle) != 0;

        protected abstract void OnGUI();
        protected virtual void OnEnable() { }
        protected virtual void OnDisable() { }
        // Called once when the panel is unregistered (explicitly or by ImUI.ShutdownAll). Release owned resources here.
        protected virtual void OnDestroy() { }

        // Registers a new panel of type T, or returns the already registered one.
        public static T Open<T>() where T : Panel, new()
        {
            foreach (Panel existing in ImUI.Panels)
            {
                if (existing is T found)
                {
                    found.Open();
                    return found;
                }
            }
            var panel = new T();
            panel.Register();
            return panel;
        }

        public bool Register()
        {
            if (_handle != 0)
                return true;
            if (!ImUI.EnsureCompatible())
                return false;

            _self = GCHandle.Alloc(this, GCHandleType.Normal);
            NativePanelDesc desc = default;
            desc.Size = (uint)sizeof(NativePanelDesc);
            desc.Slot = Slot;
            desc.PanelIcon = PanelIcon;
            desc.Flags = Flags;
            desc.OnGUI = &GUITrampoline;
            desc.OnOpen = &OpenTrampoline;
            desc.OnClose = &CloseTrampoline;
            desc.User = GCHandle.ToIntPtr(_self);
            desc.InitialRect = InitialRect ?? default;

            ImUI.Panels.Add(this);
            fixed (byte* title = Utf8.Encode(Title.AsSpan(), 0))
            fixed (byte* menu = Utf8.Encode(MenuPath, 1))
            {
                desc.Title = title;
                desc.MenuPath = menu;
                _handle = Native.RegisterPanel(&desc);
            }

            if (_handle == 0)
            {
                ImUI.Panels.Remove(this);
                _self.Free();
                Debug.LogError($"Panel '{Title}' could not be registered.");
                return false;
            }
            return true;
        }

        public void Unregister()
        {
            if (_handle == 0)
                return;
            Native.UnregisterPanel(_handle);
            _handle = 0;
            ImUI.Panels.Remove(this);
            if (_self.IsAllocated)
                _self.Free();
            try
            {
                OnDestroy();
            }
            catch (Exception e)
            {
                ReportError(e);
            }
        }

        public void Open() { if (_handle != 0) Native.OpenPanel(_handle); }
        public void Close() { if (_handle != 0) Native.ClosePanel(_handle); }
        public void Focus() { if (_handle != 0) Native.FocusPanel(_handle); }
        public void Repaint() { if (_handle != 0) Native.RequestRepaint(_handle); }

        private static Panel? FromUser(nint user) =>
            user == 0 ? null : GCHandle.FromIntPtr(user).Target as Panel;

        [UnmanagedCallersOnly]
        private static void GUITrampoline(nint ctx, nint user)
        {
            Panel? window = FromUser(user);
            if (window is null)
                return;
            nint previous = Gui.Enter(ctx);
            try
            {
                window.OnGUI();
            }
            catch (Exception e)
            {
                window.ReportError(e);
            }
            finally
            {
                Gui.Leave(previous);
            }
        }

        [UnmanagedCallersOnly]
        private static void OpenTrampoline(nint user)
        {
            Panel? window = FromUser(user);
            try
            {
                window?.OnEnable();
            }
            catch (Exception e)
            {
                window?.ReportError(e);
            }
        }

        [UnmanagedCallersOnly]
        private static void CloseTrampoline(nint user)
        {
            Panel? window = FromUser(user);
            try
            {
                window?.OnDisable();
            }
            catch (Exception e)
            {
                window?.ReportError(e);
            }
        }

        private void ReportError(Exception e)
        {
            ++_errorCount;
            if (_errorCount <= MaxReportedErrors)
                Debug.LogException(e);
            if (_errorCount == MaxReportedErrors)
                Debug.LogWarning($"Panel '{Title}': further exceptions are suppressed.");
        }
    }
}
