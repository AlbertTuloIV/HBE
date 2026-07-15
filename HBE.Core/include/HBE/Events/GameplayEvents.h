#pragma once

#include <cstdint>
#include <string>

#include "HBE/ECS/Entity.h"

namespace HBE::Events {
	struct DamageEvent {
		HBE::ECS::Entity attacker = HBE::ECS::Null;
		HBE::ECS::Entity victim = HBE::ECS::Null;

		int damage = 0;
		int hpBefore = 0;
		int hpAfter = 0;

		float victimX = 0.0f;
		float victimY = 0.0f;
	};

	struct DeathEvent {
		HBE::ECS::Entity victim = HBE::ECS::Null;
		HBE::ECS::Entity killedBy = HBE::ECS::Null;

		float victimX = 0.0f;
		float victimY = 0.0f;
	};

	struct KnockbackAppliedEvent {
		HBE::ECS::Entity victim = HBE::ECS::Null;
		float velX = 0.0f;
		float velY = 0.0f;
	};

	struct AnimationEvent {
		HBE::ECS::Entity entity = HBE::ECS::Null;
		std::string name;
		std::string stateName;
		int frameIndex = 0;
	};

	struct AudioEvent {
		std::string soundKey;
		float worldX = 0.0f;
		float worldY = 0.0f;
		float gain = 1.0f;
		bool positional = false;
	};

	struct ParticleEvent {
		std::string emitterKey;
		float worldX = 0.0f;
		float worldY = 0.0f;
		float intensity = 1.0f;
	};

	struct GameStateChangedEvent {
		enum class State : std::uint8_t { Playing = 0, Won = 1, Lost = 2};
		State previous = State::Playing;
		State current = State::Playing;
	};

	struct SceneChangedEvent {
		std::string scenePath;
		bool loaded = true;
	};

	struct InteractionEvent {
		HBE::ECS::Entity actor = HBE::ECS::Null;
		HBE::ECS::Entity target = HBE::ECS::Null;
		std::string tags;
	};

	struct CollisionEvent {
		HBE::ECS::Entity a = HBE::ECS::Null;
		HBE::ECS::Entity b = HBE::ECS::Null;
		float normalX = 0.0f;
		float normalY = 0.0f;
	};
}