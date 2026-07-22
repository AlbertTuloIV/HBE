#pragma once

#include "Game/Enemy.h"

#include <vector>

namespace HBE::Renderer {
	class ResourceCache;
	class Mesh;
	class GLShader;
	class Renderer2D;
	class DebugDraw2D;
}

namespace MegaX {

	class BulletManager;
	class Effects;

	class EnemyManager {
	public:
		bool init(HBE::Renderer::ResourceCache& resources,
			HBE::Renderer::Mesh* quadMesh,
			HBE::Renderer::GLShader* spriteShader);

		Enemy* spawn(float x, float groundY, int facing);

		int checkBulletHits(BulletManager& bullets, Effects* effects, int damagePerBullet = 1);

		void update(float dt);
		void render(HBE::Renderer::Renderer2D& r2d);

		void debugDrawBoxes(HBE::Renderer::DebugDraw2D& dbg,
			HBE::Renderer::Renderer2D& r2d) const;

		int aliveCount() const;
		const std::vector<Enemy>& enemies() const { return m_enemies; }

		void clear() { m_enemies.clear(); }

	private:
		HBE::Renderer::ResourceCache* m_resources = nullptr;
		HBE::Renderer::Mesh* m_quadMesh = nullptr;
		HBE::Renderer::GLShader* m_spriteShader = nullptr;

		std::vector<Enemy> m_enemies;
	};
}