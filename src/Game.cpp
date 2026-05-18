#include "Game.h"
#include "PowerUp.h"
#include "Particle.h"
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

// 辅助函数：字符串转Color（适配JSON中的颜色配置）
Color Game::StringToColor(const std::string& colorStr) {
    if (colorStr == "RED") return RED;
    if (colorStr == "ORANGE") return ORANGE;
    if (colorStr == "YELLOW") return YELLOW;
    if (colorStr == "GREEN") return GREEN;
    if (colorStr == "BLUE") return BLUE;
    if (colorStr == "PURPLE") return PURPLE;
    if (colorStr == "GOLD") return GOLD;
    return WHITE; // 默认颜色
}

// 加载单个关卡配置（从JSON文件）
bool Game::LoadLevelFromJSON(const std::string& path) {
    if (!FileExists(path.c_str())) {
        TraceLog(LOG_ERROR, "关卡文件 %s 不存在！", path.c_str());
        return false;
    }

    std::ifstream f(path);
    if (!f.is_open()) {
        TraceLog(LOG_ERROR, "无法打开关卡文件 %s", path.c_str());
        return false;
    }

    try {
        json j;
        f >> j;

        LevelConfig cfg;
        cfg.name = j["name"];
        cfg.rows = j["rows"];
        cfg.cols = j["cols"];
        cfg.width = j["width"];
        cfg.height = j["height"];
        cfg.startX = j["start_x"];
        cfg.startY = j["start_y"];
        cfg.spacingX = j["spacing_x"];
        cfg.spacingY = j["spacing_y"];

        // 解析颜色列表
        for (auto& c : j["colors"]) {
            cfg.colors.push_back(StringToColor(c));
        }

        levels.push_back(cfg);
        TraceLog(LOG_INFO, "成功加载关卡：%s", cfg.name.c_str());
        return true;
    } catch (json::parse_error& e) {
        TraceLog(LOG_ERROR, "关卡文件 %s 格式错误：%s", path.c_str(), e.what());
        return false;
    } catch (...) {
        TraceLog(LOG_ERROR, "关卡文件 %s 解析失败！", path.c_str());
        return false;
    }
}

// 加载所有关卡（按level1-level4的顺序）
void Game::LoadAllLevels() {
    levels.clear();
    LoadLevelFromJSON("levels/level1.json");
    LoadLevelFromJSON("levels/level2.json");
    LoadLevelFromJSON("levels/level3.json");
    LoadLevelFromJSON("levels/level4.json");

    // 兜底：如果关卡加载失败，使用默认配置
    if (levels.empty()) {
        TraceLog(LOG_WARNING, "未加载到任何关卡，使用默认配置！");
        LevelConfig defaultCfg;
        defaultCfg.name = "默认关卡";
        defaultCfg.rows = 5;
        defaultCfg.cols = 8;
        defaultCfg.width = 85.0f;
        defaultCfg.height = 25.0f;
        defaultCfg.startX = 50.0f;
        defaultCfg.startY = 80.0f;
        defaultCfg.spacingX = 95.0f;
        defaultCfg.spacingY = 35.0f;
        defaultCfg.colors = {RED, ORANGE, YELLOW, GREEN, BLUE};
        levels.push_back(defaultCfg);
    }
}

