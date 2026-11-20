// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace xg
{
    public enum JobQueue
    {
        // A job system worker thread.
        Worker = 0,
        // The main thread, between frames (or inside a main-thread Wait).
        Main = 1,
    }

    // A scheduled job. None (0) and finished jobs count as done.
    public readonly struct JobHandle : IEquatable<JobHandle>
    {
        public JobHandle(ulong value) => Value = value;

        public static JobHandle None => default;

        public ulong Value { get; }
        public bool IsDone => Jobs.IsDone(this);

        public void Wait() => Jobs.Wait(this);

        public bool Equals(JobHandle other) => Value == other.Value;
        public override bool Equals(object? obj) => obj is JobHandle other && Equals(other);
        public override int GetHashCode() => Value.GetHashCode();
    }

    // The engine's job system (xgBase/public/xgJobs.h) for managed code: work runs on a worker thread or on the main
    // thread once its dependencies are done. Exceptions are reported with Debug.LogException and never cross into the
    // engine. Jobs belong to this module, so the engine waits for them before unloading it. Without an engine (tests,
    // tools) Schedule runs the work at once and returns JobHandle.None.
    public static unsafe partial class Jobs
    {
        private const string NativeLib = "xgScriptNative";

        public static bool IsAvailable => ScriptRuntime._enginePtr != 0 && XG_JobWorkerCount(ScriptRuntime._enginePtr) > 0;

        public static int WorkerCount => ScriptRuntime._enginePtr == 0 ? 0 : (int)XG_JobWorkerCount(ScriptRuntime._enginePtr);

        public static JobHandle Schedule(Action work, JobQueue queue = JobQueue.Worker, params JobHandle[] dependencies) =>
            Schedule(work, queue, (ReadOnlySpan<JobHandle>)dependencies);

        public static JobHandle Schedule(Action work, JobQueue queue, ReadOnlySpan<JobHandle> dependencies)
        {
            ArgumentNullException.ThrowIfNull(work);
            nint engine = ScriptRuntime._enginePtr;
            if (engine == 0)
            {
                Run(work);
                return JobHandle.None;
            }
            GCHandle handle = GCHandle.Alloc(work);
            ulong job;
            fixed (JobHandle* deps = dependencies)
                job = XG_JobSchedule(engine, ScriptRuntime.ModuleID, &Trampoline, GCHandle.ToIntPtr(handle), (ulong*)deps, (uint)dependencies.Length, (int)queue);
            if (job == 0)
            {
                handle.Free();
                foreach (JobHandle dependency in dependencies)
                    Wait(dependency);
                Run(work);
            }
            return new JobHandle(job);
        }

        public static bool IsDone(JobHandle handle) =>
            handle.Value == 0 || ScriptRuntime._enginePtr == 0 || XG_JobIsDone(ScriptRuntime._enginePtr, handle.Value);

        // Blocks until the job is done, running other ready jobs meanwhile (main-thread jobs too on the main thread).
        public static void Wait(JobHandle handle)
        {
            if (handle.Value != 0 && ScriptRuntime._enginePtr != 0)
                XG_JobWait(ScriptRuntime._enginePtr, handle.Value);
        }

        public static void WaitAll(IEnumerable<JobHandle> handles)
        {
            foreach (JobHandle handle in handles)
                Wait(handle);
        }

        private static void Run(Action work)
        {
            try
            {
                work();
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }

        [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
        private static void Trampoline(nint user)
        {
            GCHandle handle = GCHandle.FromIntPtr(user);
            Action? work = handle.Target as Action;
            handle.Free();
            if (work is not null)
                Run(work);
        }

        [LibraryImport(NativeLib, EntryPoint = "XG_JobSchedule")]
        private static partial ulong XG_JobSchedule(nint engine, uint moduleId, delegate* unmanaged[Cdecl]<nint, void> run, nint user,
            ulong* dependencies, uint dependencyCount, int queue);

        [LibraryImport(NativeLib, EntryPoint = "XG_JobIsDone")]
        [return: MarshalAs(UnmanagedType.U1)]
        private static partial bool XG_JobIsDone(nint engine, ulong handle);

        [LibraryImport(NativeLib, EntryPoint = "XG_JobWait")]
        private static partial void XG_JobWait(nint engine, ulong handle);

        [LibraryImport(NativeLib, EntryPoint = "XG_JobWorkerCount")]
        private static partial uint XG_JobWorkerCount(nint engine);
    }
}
