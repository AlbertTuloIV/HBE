#pragma once

#include "HBE/Renderer/Sprite2D.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"

namespace HBE::Renderer {
	class ResourceCache;
	class Mesh;
	class GLShader;
	class Renderer2D;
}

namespace MegaX {
	class Player {
	public:
		bool init(HBE::Renderer::ResourceCache& resources,
			HBE::Renderer::Mesh* quadMesh,
			HBE::Renderer::GLShader* spriteShader);

		void setPosition(float x, float y) { m_x = x; m_y = y; }
		float x() const { return m_x; }
		float y() const { return m_y; }
		float velX() const { return m_vx; }
		float velY() const { return m_vy; }

		void setMoveInput(float ix, float iy) { m_inX = ix; m_inY = iy; }

		void setHelmet(bool on);
		void toggleHelmet() { setHelmet(!m_helmet); }
		bool hasHelmet() const { return m_helmet; }

		void update(float dt);
		void render(HBE::Renderer::Renderer2D& r2d);

		float moveSpeed = 180.0f;

	private:
		float m_x = 0.0f, m_y = 0.0f;
		float m_vx = 0.0f, m_vy = 0.0f;
		float m_inX = 0.0f, m_inY = 0.0f;
		int m_facing = 1;
		bool m_helmet = true;
		bool m_moving = false;

		HBE::Renderer::SpriteSheet m_helmSheet{};
		HBE::Renderer::SpriteSheet m_noHelmSheet{};
		HBE::Renderer::SpriteSheet m_activeSheet{};

		HBE::Renderer::SpriteAnimation m_idleAnim;
		HBE::Renderer::SpriteAnimation m_walkAnim;

		HBE::Renderer::Material m_material{};
		HBE::Renderer::RenderItem m_item{};
	};
}