// 保存游戏存档
bool Game::SaveGame() {
    try {
        SaveData data;
        data.score = score;
        data.lives = lives;
        data.currentLevelIndex = currentLevelIndex;
        data.gameTime = gameTime;
        data.version = 1;
        data.maxUnlockedLevel = maxUnlockedLevel;

        // 保存砖块状态
        for (auto& brick : bricks) {
            data.brickStates.push_back(brick.IsActive());
        }

        json j;
        j["version"] = data.version;
        j["score"] = data.score;
        j["lives"] = data.lives;
        j["current_level"] = data.currentLevelIndex;
        j["game_time"] = data.gameTime;
        j["max_unlocked_level"] = data.maxUnlockedLevel;
        j["brick_states"] = data.brickStates;

        std::ofstream f(saveFilePath);
        f << j.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}
// 加载游戏存档【修正版：只重置小球和生命，保留其他】
bool Game::LoadGame() {
    if (!FileExists(saveFilePath.c_str()))
        return false;

    try {
        std::ifstream f(saveFilePath);
        json j;
        f >> j;

        // 1. 恢复基础数据 (分数、时间、解锁进度保留)
        score = j["score"];
        gameTime = j["game_time"];
        maxUnlockedLevel = j["max_unlocked_level"];
        currentLevelIndex = j["current_level"];

        // 2. 重置生命 (按你的要求：生命重置)
        lives = cfgGame.initialLives;

        // 3. 恢复砖块状态
        // 先根据当前关卡索引创建砖块结构
        CreateBricks();

        // 再用存档数据覆盖砖块状态
        if (j.contains("brick_states")) {
            std::vector<bool> brickStates = j["brick_states"];
            winCount = 0;
            for (int i = 0; i < bricks.size() && i < brickStates.size(); i++) {
                bricks[i].SetActive(brickStates[i]);
                if (brickStates[i]) winCount++;
            }
        }

        // 4. 重置小球 (位置重置，速度归零)
        balls.clear();
        balls.emplace_back(Vector2{ cfgBall.startX, cfgBall.startY }, Vector2{ 0,0 }, cfgBall.radius);
        balls[0].ResetToPaddle(paddle.GetRect().x + paddle.GetRect().width / 2, paddle.GetRect().y);

        // 5. 清理道具
        powerUps.clear();
        paddle.SetWidth(cfgPaddle.width);

        return true;
    } catch (...) {
        return false;
    }
}
Game::Game()
    : paddle(0,0,0,0),
    currentState(Game::STATE_MENU),
    audioLoaded(false),
    // 新增：初始化异步加载纹理
    m_largeTexture({0}),
    m_tempImage({0})
{
    LoadConfig("config.json");
    // 加载所有关卡（核心修改）
    LoadAllLevels();
    std::vector<int> codepoints;
    for (int i = 0x4E00; i <= 0x9FFF; ++i) codepoints.push_back(i);
    for (int i = 0x0020; i <= 0x007E; ++i) codepoints.push_back(i);

    font = LoadFontEx("fonts/NotoSansSC.otf", 32, codepoints.data(), (int)codepoints.size());
    if (font.texture.id == 0) {
        TraceLog(LOG_ERROR, "中文字体加载失败，使用默认字体");
        font = GetFontDefault();
    }

    brickParticles = std::make_unique<ParticlePool>(150);
    powerUpAura = std::make_unique<ParticlePool>(80);
    SetRandomSeed((unsigned int)time(nullptr));

    paddle = Paddle(cfgPaddle.startX, cfgPaddle.startY, cfgPaddle.width, cfgPaddle.height);
}

void Game::LoadConfig(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        TraceLog(LOG_ERROR, "配置文件加载失败，使用默认值");
        cfgWindow = { 800,600,5, {30,30,40,255}, 50, 50 };
        cfgBall = { 10, 400,530 };
        cfgPaddle = { 120,15, 340,550, 18,28, 240 };
        cfgGame = {3,10,5.0f,0.05f,1.0f};
        powerUpCfg = {0.25f, 8.0f, 120.0f, 16.0f, 150};
        return;
    }

    json j;
    f >> j;

    cfgWindow.width = j["window"]["width"];
    cfgWindow.height = j["window"]["height"];
    cfgWindow.border = j["window"]["border"];
    cfgWindow.bg = {
        (unsigned char)j["window"]["bg_r"],
        (unsigned char)j["window"]["bg_g"],
        (unsigned char)j["window"]["bg_b"],
        (unsigned char)j["window"]["bg_a"]
    };
    cfgWindow.fallOffset = j["window"]["fall_offset"];
    cfgWindow.scorePenalty = j["window"]["score_penalty"];

    cfgBall.radius = j["ball"]["radius"];
    cfgBall.startX = j["ball"]["start_x"];
    cfgBall.startY = j["ball"]["start_y"];

    cfgPaddle.width = j["paddle"]["width"];
    cfgPaddle.height = j["paddle"]["height"];
    cfgPaddle.startX = j["paddle"]["start_x"];
    cfgPaddle.startY = j["paddle"]["start_y"];
    cfgPaddle.speedNormal = j["paddle"]["speed_normal"];
    cfgPaddle.speedBoost = j["paddle"]["speed_boost"];
    cfgPaddle.maxWidth = j["paddle"]["max_width"];

    cfgGame.initialLives = j["game"]["initial_lives"];
    cfgGame.baseScorePerBrick = j["game"]["base_score_per_brick"];
    cfgGame.timeMultBase = j["game"]["time_multiplier_base"];
    cfgGame.timeMultDecay = j["game"]["time_multiplier_decay"];
    cfgGame.timeMultMin = j["game"]["time_multiplier_min"];

    if (j.contains("powerups")) {
        powerUpCfg.dropChance = j["powerups"]["drop_chance"];
        powerUpCfg.duration = j["powerups"]["duration"];
        powerUpCfg.fallSpeed = j["powerups"]["fall_speed"];
        powerUpCfg.size = j["powerups"]["size"];
        powerUpCfg.maxParticles = j["powerups"]["max_particles"];
    }
}

