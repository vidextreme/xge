using xg;

namespace SampleGame
{
    // Any static method marked [GameInit] runs when the project's scripts load; [GameUpdate] runs every frame.
    public static class Hello
    {
        [GameInit]
        public static void Init()
        {
            Debug.Log("[Project] SampleGame initialized");
            Actor.Create("SampleGame Player");
        }
    }
}
