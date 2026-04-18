#ifndef POWER_UP_H
#define POWER_UP_H

#include "raylib.h"
#include <string>
#include <memory>

enum class PowerUpType {
    PADDLE_ENLARGE,
    MULTI_BALL,
    EXTRA_LIFE,
    NONE
};

enum class PowerUpState {
    FALLING,
    ACTIVE,
    EXPIRED
};

struct PowerUpConfig {
    float dropChance;
    float duration;
    float fallSpeed;
    float size;
    int maxParticles;
};

class PowerUpEffect {
protected:
    PowerUpType type;
    PowerUpState state = PowerUpState::FALLING;
    Rectangle rect;
    float timer = 0.0f;
    bool isPermanent = false;

public:
    PowerUpEffect(PowerUpType t, Rectangle r);
    virtual ~PowerUpEffect() = default;

    virtual void Update(float dt) = 0;
    virtual void Draw() = 0;
    virtual void Activate() = 0;
    virtual void Expire() = 0;

    PowerUpType GetType() const { return type; }
    PowerUpState GetState() const { return state; }
    Rectangle GetRect() const { return rect; }
    void SetState(PowerUpState s) { state = s; }
    bool IsPermanent() const { return isPermanent; }
};

class PaddleEnlarge : public PowerUpEffect {
private:
    float originalWidth;
    float maxWidth;
public:
    PaddleEnlarge(Rectangle r, float origW, float maxW);
    void Update(float dt) override;
    void Draw() override;
    void Activate() override;
    void Expire() override;
};

class MultiBall : public PowerUpEffect {
public:
    MultiBall(Rectangle r);
    void Update(float dt) override;
    void Draw() override;
    void Activate() override;
    void Expire() override;
};

class ExtraLife : public PowerUpEffect {
public:
    ExtraLife(Rectangle r);
    void Update(float dt) override;
    void Draw() override;
    void Activate() override;
    void Expire() override;
};

class PowerUpFactory {
public:
    static std::unique_ptr<PowerUpEffect> CreatePowerUp(
        PowerUpType type,
        Rectangle spawnRect,
        float originalPaddleWidth,
        float maxPaddleWidth
    );
};

#endif