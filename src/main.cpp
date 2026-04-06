#include "raylib.h"
#include "Game.h"

int main() {
    const int w = 800;
    const int h = 600;
    InitWindow(w, h, "Breakout JSON Config");

    Game game;
    game.Init();

    while (!WindowShouldClose()) {
        game.Update();
        game.Draw();
    }

    game.Shutdown();
    CloseWindow();
    return 0;
}