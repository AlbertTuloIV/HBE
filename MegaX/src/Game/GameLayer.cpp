#include "Game/GameLayer.h"

#include "HBE/Core/Application.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/RenderPass.h"

#include "HBE/Platform/Input.h"

#include <vector>

using namespace HBE::Core;
using namespace HBE::Renderer;
namespace Input = HBE::Platform::Input;

namespace MegaX {

	static constexpr float kLogicalWidth = 1280.0f;
	static constexpr float kLogicalHeight = 720.0f;
	static constexpr float kCameraZoom = 3.0f;

	void GameLayer::onAttach(HBE::Core::Application& app) {
		m_app = &app;

        buildSpritePipeline();

		app.gl().setClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		Camera2D& cam = m_camera.camera();
		cam.viewportWidth = kLogicalWidth;
		cam.viewportHeight = kLogicalHeight;
		m_camera.pixelSnap = false;
		m_camera.followResponse = 9.0f;
		m_camera.snapZoom(kCameraZoom);

        if (!m_world.load(app.renderer2D(), app.resources(),
            m_spriteShader, m_quadMesh, "maps/level_01.json")) {
            LogError("MegaX GameLayer: world load failed (continuing empty.)");
        }

		if (!m_player.init(app.resources(), m_quadMesh, m_spriteShader)) {
			LogError("MegaX GameLayer: player init failed.");
		}

		// --- collision: the "Ground" layer carries the solid tiles ---
		m_ground = m_world.map().findLayer("Ground");
		if (!m_ground) {
			LogError("MegaX GameLayer: no 'Ground' layer in map — player will not collide.");
		}
		m_player.setCollision(&m_world.map(), m_ground);

		if (!m_bullets.init(app.resources(), m_quadMesh, m_spriteShader)) {
			LogError("MegaX GameLayer: bullet manager init failed.");
		}

        if (!m_effects.init(app.resources(), m_quadMesh, m_world.map(), m_ground)) {
            LogError("MegaX GameLayer: effects init failed.");
        }

        if (!m_enemies.init(app.resources(), m_quadMesh, m_spriteShader)) {
            LogError("MegaX GameLayer: enemy manager init failed.");
        }

        if (!m_debug.initialize(app.resources(), m_quadMesh)) {
            LogError("MegaX GameLayer: debug draw init failed (B overlay disabled).");
        }

        const float startX = (m_world.pixelWidth() * 0.5f) - 128.0f; // X
        const float startY = 130.0f; // Y
        m_player.setPosition(startX, startY);
        m_camera.snapTo(startX, startY);
		app.gl().setCamera(m_camera.camera());

        {
            constexpr float kTilePx = 32.0f;
            const float ex = startX + 5.0f * kTilePx; // X
            const float eGroundY = 130.0f; // Y
            if (Enemy* e = m_enemies.spawn(ex, eGroundY, -1)) {
                e->maxHp();
                e->spawn(ex, eGroundY, -1);
            }
        }

		LogInfo("MegaX GameLayer attached (Play mode; press G for Ghost).");
	}

	void GameLayer::onUpdate(float dt) {
		// horizontal run (both modes)
		const float ix = (Input::IsKeyDown(SDL_SCANCODE_D) ? 1.0f : 0.0f)
			- (Input::IsKeyDown(SDL_SCANCODE_A) ? 1.0f : 0.0f);

		// vertical fly intent — only used by Ghost mode; Play mode ignores iy
		const float iy = (Input::IsKeyDown(SDL_SCANCODE_SPACE) ? 1.0f : 0.0f)
			- (Input::IsKeyDown(SDL_SCANCODE_S) ? 1.0f : 0.0f);

		// platformer intents (Play mode)
		const bool jumpPressed = Input::IsKeyPressed(SDL_SCANCODE_SPACE); // one-shot
		const bool jumpHeld    = Input::IsKeyDown(SDL_SCANCODE_SPACE);    // for variable height
		const bool crouchHeld  = Input::IsKeyDown(SDL_SCANCODE_S);

		// shooting (E) — semi/auto-fire handled in Player at a cadence
		const bool firePressed = Input::IsKeyPressed(SDL_SCANCODE_E);
		const bool fireHeld    = Input::IsKeyDown(SDL_SCANCODE_E);

		if (Input::IsKeyPressed(SDL_SCANCODE_H)) {
			m_player.toggleHelmet();
		}

		// G toggles Play <-> Ghost (fly, no gravity/collision — for map building)
		if (Input::IsKeyPressed(SDL_SCANCODE_G)) {
			m_player.toggleMode();
			LogInfo(m_player.mode() == Player::Mode::Ghost
				? "MegaX: Ghost mode (fly, no collision)."
				: "MegaX: Play mode (gravity + collision).");
		}

        if(Input::IsKeyPressed(SDL_SCANCODE_B)){
            m_showHitBoxes = !m_showHitBoxes;
            LogInfo(m_showHitBoxes
                ? "MegaX: hit/hurt box overlay ON."
                : "MegaX: hit/hurt box overlay OFF.");
        }

        m_world.update(dt);
		m_player.setMoveInput(ix, iy);
		m_player.setJumpInput(jumpPressed, jumpHeld);
		m_player.setCrouchInput(crouchHeld);
		m_player.setFireInput(firePressed, fireHeld);
		m_player.update(dt);

        // spawn any bullet the player fired this frame, then advance bullets
		float bx, by; int bdir;
		if (m_player.consumeShot(bx, by, bdir)) {
			m_bullets.spawn(bx, by, bdir);

            m_effects.spawnMuzzleFlash(bx, by, bdir);
            // Casings eject from the ~ejection port near the gun body, not the
            // barrel tip: ~5 px in front of the player center, at gun height.
            const float casingX = m_player.x() + static_cast<float>(bdir) * 5.0f;
            m_effects.spawnCasing(casingX, by, bdir);
		}
		m_bullets.update(dt, &m_world.map(), m_ground, m_camera.camera());

        m_enemies.checkBulletHits(m_bullets, &m_effects, 1);

        {
            std::vector<BulletManager::Impact> impacts;
            if (m_bullets.consumeImpacts(impacts)) {
                for (const auto& imp : impacts) {
                    m_effects.spawnBulletImpact(imp.x, imp.y, imp.tileId);
                }
            }
        }

        if (m_player.landedThisFrame()) {
            m_effects.spawnLandingDust(m_player.x(), m_player.feetY(), m_player.groundTileId());
        }

        {
            const bool moving = (ix != 0.0f);
            const bool grounded = (m_player.velY() == 0.0f) && (m_player.groundTileId() != 0);
            m_effects.tickWalkDust(dt, m_player.x(), m_player.feetY(), m_player.groundTileId(), moving, grounded);
        }

		m_camera.setFollowTarget(m_player.x(), m_player.y());
		m_camera.setFollowVelocity(m_player.velX(), m_player.velY());
		m_camera.update(dt);
		m_app->gl().setCamera(m_camera.camera());

        m_enemies.update(dt);
        m_effects.update(dt);
	}