void Game::Init() {
    SetTargetFPS(60);
    CreateBricks();
    ResetGame();

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_ERROR, "Audio device NOT ready!");
    }

    sndBrick = LoadSound("sounds/brick.wav");
    sndPowerUp = LoadSound("sounds/powerup.wav");
    sndVictory = LoadSound("sounds/victory.wav");
}
void Game::CreateBricks() {
    bricks.clear();
    // 取当前关卡的配置
    LevelConfig& cfg = levels[currentLevelIndex];

    // 使用关卡专属颜色
    std::vector<Color> cs = cfg.colors.empty() ? 
        std::vector<Color>{RED,ORANGE,YELLOW,GREEN,BLUE} : cfg.colors;

    for (int r = 0; r < cfg.rows; r++) {
        for (int c = 0; c < cfg.cols; c++) {
            bricks.emplace_back(
                cfg.startX + c * cfg.spacingX,
                cfg.startY + r * cfg.spacingY,
                cfg.width,
                cfg.height,
                cs[r % cs.size()],
                false
            );
        }
    }

    int totalBricks = bricks.size();
    int goldenCount = GetRandomValue(4, 6);

    for (int i = 0; i < goldenCount; i++) {
        int idx;
        do {
            idx = GetRandomValue(0, totalBricks - 1);
        } while (bricks[idx].IsGolden());

        bricks[idx] = Brick(
            bricks[idx].GetRect().x,
            bricks[idx].GetRect().y,
            bricks[idx].GetRect().width,
            bricks[idx].GetRect().height,
            GOLD,
            true
        );
    }

    winCount = bricks.size();
}
void Game::ResetGame() {
    lives = 3;
    gameTime = 0;
    currentState = STATE_READY;
    powerUps.clear();
    brickParticles->Clear();
    powerUpAura->Clear();
    CreateBricks();

    // 初始化一个主球
    balls.clear();
    balls.emplace_back(Vector2{ cfgBall.startX, cfgBall.startY }, Vector2{ 0,0 }, cfgBall.radius);
    balls[0].ResetToPaddle(paddle.GetRect().x + paddle.GetRect().width / 2, paddle.GetRect().y);

    // 重置异步加载状态
    std::lock_guard<std::mutex> lock(m_loadMutex);
    m_loadState = LoadState::IDLE;
    m_loadingSuccess = false;
    if (m_largeTexture.id != 0) {
        UnloadTexture(m_largeTexture);
        m_largeTexture = {0};
    }
    if (m_tempImage.data != nullptr) {
        UnloadImage(m_tempImage);
        m_tempImage = {0};
    }
}

void Game::SpawnPowerUp(Vector2 pos) {
    if ((float)GetRandomValue(0, 99) / 100.0f > powerUpCfg.dropChance) return;

    Rectangle r = { pos.x - powerUpCfg.size/2, pos.y - powerUpCfg.size/2, powerUpCfg.size, powerUpCfg.size };
    int type = GetRandomValue(0, 2);
    PowerUpType t;

    if (type == 0) t = PowerUpType::PADDLE_ENLARGE;
    else if (type == 1) t = PowerUpType::MULTI_BALL;
    else t = PowerUpType::EXTRA_LIFE;

    auto pu = PowerUpFactory::CreatePowerUp(t, r, cfgPaddle.width, cfgPaddle.maxWidth);
    if (pu) powerUps.push_back(std::move(pu));
}

