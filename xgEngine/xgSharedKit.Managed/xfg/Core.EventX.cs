// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

#nullable enable

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading;

namespace XFG
{
    public static partial class Core
    {
        /// <summary>
        /// Settings and lifetime shared by every <c>EventX&lt;TKey, ...&gt;</c> bus.
        /// </summary>
        /// <remarks>
        /// <para>
        /// <c>EventX</c> is a static, strongly typed event bus keyed by any equatable key (enum, string, struct) with
        /// handlers of zero to four parameters. Guarantees:
        /// </para>
        /// <list type="bullet">
        /// <item><description>Subscribe, Unsubscribe and Clear are thread-safe; no registration is ever lost.</description></item>
        /// <item><description>Broadcast takes no lock and allocates nothing: it iterates an immutable snapshot of the handlers.</description></item>
        /// <item><description>Changes made during a broadcast (including by a handler) apply from the next broadcast.</description></item>
        /// <item><description>A handler that throws is logged (see <see cref="ThrowOnHandlerException"/>), and the remaining handlers still run.</description></item>
        /// <item><description>The same handler may be subscribed more than once; each Unsubscribe removes the latest registration.</description></item>
        /// </list>
        /// <para>
        /// Scope: the buses are static, so handlers live until they are unsubscribed or cleared, and are shared by all
        /// code in the app domain. When Unity domain reload is disabled, static state survives between play sessions;
        /// call <see cref="ClearAllSystems"/> to drop stale handlers.
        /// </para>
        /// </remarks>
        public static class EventX
        {
            private static readonly List<IEventSystem> _systems = new();

            /// <summary>
            /// When true, an exception thrown by a handler propagates to the broadcaster instead of being reported.
            /// </summary>
            public static bool ThrowOnHandlerException { get; set; }

            /// <summary>
            /// Removes every handler from every EventX bus of this module.
            /// </summary>
            public static void ClearAllSystems()
            {
                IEventSystem[] systems;
                lock (_systems)
                    systems = _systems.ToArray();
                foreach (IEventSystem system in systems)
                    system.ClearAll();
            }

            internal static void Register(IEventSystem system)
            {
                lock (_systems)
                    _systems.Add(system);
            }

            internal static void Report(Exception exception)
            {
#if UNITY_5_3_OR_NEWER
                UnityEngine.Debug.LogException(exception);
#elif XGE
                xg.Debug.LogException(exception);
#elif GODOT
                Godot.GD.PushError(exception.ToString());
#else
                Core.Error(exception.ToString());
#endif
            }
        }

        internal interface IEventSystem
        {
            void ClearAll();
        }

        /// <summary>
        /// One subscription to an EventX bus. Dispose it once to unsubscribe; the default value does nothing.
        /// </summary>
        public readonly struct Subscription<TKey, TDelegate> : IDisposable where TKey : notnull where TDelegate : Delegate
        {
            private readonly EventSystem<TKey, TDelegate>? _system;
            private readonly TKey _key;
            private readonly TDelegate? _handler;

            internal Subscription(EventSystem<TKey, TDelegate> system, TKey key, TDelegate handler)
            {
                _system = system;
                _key = key;
                _handler = handler;
            }

            /// <summary>
            /// The subscribed key.
            /// </summary>
            public TKey Key => _key;

            /// <summary>
            /// Unsubscribes the handler (one registration).
            /// </summary>
            public void Dispose()
            {
                if (_system is not null && _handler is not null)
                    _system.Unsubscribe(_key, _handler);
            }
        }

        /// <summary>
        /// Copy-on-write handler array: writers lock and publish a new array, readers take the current one.
        /// </summary>
        internal sealed class HandlerList<TDelegate> where TDelegate : Delegate
        {
            private readonly object _gate = new();
            private TDelegate[] _handlers = Array.Empty<TDelegate>();

            public TDelegate[] Snapshot => Volatile.Read(ref _handlers);

            public int Count => Snapshot.Length;

            public void Add(TDelegate handler)
            {
                lock (_gate)
                {
                    TDelegate[] current = _handlers;
                    TDelegate[] next = new TDelegate[current.Length + 1];
                    Array.Copy(current, next, current.Length);
                    next[current.Length] = handler;
                    Volatile.Write(ref _handlers, next);
                }
            }

