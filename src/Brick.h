#ifndef BRICK_H
#define BRICK_H
#include "raylib.h"

class Brick {
private:
    Rectangle rect;
    bool active;
    Color color;
    bool isGolden;  // 新增：标记是否为金色砖块

public:
    // 修改构造函数，新增 isGolden 参数（默认值 false 保证兼容原有代码）
    Brick(float x, float y, float width, float height, Color c, bool golden = false);
    void Draw();
    bool IsActive() { return active; }
    void SetActive(bool a) { active = a; }
    Rectangle GetRect() { return rect; }
    bool IsGolden() { return isGolden; }  // 新增：获取是否为金色砖块
    float GetScoreMultiplier() { return isGolden ? 1.5f : 1.0f; }  // 新增：得分倍率
    Color GetColor() { return color; }
    // 新增：SetColor 方法声明（核心修复点）
    void SetColor(Color newColor);
    void SetGold(bool g);
};

#endif