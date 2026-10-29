// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    public partial class Actor
    {
        public ActorID ID;

        internal Actor()
        {
        }

        public static Actor Create()
        {
            Actor actor = new() { ID = ActorUtils.XGCreateActor(ScriptRuntime._enginePtr) };
            return actor;
        }

        public static Actor Create(string actorName)
        {
            Actor actor = new() { ID = ActorUtils.XGCreateActorWithName(ScriptRuntime._enginePtr, actorName) };
            return actor;
        }

        public T GetComponent<T>() where T : class
        {
            return null;
        }

        public string Name
        {
            get => ActorUtils.GetActorName(ID);
            set => ActorUtils.SetActorName(ID, value);
        }

        internal partial class ActorUtils
        {
            [LibraryImport("xgGameKernel", EntryPoint = "XGCreateActor")]
            internal static partial ulong XGCreateActor(nint engine);

            [LibraryImport("xgGameKernel", EntryPoint = "XGCreateActorWithName", StringMarshalling = StringMarshalling.Utf8)]
            internal static partial ulong XGCreateActorWithName(nint engine, string actorName);

            internal static string GetActorName(ActorID id)
            {
                nint ptr = XGGetActorName(ScriptRuntime._enginePtr, id);
                return Marshal.PtrToStringUTF8(ptr);
            }

            internal static void SetActorName(ActorID id, string newName)
            {
                XGSetActorName(ScriptRuntime._enginePtr, id, newName);
            }

            [LibraryImport("xgGameKernel", EntryPoint = "XGGetActorName")]
            internal static partial nint XGGetActorName(nint engine, ulong actorID);

            [LibraryImport("xgGameKernel", EntryPoint = "XGSetActorName", StringMarshalling = StringMarshalling.Utf8)]
            internal static partial nint XGSetActorName(nint engine, ulong actorID, string newName);
        }
    }
}