            public bool Remove(TDelegate handler)
            {
                lock (_gate)
                {
                    TDelegate[] current = _handlers;
                    int index = Array.LastIndexOf(current, handler);
                    if (index < 0)
                        return false;
                    if (current.Length == 1)
                    {
                        Volatile.Write(ref _handlers, Array.Empty<TDelegate>());
                        return true;
                    }
                    TDelegate[] next = new TDelegate[current.Length - 1];
                    Array.Copy(current, 0, next, 0, index);
                    Array.Copy(current, index + 1, next, index, current.Length - index - 1);
                    Volatile.Write(ref _handlers, next);
                    return true;
                }
            }

            public void Clear()
            {
                lock (_gate)
                    Volatile.Write(ref _handlers, Array.Empty<TDelegate>());
            }
        }

        /// <summary>
        /// Handler lists per key. Lists are never removed, so a concurrent Subscribe can't add to a detached list.
        /// </summary>
        internal sealed class EventSystem<TKey, TDelegate> : IEventSystem where TKey : notnull where TDelegate : Delegate
        {
            private readonly ConcurrentDictionary<TKey, HandlerList<TDelegate>> _events = new();

            public EventSystem() => EventX.Register(this);

            public Subscription<TKey, TDelegate> Subscribe(TKey key, TDelegate handler)
            {
                if (handler is null) throw new ArgumentNullException(nameof(handler));
                _events.GetOrAdd(key, static _ => new HandlerList<TDelegate>()).Add(handler);
                return new Subscription<TKey, TDelegate>(this, key, handler);
            }

            public bool Unsubscribe(TKey key, TDelegate handler)
            {
                if (handler is null) throw new ArgumentNullException(nameof(handler));
                return _events.TryGetValue(key, out HandlerList<TDelegate>? list) && list.Remove(handler);
            }

            public TDelegate[] Snapshot(TKey key) =>
                _events.TryGetValue(key, out HandlerList<TDelegate>? list) ? list.Snapshot : Array.Empty<TDelegate>();

            public int Count(TKey key) => _events.TryGetValue(key, out HandlerList<TDelegate>? list) ? list.Count : 0;

            public void ClearEvent(TKey key)
            {
                if (_events.TryGetValue(key, out HandlerList<TDelegate>? list))
                    list.Clear();
            }

            public void ClearAll()
            {
                foreach (HandlerList<TDelegate> list in _events.Values)
                    list.Clear();
            }
        }

        /// <summary>
        /// Keyed event bus for handlers with no parameters.
        /// </summary>
        /// <remarks>See <see cref="EventX"/> for the guarantees shared by every arity.</remarks>
        public static class EventX<TKey> where TKey : notnull
        {
            private static readonly EventSystem<TKey, Action> _sys = new();

            /// <summary>
            /// Subscribes a handler to the event key. Dispose the returned subscription (once) to unsubscribe.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="handler">The handler to subscribe.</param>
            public static Subscription<TKey, Action> Subscribe(TKey key, Action handler) =>
                _sys.Subscribe(key, handler);

            /// <summary>
            /// Unsubscribes one registration of the handler from the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="handler">The handler to unsubscribe.</param>
            /// <returns><c>true</c> if the handler was registered.</returns>
            public static bool Unsubscribe(TKey key, Action handler) =>
                _sys.Unsubscribe(key, handler);

            /// <summary>
            /// Invokes every handler subscribed to the event key, in subscription order.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static void Broadcast(TKey key)
            {
                Action[] handlers = _sys.Snapshot(key);
                for (int i = 0; i < handlers.Length; i++)
                {
                    try
                    {
                        handlers[i]();
                    }
                    catch (Exception e) when (!EventX.ThrowOnHandlerException)
                    {
                        EventX.Report(e);
                    }
                }
            }

            /// <summary>
            /// Removes all handlers for the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static void ClearEvent(TKey key) => _sys.ClearEvent(key);

            /// <summary>
            /// Removes all handlers of this key type and signature.
            /// </summary>
            public static void ClearAll() => _sys.ClearAll();

            /// <summary>
            /// Checks whether the event key has at least one handler.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static bool HasEvent(TKey key) => _sys.Count(key) > 0;

            /// <summary>
            /// Gets the number of handlers registered to the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static int Count(TKey key) => _sys.Count(key);
        }

