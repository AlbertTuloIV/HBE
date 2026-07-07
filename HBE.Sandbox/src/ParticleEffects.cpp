#include "ParticleEffects.h"
#include "HBE/Renderer/EmitterConfig.h"

// All sizes are in world units.
// SPRITE_PIXEL_SCALE = 4.0, so the player sprite is ~400 world units wide.
// A visible spark = 20-50 units. A large smoke puff = 80-200 units.

namespace HBE::Renderer {

    EffectDef makeHitSparks() {
        EmitterConfig c;
        c.name          = "hit_sparks";
        c.emissionRate  = 0.f;
        c.duration      = 0.05f;
        c.worldSpace    = true;
        c.maxParticles  = 30;
        c.additiveBlend = true;
        c.bursts.push_back({ 0.f, 20, 1 });
        c.lifetimeMin  = 0.18f; c.lifetimeMax = 0.38f;
        c.speedMin     = 160.f; c.speedMax    = 340.f;
        c.dirMin       = 0.f;   c.dirMax      = 360.f;
        c.gravityY     = -180.f;
        c.drag         = 1.4f;
        c.startSizeMin = 22.f;  c.startSizeMax = 40.f;
        c.endSizeMin   = 0.f;   c.endSizeMax   = 0.f;
        c.startR = 1.f;  c.startG = 0.9f; c.startB = 0.1f; c.startA = 1.f;
        c.endR   = 1.f;  c.endG   = 0.2f; c.endB   = 0.f;  c.endA   = 0.f;
        return { c };
    }

    EffectDef makeExplosion() {
        EmitterConfig sparks;
        sparks.name         = "explosion_sparks";
        sparks.emissionRate = 0.f;
        sparks.duration     = 0.1f;
        sparks.worldSpace   = true;
        sparks.maxParticles = 60;
        sparks.additiveBlend = true;
        sparks.bursts.push_back({ 0.f, 50, 1 });
        sparks.lifetimeMin  = 0.3f;  sparks.lifetimeMax = 0.6f;
        sparks.shape        = EmitterConfig::Shape::Circle;
        sparks.shapeRadius  = 50.f;
        sparks.speedMin     = 220.f; sparks.speedMax    = 480.f;
        sparks.dirMin       = 0.f;   sparks.dirMax      = 360.f;
        sparks.gravityY     = -120.f;
        sparks.drag         = 1.6f;
        sparks.startSizeMin = 24.f;  sparks.startSizeMax = 48.f;
        sparks.endSizeMin   = 0.f;   sparks.endSizeMax   = 0.f;
        sparks.startR = 1.f;  sparks.startG = 0.8f; sparks.startB = 0.1f; sparks.startA = 1.f;
        sparks.endR   = 0.6f; sparks.endG   = 0.1f; sparks.endB   = 0.f;  sparks.endA   = 0.f;

        EmitterConfig smoke;
        smoke.name         = "explosion_smoke";
        smoke.emissionRate = 0.f;
        smoke.duration     = 0.15f;
        smoke.worldSpace   = true;
        smoke.maxParticles = 20;
        smoke.bursts.push_back({ 0.02f, 14, 1 });
        smoke.lifetimeMin  = 0.7f;  smoke.lifetimeMax  = 1.2f;
        smoke.shape        = EmitterConfig::Shape::Circle;
        smoke.shapeRadius  = 32.f;
        smoke.speedMin     = 60.f;  smoke.speedMax     = 160.f;
        smoke.dirMin       = 60.f;  smoke.dirMax       = 120.f;
        smoke.gravityY     = 20.f;
        smoke.drag         = 2.5f;
        smoke.startSizeMin = 60.f;  smoke.startSizeMax = 100.f;
        smoke.endSizeMin   = 120.f; smoke.endSizeMax   = 180.f;
        smoke.rotVelMin    = -0.8f; smoke.rotVelMax    = 0.8f;
        smoke.startR = 0.45f; smoke.startG = 0.40f; smoke.startB = 0.40f; smoke.startA = 0.75f;
        smoke.endR   = 0.25f; smoke.endG   = 0.25f; smoke.endB   = 0.25f; smoke.endA   = 0.f;

        EmitterConfig flash;
        flash.name         = "explosion_flash";
        flash.emissionRate = 0.f;
        flash.duration     = 0.05f;
        flash.worldSpace   = true;
        flash.maxParticles = 24;
        flash.additiveBlend = true;
        flash.bursts.push_back({ 0.f, 24, 1 });
        flash.lifetimeMin  = 0.06f; flash.lifetimeMax  = 0.12f;
        flash.shape        = EmitterConfig::Shape::Ring;
        flash.shapeRadius  = 24.f;
        flash.speedMin     = 40.f;  flash.speedMax     = 120.f;
        flash.startSizeMin = 60.f;  flash.startSizeMax = 100.f;
        flash.endSizeMin   = 0.f;   flash.endSizeMax   = 0.f;
        flash.startR = 1.f; flash.startG = 0.95f; flash.startB = 0.7f; flash.startA = 1.f;
        flash.endR   = 1.f; flash.endG   = 0.5f;  flash.endB   = 0.f;  flash.endA   = 0.f;
        return { sparks, smoke, flash };
    }

