#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "HBE/ECS/Entity.h"

namespace HBE::ECS {
	enum class Faction : std::uint8_t {
		Neutral = 0,
		Player = 1,
		Enemy = 2,
		Environment = 3,
		_Count = 4
	};

	struct FactionComponent {
		Faction team = Faction::Neutral;
	};

	// Replaces HBE::Sandbox::Health (retired by Doc 01 Step 2).
	// invulnTimer is decremented by CombatSystem (do not tick again in
	// game code). deathTimer is bootstrapped by demo code on death; the
	// engine only decrements it.
	struct Health {
		int hp = 3;
		int maxHp = 3;
		float invulnTimer = 0.0f;
		float deathTimer = 0.0f;
		bool dead = false;
	};

	struct Hurtbox {
		float halfW = 0.0f;
		float halfH = 0.0f;
		float offsetX = 0.0f;
		float offsetY = 0.0f;

		bool active = true;
	};

	struct Hitbox {
		Entity owner = Null;

		float halfW = 0.5f;
		float halfH = 0.5f;
		float offsetX = 0.0f;
		float offsetY = 0.0f;

		int damage = 1;
		float lifetime = 0.1f;

		bool follows = true;
		bool active = true;

		std::array<bool, static_cast<std::size_t>(Faction::_Count)> canHit{
			true, // neutral
			true, // player
			true, // enemy,
			false // environment
		};

		Faction attackerFaction = Faction::Neutral;
		bool friendlyFire = false;

		float knockbackX = 0.0f;
		float knockbackY = 0.0f;
		bool knockbackHorizontalUsesFacing = true;

		float invulnAfterHit = 0.35f;

		std::vector<Entity> alreadyHit;
	};

	struct Knockback {
		float velX = 0.0f;
		float velY = 0.0f;
	};
}