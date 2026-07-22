#include "Game/Effects.h"

#include "HBE/Renderer/ParticleSystem.h"
#include "HBE/Renderer/EmitterConfig.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Texture2D.h"
#include "HBE/Renderer/TileMap.h"
#include "HBE/Renderer/TileMapLoader.h"
#include "HBE/Renderer/TileCollision.h"
#include "HBE/Core/Log.h"

#include <cmath>
#include <random>
#include <algorithm>

using namespace HBE::Renderer;

namespace MegaX {

    // Out-of-line ctor/dtor so std::unique_ptr<ParticleSystem> is instantiated
    // in a TU that has the full ParticleSystem definition available. Effects.h
    // only sees a forward declaration.
    Effects::Effects() = default;
    Effects::~Effects() = default;

    // Shared RNG for casing spawns (matches how the engine's ParticleEmitter
    // draws its own randoms; we don't reuse the engine RNG here to keep the
    // sim entirely game-side).
    static std::mt19937& casingRng() {
        static std::mt19937 gen{ std::random_device{}() };
        return gen;
    }
    static float casingRand(float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(casingRng());
    }


    static EffectDef makeWalkDust() {
        EmitterConfig c;
        c.name = "walk_dust";
        c.emissionRate = 0.0f;
        c.duration = 0.05f;
        c.worldSpace = true;
        c.maxParticles = 24;
        c.bursts.push_back({ 0.0f, 4, 1 });

        c.lifetimeMin = 0.20f; c.lifetimeMax = 0.40f;
        c.shape = EmitterConfig::Shape::Line;
        c.shapeWidth = 10.0f;

        c.speedMin = 20.0f; c.speedMax = 55.0f;
        c.dirMin = 60.0f; c.dirMax = 120.0f;
        c.gravityY = -180.0f;
        c.drag = 3.5f;

        c.startSizeMin = 3.0f;  c.startSizeMax = 5.0f;
        c.endSizeMin = 0.0f;  c.endSizeMax = 0.0f;

        c.startR = 0.82f; c.startG = 0.72f; c.startB = 0.55f; c.startA = 0.75f;
        c.endR = 0.75f; c.endG = 0.65f; c.endB = 0.50f; c.endA = 0.0f;

        c.sortLayer = 95;
        return { c };
    }

    static EffectDef makeLandDust() {
        EmitterConfig c;
        c.name = "land_dust";
        c.emissionRate = 0.0f;
        c.duration = 0.05f;
        c.worldSpace = true;
        c.maxParticles = 32;
        c.bursts.push_back({ 0.0f, 12, 1 });

        c.lifetimeMin = 0.30f; c.lifetimeMax = 0.60f;
        c.shape = EmitterConfig::Shape::Line;
        c.shapeWidth = 22.0f;

        c.speedMin = 50.0f; c.speedMax = 140.0f;
        c.dirMin = 30.0f; c.dirMax = 150.0f;
        c.gravityY = -260.0f;
        c.drag = 3.0f;

        c.startSizeMin = 5.0f;  c.startSizeMax = 8.0f;
        c.endSizeMin = 0.0f;  c.endSizeMax = 0.0f;

        c.startR = 0.82f; c.startG = 0.72f; c.startB = 0.55f; c.startA = 0.80f;
        c.endR = 0.72f; c.endG = 0.62f; c.endB = 0.48f; c.endA = 0.0f;

        c.sortLayer = 95;
        return { c };
    }

