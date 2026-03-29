#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

struct ScoreEntry {
    char name[32];
    int score;
    time_t timestamp;
};

class Leaderboard {
private:
    static const int MAX_ENTRIES = 10;
    ScoreEntry entries[MAX_ENTRIES];
    int count;
    const char* filename;
    
public:
    Leaderboard(const char* file) : count(0), filename(file) { Load(); }
    
    void Load() {
        FILE* f = fopen(filename, "r");
        if (f) {
            count = 0;
            while (count < MAX_ENTRIES && fscanf(f, "%31s %d %ld", entries[count].name, &entries[count].score, &entries[count].timestamp) == 3) {
                count++;
            }
            fclose(f);
        }
    }
    
    void Save() {
        FILE* f = fopen(filename, "w");
        if (f) {
            for (int i = 0; i < count; i++) fprintf(f, "%s %d %ld\n", entries[i].name, entries[i].score, entries[i].timestamp);
            fclose(f);
        }
    }
    
    int AddScore(const char* name, int score) {
        if (count >= MAX_ENTRIES && score <= entries[count - 1].score) return 0;
        ScoreEntry newEntry;
        strncpy(newEntry.name, name, 31); newEntry.name[31] = '\0';
        newEntry.score = score; newEntry.timestamp = time(nullptr);
        int pos = 0;
        while (pos < count && entries[pos].score >= score) pos++;
        if (count < MAX_ENTRIES) count++;
        for (int i = count - 1; i > pos; i--) entries[i] = entries[i - 1];
        entries[pos] = newEntry;
        Save();
        return pos + 1;
    }
    
    bool GetEntry(int rank, ScoreEntry& entry) { if (rank > 0 && rank <= count) { entry = entries[rank - 1]; return true; } return false; }
    int GetCount() { return count; }
    bool CanEnter(int score) { return count < MAX_ENTRIES || score > entries[count - 1].score; }
};

static Font chineseFont;
static bool fontLoaded = false;

void InitChineseFont() {
     const char* text = "分数生命暂停继续重新开始游戏结束胜利排行榜第名按P暂停按R重新开始时间倍率落地惩罚恭喜进入空格发射等待按空格发射P-暂停 R-重开 L-排行按 P 继续胜利!游戏结束恭喜进入排行榜第名!按 R 重新开始排行榜暂无记录按 L 关闭排行榜0123456789：！，.";
    int codepointCount = 0;
    int* codepoints = LoadCodepoints(text, &codepointCount);
   const char* fontPaths[] = {
        "../fonts/NotoSansSC.otf",      // ✅ 优先你的思源宋体（OTF更稳定）
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",  // Linux备用
        "/mnt/c/Windows/Fonts/msyh.ttc",                    // WSL备用
        "/mnt/c/Windows/Fonts/simhei.ttf"                   // 补充黑体（TTF更稳定）
    };
    for (int i = 0; i < 4; i++) {
        if (FileExists(fontPaths[i])) {
            printf("  找到字体文件: %s\n", fontPaths[i]);
            chineseFont = LoadFontEx(fontPaths[i], 48, codepoints, codepointCount);
            if (chineseFont.texture.id != 0) { printf("  字体加载成功！\n"); fontLoaded = true; break; }
        }
    }
    if (!fontLoaded) { printf("警告: 字体加载失败，使用默认字体\n"); chineseFont = GetFontDefault(); }
    UnloadCodepoints(codepoints);
}

void DrawChineseText(const char* text, int x, int y, int fontSize, Color color) { Vector2 pos = { (float)x, (float)y }; DrawTextEx(chineseFont, text, pos, (float)fontSize, 2, color); }
void DrawChineseTextCentered(const char* text, int y, int fontSize, Color color) { Vector2 size = MeasureTextEx(chineseFont, text, (float)fontSize, 2); DrawChineseText(text, (GetScreenWidth() - (int)size.x) / 2, y, fontSize, color); }

// 修改得分计算函数，新增brickMultiplier参数（适配金色砖块）
int CalculateScore(int baseScore, float gameTime, float brickMultiplier = 1.0f) {
    float multiplier = 5.0f - gameTime * 0.05f;
    if (multiplier < 1.0f) multiplier = 1.0f;
    return (int)(baseScore * multiplier * brickMultiplier);
}

