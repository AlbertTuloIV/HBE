#pragma once
#include "HBE/Renderer/EmitterConfig.h"

namespace HBE::Renderer {
	EffectDef makeHitSparks();
	EffectDef makeExplosion();
	EffectDef makeBlood();
	EffectDef makeHeal();
	EffectDef makeShieldHit();

	EffectDef makeSmoke();
	EffectDef makeFire();
	EffectDef makeFootstepDust();
	EffectDef makeWaterSplash();

	EffectDef makeTrail(float r, float g, float b);

	EffectDef makeRain();
	EffectDef makeSnow();
	EffectDef makeEmbers();

	EffectDef makeMagicAura();

	// Small ground puff kicked up beneath the player when moving on the ground.
	// The color is a neutral tan; the game tints startR/G/B and endR/G/B at spawn time
	// with the color sampled from the top of the tile the player is standing on.
	EffectDef makePlayerDust();
}