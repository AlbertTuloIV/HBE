#include "Game/Player.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <cmath>

using namespace HBE::Renderer;
namespace MegaX {
	static constexpr int kFrameW = 75;
	static constexpr int kFrameH = 48;

	static constexpr int kIdleRow = 3, kIdleCol0 = 0, kIdleCol1 = 3;
	static constexpr int kWalkRow = 6, kWalkCol0 = 0, kWalkCol1 = 9;
	static constexpr float kIdleFps = 8.0f;
	static constexpr float kWalkFps = 14.0f;

	static constexpr float kPixelScale = 1.0f;

	bool Player::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
		m_helmSheet = SpriteRenderer2D::DeclareSpriteSheet(
			resources, "player_helm",
			HBE::Core::AssetPaths::Resolve("sprites/Player/player.png"),
			kFrameW, kFrameH);

		m_noHelmSheet = SpriteRenderer2D::DeclareSpriteSheet(
			resources, "player_no_helm",
			HBE::Core::AssetPaths::Resolve("sprites/Player/player-no-helm.png"),
			kFrameW, kFrameH);

		if (!m_helmSheet.isValid() || !m_noHelmSheet.isValid()) {
			HBE::Core::LogError("Player::init: failed to load player sprite sheet(s). "
				"Asset root: " + HBE::Core::AssetPaths::AssetRootString());
			return false;
		}

		m_activeSheet = m_helmet ? m_helmSheet : m_noHelmSheet;

		m_idleAnim = SpriteAnimation(&m_activeSheet, kIdleCol0, kIdleCol1, kIdleRow, kIdleFps, true);
		m_walkAnim = SpriteAnimation(&m_activeSheet, kWalkCol0, kWalkCol1, kWalkRow, kWalkFps, true);
		m_idleAnim.play(true);

		m_material.shader = spriteShader;
		m_material.texture = m_activeSheet.texture;

		m_item.mesh = quadMesh;
		m_item.material = &m_material;
		m_item.layer = 100;
		m_item.pass = RenderPass::World;
		m_item.transform.scaleX = kFrameW * kPixelScale;
		m_item.transform.scaleY = kFrameH * kPixelScale;

		SpriteRenderer2D::SetStaticSpriteFrame(m_item, m_activeSheet, kIdleCol0, kIdleRow);
		return true;
	}

	void Player::setHelmet(bool on) {
		m_helmet = on;
		m_activeSheet = on ? m_helmSheet : m_noHelmSheet;
		m_material.texture = m_activeSheet.texture;
	}

	void Player::update(float dt) {
		float ix = m_inX, iy = m_inY;
		const float mag = std::sqrt(ix * ix + iy * iy);
		if (mag > 1.0f) { ix /= mag; iy /= mag; }

		m_vx = ix * moveSpeed;
		m_vy = iy * moveSpeed;
		m_x += m_vx * dt;
		m_y += m_vy * dt;

		if (m_inX > 0.0f) m_facing = +1;
		else if (m_inX < 0.0f) m_facing = -1;

		const bool moving = (m_inX != 0.0f || m_inY != 0.0f);
		if (moving != m_moving) {
			m_moving = moving;
			if (moving) m_walkAnim.play(true);
			else m_idleAnim.play(true);
		}

		SpriteAnimation& anim = moving ? m_walkAnim : m_idleAnim;
		anim.update(dt);
		anim.apply(m_item);

		m_item.transform.posX = m_x;
		m_item.transform.posY = m_y;
		m_item.transform.scaleX = kFrameW * kPixelScale * static_cast<float>(m_facing);
		m_item.transform.scaleY = kFrameH * kPixelScale;
	}

	void Player::render(Renderer2D& r2d) {
		r2d.draw(m_item);
	}
}