int main() {
    const int screenWidth = 800, screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Breakout - Enhanced Edition");
    InitChineseFont();
    Leaderboard leaderboard("scores.txt");
    Ball ball({400.0f, 530.0f}, {0.0f, 0.0f}, 10.0f);
    Paddle paddle(340.0f, 550.0f, 120.0f, 15.0f);
    
    std::vector<Brick> bricks;
    Color brickColors[] = {RED, ORANGE, YELLOW, GREEN, BLUE};
    // 初始化随机种子（确保金色砖块随机生成）
    srand((unsigned int)time(nullptr));
    int totalBricks = 5 * 8;  // 总砖块数：5行×8列
    int goldenBrickCount = 0;
    const float goldenProbability = 0.1f;  // 10%概率生成金色砖块
    
    // 初始化砖块（含金色砖块）
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 8; col++) {
            // 随机判断是否为金色砖块（控制总数不超过10%）
            bool isGolden = (rand() % 100 < 10) && (goldenBrickCount < totalBricks * goldenProbability);
            if (isGolden) goldenBrickCount++;
            
            bricks.emplace_back(
                50 + col * 95, 
                80 + row * 35, 
                85, 25, 
                brickColors[row],
                isGolden  // 传递金色砖块标记
            );
        }
    }
    
    int score = 0, lives = 3, winCount = (int)bricks.size(), playerRank = 0;
    bool gameOver = false, paused = false, victory = false, showLeaderboard = false;
    float gameTime = 0.0f;
    
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_P) && !gameOver) paused = !paused;
        if (IsKeyPressed(KEY_R)) {
            ball.ResetToPaddle(paddle.GetRect().x + paddle.GetRect().width / 2, paddle.GetRect().y);
            score = 0; lives = 3; gameOver = false; victory = false; paused = false; showLeaderboard = false; gameTime = 0.0f;
            // 重置砖块（重新生成金色砖块）
            bricks.clear();
            goldenBrickCount = 0;
            for (int row = 0; row < 5; row++) {
                for (int col = 0; col < 8; col++) {
                    bool isGolden = (rand() % 100 < 10) && (goldenBrickCount < totalBricks * goldenProbability);
                    if (isGolden) goldenBrickCount++;
                    bricks.emplace_back(50 + col * 95, 80 + row * 35, 85, 25, brickColors[row], isGolden);
                }
            }
            winCount = (int)bricks.size();
        }
        if (IsKeyPressed(KEY_L)) showLeaderboard = !showLeaderboard;
        
        if (!gameOver && !paused) {
            if (ball.IsLaunched()) gameTime += GetFrameTime();
            float currentSpeed = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 28.0f : 18.0f;
            if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) paddle.MoveLeft(currentSpeed);
            if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) paddle.MoveRight(currentSpeed);
            
            if (!ball.IsLaunched()) {
                ball.ResetToPaddle(paddle.GetRect().x + paddle.GetRect().width / 2, paddle.GetRect().y);
                if (IsKeyPressed(KEY_SPACE)) ball.Launch(paddle.GetRect().x + paddle.GetRect().width / 2, paddle.GetRect().width);
            }
            
            ball.ApplyGravity(); ball.Move(); ball.BounceEdge(screenWidth, screenHeight); ball.BouncePaddle(paddle.GetRect());
            // 碰撞检测：适配金色砖块得分
            for (auto& brick : bricks) {
                if (brick.IsActive() && ball.CheckBrickCollision(brick.GetRect())) {
                    brick.SetActive(false); 
                    // 传入砖块倍率（金色1.5倍）
                    score += CalculateScore(10, gameTime, brick.GetScoreMultiplier()); 
                    winCount--; 
                    break;
                }
            }
            if (winCount <= 0) { gameOver = true; victory = true; if (leaderboard.CanEnter(score)) playerRank = leaderboard.AddScore("Player", score); }
            if (ball.GetPosition().y > screenHeight + 50) {
                lives--; score -= 50; if (score < 0) score = 0;
                if (lives <= 0) { gameOver = true; victory = false; if (leaderboard.CanEnter(score)) playerRank = leaderboard.AddScore("Player", score); }
                else ball.ResetToPaddle(paddle.GetRect().x + paddle.GetRect().width / 2, paddle.GetRect().y);
            }
        }
        
        BeginDrawing();
        ClearBackground(Color{30, 30, 40, 255});
        DrawRectangle(0, 0, 5, screenHeight, GRAY); DrawRectangle(screenWidth - 5, 0, 5, screenHeight, GRAY); DrawRectangle(0, 0, screenWidth, 5, GRAY);
        for (auto& brick : bricks) brick.Draw();
        paddle.Draw(); ball.Draw();
        
        DrawChineseText("分数:", 20, 8, 24, WHITE); DrawText(TextFormat("%d", score), 80, 10, 24, YELLOW);
        DrawChineseText("生命:", 650, 8, 24, WHITE); DrawText(TextFormat("%d", lives), 710, 10, 24, lives > 1 ? GREEN : RED);
        DrawChineseText("时间:", 20, 35, 20, Fade(WHITE, 0.8f)); DrawText(TextFormat("%.1f秒", gameTime), 75, 37, 20, Fade(WHITE, 0.8f));
        float currentMultiplier = 5.0f - gameTime * 0.05f; if (currentMultiplier < 1.0f) currentMultiplier = 1.0f;
        DrawText(TextFormat("x%.1f", currentMultiplier), 140, 37, 20, currentMultiplier > 2.0f ? GREEN : Fade(WHITE, 0.5f));
        if (!ball.IsLaunched()) DrawChineseTextCentered("按空格发射", 55, 20, YELLOW);
        DrawChineseText("P-暂停 R-重开 L-排行", 280, 12, 18, Fade(WHITE, 0.6f));
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) DrawText(">>> BOOST <<<", 350, 575, 18, YELLOW);
        
        if (paused && !gameOver) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.7f));
            DrawChineseTextCentered("暂停", screenHeight/2 - 40, 48, YELLOW); DrawChineseTextCentered("按 P 继续", screenHeight/2 + 30, 24, WHITE);
        }
        
        if (gameOver) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.85f));
            if (victory) { DrawChineseTextCentered("胜利!", screenHeight/2 - 80, 48, GREEN); DrawText(TextFormat("FINAL SCORE: %d", score), screenWidth/2 - 100, screenHeight/2 - 30, 28, YELLOW); }
            else { DrawChineseTextCentered("游戏结束", screenHeight/2 - 80, 48, RED); DrawText(TextFormat("SCORE: %d", score), screenWidth/2 - 60, screenHeight/2 - 30, 28, YELLOW); }
            if (playerRank > 0) { char rankText[64]; sprintf(rankText, "恭喜进入排行榜第%d名!", playerRank); DrawChineseTextCentered(rankText, screenHeight/2 + 40, 24, GOLD); }
            DrawChineseTextCentered("按 R 重新开始", screenHeight/2 + 90, 24, WHITE);
        }
        
        if (showLeaderboard) {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.9f)); DrawChineseTextCentered("排行榜", 40, 36, GOLD);
            DrawText("RANK", 150, 90, 20, Fade(WHITE, 0.6f)); DrawText("NAME", 250, 90, 20, Fade(WHITE, 0.6f)); DrawText("SCORE", 450, 90, 20, Fade(WHITE, 0.6f)); DrawText("DATE", 550, 90, 20, Fade(WHITE, 0.6f));
            DrawLine(150, 115, 700, 115, Fade(WHITE, 0.3f));
            for (int i = 0; i < leaderboard.GetCount() && i < 10; i++) {
                ScoreEntry entry; if (leaderboard.GetEntry(i + 1, entry)) {
                    int y = 130 + i * 35; Color rowColor = (i == 0) ? GOLD : (i == 1) ? Color{192,192,192,255} : (i == 2) ? Color{205,127,50,255} : WHITE;
                    DrawText(TextFormat("#%d", i + 1), 150, y, 22, rowColor); DrawText(entry.name, 250, y, 22, rowColor); DrawText(TextFormat("%d", entry.score), 450, y, 22, rowColor);
                    char dateStr[32]; strftime(dateStr, sizeof(dateStr), "%m/%d", localtime(&entry.timestamp)); DrawText(dateStr, 550, y, 20, Fade(rowColor, 0.7f));
                }
            }
            if (leaderboard.GetCount() == 0) DrawChineseTextCentered("暂无记录", screenHeight/2, 24, Fade(WHITE, 0.5f));
            DrawChineseTextCentered("按 L 关闭排行榜", screenHeight - 50, 20, Fade(WHITE, 0.5f));
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}