void Game::CheckPowerUpCollisions() {
    Rectangle pad = paddle.GetRect();
    for (auto& pu : powerUps) {
        if (pu->GetState() == PowerUpState::FALLING && CheckCollisionRecs(pu->GetRect(), pad)) {
            pu->Activate();
            StopSound(sndPowerUp);
            PlaySound(sndPowerUp);

            if (pu->GetType() == PowerUpType::PADDLE_ENLARGE) {
                paddle.SetWidth(cfgPaddle.maxWidth);
            }
            if (pu->GetType() == PowerUpType::EXTRA_LIFE) {
                lives++;
            }

            //多球道具
            if (pu->GetType() == PowerUpType::MULTI_BALL) {
                if (balls.empty()) return;
                Ball& main = balls[0];
                
                float speed = sqrtf(main.GetSpeed().x * main.GetSpeed().x + main.GetSpeed().y * main.GetSpeed().y);
                
                for (int i = 0; i < 2; i++) {
                    Ball b = main;
                    
                    float baseAngle = atan2f(main.GetSpeed().y, main.GetSpeed().x);
                    float randomAngle = baseAngle + GetRandomValue(-30, 30) * DEG2RAD;
                    
                    Vector2 newSpeed = {
                        cosf(randomAngle) * speed,
                        sinf(randomAngle) * speed
                    };
                    
                    b.SetSpeed(newSpeed);
                    b.SetLifeTime(8.0f);
                    balls.push_back(b);
                }
            }
        } 
    } 
}

void Game::CleanupExpiredPowerUps() {
    auto it = powerUps.begin();
    while (it != powerUps.end()) {
        if ((*it)->GetState() == PowerUpState::EXPIRED) {
            if ((*it)->GetType() == PowerUpType::PADDLE_ENLARGE) {
                paddle.SetWidth(cfgPaddle.width);
            }
            it = powerUps.erase(it);
        } else ++it;
    }
}

// 异步加载大纹理
void Game::LoadLargeTextureAsync() {
    std::lock_guard<std::mutex> lock(m_loadMutex);
    if (m_loadState != LoadState::IDLE) return;

    m_loadState = LoadState::LOADING;
    m_loadingSuccess = false;

    m_loadFuture = std::async(std::launch::async, [this]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (FileExists("large_texture.png")) {
            m_tempImage = LoadImage("large_texture.png");
        }
        std::lock_guard<std::mutex> lock(m_loadMutex);
        m_loadState = LoadState::DONE;
        m_loadingSuccess = true;
    });
}

