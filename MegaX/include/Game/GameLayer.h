#pragma once

#include "HBE/Core/Layer.h"
#include "HBE/Renderer/CameraController.h"
#include "HBE/Renderer/DebugDraw2D.h"

#include "Game/Player.h"
#include "Game/Bullet.h"
#include "Game/Effects.h"
#include "Game/EnemyManager.h"
#include "World/World.h"

namespace HBE::Core { class Application; }

namespace MegaX {

	class GameLayer : public HBE::Core::Layer {
	public:
		void onAttach(HBE::Core::Application& app) override;
		void onUpdate(float dt) override;
		void onRender() override;

	private:
		void buildSpritePipeline();
		
		HBE::Core::Application* m_app = nullptr;
		
		HBE::Renderer::Mesh* m_quadMesh = nullptr;
		HBE::Renderer::GLShader* m_spriteShader = nullptr;

		HBE::Renderer::CameraController m_camera{};
		Player m_player{};
		World m_world{};
		BulletManager m_bullets{};
		Effects m_effects{};
		EnemyManager m_enemies{};
		const HBE::Renderer::TileMapLayer* m_ground = nullptr;

		HBE::Renderer::DebugDraw2D m_debug{};
		bool m_showHitBoxes = false;
	};
}