    static EffectDef makeMuzzleFlash() {
        EmitterConfig core;
        core.name = "muzzle_flash_core";
        core.emissionRate = 0.0f;
        core.duration = 0.04f;
        core.worldSpace = true;
        core.maxParticles = 8;
        core.additiveBlend = true;
        core.bursts.push_back({ 0.0f, 4, 1 });

        core.lifetimeMin = 0.05f; core.lifetimeMax = 0.09f;
        core.shape = EmitterConfig::Shape::Point;
        core.speedMin = 0.0f;  core.speedMax = 0.0f;
        core.startSizeMin = 10.0f; core.startSizeMax = 14.0f;
        core.endSizeMin = 2.0f;  core.endSizeMax = 4.0f;

        core.startR = 1.0f; core.startG = 1.0f;  core.startB = 0.85f; core.startA = 1.0f;
        core.endR = 1.0f; core.endG = 0.55f; core.endB = 0.0f;  core.endA = 0.0f;

        core.sortLayer = 102;

        EmitterConfig sparks;
        sparks.name = "muzzle_flash_sparks";
        sparks.emissionRate = 0.0f;
        sparks.duration = 0.05f;
        sparks.worldSpace = true;
        sparks.maxParticles = 12;
        sparks.additiveBlend = true;
        sparks.bursts.push_back({ 0.0f, 6, 1 });

        sparks.lifetimeMin = 0.06f; sparks.lifetimeMax = 0.14f;
        sparks.shape = EmitterConfig::Shape::Point;
        sparks.speedMin = 160.0f; sparks.speedMax = 300.0f;

        sparks.dirMin = -18.0f; sparks.dirMax = 18.0f;

        sparks.gravityY = 0.0f;
        sparks.drag = 4.0f;

        sparks.startSizeMin = 3.0f;  sparks.startSizeMax = 5.0f;
        sparks.endSizeMin = 0.0f;  sparks.endSizeMax = 0.0f;

        sparks.startR = 1.0f; sparks.startG = 0.95f; sparks.startB = 0.4;sparks.endA = 0.0f;

        sparks.sortLayer = 102;

        return { core, sparks };
    }

    static EffectDef makeCasing() {
        EmitterConfig c;
        c.name = "bullet_casing";
        c.emissionRate = 0.0f;
        c.duration = 0.05f;
        c.worldSpace = true;
        c.maxParticles = 4;
        c.bursts.push_back({ 0.0f, 1, 1 });

        c.lifetimeMin = 0.15f; c.lifetimeMax = 0.25f;
        c.shape = EmitterConfig::Shape::Point;
        c.speedMin = 90.0f; c.speedMax = 130.0f;
        c.dirMin = 50.0f; c.dirMax = 60.0f;

        c.gravityY = -900.0f;
        c.drag = 0.2f;

        c.rotVelMin = -12.0f; c.rotVelMax = 12.0f;

        c.startSizeMin = 3.0f;  c.startSizeMax = 4.0f;
        c.endSizeMin = 3.0f;  c.endSizeMax = 4.0f;

        c.startR = 0.95f; c.startG = 0.78f; c.startB = 0.25f; c.startA = 1.0f;
        c.endR = 0.75f; c.endG = 0.60f; c.endB = 0.15f; c.endA = 1.0f;

        c.sortLayer = 102;
        return { c };
    }

    static EffectDef makeBulletImpact() {
        EmitterConfig chunks;
        chunks.name = "bullet_impact_chunks";
        chunks.emissionRate = 0.0f;
        chunks.duration = 0.05f;
        chunks.worldSpace = true;
        chunks.maxParticles = 20;
        chunks.bursts.push_back({ 0.0f, 10, 1 });

        chunks.lifetimeMin = 0.20f; chunks.lifetimeMax = 0.40f;
        chunks.shape = EmitterConfig::Shape::Point;
        chunks.speedMin = 120.0f; chunks.speedMax = 260.0f;
        chunks.dirMin = 0.0f;   chunks.dirMax = 360.0f;

        chunks.gravityY = -420.0f;
        chunks.drag = 1.4f;

        chunks.startSizeMin = 3.0f;  chunks.startSizeMax = 5.0f;
        chunks.endSizeMin = 0.0f;  chunks.endSizeMax = 0.0f;

        chunks.startR = 0.75f; chunks.startG = 0.70f; chunks.startB = 0.60f; chunks.startA = 1.0f;
        chunks.endR = 0.55f; chunks.endG = 0.50f; chunks.endB = 0.40f; chunks.endA = 0.0f;

        chunks.sortLayer = 102;

        EmitterConfig sparks;
        sparks.name = "bullet_impact_sparks";
        sparks.emissionRate = 0.0f;
        sparks.duration = 0.05f;
        sparks.worldSpace = true;
        sparks.maxParticles = 20;
        sparks.additiveBlend = true;
        sparks.bursts.push_back({ 0.0f, 12, 1 });

        sparks.lifetimeMin = 0.10f; sparks.lifetimeMax = 0.22f;
        sparks.shape = EmitterConfig::Shape::Point;
        sparks.speedMin = 180.0f; sparks.speedMax = 340.0f;
        sparks.dirMin = 0.0f;   sparks.dirMax = 360.0f;

        sparks.gravityY = -220.0f;
        sparks.drag = 1.2f;

        sparks.startSizeMin = 3.0f;  sparks.startSizeMax = 5.0f;
        sparks.endSizeMin = 0.0f;  sparks.endSizeMax = 0.0f;

        sparks.startR = 1.0f;  sparks.startG = 0.95f; sparks.startB = 0.35f; sparks.startA = 1.0f;
        sparks.endR = 1.0f;  sparks.endG = 0.40f; sparks.endB = 0.0f;  sparks.endA = 0.0f;

        sparks.sortLayer = 102;

        return { chunks, sparks };
    }

