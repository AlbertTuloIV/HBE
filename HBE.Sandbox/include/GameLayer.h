#pragma once
#include "DevConsole.h"
#include "ParticleEffects.h"
#include "DemoGame.h"

#include "HBE/Core/Layer.h"
#include "HBE/Core/FileWatcher.h"
#include "HBE/Core/EventBus.h"

#include "HBE/ECS/CombatComponents.h"
#include "HBE/ECS/CombatEvents.h"
#include "HBE/Events/GameplayEvents.h"

#include "HBE/Renderer/Camera2D.h"
#include "HBE/Renderer/CameraController.h"
#include "HBE/Renderer/Scene2D.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/SpriteRenderer2D.h"
#include "HBE/Renderer/DebugDraw2D.h"
#include "HBE/Renderer/TileMap.h"
#include "HBE/Renderer/TileMapRenderer.h"
#include "HBE/Renderer/TileMapLoader.h"
#include "HBE/Renderer/TileCollision.h"
#include "HBE/Renderer/TextRenderer2D.h"
#include "HBE/Renderer/UI/UIContext.h"
#include "HBE/Renderer/ParticleSystem.h"
#include "HBE/Renderer/PostProcessStack.h"
#include "HBE/Renderer/PrefabLibrary.h"
#include "HBE/Renderer/ScriptRegistry.h"
#include "HBE/Renderer/AnimationPresetRegistry.h"

#include <array>
#include <unordered_map>
#include <vector>
#include <string>

namespace HBE::Core { class Application; }

class GameLayer final : public HBE::Core::Layer {
public:
	void onAttach(HBE::Core::Application& app) override;
	void onUpdate(float dt) override;
	void onRender() override;
	void onDetach() override;

	HBE::Renderer::DebugDraw2D m_debug{};
	HBE::Renderer::TextRenderer2D m_text{};

	// --- stats (FPS/UPS) ---
	double m_statTimer = 0.0;
	int    m_frameCount = 0;
	int    m_updateCount = 0;
	float  m_fps = 0.0f;
	float  m_ups = 0.0f;
	float  m_lastDt = 0.016f;

	float m_volume = 0.75f;
	float m_brightness = 1.0f;
	int   m_statBars = 5;

	float m_uiAnimT = 0.0f;

	bool onEvent(HBE::Core::Event& e);
	bool m_debugDraw = true;
	bool m_uiDebugDraw = false;
	bool m_uiGodMode = false;

private:
	HBE::Renderer::UI::UIContext m_ui{};
	HBE::Renderer::ParticleSystem m_particles{};
	HBE::Core::FileWatcher m_watcher{};
	HBE::Core::ScopedSubscription m_subAnimEvent{};
	HBE::Core::ScopedSubscription m_subDamageEvent{};
	HBE::Core::ScopedSubscription m_subDeathEvent{};

	// Hot reload targets (relative to working directory)
	std::string m_tileMapPath = "maps/test_map.json";
	std::string m_uiThemePath = "ui/theme.json";
	std::string m_spriteVsPath = "shaders/sprite.vert";
	std::string m_spriteFsPath = "shaders/sprite.frag";

	struct DebugPopup {
		std::string text;
		float x = 0.0f;
		float y = 0.0f;
		HBE::Renderer::Color4 color{ 1,1,1,1 };

		float life = 0.0f;
		float maxLife = 0.0f;
		float floatSpeed = 0.0f;
	};

	std::vector<DebugPopup> m_popups;
	void spawnPopup(float x, float y, const std::string& text, const HBE::Renderer::Color4& color,
		float lifetimeSeconds = 1.0f, float floatUpSpeed = 30.0f);

	HBE::Core::Application* m_app = nullptr;

	static constexpr float LOGICAL_WIDTH = 1280.0f;
	static constexpr float LOGICAL_HEIGHT = 720.0f;

	HBE::Sandbox::DemoGameState m_demo{};

