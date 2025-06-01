#pragma once
#include "../../Common/MathHelper.h"
#include <vector>
#include <memory> // For std::unique_ptr
#include <iostream>
// Structure for individual particle data on the CPU
struct Particle
{
    DirectX::XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 PrevPosition{ 0.0f, 0.0f, 0.0f }; // Not strictly used in basic point rendering yet, but good for physics
    DirectX::XMFLOAT3 Velocity{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 Acceleration{ 0.0f, 0.0f, 0.0f };
    float Energy = 0.0f;      // Lifespan
    float Size = 1.0f;
    float SizeDelta = 0.0f;   // How size changes over time
    float Weight = 1.0f;
    float WeightDelta = 0.0f; // How weight changes over time
    DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 ColorDelta{ 0.0f, 0.0f, 0.0f, 0.0f }; // How color changes over time
    bool IsAlive = false;
};

// Structure for particle instance data to be sent to the GPU
struct ParticleInstanceData
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT4 Color;
    float Size;
};

class ParticleSystem
{
public:
    ParticleSystem(int maxParticles, DirectX::XMFLOAT3 origin)
        : m_maxParticles(maxParticles), m_origin(origin), m_numParticles(0), m_accumulatedTime(0.0f)
    {
        m_particleList.resize(maxParticles);
    }
    virtual ~ParticleSystem() = default;

    virtual void Update(float elapsedTime, DirectX::XMFLOAT3 force) = 0;
    virtual void Render() = 0; // This might not be used directly if TexColumnsApp handles rendering
    virtual int Emit(int numToEmit) = 0;
    virtual void InitializeSystem() = 0;
    virtual void KillSystem() = 0;

    const std::vector<ParticleInstanceData>& GetParticleRenderData() const { return m_renderData; }
    int GetAliveParticleCount() const { return static_cast<int>(m_renderData.size()); }

protected:
    virtual void InitializeParticle(int index) = 0;

    std::vector<Particle> m_particleList;
    std::vector<ParticleInstanceData> m_renderData; // Data ready for GPU upload

    int m_maxParticles;
    int m_numParticles; // Could track active particles here or derive from m_renderData.size()
    DirectX::XMFLOAT3 m_origin;
    float m_accumulatedTime;
    // DirectX::XMFLOAT3 m_force; // Force can be passed in Update
};



class FountainParticleSystem : public ParticleSystem
{
public:
    FountainParticleSystem(int maxParticles, DirectX::XMFLOAT3 origin)
        : ParticleSystem(maxParticles, origin)
    {
        InitializeSystem();
    }

    void InitializeSystem() override
    {
        for (int i = 0; i < m_maxParticles; ++i)
        {
            m_particleList[i].IsAlive = false;
        }
        m_renderData.clear();
        m_numParticles = 0;
    }

    void InitializeParticle(int index) override
    {
        Particle& p = m_particleList[index];
        p.IsAlive = true;
        p.Position = m_origin;
        p.PrevPosition = m_origin;

        // Random upward velocity
        float spread = 1.5f;
        p.Velocity = DirectX::XMFLOAT3(
            MathHelper::RandF(-spread, spread),
            MathHelper::RandF(3.0f, 8.0f), // Upwards
            MathHelper::RandF(-spread, spread)
        );
        p.Acceleration = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f); // Gravity will be the main force
        p.Energy = MathHelper::RandF(1.0f, 3.0f); // Lifespan in seconds
        p.Size = MathHelper::RandF(0.05f, 0.15f);
        p.SizeDelta = -p.Size / p.Energy; // Shrink to nothing over its lifetime
        p.Weight = MathHelper::RandF(0.5f, 1.5f);
        p.WeightDelta = 0.0f;
        p.Color = DirectX::XMFLOAT4(MathHelper::RandF(0.5f, 1.0f), MathHelper::RandF(0.5f, 1.0f), MathHelper::RandF(0.5f, 1.0f), 1.0f); // Random bright color
        p.ColorDelta = DirectX::XMFLOAT4(0.0f, -0.5f / p.Energy, -0.5f / p.Energy, -1.0f / p.Energy); // Fade out
    }

    int Emit(int numToEmit) override
    {
        int emittedCount = 0;
        for (int i = 0; i < m_maxParticles && emittedCount < numToEmit; ++i)
        {
            if (!m_particleList[i].IsAlive)
            {
                InitializeParticle(i);
                emittedCount++;
            }
        }
        m_numParticles = 0; // Recalculate below
        for (const auto& p : m_particleList) if (p.IsAlive) m_numParticles++;
        return emittedCount;
    }

    void Update(float elapsedTime, DirectX::XMFLOAT3 globalForce) override
    {
        m_renderData.clear(); // Prepare to fill with live particles

        for (int i = 0; i < m_maxParticles; ++i)
        {
            Particle& p = m_particleList[i];
            if (p.IsAlive)
            {
                p.Energy -= elapsedTime;
                if (p.Energy <= 0.0f || p.Position.y < -1.0f) // Kill if lifetime ends or falls below a certain y
                {
                    p.IsAlive = false;
                    continue;
                }

                // Basic Euler integration
                DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&p.Position);
                DirectX::XMVECTOR vel = DirectX::XMLoadFloat3(&p.Velocity);
                DirectX::XMVECTOR acc = DirectX::XMLoadFloat3(&p.Acceleration);
                DirectX::XMVECTOR force = DirectX::XMLoadFloat3(&globalForce);

                // Apply gravity (scaled by weight)
                DirectX::XMVECTOR weightedForce = DirectX::XMVectorScale(force, p.Weight);
                acc = DirectX::XMVectorAdd(acc, weightedForce);

                vel = DirectX::XMVectorAdd(vel, DirectX::XMVectorScale(acc, elapsedTime));
                pos = DirectX::XMVectorAdd(pos, DirectX::XMVectorScale(vel, elapsedTime));

                DirectX::XMStoreFloat3(&p.Position, pos);
                DirectX::XMStoreFloat3(&p.Velocity, vel);
                // Reset acceleration if it's not persistent (like wind)
                // p.Acceleration = {0,0,0};


                p.Size += p.SizeDelta * elapsedTime;
                if (p.Size < 0.0f) p.Size = 0.0f;

                p.Color.x += p.ColorDelta.x * elapsedTime;
                p.Color.y += p.ColorDelta.y * elapsedTime;
                p.Color.z += p.ColorDelta.z * elapsedTime;
                p.Color.w += p.ColorDelta.w * elapsedTime;
                // Clamp colors
                p.Color.x = MathHelper::Clamp(p.Color.x, 0.0f, 1.0f);
                p.Color.y = MathHelper::Clamp(p.Color.y, 0.0f, 1.0f);
                p.Color.z = MathHelper::Clamp(p.Color.z, 0.0f, 1.0f);
                p.Color.w = MathHelper::Clamp(p.Color.w, 0.0f, 1.0f);

                // Add to render data
                m_renderData.push_back({ p.Position, p.Color, p.Size });
            }
        }
    }

    void KillSystem() override
    {
        for (int i = 0; i < m_maxParticles; ++i)
        {
            m_particleList[i].IsAlive = false;
        }
        m_renderData.clear();
        m_numParticles = 0;
    }

    // This is not used if TexColumnsApp handles rendering
    void Render() override { /* TexColumnsApp will handle rendering */ }
};