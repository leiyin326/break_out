#include "Game.h"
#include <vector>

Game::Game()
    : ball({ 0,0 }, { 0,0 }, 0),
    paddle(0,0,0,0),  // 👈 这里必须加逗号！
    currentState(Game::STATE_MENU)  // 👈 新增这一行
{
    LoadConfig("config.json");

    // 加载中文字体（完全不动）
    std::vector<int> codepoints;
    for (int i = 0x4E00; i <= 0x9FFF; ++i) {
        codepoints.push_back(i);
    }
    for (int i = 0x0020; i <= 0x007E; ++i) {
        codepoints.push_back(i);
    }

    font = LoadFontEx("fonts/NotoSansSC.otf", 32, codepoints.data(), (int)codepoints.size());

    if (font.texture.id == 0) {
        TraceLog(LOG_ERROR, "中文字体加载失败，使用默认字体");
        font = GetFontDefault();
    }

    ball = Ball({ cfgBall.startX, cfgBall.startY }, { 0,0 }, cfgBall.radius);
    paddle = Paddle(cfgPaddle.startX, cfgPaddle.startY, cfgPaddle.width, cfgPaddle.height);
}
void Game::LoadConfig(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        TraceLog(LOG_ERROR, "配置文件加载失败，使用默认值");
        cfgWindow = { 800,600,5, {30,30,40,255}, 50, 50 };
        cfgBall = { 10, 400,530 };
        cfgPaddle = { 120,15, 340,550, 18,28 };
        cfgBricks = {5,8, 85,25, 50,80,95,35};
        cfgGame = {3,10,5.0f,0.05f,1.0f};
        return;
    }

    json j;
    f >> j;

    // window
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

    // ball
    cfgBall.radius = j["ball"]["radius"];
    cfgBall.startX = j["ball"]["start_x"];
    cfgBall.startY = j["ball"]["start_y"];

    // paddle
    cfgPaddle.width = j["paddle"]["width"];
    cfgPaddle.height = j["paddle"]["height"];
    cfgPaddle.startX = j["paddle"]["start_x"];
    cfgPaddle.startY = j["paddle"]["start_y"];
    cfgPaddle.speedNormal = j["paddle"]["speed_normal"];
    cfgPaddle.speedBoost = j["paddle"]["speed_boost"];

    // bricks
    cfgBricks.rows = j["bricks"]["rows"];
    cfgBricks.cols = j["bricks"]["cols"];
    cfgBricks.width = j["bricks"]["width"];
    cfgBricks.height = j["bricks"]["height"];
    cfgBricks.startX = j["bricks"]["start_x"];
    cfgBricks.startY = j["bricks"]["start_y"];
    cfgBricks.spacingX = j["bricks"]["spacing_x"];
    cfgBricks.spacingY = j["bricks"]["spacing_y"];

    // game
    cfgGame.initialLives = j["game"]["initial_lives"];
    cfgGame.baseScorePerBrick = j["game"]["base_score_per_brick"];
    cfgGame.timeMultBase = j["game"]["time_multiplier_base"];
    cfgGame.timeMultDecay = j["game"]["time_multiplier_decay"];
    cfgGame.timeMultMin = j["game"]["time_multiplier_min"];
}

void Game::Init() {
    SetTargetFPS(60);
    CreateBricks();
    ResetGame();
}

// ==============================================
// ✅ 已改好：随机生成金色砖块（每次开局不一样）
// ==============================================
void Game::CreateBricks() {
    bricks.clear();
    Color cs[] = { RED,ORANGE,YELLOW,GREEN,BLUE };

    // 1. 先创建所有普通砖块，必须传 golden=false
    for (int r = 0; r < cfgBricks.rows; r++) {
        for (int c = 0; c < cfgBricks.cols; c++) {
            bricks.emplace_back(
                cfgBricks.startX + c * cfgBricks.spacingX,
                cfgBricks.startY + r * cfgBricks.spacingY,
                cfgBricks.width,
                cfgBricks.height,
                cs[r],
                false   // 必须加这个！标记为普通砖块
            );
        }
    }

    // 2. 随机选 4~6 个砖块，改成金色
    int totalBricks = bricks.size();
    int goldenCount = GetRandomValue(4, 6); // 每次随机4-6个金色砖块

    for (int i = 0; i < goldenCount; i++) {
        int idx;
        // 避免重复选同一个金色砖块
        do {
            idx = GetRandomValue(0, totalBricks - 1);
        } while (bricks[idx].IsGolden());

        // 把选中的砖块替换为金色
        bricks[idx] = Brick(
            bricks[idx].GetRect().x,
            bricks[idx].GetRect().y,
            bricks[idx].GetRect().width,
            bricks[idx].GetRect().height,
            GOLD,   // 金色
            true    // 标记为金色砖块
        );
    }

    winCount = bricks.size();
}

void Game::ResetGame() {
    score = 0;
    lives = cfgGame.initialLives;
    gameTime = 0;
    currentState = STATE_READY;
    CreateBricks();
    ball.ResetToPaddle(paddle.GetRect().x + paddle.GetRect().width / 2, paddle.GetRect().y);
}