    EffectDef makeSmoke() {
        EmitterConfig c;
        c.name         = "smoke";
        c.emissionRate = 8.f;
        c.duration     = -1.f;
        c.worldSpace   = true;
        c.maxParticles = 60;
        c.lifetimeMin  = 1.2f; c.lifetimeMax = 2.2f;
        c.shape        = EmitterConfig::Shape::Circle;
        c.shapeRadius  = 16.f;
        c.speedMin     = 20.f; c.speedMax    = 60.f;
        c.dirMin       = 70.f; c.dirMax      = 110.f;
        c.gravityY     = 15.f;
        c.drag         = 1.8f;
        c.startSizeMin = 48.f;  c.startSizeMax = 80.f;
        c.endSizeMin   = 120.f; c.endSizeMax   = 200.f;
        c.rotVelMin    = -0.5f; c.rotVelMax    = 0.5f;
        c.startR = 0.5f; c.startG = 0.5f; c.startB = 0.5f; c.startA = 0.55f;
        c.endR   = 0.3f; c.endG   = 0.3f; c.endB   = 0.3f; c.endA   = 0.f;
        return { c };
    }

    EffectDef makeFire() {
        EmitterConfig core;
        core.name         = "fire_core";
        core.emissionRate = 40.f;
        core.duration     = -1.f;
        core.worldSpace   = true;
        core.maxParticles = 80;
        core.additiveBlend = true;
        core.lifetimeMin  = 0.4f; core.lifetimeMax = 0.8f;
        core.shape        = EmitterConfig::Shape::Rect;
        core.shapeWidth   = 64.f; core.shapeHeight = 16.f;
        core.speedMin     = 60.f; core.speedMax    = 140.f;
        core.dirMin       = 75.f; core.dirMax      = 105.f;
        core.gravityY     = 30.f;
        core.drag         = 0.8f;
        core.startSizeMin = 32.f; core.startSizeMax = 56.f;
        core.endSizeMin   = 0.f;  core.endSizeMax   = 8.f;
        core.startR = 1.f;  core.startG = 0.85f; core.startB = 0.1f; core.startA = 1.f;
        core.endR   = 0.9f; core.endG   = 0.15f; core.endB   = 0.f;  core.endA   = 0.f;
        core.startR2 = 1.f; core.startG2 = 1.f;  core.startB2 = 0.6f; core.startA2 = 1.f;

        EmitterConfig smoke;
        smoke.name         = "fire_smoke";
        smoke.emissionRate = 10.f;
        smoke.duration     = -1.f;
        smoke.worldSpace   = true;
        smoke.maxParticles = 40;
        smoke.lifetimeMin  = 0.8f; smoke.lifetimeMax = 1.4f;
        smoke.shape        = EmitterConfig::Shape::Rect;
        smoke.shapeWidth   = 40.f; smoke.shapeHeight = 8.f;
        smoke.speedMin     = 30.f; smoke.speedMax    = 70.f;
        smoke.dirMin       = 80.f; smoke.dirMax      = 100.f;
        smoke.gravityY     = 18.f;
        smoke.drag         = 2.f;
        smoke.startSizeMin = 40.f; smoke.startSizeMax = 64.f;
        smoke.endSizeMin   = 88.f; smoke.endSizeMax   = 136.f;
        smoke.rotVelMin    = -0.6f; smoke.rotVelMax   = 0.6f;
        smoke.startR = 0.4f; smoke.startG = 0.4f; smoke.startB = 0.4f; smoke.startA = 0.4f;
        smoke.endR   = 0.2f; smoke.endG   = 0.2f; smoke.endB   = 0.2f; smoke.endA   = 0.f;
        return { core, smoke };
    }