    bool Effects::init(ResourceCache& resources, Mesh* quadMesh, const TileMap& map, const TileMapLayer* solidLayer) {
        m_ps = std::make_unique<ParticleSystem>();
        if (!m_ps->initialize(resources, quadMesh)) {
            HBE::Core::LogError("MegaX Effects: ParticleSystem init failed.");
            m_ps.reset();
            return false;
        }

        m_ps->registerEffect("walk_dust", makeWalkDust());
        m_ps->registerEffect("land_dust", makeLandDust());
        m_ps->registerEffect("muzzle_flash", makeMuzzleFlash());
        // NOTE: casings are NOT registered with the particle system anymore --
        // they need tile collisions (bouncing), so they are simulated game-side
        // in updateCasings() and drawn in renderCasings().
        m_ps->registerEffect("bullet_impact", makeBulletImpact());
        if (!TileMapLoader::sampleTileTopColors(map, m_tileTop, 3)) {
            HBE::Core::LogInfo("MegaX Effects: tile top colours unavailable "
                "(tile-tinted effects will use the fallback tan).");
        }

        // Casing render item: solid white 1x1 texture tinted per-draw with
        // the individual casing's colour. Sort in front of bullets.
        m_map = &map;
        m_solid = solidLayer;
        m_quad = quadMesh;

        const unsigned char white[4] = { 255, 255, 255, 255 };
        Texture2D* tex = resources.getOrCreateTextureFromRGBA("megax_white1x1", 1, 1, white);
        if (tex) {
            // The sprite shader is fetched by name in the particle system's
            // initialize; the same "sprite" cache entry is used here.
            m_casingMat.shader  = resources.getShader("sprite");
            m_casingMat.texture = tex;
        }
        m_casingItem.mesh = m_quad;
        m_casingItem.material = &m_casingMat;
        m_casingItem.layer = 102;
        m_casingItem.pass = RenderPass::World;
        m_casingItem.tint = Color4{ 0.95f, 0.78f, 0.25f, 1.0f };

        m_initialized = true;
        return true;
    }

    void Effects::shutdown() {
        if (m_ps) m_ps->shutdown();
        m_ps.reset();
        m_tileTop.clear();
        m_casings.clear();
        m_map = nullptr;
        m_solid = nullptr;
        m_quad = nullptr;
        m_initialized = false;
    }

    void Effects::colorForTile(int tileId, float& r, float& g, float& b) const {
        r = 0.82f; g = 0.72f; b = 0.55f;
        if (tileId <= 0) return;
        auto it = m_tileTop.find(tileId);
        if (it != m_tileTop.end()) {
            r = it->second[0]; g = it->second[1]; b = it->second[2];
        }
    }

    void Effects::spawnMuzzleFlash(float x, float y, int dir) {
        if (!m_ps) return;
        EffectDef def = makeMuzzleFlash();
        if (def.size() >= 2) {
            EmitterConfig& sparks = def[1];
            if (dir < 0) {
                const float lo = 180.0f - sparks.dirMax;
                const float hi = 180.0f - sparks.dirMin;
                sparks.dirMin = lo;
                sparks.dirMax = hi;
            }
        }
        m_ps->registerEffect("muzzle_flash", def);
        m_ps->spawn("muzzle_flash", x, y);
    }

