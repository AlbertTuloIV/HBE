#pragma once

#include "HBE/Renderer/Sprite2D.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/TileCollision.h"   // AABB, TileMap, TileMapLayer, MoveResult2D

namespace HBE::Renderer {
	class ResourceCache;
	class Mesh;
	class GLShader;
	class Renderer2D;
}

namespace MegaX {

	class Player {
	public:
		enum class Mode { Play, Ghost };

		bool init(HBE::Renderer::ResourceCache& resources,
			HBE::Renderer::Mesh* quadMesh,
			HBE::Renderer::GLShader* spriteShader);

		// Collision world (owned by World). `solidLayer` is the layer whose
		// solid tiles the player collides with (the "Ground" layer).
		void setCollision(const HBE::Renderer::TileMap* map,
			const HBE::Renderer::TileMapLayer* solidLayer) {
			m_map = map;
			m_collLayer = solidLayer;
		}

		void setPosition(float x, float y);   // x,y = sprite center, world space
		float x() const { return m_x; }
		float y() const { return m_y; }
		float velX() const { return m_vx; }
		float velY() const { return m_vy; }

		// --- per-frame input intents (set by GameLayer) ---
		void setMoveInput(float ix, float iy) { m_inX = ix; m_inY = iy; }
		void setJumpInput(bool pressed, bool held) {
			if (pressed) m_jumpPressed = true;   // latched until consumed in update()
			m_jumpHeld = held;
		}
		void setCrouchInput(bool held) { m_crouchHeld = held; }
		void setFireInput(bool pressed, bool held) {
			if (pressed) m_firePressed = true;   // latched until consumed in update()
			m_fireHeld = held;
		}

		// If the player fired this frame, returns true once and outputs the
		// muzzle world position + direction (+1/-1) for a bullet spawn.
		bool consumeShot(float& x, float& y, int& dir);

		bool landedThisFrame() const { return m_landedThisFrame; }

		int groundTileId() const { return m_groundTileId; }

		float feetY() const { return m_box.cy - m_box.h * 0.5f; }

		HBE::Renderer::AABB hurtbox() const { return m_box; }

		// --- mode (item 05 flips this on a hot-key) ---
		void setMode(Mode m) { m_mode = m; }
		Mode mode() const { return m_mode; }
		void toggleMode() { m_mode = (m_mode == Mode::Play) ? Mode::Ghost : Mode::Play; }

		void setHelmet(bool on);
		void toggleHelmet() { setHelmet(!m_helmet); }
		bool hasHelmet() const { return m_helmet; }

		void update(float dt);
		void render(HBE::Renderer::Renderer2D& r2d);

		// --- tunables (world px, seconds) ---
		float moveSpeed = 200.0f;   // ghost fly speed AND play-mode run speed
		float gravity   = 2100.0f;  // downward acceleration
		float jumpSpeed = 640.0f;   // initial jump velocity (up)
		float maxFall   = 900.0f;   // terminal velocity

	private:
		void updateGhost(float dt);
		void updatePlay(float dt);
		void syncRenderFromBox();                 // box -> m_x/m_y (feet aligned)
		void setAnimState(int s);
		HBE::Renderer::SpriteAnimation& animForState(int s);
		bool boxOverlapsSolid(const HBE::Renderer::AABB& b) const;

		// position = sprite center (world space)
		float m_x = 0.0f, m_y = 0.0f;
		float m_vx = 0.0f, m_vy = 0.0f;
		float m_inX = 0.0f, m_inY = 0.0f;

		// play-mode physics box (center-based, world space)
		HBE::Renderer::AABB m_box{};
		bool  m_grounded  = false;
		bool  m_crouching = false;
		float m_coyote    = 0.0f;   // time left to still jump after leaving ground
		float m_jumpBuf   = 0.0f;   // time left for a buffered jump press
		float m_landTimer = 0.0f;   // time left to show the landing animation

		// latched input intents
		bool m_jumpPressed = false;
		bool m_jumpHeld    = false;
		bool m_crouchHeld  = false;
		bool m_firePressed = false;
		bool m_fireHeld    = false;

		bool m_landedThisFrame = false;
		int m_groundTileId = 0;

		// shooting state
		float m_fireCooldown = 0.0f;   // time until the next shot may fire
		float m_shootTimer   = 0.0f;   // time left showing a shoot pose
		float m_recoilVx     = 0.0f;   // decaying air knockback velocity
		bool  m_shotPending  = false;  // a bullet is waiting to be spawned
		float m_shotX = 0.0f, m_shotY = 0.0f;
		int   m_shotDir = 1;

		int  m_facing = 1;
		bool m_helmet = true;
		Mode m_mode = Mode::Play;   // item 04 tests Play; item 05 adds the toggle

		// collision world (not owned)
		const HBE::Renderer::TileMap*      m_map = nullptr;
		const HBE::Renderer::TileMapLayer* m_collLayer = nullptr;

		// animation
		int m_animState = -1;   // 0 idle,1 walk,2 crouch,3 rise,4 fall,5 land,6 shootStand,7 shootWalk,8 shootAir
		HBE::Renderer::SpriteSheet m_helmSheet{};
		HBE::Renderer::SpriteSheet m_noHelmSheet{};
		HBE::Renderer::SpriteSheet m_activeSheet{};

		HBE::Renderer::SpriteAnimation m_idleAnim;
		HBE::Renderer::SpriteAnimation m_walkAnim;
		HBE::Renderer::SpriteAnimation m_crouchAnim;
		HBE::Renderer::SpriteAnimation m_riseAnim;
		HBE::Renderer::SpriteAnimation m_fallAnim;
		HBE::Renderer::SpriteAnimation m_landAnim;
		HBE::Renderer::SpriteAnimation m_shootStandAnim;
		HBE::Renderer::SpriteAnimation m_shootWalkAnim;
		HBE::Renderer::SpriteAnimation m_shootAirAnim;

		HBE::Renderer::Material   m_material{};
		HBE::Renderer::RenderItem m_item{};
	};
}