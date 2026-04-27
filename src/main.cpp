#define _POSIX_C_SOURCE 200809L
#include "raylib.h"
#include "Game.h"
#include <string.h>

int main(int argc, char* argv[]) {
    const int w = 800;
    const int h = 600;
    InitWindow(w, h, "Breakout 双窗口联机对战");
    SetTargetFPS(60);

    Game game;
    game.Init();

    if (argc >= 2) {
        if (strcmp(argv[1], "server") == 0) {
            game.InitNetServer();
        } else if (strcmp(argv[1], "client") == 0 && argc >= 3) {
            game.InitNetClient(argv[2]);
        }
    }

    while (!WindowShouldClose()) {
        game.NetUpdate(GetFrameTime());
        game.Update();
        game.Draw();
    }

    game.NetClose();
    game.Shutdown();
    CloseWindow();
    return 0;
}