        /// <summary>
        /// Keyed event bus for handlers with one parameter.
        /// </summary>
        public static class EventX<TKey, T1> where TKey : notnull
        {
            private static readonly EventSystem<TKey, Action<T1>> _sys = new();

            /// <summary>
            /// Subscribes a handler to the event key. Dispose the returned subscription (once) to unsubscribe.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="handler">The handler to subscribe.</param>
            public static Subscription<TKey, Action<T1>> Subscribe(TKey key, Action<T1> handler) =>
                _sys.Subscribe(key, handler);

            /// <summary>
            /// Unsubscribes one registration of the handler from the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="handler">The handler to unsubscribe.</param>
            /// <returns><c>true</c> if the handler was registered.</returns>
            public static bool Unsubscribe(TKey key, Action<T1> handler) =>
                _sys.Unsubscribe(key, handler);

            /// <summary>
            /// Invokes every handler subscribed to the event key, in subscription order.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="v1">The first event argument.</param>
            public static void Broadcast(TKey key, T1 v1)
            {
                Action<T1>[] handlers = _sys.Snapshot(key);
                for (int i = 0; i < handlers.Length; i++)
                {
                    try
                    {
                        handlers[i](v1);
                    }
                    catch (Exception e) when (!EventX.ThrowOnHandlerException)
                    {
                        EventX.Report(e);
                    }
                }
            }

            /// <summary>
            /// Removes all handlers for the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static void ClearEvent(TKey key) => _sys.ClearEvent(key);

            /// <summary>
            /// Removes all handlers of this key type and signature.
            /// </summary>
            public static void ClearAll() => _sys.ClearAll();

            /// <summary>
            /// Checks whether the event key has at least one handler.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static bool HasEvent(TKey key) => _sys.Count(key) > 0;

            /// <summary>
            /// Gets the number of handlers registered to the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static int Count(TKey key) => _sys.Count(key);
        }

        /// <summary>
        /// Keyed event bus for handlers with two parameters.
        /// </summary>
        public static class EventX<TKey, T1, T2> where TKey : notnull
        {
            private static readonly EventSystem<TKey, Action<T1, T2>> _sys = new();

            /// <summary>
            /// Subscribes a handler to the event key. Dispose the returned subscription (once) to unsubscribe.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="handler">The handler to subscribe.</param>
            public static Subscription<TKey, Action<T1, T2>> Subscribe(TKey key, Action<T1, T2> handler) =>
                _sys.Subscribe(key, handler);

            /// <summary>
            /// Unsubscribes one registration of the handler from the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="handler">The handler to unsubscribe.</param>
            /// <returns><c>true</c> if the handler was registered.</returns>
            public static bool Unsubscribe(TKey key, Action<T1, T2> handler) =>
                _sys.Unsubscribe(key, handler);

            /// <summary>
            /// Invokes every handler subscribed to the event key, in subscription order.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="v1">The first event argument.</param>
            /// <param name="v2">The second event argument.</param>
            public static void Broadcast(TKey key, T1 v1, T2 v2)
            {
                Action<T1, T2>[] handlers = _sys.Snapshot(key);
                for (int i = 0; i < handlers.Length; i++)
                {
                    try
                    {
                        handlers[i](v1, v2);
                    }
                    catch (Exception e) when (!EventX.ThrowOnHandlerException)
                    {
                        EventX.Report(e);
                    }
                }
            }

            /// <summary>
            /// Removes all handlers for the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static void ClearEvent(TKey key) => _sys.ClearEvent(key);

            /// <summary>
            /// Removes all handlers of this key type and signature.
            /// </summary>
            public static void ClearAll() => _sys.ClearAll();

            /// <summary>
            /// Checks whether the event key has at least one handler.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static bool HasEvent(TKey key) => _sys.Count(key) > 0;

            /// <summary>
            /// Gets the number of handlers registered to the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static int Count(TKey key) => _sys.Count(key);
        }

        /// <summary>
        /// Keyed event bus for handlers with three parameters.
        /// </summary>
        public static class EventX<TKey, T1, T2, T3> where TKey : notnull
        {
            private static readonly EventSystem<TKey, Action<T1, T2, T3>> _sys = new();

