#ifndef PARTICLE_H
#define PARTICLE_H

#include "raylib.h"
#include <vector>

struct Particle {
    Vector2 pos;
    Vector2 vel;
    Color color;
    float life;
    float maxLife;
    float size;
    bool active;
};

class ParticleSystem {
private:
    std::vector<Particle> pool;
    int maxCount;
    float emitRate;

public:
    ParticleSystem(int maxParticles);
    void Emit(Vector2 pos, Color baseColor);
    void Update(float dt, int screenW, int screenH);
    void Draw();
    void Clear();
};

#endif