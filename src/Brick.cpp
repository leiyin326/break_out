#include "Brick.h"
#include <raylib.h>

// 修改构造函数，初始化金色砖块标记，金色砖块强制设为金色
Brick::Brick(float x, float y, float width, float height, Color c, bool golden) {
    rect = { x, y, width, height };
    active = true;
    isGolden = golden;
    // 金色砖块用金色，普通砖块用原有颜色
    color = isGolden ? GOLD : c;
}

void Brick::Draw() {
    if (active) {
        DrawRectangleRec(rect, color);
        // 金色砖块添加特殊边框+闪烁效果，普通砖块保持原有样式
        if (isGolden) {
            DrawRectangleLinesEx(rect, 3, YELLOW);  // 粗边框
            // 闪烁效果：每隔0.5秒切换边框透明度
            if ((int)(GetTime() * 2) % 2 == 0) {
                DrawRectangleLinesEx(rect, 2, Fade(YELLOW, 0.5f));
            }
        } else {
            DrawRectangleLinesEx(rect, 1, WHITE);
        }
    }
}
