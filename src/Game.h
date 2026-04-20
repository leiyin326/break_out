#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include "PowerUp.h"
#include "Particle.h"
#include "json.hpp"
#include <vector>
#include <fstream>
#include <memory>

using json = nlohmann::json;

class Game {
public:
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
    Paddle paddle;
    std::vector<Brick> bricks;
    Font font;
    Sound sndBrick;
    Sound sndPowerUp;
    Sound sndVictory;
    int score;
    int lives;
    float gameTime;
    int winCount;
    GameState currentState;

    std::vector<std::unique_ptr<PowerUpEffect>> powerUps;
    std::unique_ptr<ParticleSystem> brickParticles;
    std::unique_ptr<ParticleSystem> powerUpAura;
    bool audioLoaded;

    PowerUpConfig powerUpCfg;

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
        float maxWidth;
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
    void SpawnPowerUp(Vector2 pos);
    void CheckPowerUpCollisions();
    void CleanupExpiredPowerUps();

public:
    Game();
    void LoadConfig(const std::string& path);
    void Init();
    void Update();
    void Draw();
    void Shutdown();

    int& GetLives() { return lives; }
    Paddle& GetPaddle() { return paddle; }
    std::vector<Ball> balls;
};

#endif