    void Effects::spawnCasing(float x, float y, int dir) {
        // Casings are simulated game-side so they can collide + bounce; the
        // particle system doesn't do tile collisions. We match the visual
        // params of the old `makeCasing` particle: speed 90-130, up+back cone,
        // size 3-4, brass tint, +/- 12 rad/s spin.
        const float speed = casingRand(90.0f, 130.0f);
        float dirDegLo = 105.0f, dirDegHi = 130.0f;
        if (dir < 0) {
            // Mirror the cone across the vertical axis (same convention as
            // the muzzle_flash/particle spawns).
            const float lo = 180.0f - dirDegHi;
            const float hi = 180.0f - dirDegLo;
            dirDegLo = lo;
            dirDegHi = hi;
        }
        const float rad = casingRand(dirDegLo, dirDegHi) * 0.01745329252f;

        Casing c;
        c.x = x;
        c.y = y;
        c.vx = std::cos(rad) * speed;
        c.vy = std::sin(rad) * speed;   // world +y = up
        c.angle = casingRand(0.0f, 6.28318530718f);
        c.angVel = casingRand(-16.0f, 16.0f);
        c.size = casingRand(3.0f, 4.0f);
        c.maxLife = casingRand(1.80f, 2.20f);
        c.life = c.maxLife;
        c.r = 0.95f; c.g = 0.78f; c.b = 0.25f;
        m_casings.push_back(c);
    }

    void Effects::spawnLandingDust(float feetX, float feetY, int tileId) {
        if (!m_ps) return;
        float r, g, b;
        colorForTile(tileId, r, g, b);

        EffectDef def = makeLandDust();
        EmitterConfig& c = def[0];
        c.startR = r;         c.startG = g;         c.startB = b;
        c.endR = r * 0.55f; c.endG = g * 0.55f; c.endB = b * 0.55f;

        m_ps->registerEffect("land_dust", def);
        m_ps->spawn("land_dust", feetX, feetY);
    }

    void Effects::spawnBulletImpact(float x, float y, int tileId) {
        if (!m_ps) return;
        float r, g, b;
        colorForTile(tileId, r, g, b);

        EffectDef def = makeBulletImpact();
        EmitterConfig& chunks = def[0];
        chunks.startR = r;         chunks.startG = g;         chunks.startB = b;
        chunks.endR = r * 0.55f; chunks.endG = g * 0.55f; chunks.endB = b * 0.55f;

        m_ps->registerEffect("bullet_impact", def);
        m_ps->spawn("bullet_impact", x, y);
    }

    void Effects::tickWalkDust(float dt,
        float feetX, float feetY,
        int tileId,
        bool moving, bool grounded)
    {
        if (!m_ps || !moving || !grounded) {
            m_walkAccum = 0.0f;
            return;
        }

        m_walkAccum += dt;
        if (m_walkAccum < walkDustPeriod) return;
        m_walkAccum -= walkDustPeriod;

        float r, g, b;
        colorForTile(tileId, r, g, b);

        EffectDef def = makeWalkDust();
        EmitterConfig& c = def[0];
        c.startR = r;         c.startG = g;         c.startB = b;
        c.endR = r * 0.55f; c.endG = g * 0.55f; c.endB = b * 0.55f;

        m_ps->registerEffect("walk_dust", def);
        m_ps->spawn("walk_dust", feetX, feetY);
    }

    void Effects::update(float dt) {
        if (!m_initialized) return;
        if (m_ps) m_ps->update(dt);
        updateCasings(dt);
    }

    void Effects::render(Renderer2D& r2d) {
        if (!m_initialized) return;
        if (m_ps) m_ps->render(r2d);
        renderCasings(r2d);
    }

    int Effects::liveParticles() const {
        const int ps = m_ps ? m_ps->totalLiveParticles() : 0;
        return ps + static_cast<int>(m_casings.size());
    }

