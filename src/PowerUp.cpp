#include "PowerUp.h"
#include "Game.h"
#include "Paddle.h"
#include <random>

PowerUpEffect::PowerUpEffect(PowerUpType t, Rectangle r)
    : type(t), rect(r) {}

PaddleEnlarge::PaddleEnlarge(Rectangle r, float origW, float maxW)
    : PowerUpEffect(PowerUpType::PADDLE_ENLARGE, r),
      originalWidth(origW), maxWidth(maxW) {}

void PaddleEnlarge::Update(float dt) {
    if (state == PowerUpState::FALLING) {
        rect.y += 120.0f * dt;
    } else if (state == PowerUpState::ACTIVE) {
        timer -= dt;
        if (timer <= 0) state = PowerUpState::EXPIRED;
    }
}

void PaddleEnlarge::Draw() {
    if (state == PowerUpState::FALLING) {
        DrawRectangleRec(rect, SKYBLUE);
        DrawRectangleLinesEx(rect, 1, WHITE);
        DrawText("+W", (int)rect.x + 2, (int)rect.y + 2, 10, WHITE);
    }
}

void PaddleEnlarge::Activate() {
    state = PowerUpState::ACTIVE;
    timer = 8.0f;
}

void PaddleEnlarge::Expire() {}

MultiBall::MultiBall(Rectangle r)
    : PowerUpEffect(PowerUpType::MULTI_BALL, r) {}

void MultiBall::Update(float dt) {
    if (state == PowerUpState::FALLING) rect.y += 120.0f * dt;
    else if (state == PowerUpState::ACTIVE) {
        timer -= dt;
        if (timer <= 0) state = PowerUpState::EXPIRED;
    }
}

void MultiBall::Draw() {
    if (state == PowerUpState::FALLING) {
        DrawCircle((int)rect.x + 8, (int)rect.y + 8, 8, ORANGE);
        DrawText("×2", (int)rect.x + 3, (int)rect.y + 3, 10, WHITE);
    }
}

void MultiBall::Activate() {
    state = PowerUpState::ACTIVE;
    timer = 10.0f;
}

void MultiBall::Expire() {}

ExtraLife::ExtraLife(Rectangle r)
    : PowerUpEffect(PowerUpType::EXTRA_LIFE, r) {
    isPermanent = true;
}

void ExtraLife::Update(float dt) {
    if (state == PowerUpState::FALLING) rect.y += 120.0f * dt;
}

void ExtraLife::Draw() {
    if (state == PowerUpState::FALLING) {
        DrawCircle((int)rect.x + 8, (int)rect.y + 8, 8, GREEN);
        DrawText("♥", (int)rect.x + 4, (int)rect.y + 2, 12, WHITE);
    }
}

void ExtraLife::Activate() {
    state = PowerUpState::ACTIVE;
}

void ExtraLife::Expire() {}

std::unique_ptr<PowerUpEffect> PowerUpFactory::CreatePowerUp(
    PowerUpType type, Rectangle spawnRect,
    float originalPaddleWidth, float maxPaddleWidth
) {
    switch (type) {
        case PowerUpType::PADDLE_ENLARGE:
            return std::make_unique<PaddleEnlarge>(spawnRect, originalPaddleWidth, maxPaddleWidth);
        case PowerUpType::MULTI_BALL:
            return std::make_unique<MultiBall>(spawnRect);
        case PowerUpType::EXTRA_LIFE:
            return std::make_unique<ExtraLife>(spawnRect);
        default: return nullptr;
    }
}