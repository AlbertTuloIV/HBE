#pragma once

// NOTE: we deliberately do NOT include "HBE/Renderer/ParticleSystem.h" here.
// That header transitively pulls in Scene2D.h -> SpriteAnimationStateMachine.h
// -> SpriteRenderer2D.h, which defines `class HBE::Renderer::SpriteRenderer2D`.
// Player.h already includes "HBE/Renderer/Sprite2D.h", which ALSO defines a
// class with the exact same name. Any translation unit that sees both headers
// (e.g. GameLayer.cpp, which includes both Player.h and Effects.h) fails with
// C2011 (SpriteRenderer2D redefinition). Until the engine consolidates those
// two headers, keep ParticleSystem.h out of this header and hold the system
// through a pimpl-style unique_ptr so only Effects.cpp sees it.
//
// The other engine headers below (Material / RenderItem / TileCollision) are
// header-hygiene safe -- none pull Scene2D or SpriteRenderer2D in.

#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/TileCollision.h"   // brings TileMap.h (TileMap + TileMapLayer)

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

namespace HBE::Renderer {
	class Renderer2D;
	class ResourceCache;
	class Mesh;
	class ParticleSystem;
}

namespace MegaX {

	class Effects {
	public:
		Effects();
		~Effects();

		Effects(const Effects&) = delete;
		Effects& operator=(const Effects&) = delete;

		bool init(HBE::Renderer::ResourceCache& resources,
			HBE::Renderer::Mesh* quadMesh,
			const HBE::Renderer::TileMap& map,
			const HBE::Renderer::TileMapLayer* solidLayer);

		void shutdown();

		void spawnMuzzleFlash(float x, float y, int dir);
		void spawnCasing(float x, float y, int dir);
		void spawnLandingDust(float feetX, float feetY, int tileId);
		void spawnBulletImpact(float x, float y, int tileId);

		void tickWalkDust(float dt, float feetX, float feetY, int tileId, bool moving, bool grounded);

		void update(float dt);
		void render(HBE::Renderer::Renderer2D& r2d);

		int liveParticles() const;

		float walkDustPeriod = 0.18f;

	private:
		void colorForTile(int tileId, float& r, float& g, float& b) const;

		// Casings are simulated game-side (not as particles) so they can
		// collide + bounce on the tilemap. See updateCasings / renderCasings.
		struct Casing {
			float x = 0.0f, y = 0.0f;
			float vx = 0.0f, vy = 0.0f;
			float angle = 0.0f;        // radians
			float angVel = 0.0f;
			float size = 3.0f;         // half-extent in world px (quad = size x size)
			float life = 0.0f;         // seconds remaining
			float maxLife = 1.0f;
			float r = 0.95f, g = 0.78f, b = 0.25f;
			bool  alive = true;
			bool  resting = false;     // stopped bouncing, just fading
		};

		void updateCasings(float dt);
		void renderCasings(HBE::Renderer::Renderer2D& r2d);

		std::unique_ptr<HBE::Renderer::ParticleSystem> m_ps;
		std::unordered_map<int, std::array<float, 4>> m_tileTop;
		float m_walkAccum = 0.0f;
		bool m_initialized = false;

		// casing sim state
		const HBE::Renderer::TileMap*      m_map = nullptr;
		const HBE::Renderer::TileMapLayer* m_solid = nullptr;
		HBE::Renderer::Mesh*      m_quad = nullptr;
		HBE::Renderer::Material   m_casingMat{};
		HBE::Renderer::RenderItem m_casingItem{};
		std::vector<Casing> m_casings;
	};
}