    // ---- casing simulator ---------------------------------------------------
    //
    // Simple axis-separated physics: apply gravity + drag, move the casing as
    // a tiny AABB through the ground layer using the engine's TileCollision
    // solver, then reflect the velocity component that got clamped (bounce).
    // Bounces get smaller each hit; once vertical energy is spent the casing
    // rolls horizontally with ground friction, and finally parks when nearly
    // motionless. Life ticks down the whole time and drives a fade-out.
    void Effects::updateCasings(float dt) {
        if (m_casings.empty()) return;

        constexpr float kGravity     = -900.0f;
        constexpr float kAirDrag     = 0.5f;    // per-second exp decay of vx in air
        constexpr float kRestitution = 0.55f;   // vertical bounce (springier)
        constexpr float kBounceFric  = 0.78f;   // horizontal slowdown on floor hit
        constexpr float kSpinFric    = 0.72f;   // angular slowdown on any hit
        constexpr float kGroundFric  = 3.5f;    // per-second exp decay of vx while rolling
        constexpr float kSpinRoll    = 2.0f;    // per-second exp decay of angVel while rolling
        constexpr float kMinBounceVy = 45.0f;   // below this, stop bouncing and start rolling
        constexpr float kRestSpeed   = 4.0f;    // total |vx|+|vy| threshold to park

        for (auto& c : m_casings) {
            if (!c.alive) continue;

            c.life -= dt;
            if (c.life <= 0.0f) { c.alive = false; continue; }

            if (c.resting) {
                // Just tumble-fade in place; life is already ticking down.
                c.angVel *= 0.90f;
                c.angle  += c.angVel * dt;
                continue;
            }

            // Integrate velocity.
            c.vy += kGravity * dt;
            c.vx *= std::exp(-kAirDrag * dt);   // exp drag = frame-rate independent

            c.angle += c.angVel * dt;

            if (m_map && m_solid) {
                AABB box{ c.x, c.y, c.size, c.size };
                // Capture pre-move velocity: moveAndCollideEx zeroes velX/velY
                // on the axis it clamped, so we need these to compute the
                // reflected bounce ourselves.
                const float preVx = c.vx;
                const float preVy = c.vy;

                MoveResult2D res = TileCollision::moveAndCollideEx(
                    *m_map, *m_solid, box, c.vx, c.vy, dt,
                    /*maxStepUp*/ 0.0f, /*enableOneWay*/ false,
                    /*enableSlopes*/ false, /*oneWayPrevBottom*/ box.cy - box.h * 0.5f);
                c.x = box.cx;
                c.y = box.cy;

                if (res.hitX) {
                    c.vx = -preVx * kRestitution;   // wall reflect
                    c.angVel *= kSpinFric;
                }
                if (res.hitY) {
                    c.vy = -preVy * kRestitution;
                    // Preserve horizontal from before the move (mover zeroed it
                    // only if hitX also fired -- which we already handled).
                    if (!res.hitX) c.vx = preVx * kBounceFric;
                    c.angVel *= kSpinFric;

                    // If the vertical bounce would be barely visible, kill it
                    // and let the casing roll along the floor instead of
                    // micro-bouncing forever (looks like it's stuck).
                    if (res.grounded && std::fabs(c.vy) < kMinBounceVy) {
                        c.vy = 0.0f;
                    }
                }

                // Rolling phase: on the floor with (nearly) no vertical
                // energy. Apply strong horizontal + angular friction so the
                // casing rolls to rest instead of skittering forever.
                if (res.grounded && std::fabs(c.vy) < 1.0f) {
                    c.vx *= std::exp(-kGroundFric * dt);
                    c.angVel *= std::exp(-kSpinRoll  * dt);

                    if (std::fabs(c.vx) + std::fabs(c.vy) < kRestSpeed) {
                        c.vx = 0.0f;
                        c.vy = 0.0f;
                        c.resting = true;
                    }
                }
            } else {
                // No collision world (shouldn't happen at runtime): just drift.
                c.x += c.vx * dt;
                c.y += c.vy * dt;
            }
        }

        // Compact dead casings.
        m_casings.erase(
            std::remove_if(m_casings.begin(), m_casings.end(),
                [](const Casing& c) { return !c.alive; }),
            m_casings.end());
    }

    void Effects::renderCasings(Renderer2D& r2d) {
        if (m_casings.empty() || !m_casingItem.mesh || !m_casingItem.material) return;
        for (const auto& c : m_casings) {
            if (!c.alive) continue;
            // Fade over the last ~0.6s of life so the disappearance is soft.
            const float fadeStart = 0.60f;
            const float alpha = (c.life >= fadeStart) ? 1.0f : (c.life / fadeStart);
            m_casingItem.transform.posX = c.x;
            m_casingItem.transform.posY = c.y;
            m_casingItem.transform.rotation = c.angle;
            m_casingItem.transform.scaleX = c.size;
            m_casingItem.transform.scaleY = c.size;
            m_casingItem.tint = Color4{ c.r, c.g, c.b, alpha };
            r2d.draw(m_casingItem);
        }
    }
}