            /// <summary>
            /// Subscribes a handler to the event key. Dispose the returned subscription (once) to unsubscribe.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="handler">The handler to subscribe.</param>
            public static Subscription<TKey, Action<T1, T2, T3>> Subscribe(TKey key, Action<T1, T2, T3> handler) =>
                _sys.Subscribe(key, handler);

            /// <summary>
            /// Unsubscribes one registration of the handler from the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="handler">The handler to unsubscribe.</param>
            /// <returns><c>true</c> if the handler was registered.</returns>
            public static bool Unsubscribe(TKey key, Action<T1, T2, T3> handler) =>
                _sys.Unsubscribe(key, handler);

            /// <summary>
            /// Invokes every handler subscribed to the event key, in subscription order.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="v1">The first event argument.</param>
            /// <param name="v2">The second event argument.</param>
            /// <param name="v3">The third event argument.</param>
            public static void Broadcast(TKey key, T1 v1, T2 v2, T3 v3)
            {
                Action<T1, T2, T3>[] handlers = _sys.Snapshot(key);
                for (int i = 0; i < handlers.Length; i++)
                {
                    try
                    {
                        handlers[i](v1, v2, v3);
                    }
                    catch (Exception e) when (!EventX.ThrowOnHandlerException)
                    {
                        EventX.Report(e);
                    }
                }
            }

            /// <summary>
            /// Removes all handlers for the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static void ClearEvent(TKey key) => _sys.ClearEvent(key);

            /// <summary>
            /// Removes all handlers of this key type and signature.
            /// </summary>
            public static void ClearAll() => _sys.ClearAll();

            /// <summary>
            /// Checks whether the event key has at least one handler.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static bool HasEvent(TKey key) => _sys.Count(key) > 0;

            /// <summary>
            /// Gets the number of handlers registered to the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static int Count(TKey key) => _sys.Count(key);
        }

        /// <summary>
        /// Keyed event bus for handlers with four parameters.
        /// </summary>
        public static class EventX<TKey, T1, T2, T3, T4> where TKey : notnull
        {
            private static readonly EventSystem<TKey, Action<T1, T2, T3, T4>> _sys = new();

            /// <summary>
            /// Subscribes a handler to the event key. Dispose the returned subscription (once) to unsubscribe.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="handler">The handler to subscribe.</param>
            public static Subscription<TKey, Action<T1, T2, T3, T4>> Subscribe(TKey key, Action<T1, T2, T3, T4> handler) =>
                _sys.Subscribe(key, handler);

            /// <summary>
            /// Unsubscribes one registration of the handler from the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="handler">The handler to unsubscribe.</param>
            /// <returns><c>true</c> if the handler was registered.</returns>
            public static bool Unsubscribe(TKey key, Action<T1, T2, T3, T4> handler) =>
                _sys.Unsubscribe(key, handler);

            /// <summary>
            /// Invokes every handler subscribed to the event key, in subscription order.
            /// </summary>
            /// <param name="key">The event key.</param>
            /// <param name="v1">The first event argument.</param>
            /// <param name="v2">The second event argument.</param>
            /// <param name="v3">The third event argument.</param>
            /// <param name="v4">The fourth event argument.</param>
            public static void Broadcast(TKey key, T1 v1, T2 v2, T3 v3, T4 v4)
            {
                Action<T1, T2, T3, T4>[] handlers = _sys.Snapshot(key);
                for (int i = 0; i < handlers.Length; i++)
                {
                    try
                    {
                        handlers[i](v1, v2, v3, v4);
                    }
                    catch (Exception e) when (!EventX.ThrowOnHandlerException)
                    {
                        EventX.Report(e);
                    }
                }
            }

            /// <summary>
            /// Removes all handlers for the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static void ClearEvent(TKey key) => _sys.ClearEvent(key);

            /// <summary>
            /// Removes all handlers of this key type and signature.
            /// </summary>
            public static void ClearAll() => _sys.ClearAll();

            /// <summary>
            /// Checks whether the event key has at least one handler.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static bool HasEvent(TKey key) => _sys.Count(key) > 0;

            /// <summary>
            /// Gets the number of handlers registered to the event key.
            /// </summary>
            /// <param name="key">The event key.</param>
            public static int Count(TKey key) => _sys.Count(key);
        }
    }
}
