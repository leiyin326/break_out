#include "Particle.h"
#include <random>

ParticleSystem::ParticleSystem(int maxParticles) : maxCount(maxParticles) {
    pool.resize(maxParticles);
    for (auto& p : pool) p.active = false;
}

void ParticleSystem::Emit(Vector2 pos, Color baseColor) {
    for (auto& p : pool) {
        if (!p.active) {
            p.active = true;
            p.pos = pos;
            p.vel.x = (rand() % 100 - 50) / 30.0f;
            p.vel.y = (rand() % 100 - 50) / 30.0f;
            p.color = baseColor;
            p.life = 1.0f;
            p.maxLife = 1.0f;
            p.size = rand() % 4 + 2;
            break;
        }
    }
}

void ParticleSystem::Update(float dt, int screenW, int screenH) {
    for (auto& p : pool) {
        if (!p.active) continue;
        p.pos.x += p.vel.x;
        p.pos.y += p.vel.y;
        p.life -= dt;
        p.color.a = (unsigned char)(255 * (p.life / p.maxLife));

        if (p.pos.x < 5 || p.pos.x > screenW - 5) p.vel.x *= -0.8f;
        if (p.pos.y < 5) p.vel.y *= -0.8f;

        if (p.life <= 0) p.active = false;
    }
}

void ParticleSystem::Draw() {
    for (auto& p : pool) {
        if (p.active) DrawCircleV(p.pos, p.size, p.color);
    }
}

void ParticleSystem::Clear() {
    for (auto& p : pool) p.active = false;
}