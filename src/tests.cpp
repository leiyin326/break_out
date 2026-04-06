#include "Ball.h"
#include "Brick.h"
#include <cassert>

void TestBallBrickCollision() {
    Ball ball(100, 100, 0, 0, 10, RED, true, 1);
    Brick brick(90, 90, 20, 20, RED, false);
    
    // 测试碰撞检测
    assert(CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), brick.GetRect()));
    brick.SetActive(false);
    assert(!brick.IsActive());
}

int main() {
    TestBallBrickCollision();
    return 0;
}