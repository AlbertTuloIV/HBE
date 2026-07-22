#pragma once

#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/TileCollision.h"   // TileMap, TileMapLayer
#include "HBE/Renderer/Camera2D.h"

#include <vector>

namespace HBE::Renderer {
	class ResourceCache;
	class Mesh;
	class GLShader;
	class Renderer2D;
}

namespace MegaX {

	// Owns and simulates the player's bullets. Rendered as sleek, tinted quads.
	// Bullets die when they hit a solid tile or travel past the view + margin.
	class BulletManager {
	public:
		bool init(HBE::Renderer::ResourceCache& resources,
			HBE::Renderer::Mesh* quadMesh,
			HBE::Renderer::GLShader* spriteShader);

		// Spawn a bullet at world (x,y) travelling horizontally in dir (+1 / -1).
		void spawn(float x, float y, int dir);

		// Advance bullets; cull those that hit a solid tile or leave the view.
		void update(float dt,
			const HBE::Renderer::TileMap* map,
			const HBE::Renderer::TileMapLayer* solidLayer,
			const HBE::Renderer::Camera2D& cam);

		void render(HBE::Renderer::Renderer2D& r2d);

		void clear() { m_bullets.clear(); m_impacts.clear(); }
		int  count() const { return static_cast<int>(m_bullets.size()); }

		struct Impact {
			float x = 0.0f;
			float y = 0.0f;
			int tileId = 0;
		};

		struct Bullet {
			float x = 0.0f, y = 0.0f;
			float vx = 0.0f;
			bool  alive = true;
		};

		std::vector<Bullet>& bullets() { return m_bullets; }
		const std::vector<Bullet>& bullets() const { return m_bullets; }

		bool consumeImpacts(std::vector<Impact>& out) {
			if (m_impacts.empty()) return false;
			out.insert(out.end(), m_impacts.begin(), m_impacts.end());
			m_impacts.clear();
			return true;
		}

		// tunables
		float speed = 640.0f;        // world px / sec
		float offscreenTiles = 4.0f; // despawn this many tiles past the view edge
		float length = 16.0f;        // visual length (world px)
		float height = 4.0f;         // visual thickness (world px)

	private:
		bool pointInSolid(const HBE::Renderer::TileMap* map,
			const HBE::Renderer::TileMapLayer* layer,
			float x, float y) const;

		std::vector<Bullet> m_bullets;
		std::vector<Impact> m_impacts;

		HBE::Renderer::Material   m_material{};
		HBE::Renderer::RenderItem m_item{};
		HBE::Renderer::Mesh*      m_quad = nullptr;
	};
}
