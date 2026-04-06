#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include "json.hpp"
#include <vector>
#include <fstream>
#include <string>

using json = nlohmann::json;

class Game {
public:
    // ✅ 把枚举移到类内部！现在它就是 Game 的成员了！
    enum GameState {
        STATE_MENU,
        STATE_LEVEL_SELECT,
        STATE_READY,
        STATE_PLAYING,
        STATE_PAUSED,
        STATE_GAME_OVER,
        STATE_VICTORY
    };

private:
    Ball ball;
    Paddle paddle;
    std::vector<Brick> bricks;
    Font font;

    int score;
    int lives;
    float gameTime;
    int winCount;
    GameState currentState;  // 自动识别类内枚举

    // 配置结构体
    struct {
        int width;
        int height;
        int border;
        Color bg;
        int fallOffset;
        int scorePenalty;
    } cfgWindow;

    struct {
        float radius;
        float startX;
        float startY;
    } cfgBall;

    struct {
        float width;
        float height;
        float startX;
        float startY;
        float speedNormal;
        float speedBoost;
    } cfgPaddle;

    struct {
        int rows;
        int cols;
        float width;
        float height;
        float startX;
        float startY;
        float spacingX;
        float spacingY;
    } cfgBricks;

    struct {
        int initialLives;
        int baseScorePerBrick;
        float timeMultBase;
        float timeMultDecay;
        float timeMultMin;
    } cfgGame;

    void CreateBricks();
    void ResetGame();

public:
    Game();
    void LoadConfig(const std::string& path);
    void Init();
    void Update();
    void Draw();
    void Shutdown();
};

#endif