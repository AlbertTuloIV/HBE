#include "Game/Enemy.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <algorithm>

using namespace HBE::Renderer;

namespace MegaX {
	
	static constexpr int kFrameW = 64;
	static constexpr int kFrameH = 64;
	static constexpr float kPixelScale = 1.0f;

	static constexpr int kIdleRow = 0;
	static constexpr int kIdleCol0 = 0;
	static constexpr int kIdleCol1 = 3;
	static constexpr float kIdleFps = 6.0f;

	static float posYForFeet(float feetY) {
		return feetY + static_cast<float>(kFrameH) * kPixelScale * 0.5f;
	}

	bool Enemy::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
		if (!quadMesh || !spriteShader) {
			return false;
		}

		m_idleSheet = SpriteRenderer2D::DeclareSpriteSheet(
			resources, "robot_soldier_idle",
			HBE::Core::AssetPaths::Resolve("sprites/Enemies/RobotSoldier/SoldierIdle_Spritesheet.png"),
			kFrameW, kFrameH);

		if (!m_idleSheet.isValid()) {
			HBE::Core::LogError("Enemy::init: failed to load Robot Soldier Idle Sheet at "
				+ HBE::Core::AssetPaths::AssetRootString());
			return false;
		}

		m_idleAnim = SpriteAnimation(&m_idleSheet, kIdleCol0, kIdleCol1, kIdleRow, kIdleFps, true);
		m_idleAnim.play(true);

		m_material.shader = spriteShader;
		m_material.texture = m_idleSheet.texture;

		m_item.mesh = quadMesh;
		m_item.material = &m_material;
		m_item.layer = 99;
		m_item.pass = RenderPass::World;
		m_item.transform.scaleX = kFrameW * kPixelScale;
		m_item.transform.scaleY = kFrameH * kPixelScale;
		m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, 1.0f };

		m_maxHp = startHp;
		m_hp = m_maxHp;

		SpriteRenderer2D::SetStaticSpriteFrame(m_item, m_idleSheet, kIdleCol0, kIdleRow);
		return true;
	}

	void Enemy::spawn(float x, float groundY, int facing) {
		m_x = x;
		m_feetY = groundY;
		m_y = posYForFeet(groundY);
		m_facing = (facing >= 0) ? +1 : -1;

		m_maxHp = startHp;
		m_hp = m_maxHp;
		m_invulnTimer = 0.0f;
		m_flashTimer = 0.0f;
		m_deathTimer = 0.0f;
		m_dead = false;
	}

	void Enemy::update(float dt) {
		if (m_invulnTimer > 0.0f) m_invulnTimer = std::max(0.0f, m_invulnTimer - dt);
		if (m_flashTimer > 0.0f) m_flashTimer = std::max(0.0f, m_flashTimer - dt);

		if (m_dead) {
			if (m_deathTimer > 0.0f) {
				m_deathTimer = std::max(0.0f, m_deathTimer - dt);
			}
		}
		else {
			m_idleAnim.update(dt);
		}
		applyAnimFrameToRenderItem();
	}

	void Enemy::render(Renderer2D& r2d) {
		if (isFinished()) return;

		m_item.transform.posX = m_x;
		m_item.transform.posY = m_y;
		m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
		m_item.transform.scaleY = kFrameH * kPixelScale;

		if (m_dead) {
			const float t = (deathFadeTime > 0.0f) ? (m_deathTimer / deathFadeTime) : 0.0f;
			m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, std::clamp(t, 0.0f, 1.0f) };
		}
		else if (m_flashTimer > 0.0f) {
			m_item.tint = Color4{ 1.0f, 0.4f, 0.4f, 1.0f };
		}
		else {
			m_item.tint = Color4{ 1.0f, 1.0f, 1.0f, 1.0f };
		}
		r2d.draw(m_item);
	}

	bool Enemy::takeDamage(int amount) {
		if (m_dead) return false;
		if (m_invulnTimer > 0.0f) return false;
		if (amount <= 0) return false;

		m_hp -= amount;
		m_invulnTimer = invulnAfterHit;
		m_flashTimer = hitFlashTime;

		if (m_hp <= 0) {
			m_hp = 0;
			m_dead = true;
			m_hitboxActive = false;
			m_deathTimer = deathFadeTime;
		}
		return true;
	}

	AABB Enemy::hurtbox() const {
		AABB b{};
		const float fx = static_cast<float>(m_facing);
		b.cx = m_x + hurtOffsetX * fx;
		b.cy = m_feetY + hurtHalfH + hurtOffsetY;
		b.w = hurtHalfW * 2.0f;
		b.h = hurtHalfH * 2.0f;
		return b;
	}

	AABB Enemy::hitbox() const {
		AABB b{};
		const float fx = static_cast<float>(m_facing);
		b.cx = m_x + hitOffsetX * fx;
		b.cy = m_feetY + hitHalfH + hitOffsetY;
		b.w = hitHalfW * 2.0f;
		b.h = hitHalfH * 2.0f;
		return b;
	}

	void Enemy::applyAnimFrameToRenderItem() {
		m_idleAnim.apply(m_item);
	}
}