void Game::Update() {
    if ((currentState == STATE_VICTORY || currentState == STATE_GAME_OVER) && IsKeyPressed(KEY_R)) {
    ResetGame();
    currentState = STATE_MENU; // 👈 强制切回主菜单！
    return;
}

    switch (currentState) {
    case Game::STATE_MENU: {
        if (IsKeyPressed(KEY_ENTER)) {
            currentState = Game::STATE_LEVEL_SELECT;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            CloseWindow();
        }
        break;
    }

    case Game::STATE_LEVEL_SELECT: {
        if (IsKeyPressed(KEY_ONE)) {
            cfgBricks.rows = 3;
            cfgBricks.cols = 6;
            currentState = Game::STATE_READY;
            ResetGame();
        }
        if (IsKeyPressed(KEY_TWO)) {
            cfgBricks.rows = 4;
            cfgBricks.cols = 7;
            currentState = Game::STATE_READY;
            ResetGame();
        }
        if (IsKeyPressed(KEY_THREE)) {
            cfgBricks.rows = 5;
            cfgBricks.cols = 8;
            currentState = Game::STATE_READY;
            ResetGame();
        }
        if (IsKeyPressed(KEY_FOUR)) {
            cfgBricks.rows = 6;
            cfgBricks.cols = 9;
            currentState = Game::STATE_READY;
            ResetGame();
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            currentState = Game::STATE_MENU;
        }
        break;
    }

    case Game::STATE_READY: {
        float sp = IsKeyDown(KEY_LEFT_SHIFT) ? cfgPaddle.speedBoost : cfgPaddle.speedNormal;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) paddle.MoveLeft(sp);
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) paddle.MoveRight(sp);

        if (IsKeyPressed(KEY_SPACE)) {
            ball.Launch(paddle.GetRect().x + paddle.GetRect().width / 2, paddle.GetRect().width);
            currentState = Game::STATE_PLAYING;
        }
        break;
    }

    case Game::STATE_PLAYING: {
        if (IsKeyPressed(KEY_ESCAPE)) {
        currentState = STATE_MENU;
        break;
        }

    if (IsKeyPressed(KEY_P)) {
        currentState = STATE_PAUSED;
        break;
        }
        if (IsKeyPressed(KEY_P)) {
            currentState = Game::STATE_PAUSED;
            break;
        }

        gameTime += GetFrameTime();
        float sp = IsKeyDown(KEY_LEFT_SHIFT) ? cfgPaddle.speedBoost : cfgPaddle.speedNormal;
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) paddle.MoveLeft(sp);
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) paddle.MoveRight(sp);

        ball.ApplyGravity();
        ball.Move();
        ball.BounceEdge(cfgWindow.width, cfgWindow.height);
        ball.BouncePaddle(paddle.GetRect());

        for (auto& b : bricks) {
            if (b.IsActive() && ball.CheckBrickCollision(b.GetRect())) {
                b.SetActive(false);
                float mul = cfgGame.timeMultBase - gameTime * cfgGame.timeMultDecay;
                if (mul < cfgGame.timeMultMin) mul = cfgGame.timeMultMin;

                float scoreMulti = b.GetScoreMultiplier();
                score += (int)(cfgGame.baseScorePerBrick * mul * scoreMulti);

                winCount--;
                break;
            }
        }

        if (winCount <= 0) {
            currentState = Game::STATE_VICTORY;
            break;
        }

        if (ball.GetPosition().y > cfgWindow.height + cfgWindow.fallOffset) {
            lives--;
            score -= cfgWindow.scorePenalty;
            if (score < 0) score = 0;

            if (lives <= 0) {
                currentState = Game::STATE_GAME_OVER;
            } else {
                ball.ResetToPaddle(paddle.GetRect().x + paddle.GetRect().width / 2, paddle.GetRect().y);
                currentState = Game::STATE_READY;
            }
        }
        break;
    }

    case Game::STATE_PAUSED:
        if (IsKeyPressed(KEY_P))
            currentState = Game::STATE_PLAYING;
        break;

    case Game::STATE_VICTORY:
    case Game::STATE_GAME_OVER:
        break;
    }
}
// ======================
// ✅ 全部使用 DrawTextEx 显示中文
// ======================
void Game::Draw() {
    BeginDrawing();
    ClearBackground(cfgWindow.bg);

    if (currentState == Game::STATE_MENU) {
        DrawTextEx(font, "打砖块", {(float)(cfgWindow.width/2 - 120), 150}, 64, 2, YELLOW);
        DrawTextEx(font, "按 ENTER 开始游戏", {(float)(cfgWindow.width/2 - 150), 300}, 32, 1, WHITE);
        DrawTextEx(font, "按 ESC 退出游戏", {(float)(cfgWindow.width/2 - 130), 350}, 24, 1, LIGHTGRAY);
        EndDrawing();
        return;
    }

    if (currentState == Game::STATE_LEVEL_SELECT) {
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

    for (auto& b : bricks) b.Draw();
    paddle.Draw();
    ball.Draw();

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
        DrawRectangle(0, 0, cfgWindow.width, cfgWindow.height, Fade(BLACK, 0.85f));
        DrawTextEx(font, "游戏结束", {320, 220}, 48, 1, RED);
        DrawTextEx(font, TextFormat("%d", score), {370, 280}, 28, 1, YELLOW);
        DrawTextEx(font, "按R重来", {350, 330}, 24, 1, WHITE);
        break;
    default:
        break;
    }

    EndDrawing();
}
void Game::Shutdown() {
    UnloadFont(font); // ✅ 安全卸载字体
}