#pragma once

#include "HBE/Renderer/Sprite2D.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/TileCollision.h"

namespace HBE::Renderer {
	class ResourceCache;
	class Mesh;
	class GLShader;
	class Renderer2D;
}

namespace MegaX {
	class Enemy {
	public:
		bool init(HBE::Renderer::ResourceCache& resources, HBE::Renderer::Mesh* quadMesh, HBE::Renderer::GLShader* spriteShader);

		void spawn(float x, float groundY, int facing);

		void update(float dt);
		void render(HBE::Renderer::Renderer2D& r2d);

		bool takeDamage(int amount);

		HBE::Renderer::AABB hurtbox() const;

		HBE::Renderer::AABB hitbox() const;

		bool hurtboxActive() const { return !m_dead; }
		bool hitboxActive() const { return m_hitboxActive && !m_dead; }

		bool isAlive() const { return !m_dead; }
		bool isDying() const { return m_dead && m_deathTimer > 0.0f; }
		bool isFinished() const { return m_dead && m_deathTimer <= 0.0f; }

		int hp() const { return m_hp; }
		int maxHp() const { return m_maxHp; }
		int facing() const { return m_facing; }
		float x() const { return m_x; }
		float y() const { return m_y; }
		float feetY() const { return m_feetY; }

		int damagePerHit = 1;
		float invulnAfterHit = 0.08f;
		float hitFlashTime = 0.10f;
		float deathFadeTime = 0.60f;

		int startHp = 3;

		float hurtHalfW = 11.0f;
		float hurtHalfH = 18.0f;
		float hurtOffsetX = 0.0f;
		float hurtOffsetY = 0.0f;

		float hitHalfW = 20.0f;
		float hitHalfH = 14.0f;
		float hitOffsetX = 20.0f;
		float hitOffsetY = 0.0f;
		int hitDamage = 1;

	private:
		void applyAnimFrameToRenderItem();

		float m_x = 0.0f;
		float m_y = 0.0f;
		float m_feetY = 0.0f;

		int m_facing = -1;
		
		int m_hp = 3;
		int m_maxHp = 3;

		float m_invulnTimer = 0.0f;
		float m_flashTimer = 0.0f;
		float m_deathTimer = 0.0f;
		bool m_dead = false;

		bool m_hitboxActive = false;

		HBE::Renderer::SpriteSheet m_idleSheet{};
		HBE::Renderer::SpriteAnimation m_idleAnim;

		HBE::Renderer::Material m_material{};
		HBE::Renderer::RenderItem m_item{};
	};
}