    EffectDef makeFootstepDust() {
        EmitterConfig c;
        c.name         = "footstep_dust";
        c.emissionRate = 0.f;
        c.duration     = 0.05f;
        c.worldSpace   = true;
        c.maxParticles = 16;
        c.bursts.push_back({ 0.f, 10, 1 });
        c.lifetimeMin  = 0.25f; c.lifetimeMax = 0.50f;
        c.shape        = EmitterConfig::Shape::Line;
        c.shapeWidth   = 60.f;
        c.speedMin     = 80.f;  c.speedMax    = 200.f;
        c.dirMin       = 150.f; c.dirMax      = 210.f;
        c.gravityY     = -40.f;
        c.drag         = 3.f;
        c.startSizeMin = 28.f;  c.startSizeMax = 50.f;
        c.endSizeMin   = 0.f;   c.endSizeMax   = 0.f;
        c.startR = 0.82f; c.startG = 0.72f; c.startB = 0.55f; c.startA = 0.65f;
        c.endR   = 0.75f; c.endG   = 0.65f; c.endB   = 0.50f; c.endA   = 0.f;
        return { c };
    }

    EffectDef makeBlood() {
        EmitterConfig c;
        c.name         = "blood";
        c.emissionRate = 0.f;
        c.duration     = 0.05f;
        c.worldSpace   = true;
        c.maxParticles = 24;
        c.bursts.push_back({ 0.f, 18, 1 });
        c.lifetimeMin  = 0.3f;  c.lifetimeMax = 0.55f;
        c.speedMin     = 120.f; c.speedMax    = 280.f;
        c.dirMin       = 0.f;   c.dirMax      = 360.f;
        c.gravityY     = -400.f;
        c.drag         = 1.2f;
        c.startSizeMin = 20.f;  c.startSizeMax = 40.f;
        c.endSizeMin   = 8.f;   c.endSizeMax   = 16.f;
        c.startR = 0.7f;  c.startG = 0.05f; c.startB = 0.05f; c.startA = 1.f;
        c.endR   = 0.35f; c.endG   = 0.02f; c.endB   = 0.02f; c.endA   = 0.8f;
        return { c };
    }

    EffectDef makeHeal() {
        EmitterConfig c;
        c.name         = "heal";
        c.emissionRate = 18.f;
        c.duration     = 0.8f;
        c.worldSpace   = false;
        c.maxParticles = 40;
        c.additiveBlend = true;
        c.lifetimeMin  = 0.5f; c.lifetimeMax = 0.9f;
        c.shape        = EmitterConfig::Shape::Ring;
        c.shapeRadius  = 80.f;
        c.speedMin     = 40.f; c.speedMax    = 100.f;
        c.dirMin       = 60.f; c.dirMax      = 120.f;
        c.gravityY     = 10.f;
        c.drag         = 1.f;
        c.startSizeMin = 24.f; c.startSizeMax = 44.f;
        c.endSizeMin   = 0.f;  c.endSizeMax   = 0.f;
        c.startR = 0.2f; c.startG = 1.f;  c.startB = 0.4f;  c.startA = 1.f;
        c.endR   = 0.f;  c.endG   = 0.8f; c.endB   = 0.2f;  c.endA   = 0.f;
        return { c };
    }

    EffectDef makeShieldHit() {
        EmitterConfig c;
        c.name         = "shield_hit";
        c.emissionRate = 0.f;
        c.duration     = 0.05f;
        c.worldSpace   = true;
        c.maxParticles = 30;
        c.additiveBlend = true;
        c.bursts.push_back({ 0.f, 24, 1 });
        c.lifetimeMin  = 0.2f;  c.lifetimeMax = 0.4f;
        c.shape        = EmitterConfig::Shape::Ring;
        c.shapeRadius  = 72.f;
        c.speedMin     = 80.f;  c.speedMax    = 180.f;
        c.drag         = 2.f;
        c.startSizeMin = 24.f;  c.startSizeMax = 48.f;
        c.endSizeMin   = 0.f;   c.endSizeMax   = 0.f;
        c.startR = 0.5f; c.startG = 0.8f; c.startB = 1.f; c.startA = 1.f;
        c.endR   = 0.2f; c.endG   = 0.5f; c.endB   = 1.f; c.endA   = 0.f;
        return { c };
    }

