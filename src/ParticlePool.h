#ifndef PARTICLEPOOL_H
#define PARTICLEPOOL_H

#include "raylib.h"
#include "Particle.h" // 引入你项目的Particle定义，不重复写
#include <vector>
#include <cstdlib> // 适配你原有rand()逻辑

class ParticlePool {
private:
    std::vector<Particle> pool;       // 复用你项目的Particle结构体
    std::vector<int> availableIndices; // 闲置粒子索引（核心优化：快速找闲置）
    int maxCount;                     // 和你原有maxCount一致

public:
    // 初始化：和你原有ParticleSystem构造函数逻辑一致
    ParticlePool(int maxParticles) : maxCount(maxParticles) {
        pool.resize(maxParticles);
        // 初始化：所有粒子标记为闲置，索引存入availableIndices
        for (int i = maxParticles - 1; i >= 0; i--) {
            pool[i].active = false;
            availableIndices.push_back(i);
        }
    }

    // 适配你原有Emit逻辑（快速复用闲置粒子，无需遍历）
    void Emit(Vector2 pos, Color baseColor) {
        if (availableIndices.empty()) return; // 无闲置粒子，直接返回

        // 快速取最后一个闲置粒子索引（O(1)，比遍历快）
        int idx = availableIndices.back();
        availableIndices.pop_back();

        // 复用你原有Emit的粒子初始化逻辑（变量名完全一致）
        Particle& p = pool[idx];
        p.active = true;
        p.pos = pos;
        p.vel.x = (rand() % 100 - 50) / 30.0f;
        p.vel.y = (rand() % 100 - 50) / 30.0f;
        p.color = baseColor;
        p.life = 1.0f;
        p.maxLife = 1.0f;
        p.size = rand() % 4 + 2;
    }

    // 适配你原有Update逻辑（变量名/逻辑完全一致，仅新增回收闲置）
    void Update(float dt, int screenW, int screenH) {
        for (int i = 0; i < maxCount; i++) {
            Particle& p = pool[i];
            if (!p.active) continue;

            // 完全复用你原有Update逻辑
            p.pos.x += p.vel.x;
            p.pos.y += p.vel.y;
            p.life -= dt;
            p.color.a = (unsigned char)(255 * (p.life / p.maxLife));

            if (p.pos.x < 5 || p.pos.x > screenW - 5) p.vel.x *= -0.8f;
            if (p.pos.y < 5) p.vel.y *= -0.8f;

            // 粒子失效：标记为闲置，回收索引（核心优化）
            if (p.life <= 0) {
                p.active = false;
                availableIndices.push_back(i); // 回收至闲置列表
            }
        }
    }

    // 完全复用你原有Draw逻辑
    void Draw() {
        for (auto& p : pool) {
            if (p.active) DrawCircleV(p.pos, p.size, p.color);
        }
    }

    // 适配你原有Clear逻辑
    void Clear() {
        availableIndices.clear();
        for (int i = maxCount - 1; i >= 0; i--) {
            pool[i].active = false;
            availableIndices.push_back(i);
        }
    }
};

#endif