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
		// pixelSnap rounds the camera to whole WORLD units. At zoom 3 one world
		// unit = 3 screen pixels, so snapping makes the camera stair-step in 3px
		// jumps while the player glides -> judder + blur. Keep it off while the
		// camera is magnified; textures are already GL_NEAREST so sprites stay crisp.
		m_camera.pixelSnap = false;
		m_camera.followResponse = 9.0f;
		m_camera.snapZoom(kCameraZoom);

		if (!m_player.init(app.resources(), m_quadMesh, m_spriteShader)) {
			LogError("MegaX GameLayer: player init failed.");
		}
		m_player.setPosition(0.0f, 0.0f);
		m_camera.snapTo(0.0f, 0.0f);
		app.gl().setCamera(m_camera.camera());

		LogInfo("MegaX GameLayer attached (player ghost state).");
	}

	void GameLayer::onUpdate(float dt) {
		const float ix = (Input::IsKeyDown(SDL_SCANCODE_D) ? 1.0f : 0.0f)
			- (Input::IsKeyDown(SDL_SCANCODE_A) ? 1.0f : 0.0f);
		const float iy = (Input::IsKeyDown(SDL_SCANCODE_SPACE) ? 1.0f : 0.0f)
			- (Input::IsKeyDown(SDL_SCANCODE_S) ? 1.0f : 0.0f);

		if (Input::IsKeyPressed(SDL_SCANCODE_H)) {
			m_player.toggleHelmet();
		}

		m_player.setMoveInput(ix, iy);
		m_player.update(dt);

		m_camera.setFollowTarget(m_player.x(), m_player.y());
		m_camera.setFollowVelocity(m_player.velX(), m_player.velY());
		m_camera.update(dt);
		m_app->gl().setCamera(m_camera.camera());
	}

	void GameLayer::onRender() {
		Renderer2D& r2d = m_app->renderer2D();

		r2d.beginScene(m_camera.camera(), RenderPass::World);
		m_player.render(r2d);
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