    EffectDef makeWaterSplash() {
        EmitterConfig c;
        c.name         = "water_splash";
        c.emissionRate = 0.f;
        c.duration     = 0.05f;
        c.worldSpace   = true;
        c.maxParticles = 20;
        c.bursts.push_back({ 0.f, 14, 1 });
        c.lifetimeMin  = 0.35f; c.lifetimeMax = 0.6f;
        c.shape        = EmitterConfig::Shape::Line;
        c.shapeWidth   = 40.f;
        c.speedMin     = 120.f; c.speedMax    = 280.f;
        c.dirMin       = 50.f;  c.dirMax      = 130.f;
        c.gravityY     = -600.f;
        c.drag         = 0.5f;
        c.startSizeMin = 20.f;  c.startSizeMax = 40.f;
        c.endSizeMin   = 8.f;   c.endSizeMax   = 16.f;
        c.startR = 0.4f; c.startG = 0.7f; c.startB = 1.f; c.startA = 0.9f;
        c.endR   = 0.3f; c.endG   = 0.6f; c.endB   = 1.f; c.endA   = 0.f;
        return { c };
    }

    EffectDef makeTrail(float r, float g, float b) {
        EmitterConfig c;
        c.name         = "trail";
        c.emissionRate = 60.f;
        c.duration     = -1.f;
        c.worldSpace   = true;
        c.maxParticles = 120;
        c.additiveBlend = true;
        c.lifetimeMin  = 0.18f; c.lifetimeMax = 0.35f;
        c.speedMin     = 0.f;   c.speedMax    = 20.f;
        c.dirMin       = 0.f;   c.dirMax      = 360.f;
        c.drag         = 4.f;
        c.startSizeMin = 45.f;  c.startSizeMax = 72.f;
        c.endSizeMin   = 0.f;   c.endSizeMax   = 0.f;
        c.startR = r; c.startG = g; c.startB = b; c.startA = 0.85f;
        c.endR   = r; c.endG   = g; c.endB   = b; c.endA   = 0.f;
        return { c };
    }

    EffectDef makeRain() {
        // Rain is emitted uniformly across a rectangle that covers the viewport plus a 2-tile
        // buffer on every side. Because the emitter is re-centered on the camera each frame,
        // every new drop spawns somewhere already inside the visible area (not just above it),
        // so panning the camera never reveals the emitter's edge. Lifetime spans "just spawned
        // near the bottom" to "spawned near the top and about to exit" so drops die naturally
        // near the bottom of the buffer.
        EmitterConfig c;
        c.name         = "rain";
        c.emissionRate = 520.f;
        c.duration     = -1.f;
        c.worldSpace   = true;
        c.maxParticles = 1600;
        c.shape        = EmitterConfig::Shape::Rect;
        c.shapeWidth   = 1536.f; // 1280 viewport + 2 tiles (128) on each side
        c.shapeHeight  = 976.f;  // 720  viewport + 2 tiles (128) on top/bottom
        c.speedMin     = 380.f; c.speedMax    = 440.f;
        c.dirMin       = 255.f; c.dirMax      = 275.f;
        c.gravityY     = -160.f;
        c.lifetimeMin  = 0.6f;  c.lifetimeMax = 2.4f;
        c.startSizeMin = 4.f;   c.startSizeMax = 8.f;
        c.endSizeMin   = 1.f;   c.endSizeMax   = 2.f;
        c.startR = 0.55f; c.startG = 0.72f; c.startB = 1.f; c.startA = 0.7f;
        c.endR   = 0.45f; c.endG   = 0.62f; c.endB   = 1.f; c.endA   = 0.f;
        return { c };
    }

    EffectDef makeSnow() {
        EmitterConfig c;
        c.name         = "snow";
        c.emissionRate = 60.f;
        c.duration     = -1.f;
        c.worldSpace   = true;
        c.maxParticles = 300;
        c.shape        = EmitterConfig::Shape::Line;
        c.shapeWidth   = 1600.f;
        c.speedMin     = 40.f;  c.speedMax    = 80.f;
        c.dirMin       = 260.f; c.dirMax      = 290.f;
        c.inheritVX    = 18.f;
        c.gravityY     = -25.f;
        c.drag         = 0.3f;
        c.lifetimeMin  = 3.f;   c.lifetimeMax = 5.f;
        c.startSizeMin = 10.f;  c.startSizeMax = 22.f;
        c.endSizeMin   = 4.f;   c.endSizeMax   = 10.f;
        c.rotVelMin    = -0.3f; c.rotVelMax    = 0.3f;
        c.startR = 1.f; c.startG = 1.f; c.startB = 1.f; c.startA = 0.9f;
        c.endR   = 1.f; c.endG   = 1.f; c.endB   = 1.f; c.endA   = 0.f;
        return { c };
    }

