#include "Game.h"
#include "PowerUp.h"
#include "Particle.h"
#include <vector>
#include <random>
#include <algorithm>

Game::Game()
    : ball({ 0,0 }, { 0,0 }, 0),
    paddle(0,0,0,0),
    currentState(Game::STATE_MENU),
    audioLoaded(false)
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

    brickParticles = std::make_unique<ParticleSystem>(150);
    powerUpAura = std::make_unique<ParticleSystem>(80);

    SetRandomSeed((unsigned int)time(nullptr));

    ball = Ball({ cfgBall.startX, cfgBall.startY }, { 0,0 }, cfgBall.radius);
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
    // 只加载一次！不要在 Update 里重复 LoadSound
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
    ball.ResetToPaddle(paddle.GetRect().x + paddle.GetRect().width / 2, paddle.GetRect().y);
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
            ball.Launch(paddle.GetRect().x + paddle.GetRect().width/2, paddle.GetRect().width);
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

        ball.ApplyGravity();
        ball.Move();
        ball.BounceEdge(cfgWindow.width, cfgWindow.height);
        ball.BouncePaddle(paddle.GetRect());

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

        if (ball.GetPosition().y > cfgWindow.height + cfgWindow.fallOffset) {
            lives--;
            score -= cfgWindow.scorePenalty;
            if (score < 0) score = 0;
            paddle.SetWidth(cfgPaddle.width);
            powerUps.clear();

            if (lives <= 0) currentState = STATE_GAME_OVER;
            else {
                ball.ResetToPaddle(paddle.GetRect().x + paddle.GetRect().width/2, paddle.GetRect().y);
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
    ball.Draw();

    for (auto& pu : powerUps) {
        pu->Draw();
        DrawRectangleLinesEx(pu->GetRect(), 1, MAGENTA);
    }

    DrawTextEx(font, TextFormat("分数: %d", score), {20, 10}, 24, 1, YELLOW);
    DrawTextEx(font, TextFormat("生命: %d", lives), {650, 10}, 24, 1, lives > 1 ? GREEN : RED);
    DrawTextEx(font, TextFormat("时间: %.1f", gameTime), {20, 40}, 20, 1, LIGHTGRAY);

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

    EndDrawing();
}

void Game::Shutdown() {
    UnloadFont(font);

    UnloadSound(sndBrick);
    UnloadSound(sndPowerUp);
    UnloadSound(sndVictory);
    CloseAudioDevice();
}