	void GameLayer::onRender() {
		Renderer2D& r2d = m_app->renderer2D();

		r2d.beginScene(m_camera.camera(), RenderPass::World);
        m_world.render(r2d);
        m_enemies.render(r2d);
		m_player.render(r2d);
		m_bullets.render(r2d);
        m_effects.render(r2d);

        if (m_showHitBoxes) {
            const HBE::Renderer::AABB pb = m_player.hurtbox();
            m_debug.rect(r2d, pb.cx, pb.cy, pb.w, pb.h, 0.35f, 0.55f, 1.0f, 1.0f, false);
            m_enemies.debugDrawBoxes(m_debug, r2d);
        }

		r2d.endScene();
	}

    void GameLayer::buildSpritePipeline() {
        auto& resources = m_app->resources();

        m_spriteShader = resources.getOrCreateShaderFromFiles(
            "sprite",
            AssetPaths::Resolve("shaders/sprite.vert"),
            AssetPaths::Resolve("shaders/sprite.frag"));

        if (!m_spriteShader) {
            const char* spriteVs = R"(#version 330 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec2 aUV;
            layout(location = 2) in vec4 aColor;

            out vec2 vUV;
            out vec4 vColor;

            uniform mat4 uMVP;
            uniform vec4 uUVRect; // xy offset, zw scale

            void main() {
                vUV = aUV * uUVRect.zw + uUVRect.xy;
                vColor = aColor;
                gl_Position = uMVP * vec4(aPos, 1.0);
            }
            )";
                        const char* spriteFs = R"(#version 330 core
            in vec2 vUV;
            in vec4 vColor;
            out vec4 FragColor;

            uniform sampler2D uTex;
            uniform vec4 uColor;

            uniform int uIsSDF;
            uniform float uSDFSoftness;

            void main() {
                vec4 tex = texture(uTex, vUV);
                if (uIsSDF == 0) {
                    FragColor = tex * uColor * vColor;
                    return;
                }
                float dist = tex.a;
                float w = fwidth(dist) * max(uSDFSoftness, 0.001);
                float alpha = smoothstep(0.5 - w, 0.5 + w, dist);
                vec4 tinted = uColor * vColor;
                FragColor = vec4(tinted.rgb, tinted.a * alpha);
            }
            )";
            m_spriteShader = resources.getOrCreateShader("sprite", spriteVs, spriteFs);
        }

        if (!m_spriteShader) {
            LogFatal("MegaX GameLayer: failed to create sprite shader.");
            m_app->requestQuit();
            return;
        }

        // Unit quad centered on origin, with UVs. pos.xyz, uv.xy — 6 verts.
        const std::vector<float> quadVerts = {
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
             0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,

             0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
            -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
        };

        m_quadMesh = resources.getOrCreateMeshPosUV("quad", quadVerts, 6);
        if (!m_quadMesh) {
            LogFatal("MegaX GameLayer: failed to create quad mesh.");
            m_app->requestQuit();
            return;
        }

        m_app->renderer2D().setSpriteQuadMesh(m_quadMesh);
    }

}