    EffectDef makeEmbers() {
        EmitterConfig c;
        c.name         = "embers";
        c.emissionRate = 12.f;
        c.duration     = -1.f;
        c.worldSpace   = true;
        c.maxParticles = 80;
        c.additiveBlend = true;
        c.lifetimeMin  = 1.5f; c.lifetimeMax = 3.f;
        c.shape        = EmitterConfig::Shape::Circle;
        c.shapeRadius  = 120.f;
        c.speedMin     = 20.f; c.speedMax    = 80.f;
        c.dirMin       = 50.f; c.dirMax      = 130.f;
        c.gravityY     = 5.f;
        c.drag         = 0.4f;
        c.inheritVX    = 12.f;
        c.startSizeMin = 12.f; c.startSizeMax = 24.f;
        c.endSizeMin   = 0.f;  c.endSizeMax   = 0.f;
        c.startR  = 1.f;  c.startG  = 0.6f;  c.startB  = 0.1f; c.startA  = 1.f;
        c.endR    = 0.8f; c.endG    = 0.1f;  c.endB    = 0.f;  c.endA    = 0.f;
        c.startR2 = 1.f;  c.startG2 = 0.15f; c.startB2 = 0.f;  c.startA2 = 1.f;
        return { c };
    }

    EffectDef makeMagicAura() {
        EmitterConfig c;
        c.name         = "magic_aura";
        c.emissionRate = 24.f;
        c.duration     = -1.f;
        c.worldSpace   = false;
        c.maxParticles = 80;
        c.additiveBlend = true;
        c.lifetimeMin  = 0.6f; c.lifetimeMax = 1.0f;
        c.shape        = EmitterConfig::Shape::Ring;
        c.shapeRadius  = 112.f;
        c.speedMin     = 10.f; c.speedMax    = 30.f;
        c.drag         = 2.5f;
        c.startSizeMin = 24.f; c.startSizeMax = 44.f;
        c.endSizeMin   = 0.f;  c.endSizeMax   = 0.f;
        c.startR  = 0.7f; c.startG  = 0.3f; c.startB  = 1.f;  c.startA  = 1.f;
        c.endR    = 0.3f; c.endG    = 0.1f; c.endB    = 0.8f; c.endA    = 0.f;
        c.startR2 = 0.2f; c.startG2 = 0.5f; c.startB2 = 1.f;  c.startA2 = 1.f;
        return { c };
    }

    EffectDef makePlayerDust() {
        // Small burst of dust puffs kicked up under the player's feet.
        // Colors are placeholders; the game re-registers this effect with tinted
        // start/end R/G/B sampled from the tile top before each spawn.
        EmitterConfig c;
        c.name         = "player_dust";
        c.emissionRate = 0.f;
        c.duration     = 0.05f;
        c.worldSpace   = true;
        c.maxParticles = 24;
        c.bursts.push_back({ 0.f, 6, 1 });
        c.lifetimeMin  = 0.25f; c.lifetimeMax = 0.50f;
        c.shape        = EmitterConfig::Shape::Line;
        c.shapeWidth   = 24.f;
        c.speedMin     = 40.f;  c.speedMax    = 130.f;
        c.dirMin       = 60.f;  c.dirMax      = 120.f; // fan upward
        c.gravityY     = -260.f;                        // pull back down
        c.drag         = 3.5f;
        c.startSizeMin = 9.f;  c.startSizeMax = 15.f;
        c.endSizeMin   = 0.f;   c.endSizeMax   = 0.f;
        c.startR = 0.82f; c.startG = 0.72f; c.startB = 0.55f; c.startA = 0.75f;
        c.endR   = 0.75f; c.endG   = 0.65f; c.endB   = 0.50f; c.endA   = 0.f;
        return { c };
    }

} // namespace HBE::Renderer
