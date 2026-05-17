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
// 新增：异步加载所需头文件
#include <thread>
#include <mutex>
#include <future>
#include <chrono>
#include "ParticlePool.h"

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

    // 新增：异步加载状态枚举
    enum class LoadState {
        IDLE,       // 未加载
        LOADING,    // 加载中
        DONE        // 加载完成
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
    std::unique_ptr<ParticlePool> brickParticles;    // 替换 ParticleSystem → ParticlePool
    std::unique_ptr<ParticlePool> powerUpAura;       // 替换 ParticleSystem → ParticlePool
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

    // 新增：异步加载相关成员变量
    LoadState m_loadState = LoadState::IDLE;
    std::future<void> m_loadFuture;
    std::mutex m_loadMutex;       // 保护共享状态的互斥锁
    bool m_loadingSuccess = false; // 标记加载是否完成
    Texture2D m_largeTexture;     // 异步加载的大纹理（加分项）
    Image m_tempImage;            // 临时存储图片数据（加分项）

    void CreateBricks();
    void ResetGame();
    void SpawnPowerUp(Vector2 pos);
    void CheckPowerUpCollisions();
    void CleanupExpiredPowerUps();
    // 新增：异步加载函数声明
    void LoadLargeTextureAsync();

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