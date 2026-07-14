#pragma once

namespace HBE::Sandbox {

	struct Health {
		int hp = 3;
		int maxHp = 3;
		float invulnTimer = 0.0f;
		float deathTimer = 0.0f;
		bool dead = false;
	};

	enum class DemoState {
		Playing,
		Won,
		Lost,
	};

	struct DemoGameState {
		DemoState state = DemoState::Playing;

		float playerSpawnX = 0.0f;
		float playerSpawnY = 0.0f;

		float goblinSpawnX = 64.0f;
		float goblinSpawnY = 0.0f;

		float endBannerTimer = 0.0f;
	};

	namespace demo {
		constexpr float LOSE_Y_THRESHOLD = -800.0f;
		constexpr float ATTACK_REACH_PX = 44.0f;
		constexpr float ATTACK_HEIGHT_PX = 44.0f;
		constexpr float HIT_IFRAMES_SEC = 0.35f;
		constexpr float DEATH_FADE_SEC = 0.60f;
		constexpr float GOBLIN_PATROL_RADIUS = 96.0f;
		constexpr float GOBLIN_PATROL_SPEED = 60.0f;
		constexpr float CAMERA_LERP = 0.15f;
		constexpr float RESTART_DELAY = 0.75f;
	}
}