	HBE::Renderer::CameraController m_cameraCtrl{};
	HBE::Renderer::Scene2D m_scene{};
	HBE::Renderer::PrefabLibrary m_prefabs{};
	HBE::Renderer::ScriptRegistry m_scripts{};
	HBE::Renderer::AnimationPresetRegistry m_animPresets{};

	HBE::ECS::HitboxPresetRegistry m_hitboxPresets{};

	HBE::Renderer::TileMap m_tileMap{};
	HBE::Renderer::TileMapRenderer m_tileRenderer{};
	const HBE::Renderer::TileMapLayer* m_collisionLayer = nullptr;

	// Non-empty when a structural asset (e.g. tilemap) failed to load and a
	// fallback is in use; drawn as a red banner in drawDemoHUD().
	std::string m_missingAssetBanner{};

	HBE::Renderer::GLShader* m_spriteShader = nullptr;
	HBE::Renderer::Mesh* m_quadMesh = nullptr;

	HBE::Renderer::Material m_goblinMaterial{};
	HBE::Renderer::SpriteRenderer2D::SpriteSheetHandle m_goblinSheet{};

	HBE::Renderer::Material m_soldierMaterial{};
	HBE::Renderer::SpriteRenderer2D::SpriteSheetHandle m_soldierSheet{};

	HBE::Renderer::EntityID m_goblinEntity = HBE::Renderer::InvalidEntityID;
	HBE::Renderer::EntityID m_soldierEntity = HBE::Renderer::InvalidEntityID;

	HBE::Renderer::ParticleHandle m_rainHandle = HBE::Renderer::kInvalidParticle;

	HBE::Renderer::PostProcessStack m_postProcess;
	HBE::Renderer::PostProcessEffect* m_fxBloom = nullptr;
	HBE::Renderer::PostProcessEffect* m_fxColorGrade = nullptr;
	HBE::Renderer::PostProcessEffect* m_fxVignette = nullptr;
	HBE::Renderer::PostProcessEffect* m_fxCrt = nullptr;
	HBE::Renderer::PostProcessEffect* m_fxPixelate = nullptr;

	// Set to true after the first onUpdate pre-warms the rain emitter (see onUpdate).
	bool m_rainPreWarmed = false;

	// Cached average color of the top rows of pixels of each tile ID (from tileset 0).
	// Used to tint the player dust particles based on the tile the player is standing on.
	std::unordered_map<int, std::array<float, 4>> m_tileTopColors;

	// Cooldown (seconds) between successive player-dust bursts while grounded + moving.
	float m_playerDustCooldown = 0.0f;

	void spawnPlayerDustAtFeet(float feetX, float feetY);

	DevConsole m_console{};

	HBE::Renderer::EntityID m_selectedEntity = HBE::Renderer::InvalidEntityID;

	bool m_showDevTools = true;
	bool m_showInspector = true;
	bool m_showFpsGraph = true;

	bool m_drawAllColliders = true;
	bool m_enableCulling = true;

	// FPS graph history
	std::vector<float> m_fpsHistory;
	int m_fpsHistoryMax = 120;

	void buildSpritePipeline();

	// Hot reload handlers
	void setupHotReloadWatches();
	void hotReloadShader();
	void hotReloadTileMap();
	void hotReloadUITheme();
	
	void registerScripts();
	void registerAnimatorPresets();
	void rewireBusSubscriptions();
	void onAnimationEvent(const HBE::Events::AnimationEvent& ev);
	void onDamageEvent(const HBE::Events::DamageEvent& ev);
	void onDeathEvent(const HBE::Events::DeathEvent& ev);

	void registerHitboxPresets();

	void resetDemoState();
	void updateDemoLogic(float dt);
	void drawDemoHUD(HBE::Renderer::Renderer2D& r2d);

	void drawInspector();

	bool saveSceneNow(std::string* outError = nullptr);
	bool loadSceneNow(std::string* outError = nullptr);

	int m_inspectorFirstIndex = 0;

	static constexpr int kInspectorPageSize = 12;

	int m_inspectorCreateTagIdx = 0;
	int m_inspectorRetagIdx = 0;

	bool m_inspectorShowAddComponent = false;
};