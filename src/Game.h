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

    enum class LoadState {
        IDLE,
        LOADING,
        DONE
    };

   struct LevelConfig {
    std::string name;
    float width;
    float height;
    float startX;
    float startY;
    float spacingX;
    float spacingY;
    std::vector<Color> colors;
    std::vector<std::vector<int>> shape; // 加这行
    };

    struct SaveData {
        int score;
        int lives;
        int currentLevelIndex;
        float gameTime;
        int version = 1;
        int maxUnlockedLevel;
        std::vector<bool> brickStates;
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
    void ResetAllProgress();
    int maxUnlockedLevel = 0;

    enum class SavePromptState {
        NONE,
        LOAD_PROMPT,
        SAVE_PROMPT
    };

    SavePromptState promptState = SavePromptState::NONE;
    float promptTimer = 0.0f;

    std::vector<std::unique_ptr<PowerUpEffect>> powerUps;
    std::unique_ptr<ParticlePool> brickParticles;
    std::unique_ptr<ParticlePool> powerUpAura;
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
        int initialLives;
        int baseScorePerBrick;
        float timeMultBase;
        float timeMultDecay;
        float timeMultMin;
    } cfgGame;

    LoadState m_loadState = LoadState::IDLE;
    std::future<void> m_loadFuture;
    std::mutex m_loadMutex;
    bool m_loadingSuccess = false;
    Texture2D m_largeTexture = { 0 };
    Image m_tempImage = { 0 };

    std::vector<LevelConfig> levels;
    int currentLevelIndex = 0;
    std::string saveFilePath = "savegame.json";

    void CreateBricks();
    void ResetGame();
    void SpawnPowerUp(Vector2 pos);
    void CheckPowerUpCollisions();
    void CleanupExpiredPowerUps();
    void LoadLargeTextureAsync();

    bool LoadLevelFromJSON(const std::string& path);
    void LoadAllLevels();
    bool SaveGame();
    bool LoadGame();
    Color StringToColor(const std::string& colorStr);

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