void Game::Update() {
    float dt = GetFrameTime();
    promptTimer += dt;
    
    // 弹窗交互逻辑【最终正确版】
    if (promptState != SavePromptState::NONE && promptTimer > 0.5f)
    {
        if (promptState == SavePromptState::LOAD_PROMPT)
        {
            if (IsKeyPressed(KEY_Y))
            {
                LoadGame();
                currentState = STATE_READY;
                promptState = SavePromptState::NONE;
            }
            if (IsKeyPressed(KEY_N))
            {
                ResetGame();
                currentState = STATE_LEVEL_SELECT;
                promptState = SavePromptState::NONE;
            }
        }

        if (promptState == SavePromptState::SAVE_PROMPT)
        {
            if (IsKeyPressed(KEY_Y))
            {
                SaveGame();
                promptState = SavePromptState::NONE;
                if (currentState == STATE_MENU) CloseWindow();
                else currentState = STATE_MENU;
            }
            if (IsKeyPressed(KEY_N))
            {
                promptState = SavePromptState::NONE;
                if (currentState == STATE_MENU) CloseWindow();
                else currentState = STATE_MENU;
            }
        }
    }

    if ((currentState == STATE_VICTORY || currentState == STATE_GAME_OVER) && IsKeyPressed(KEY_R)) {
        ResetGame();
        currentState = STATE_MENU;
        return;
    }

    if (currentState == STATE_PLAYING && IsKeyPressed(KEY_K)) {
        powerUps.clear();
        paddle.SetWidth(cfgPaddle.width);
    }

    if (currentState == STATE_PLAYING && IsKeyPressed(KEY_L)) {
        LoadLargeTextureAsync();
    }

    {
        std::lock_guard<std::mutex> lock(m_loadMutex);
        if (m_loadState == LoadState::LOADING) {
            auto status = m_loadFuture.wait_for(std::chrono::seconds(0));
            if (status == std::future_status::ready) {
                m_loadFuture.get();
            }
        }
        if (m_loadState == LoadState::DONE && m_loadingSuccess) {
            if (m_tempImage.data != nullptr && m_largeTexture.id == 0) {
                m_largeTexture = LoadTextureFromImage(m_tempImage);
                UnloadImage(m_tempImage);
                m_tempImage = {0};
            }
            for (auto& brick : bricks) {
                if (brick.IsActive()) brick.SetColor(GREEN);
            }
            m_loadingSuccess = false;
        }
    }

    // 多球计时清理
    for (auto& ball : balls) {
        if (ball.GetLifeTime() > 0) {
            ball.SetLifeTime(ball.GetLifeTime() - dt);
        }
    }
    auto it = balls.begin();
    while (it != balls.end()) {
        if (it->GetLifeTime() <= 0 && it->GetLifeTime() != -1) {
            it = balls.erase(it);
        } else ++it;
    }

    // 球球碰撞
    for (int i = 0; i < (int)balls.size(); i++) {
        for (int j = i+1; j < (int)balls.size(); j++) {
            Ball& a = balls[i];
            Ball& b = balls[j];
            if (CheckCollisionCircles(a.GetPosition(), a.GetRadius(), b.GetPosition(), b.GetRadius())) {
                Vector2 va = a.GetSpeed();
                Vector2 vb = b.GetSpeed();
                a.SetSpeed(vb);
                b.SetSpeed(va);
            }
        }
    }

    switch (currentState)
    {
    case Game::STATE_MENU:
        if (IsKeyPressed(KEY_ENTER))
        {
            if (FileExists(saveFilePath.c_str()))
            {
                promptState = SavePromptState::LOAD_PROMPT;
                promptTimer = 0.0f;
            }
            else
            {
                ResetGame();
                currentState = STATE_LEVEL_SELECT;
            }
        }
        if (IsKeyPressed(KEY_R))
        {
            ResetAllProgress();
        }
        if (IsKeyPressed(KEY_ESCAPE) && promptState == SavePromptState::NONE)
        {
            promptState = SavePromptState::SAVE_PROMPT;
            promptTimer = 0.0f;
        }
        break;

    case Game::STATE_LEVEL_SELECT:
        if (IsKeyPressed(KEY_ONE) && 0 <= maxUnlockedLevel)
        { 
            currentLevelIndex = 0; 
            ResetGame(); 
            currentState=STATE_READY; 
        }
        if (IsKeyPressed(KEY_TWO) && 1 <= maxUnlockedLevel)
        { 
            currentLevelIndex = 1; 
            ResetGame(); 
            currentState=STATE_READY; 
        }
        if (IsKeyPressed(KEY_THREE) && 2 <= maxUnlockedLevel)
        { 
            currentLevelIndex = 2; 
            ResetGame(); 
            currentState=STATE_READY; 
        }
        if (IsKeyPressed(KEY_FOUR) && 3 <= maxUnlockedLevel)
        { 
            currentLevelIndex = 3; 
            ResetGame(); 
            currentState=STATE_READY; 
        }
        if (IsKeyPressed(KEY_ESCAPE)) currentState = STATE_MENU;
        break;

    case Game::STATE_READY: {
        float sp = IsKeyDown(KEY_LEFT_SHIFT) ? cfgPaddle.speedBoost : cfgPaddle.speedNormal;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) paddle.MoveLeft(sp);
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) paddle.MoveRight(sp);
        if (IsKeyPressed(KEY_SPACE)) {
            for (auto& ball : balls) {
                ball.Launch(paddle.GetRect().x + paddle.GetRect().width/2, paddle.GetRect().width);
            }
            currentState = STATE_PLAYING;
        }
        break;
    }

    case Game::STATE_PLAYING: {
        if (IsKeyPressed(KEY_ESCAPE)) { currentState=STATE_MENU; break; }
        if (IsKeyPressed(KEY_P)) { currentState=STATE_PAUSED; break; }

        gameTime += dt;
        float sp = IsKeyDown(KEY_LEFT_SHIFT) ? cfgPaddle.speedBoost : cfgPaddle.speedNormal;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) paddle.MoveLeft(sp);
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) paddle.MoveRight(sp);

        for (auto& ball : balls) {
            ball.ApplyGravity();
            ball.Move();
            ball.BounceEdge(cfgWindow.width, cfgWindow.height);
            ball.BouncePaddle(paddle.GetRect());
        }

        for (auto& ball : balls) {
            bool hitProcessed = false;
            for (auto& b : bricks) {
                if (!hitProcessed && b.IsActive() && ball.CheckBrickCollision(b.GetRect())) {
                    b.SetActive(false);
                    brickParticles->Emit({b.GetRect().x + b.GetRect().width/2, b.GetRect().y + b.GetRect().height/2}, b.GetColor());
                    StopSound(sndBrick);
                    PlaySound(sndBrick);
                    SpawnPowerUp({b.GetRect().x + b.GetRect().width/2, b.GetRect().y + b.GetRect().height/2});

                    float mul = cfgGame.timeMultBase - gameTime * cfgGame.timeMultDecay;
                    if (mul < cfgGame.timeMultMin) mul = cfgGame.timeMultMin;
                    score += (int)(cfgGame.baseScorePerBrick * mul * b.GetScoreMultiplier());
                    winCount--;
                    hitProcessed = true;
                }
            }
        }

        for (auto& pu : powerUps) pu->Update(dt);
        CheckPowerUpCollisions();
        CleanupExpiredPowerUps();

        brickParticles->Update(dt, cfgWindow.width, cfgWindow.height);
        for (auto& pu : powerUps) {
            if (pu->GetState() == PowerUpState::FALLING) {
                powerUpAura->Emit({pu->GetRect().x + pu->GetRect().width/2, pu->GetRect().y + pu->GetRect().height/2}, Fade(WHITE, 0.5f));
            }
        }
        powerUpAura->Update(dt, cfgWindow.width, cfgWindow.height);

        if (winCount <= 0){
            if(currentLevelIndex >= maxUnlockedLevel)
            {
                maxUnlockedLevel = currentLevelIndex + 1;
            }
            if (currentLevelIndex + 1 < (int)levels.size())
            {
                currentLevelIndex++;
                ResetGame();
                currentState = STATE_READY;
                PlaySound(sndVictory);
            }
            else
            {
                currentState = STATE_VICTORY;
                StopSound(sndVictory);
                PlaySound(sndVictory);
            }
            break;
        }

        int aliveBalls = 0;
        for (auto& ball : balls) {
            if (ball.GetPosition().y < cfgWindow.height + cfgWindow.fallOffset) {
                aliveBalls++;
            }
        }
        if (aliveBalls == 0) {
            lives--;
            score -= cfgWindow.scorePenalty;
            if (score < 0) score = 0;
            paddle.SetWidth(cfgPaddle.width);
            powerUps.clear();
            if (lives <= 0) {
                currentState = STATE_GAME_OVER;
            } else {
                balls.clear();
                balls.emplace_back(Vector2{ cfgBall.startX, cfgBall.startY }, Vector2{ 0,0 }, cfgBall.radius);
                balls[0].ResetToPaddle(paddle.GetRect().x + paddle.GetRect().width / 2, paddle.GetRect().y);
                currentState = STATE_READY;
            }
        }
        break;
    }

    case Game::STATE_PAUSED:
        if (IsKeyPressed(KEY_P)) currentState = STATE_PLAYING;
        if (IsKeyPressed(KEY_S) && promptState == SavePromptState::NONE) {
            promptState = SavePromptState::SAVE_PROMPT;
            promptTimer = 0.0f;
        }
        break;

    case Game::STATE_VICTORY:
    case Game::STATE_GAME_OVER:
        if (IsKeyPressed(KEY_S) && promptState == SavePromptState::NONE) {
            promptState = SavePromptState::SAVE_PROMPT;
            promptTimer = 0.0f;
        }
        break;
    }
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(cfgWindow.bg);

    // ================== 1. 绘制主界面逻辑 ==================
    if (currentState == STATE_MENU)
    {
        DrawTextEx(font, "打砖块", {(float)(cfgWindow.width/2 - 120), 150}, 64, 2, YELLOW);
        DrawTextEx(font, "按 ENTER 开始游戏", {(float)(cfgWindow.width/2 - 150), 300}, 32, 1, WHITE);
        DrawTextEx(font, "按 ESC 退出游戏", {(float)(cfgWindow.width/2 - 130), 350}, 24, 1, LIGHTGRAY);
        DrawTextEx(font, "按 R 重置所有进度", {(float)(cfgWindow.width/2 - 130), 400}, 24, 1, LIGHTGRAY);
    }
    else if (currentState == STATE_LEVEL_SELECT)
    {
        DrawTextEx(font, "1 - 简单 (3行)", {(float)(cfgWindow.width/2 - 100), 250}, 32, 1, 0<=maxUnlockedLevel ? GREEN : DARKGRAY);
        DrawTextEx(font, "2 - 普通 (4行)", {(float)(cfgWindow.width/2 - 100), 300}, 32, 1, 1<=maxUnlockedLevel ? ORANGE : DARKGRAY);
        DrawTextEx(font, "3 - 困难 (5行)", {(float)(cfgWindow.width/2 - 100), 350}, 32, 1, 2<=maxUnlockedLevel ? RED : DARKGRAY);
        DrawTextEx(font, "4 - 极难 (6行)", {(float)(cfgWindow.width/2 - 100), 400}, 32, 1, 3<=maxUnlockedLevel ? MAROON : DARKGRAY);
        DrawTextEx(font, "按 ESC 返回主菜单", {(float)(cfgWindow.width/2 - 120), 500}, 24, 1, LIGHTGRAY);
    }
    else
    {
        // ================== 2. 绘制游戏场景 (砖块、球等) ==================
        // 只有非菜单/选关状态下才绘制游戏元素
        DrawRectangle(0, 0, cfgWindow.border, cfgWindow.height, GRAY);
        DrawRectangle(cfgWindow.width - cfgWindow.border, 0, cfgWindow.border, cfgWindow.height, GRAY);
        DrawRectangle(0, 0, cfgWindow.width, cfgWindow.border, GRAY);

        brickParticles->Draw();
        powerUpAura->Draw();

        for (auto& b : bricks) b.Draw();
        paddle.Draw();
        for (auto& ball : balls) ball.Draw();
        for (auto& pu : powerUps)
        {
            pu->Draw();
            DrawRectangleLinesEx(pu->GetRect(), 1, MAGENTA);
        }

        // UI信息
        DrawTextEx(font, TextFormat("分数: %d", score), {20, 10}, 24, 1, YELLOW);
        DrawTextEx(font, TextFormat("生命: %d", lives), {650, 10}, 24, 1, lives > 1 ? GREEN : RED);
        DrawTextEx(font, TextFormat("时间: %.1f", gameTime), {20, 40}, 20, 1, LIGHTGRAY);
        DrawTextEx(font, TextFormat("球数量: %d", (int)balls.size()), {20, 70}, 20, 1, SKYBLUE);

        // 状态提示 (Ready, Paused, Victory, Game Over)
        switch (currentState)
        {
        case Game::STATE_READY:
            DrawTextEx(font, "按空格发射", {350, 55}, 20, 1, YELLOW);
            break;
        case Game::STATE_PAUSED:
            DrawRectangle(0, 0, cfgWindow.width, cfgWindow.height, Fade(BLACK, 0.7f));
            DrawTextEx(font, "暂停", {370, 280}, 48, 1, YELLOW);
            DrawTextEx(font, "按P继续", {350, 330}, 24, 1, WHITE);
            break;
        case Game::STATE_VICTORY:
            DrawRectangle(0, 0, cfgWindow.width, cfgWindow.height, Fade(BLACK, 0.85f));
            DrawTextEx(font, "胜利!", {350, 220}, 48, 1, GREEN);
            DrawTextEx(font, TextFormat("%d", score), {370, 280}, 28, 1, YELLOW);
            DrawTextEx(font, "按R重来", {350, 330}, 24, 1, WHITE);
            break;
        case Game::STATE_GAME_OVER:
            DrawRectangle(0,0,cfgWindow.width,cfgWindow.height,Fade(BLACK,0.85f));
            DrawTextEx(font, "游戏结束", {320, 220}, 48, 1, RED);
            DrawTextEx(font, TextFormat("%d", score), {370, 280}, 28, 1, YELLOW);
            DrawTextEx(font, "按R重来", {350, 330}, 24, 1, WHITE);
            break;
        default: break;
        }

        // 调试信息
        DrawTextEx(font, TextFormat("FPS: %d", GetFPS()), {10, 100}, 20, 1, WHITE);
    }

    // ================== 3. 绘制加载遮罩 (如果有) ==================
    {
        std::lock_guard<std::mutex> lock(m_loadMutex);
        if (m_loadState == LoadState::LOADING)
        {
            DrawRectangle(0, 0, cfgWindow.width, cfgWindow.height, Fade(BLACK, 0.5f));
            DrawTextEx(font, "加载中...", {(float)(cfgWindow.width/2 - 60), (float)(cfgWindow.height/2)}, 32, 1, YELLOW);
        }
        if (m_largeTexture.id != 0)
        {
            DrawTexture(m_largeTexture, cfgWindow.width - m_largeTexture.width - 20, 20, WHITE);
        }
    }

    // ================== 4. 【最后】绘制全局弹窗 ==================
    // 放在 EndDrawing 之前，确保覆盖所有背景
    if (promptState != SavePromptState::NONE)
    {
        int w = 500;
        int h = 220;
        int x = (cfgWindow.width - w) / 2;
        int y = (cfgWindow.height - h) / 2;

        // 弹窗背景 (BLACK 是不透明的，会挡住底下的东西)
        DrawRectangle(x, y, w, h, BLACK);
        DrawRectangleLines(x, y, w, h, LIGHTGRAY);

        std::string title, desc;
        if (promptState == SavePromptState::LOAD_PROMPT)
        {
            title = "检测到存档";
            desc = "是否加载上次进度？ Y确认 / N新开";
        }
        else
        {
            title = "保存当前进度";
            desc = "是否保存游戏进度？ Y保存 / N取消";
        }

        Vector2 ts = MeasureTextEx(font, title.c_str(), 42, 2);
        DrawTextEx(font, title.c_str(), { (float)(cfgWindow.width/2 - ts.x/2), (float)y + 50 }, 42, 2, YELLOW);
        Vector2 ds = MeasureTextEx(font, desc.c_str(), 28, 1);
        DrawTextEx(font, desc.c_str(), { (float)(cfgWindow.width/2 - ds.x/2), (float)y + 120 }, 28, 1, WHITE);
    }

    EndDrawing();
}
void Game::Shutdown() {
    UnloadFont(font);
    UnloadSound(sndBrick);
    UnloadSound(sndPowerUp);
    UnloadSound(sndVictory);
    CloseAudioDevice();
    if (m_largeTexture.id != 0) UnloadTexture(m_largeTexture);
    if (m_tempImage.data != nullptr) UnloadImage(m_tempImage);
}

void Game::ResetAllProgress() {
    maxUnlockedLevel = 0;
    score = 0;
    lives = cfgGame.initialLives;
    currentLevelIndex = 0;
    gameTime = 0;
    if (FileExists(saveFilePath.c_str())) {
        std::remove(saveFilePath.c_str());
    }
    ResetGame();
    currentState = STATE_LEVEL_SELECT;
}
