#include "Game.h"
#include "PowerUp.h"
#include "Particle.h"
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

Game::Game()
    : paddle(0,0,0,0),
    currentState(Game::STATE_MENU),
    audioLoaded(false),
    // 新增：初始化异步加载纹理
    m_largeTexture({0}),
    m_tempImage({0})
{
    LoadConfig("config.json");

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
        cfgBricks = {5,8, 85,25, 50,80,95,35};
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

    cfgBricks.rows = j["bricks"]["rows"];
    cfgBricks.cols = j["bricks"]["cols"];
    cfgBricks.width = j["bricks"]["width"];
    cfgBricks.height = j["bricks"]["height"];
    cfgBricks.startX = j["bricks"]["start_x"];
    cfgBricks.startY = j["bricks"]["start_y"];
    cfgBricks.spacingX = j["bricks"]["spacing_x"];
    cfgBricks.spacingY = j["bricks"]["spacing_y"];

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
    Color cs[] = { RED,ORANGE,YELLOW,GREEN,BLUE };

    for (int r = 0; r < cfgBricks.rows; r++) {
        for (int c = 0; c < cfgBricks.cols; c++) {
            bricks.emplace_back(
                cfgBricks.startX + c * cfgBricks.spacingX,
                cfgBricks.startY + r * cfgBricks.spacingY,
                cfgBricks.width,
                cfgBricks.height,
                cs[r],
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
    score = 0;
    lives = cfgGame.initialLives;
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

    // 新增：重置异步加载状态
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

            // 👇 多球道具：生成2个限时8秒的球
            if (pu->GetType() == PowerUpType::MULTI_BALL) {
                if (balls.empty()) return;
                Ball& main = balls[0];
                for (int i = 0; i < 2; i++) {
                    Ball b = main;
                    b.SetSpeed(Vector2{
                        main.GetSpeed().x + GetRandomValue(-70,70),
                        main.GetSpeed().y + GetRandomValue(-40,-60)
                    });
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

// 新增：异步加载大纹理的核心函数
void Game::LoadLargeTextureAsync() {
    // 加锁修改状态，防止数据竞争
    std::lock_guard<std::mutex> lock(m_loadMutex);
    if (m_loadState != LoadState::IDLE) return;

    m_loadState = LoadState::LOADING;
    m_loadingSuccess = false;

    // 用 async 启动后台线程，模拟耗时加载（sleep 代替实际纹理加载）
    m_loadFuture = std::async(std::launch::async, [this]() {
        // 模拟加载耗时（2秒），真实场景可替换为加载大图片/资源
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 加分项：真实图片加载（后台线程加载Image，主线程转Texture2D）
        // 注意：需要在项目根目录放一张名为 large_texture.png 的图片
        if (FileExists("large_texture.png")) {
            m_tempImage = LoadImage("large_texture.png");
        }

        // 加载完成后，修改状态（加锁保护共享变量）
        std::lock_guard<std::mutex> lock(m_loadMutex);
        m_loadState = LoadState::DONE;
        m_loadingSuccess = true;
    });
}

void Game::Update() {
    if ((currentState == STATE_VICTORY || currentState == STATE_GAME_OVER) && IsKeyPressed(KEY_R)) {
        ResetGame();
        currentState = STATE_MENU;
        return;
    }

    float dt = GetFrameTime();

    if (currentState == STATE_PLAYING && IsKeyPressed(KEY_K)) {
        powerUps.clear();
        paddle.SetWidth(cfgPaddle.width);
        TraceLog(LOG_INFO, "DEBUG: 所有道具已清除");
    }

    // ==============================================
    // 新增：异步加载逻辑（按下L键触发）
    // ==============================================
    if (currentState == STATE_PLAYING && IsKeyPressed(KEY_L)) {
        LoadLargeTextureAsync();
        TraceLog(LOG_INFO, "开始异步加载大纹理...");
    }

    // 新增：非阻塞检查异步加载状态
    {
        std::lock_guard<std::mutex> lock(m_loadMutex);
        if (m_loadState == LoadState::LOADING) {
            // 检查future是否完成（非阻塞）
            auto status = m_loadFuture.wait_for(std::chrono::seconds(0));
            if (status == std::future_status::ready) {
                m_loadFuture.get(); // 清理线程资源，必须调用
            }
        }

        // 加载完成后处理：主线程转Texture2D + 修改砖块颜色
        if (m_loadState == LoadState::DONE && m_loadingSuccess) {
            // 加分项：将后台加载的Image转为Texture2D（必须在主线程）
            if (m_tempImage.data != nullptr && m_largeTexture.id == 0) {
                m_largeTexture = LoadTextureFromImage(m_tempImage);
                UnloadImage(m_tempImage);
                m_tempImage = {0};
                TraceLog(LOG_INFO, "大纹理加载完成！");
            }

            // 视觉反馈：所有砖块变为绿色，标记加载完成
            for (auto& brick : bricks) {
                if (brick.IsActive()) brick.SetColor(GREEN);
            }
            m_loadingSuccess = false; // 避免重复修改
        }
    }

    // ==============================================
    // 多球更新：倒计时 + 自动删除过期球
    // ==============================================
    for (auto& ball : balls) {
        if (ball.GetLifeTime() > 0) {
            ball.SetLifeTime(ball.GetLifeTime() - dt);
        }
    }

    auto it = balls.begin();
    while (it != balls.end()) {
        if (it->GetLifeTime() <= 0 && it->GetLifeTime() != -1) {
            it = balls.erase(it);
        } else {
            ++it;
        }
    }

    // ==============================================
    // 球与球碰撞物理
    // ==============================================
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

    switch (currentState) {
    case Game::STATE_MENU:
        if (IsKeyPressed(KEY_ENTER)) currentState = Game::STATE_LEVEL_SELECT;
        if (IsKeyPressed(KEY_ESCAPE)) CloseWindow();
        break;

    case Game::STATE_LEVEL_SELECT:
        if (IsKeyPressed(KEY_ONE)) { cfgBricks.rows=3; cfgBricks.cols=6; ResetGame(); currentState=STATE_READY; }
        if (IsKeyPressed(KEY_TWO)) { cfgBricks.rows=4; cfgBricks.cols=7; ResetGame(); currentState=STATE_READY; }
        if (IsKeyPressed(KEY_THREE)) { cfgBricks.rows=5; cfgBricks.cols=8; ResetGame(); currentState=STATE_READY; }
        if (IsKeyPressed(KEY_FOUR)) { cfgBricks.rows=6; cfgBricks.cols=9; ResetGame(); currentState=STATE_READY; }
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

        // 更新所有球
        for (auto& ball : balls) {
            ball.ApplyGravity();
            ball.Move();
            ball.BounceEdge(cfgWindow.width, cfgWindow.height);
            ball.BouncePaddle(paddle.GetRect());
        }

        // 所有球碰撞砖块
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

        if (winCount <= 0) {
            currentState = STATE_VICTORY;
            StopSound(sndVictory);
            PlaySound(sndVictory);
            break;
        }

        // ==============================================
        // 生命规则：所有球都掉下去才减命
        // ==============================================
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
                balls[0].ResetToPaddle(paddle.GetRect().x + paddle.GetRect().width/2, paddle.GetRect().y);
                currentState = STATE_READY;
            }
        }

        break;
    }

    case Game::STATE_PAUSED:
        if (IsKeyPressed(KEY_P)) currentState = STATE_PLAYING;
        break;

    case Game::STATE_VICTORY:
    case Game::STATE_GAME_OVER:
        break;
    }
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(cfgWindow.bg);

    if (currentState == STATE_MENU) {
        DrawTextEx(font, "打砖块", {(float)(cfgWindow.width/2 - 120), 150}, 64, 2, YELLOW);
        DrawTextEx(font, "按 ENTER 开始游戏", {(float)(cfgWindow.width/2 - 150), 300}, 32, 1, WHITE);
        DrawTextEx(font, "按 ESC 退出游戏", {(float)(cfgWindow.width/2 - 130), 350}, 24, 1, LIGHTGRAY);
        EndDrawing();
        return;
    }

    if (currentState == STATE_LEVEL_SELECT) {
        DrawTextEx(font, "选择关卡", {(float)(cfgWindow.width/2 - 80), 150}, 48, 2, YELLOW);
        DrawTextEx(font, "1 - 简单 (3行)", {(float)(cfgWindow.width/2 - 100), 250}, 32, 1, GREEN);
        DrawTextEx(font, "2 - 普通 (4行)", {(float)(cfgWindow.width/2 - 100), 300}, 32, 1, ORANGE);
        DrawTextEx(font, "3 - 困难 (5行)", {(float)(cfgWindow.width/2 - 100), 350}, 32, 1, RED);
        DrawTextEx(font, "4 - 极难 (6行)", {(float)(cfgWindow.width/2 - 100), 400}, 32, 1, MAROON);
        DrawTextEx(font, "按 ESC 返回主菜单", {(float)(cfgWindow.width/2 - 120), 500}, 24, 1, LIGHTGRAY);
        EndDrawing();
        return;
    }

    DrawRectangle(0, 0, cfgWindow.border, cfgWindow.height, GRAY);
    DrawRectangle(cfgWindow.width - cfgWindow.border, 0, cfgWindow.border, cfgWindow.height, GRAY);
    DrawRectangle(0, 0, cfgWindow.width, cfgWindow.border, GRAY);

    brickParticles->Draw();
    powerUpAura->Draw();

    for (auto& b : bricks) b.Draw();
    paddle.Draw();

    // 绘制所有球
    for (auto& ball : balls) {
        ball.Draw();
    }

    for (auto& pu : powerUps) {
        pu->Draw();
        DrawRectangleLinesEx(pu->GetRect(), 1, MAGENTA);
    }

    DrawTextEx(font, TextFormat("分数: %d", score), {20, 10}, 24, 1, YELLOW);
    DrawTextEx(font, TextFormat("生命: %d", lives), {650, 10}, 24, 1, lives > 1 ? GREEN : RED);
    DrawTextEx(font, TextFormat("时间: %.1f", gameTime), {20, 40}, 20, 1, LIGHTGRAY);
    DrawTextEx(font, TextFormat("球数量: %d", (int)balls.size()), {20, 70}, 20, 1, SKYBLUE);

    // 新增：绘制加载中提示
    {
        std::lock_guard<std::mutex> lock(m_loadMutex);
        if (m_loadState == LoadState::LOADING) {
            DrawRectangle(0, 0, cfgWindow.width, cfgWindow.height, Fade(BLACK, 0.5f));
            DrawTextEx(font, "加载中...", {(float)(cfgWindow.width/2 - 60), (float)(cfgWindow.height/2)}, 32, 1, YELLOW);
        }

        // 加分项：绘制加载完成的大纹理（右上角）
        if (m_largeTexture.id != 0) {
            DrawTexture(m_largeTexture, cfgWindow.width - m_largeTexture.width - 20, 20, WHITE);
        }
    }

    switch (currentState) {
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
    DrawTextEx(font, TextFormat("FPS: %d", GetFPS()), {10, 100}, 20, 1, WHITE);
    EndDrawing();
}

void Game::Shutdown() {
    UnloadFont(font);
    UnloadSound(sndBrick);
    UnloadSound(sndPowerUp);
    UnloadSound(sndVictory);
    CloseAudioDevice();

    // 新增：释放异步加载的纹理
    if (m_largeTexture.id != 0) {
        UnloadTexture(m_largeTexture);
    }
    if (m_tempImage.data != nullptr) {
        UnloadImage(m_tempImage);
    }
}