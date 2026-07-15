#include "GameLayer.h"
#include "DevConsole.h"

#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"
#include "HBE/Core/Event.h"
#include "HBE/Core/AssetPaths.h"

#include "HBE/Platform/Input.h"
#include "HBE/Input/InputMap.h"

#include "HBE/Renderer/GLShader.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Transform2D.h"
#include "HBE/Renderer/UI/UIThemeLoader.h"
#include "HBE/Renderer/TileMapLoader.h"
#include "HBE/Renderer/PostProcessStack.h"

#include "HBE/Renderer/SceneSerializer.h"
#include "HBE/Renderer/Prefab.h"r
#include "HBE/ECS/RuntimeComponents.h"
#include "HBE/ECS/Components.h"
#include "HBE/ECS/CombatComponents.h"

#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_gamepad.h>

#include <cmath>
#include <vector>
#include <string>
#include <filesystem>
#include <system_error>

#include <algorithm>
#include <utility>

using namespace HBE::Core;
using namespace HBE::Renderer;
using namespace HBE::Platform;

namespace {
    constexpr float SPRITE_PIXEL_SCALE = 4.0f;
    constexpr float PLAYER_BODY_W_PX = 8.0f;
    constexpr float PLAYER_BODY_H_PX = 14.0f;
    constexpr float PLAYER_BODY_Y_OFFSET_PX = +0.5f;

    constexpr float GOBLIN_BODY_W_PX = 10.0f;
    constexpr float GOBLIN_BODY_H_PX = 14.0f;
    constexpr float GOBLIN_BODY_Y_OFFSET_PX = +0.5f;

    constexpr const char* BINDINGS_LOGICAL = "bindings.json";
    constexpr const char* SCENE_LOGICAL = "scenes/sandbox.scene.json";

    static void SetAnimatorPreset(HBE::ECS::Registry& reg, HBE::ECS::Entity e, const std::string& presetName) {
        HBE::ECS::AnimatorPresetComponent ap{};
        ap.preset = presetName;
        reg.emplace<HBE::ECS::AnimatorPresetComponent>(e, ap);
    }

#if defined(_DEBUG)
    constexpr bool kSceneSaveGoesToUserData = false;
#else
    constexpr bool kSceneSaveGoesToUserData = true;
#endif

    static float Approach(float v, float target, float maxDelta) {
        if (v < target) return std::min(v + maxDelta, target);
        if (v > target) return std::max(v - maxDelta, target);
        return target;
    }

    static float LerpF(float from, float to, float t) {
        return from + (to - from) * t;
    }

    static void RegisterDefaultBindings(HBE::Input::InputMap& map)
    {
        using namespace HBE::Input;

        map.bindAction(Action::Jump, Binding::Key(SDL_SCANCODE_SPACE), true);
        map.bindAction(Action::Jump, Binding::GamepadButton(SDL_GAMEPAD_BUTTON_SOUTH), false);
        map.bindAction(Action::Attack, Binding::Key(SDL_SCANCODE_E), true);
        map.bindAction(Action::Attack, Binding::GamepadButton(SDL_GAMEPAD_BUTTON_WEST), false);
        map.bindAction(Action::UIConfirm, Binding::Key(SDL_SCANCODE_RETURN), true);
        map.bindAction(Action::UIConfirm, Binding::GamepadButton(SDL_GAMEPAD_BUTTON_SOUTH), false);
        map.bindAction(Action::UICancel, Binding::Key(SDL_SCANCODE_ESCAPE), true);
        map.bindAction(Action::UICancel, Binding::GamepadButton(SDL_GAMEPAD_BUTTON_EAST), false);
        map.bindAction(Action::Pause, Binding::Key(SDL_SCANCODE_ESCAPE), true);
        map.bindAction(Action::Pause, Binding::GamepadButton(SDL_GAMEPAD_BUTTON_START), false);
        map.bindAction(Action::FullscreenToggle, Binding::Key(SDL_SCANCODE_F11), true);
        map.bindAction(Action::FullscreenToggle, Binding::None(), false);

        AxisBinding moveX{};
        moveX.negative = Binding::Key(SDL_SCANCODE_A);
        moveX.positive = Binding::Key(SDL_SCANCODE_D);
        moveX.negative2 = Binding::Key(SDL_SCANCODE_LEFT);
        moveX.positive2 = Binding::Key(SDL_SCANCODE_RIGHT);
        moveX.useGamepadAxis = true;
        moveX.gamepadAxis = SDL_GAMEPAD_AXIS_LEFTX;
        moveX.deadzone = 0.20f;
        moveX.invert = false;
        moveX.scale = 1.0f;
        map.bindAxis(Axis::MoveX, moveX);

        AxisBinding moveY{};
        moveY.negative = Binding::Key(SDL_SCANCODE_W);
        moveY.positive = Binding::Key(SDL_SCANCODE_S);
        moveY.negative2 = Binding::Key(SDL_SCANCODE_UP);
        moveY.positive2 = Binding::Key(SDL_SCANCODE_DOWN);
        moveY.useGamepadAxis = true;
        moveY.gamepadAxis = SDL_GAMEPAD_AXIS_LEFTY;
        moveY.deadzone = 0.20f;
        moveY.invert = false;
        moveY.scale = 1.0f;
        map.bindAxis(Axis::MoveY, moveY);
    }

    static bool KeyPressed(SDL_Scancode sc) {
        return HBE::Platform::Input::IsKeyPressed(sc);
    }

    static const char* kEntityTagPresets[] = {
        "Entity",
        "Prop",
        "Trigger",
        "Enemy",
        "Pickup",
        "Torch",
        "Marker",
        "Player",
        "Goblin",
    };
    static constexpr int kEntityTagPresetsCount = sizeof(kEntityTagPresets) / sizeof(kEntityTagPresets[0]);

    // Item 21: faction cycle in inspector. Must stay in sync with the
    // HBE::ECS::Faction enum ordering in CombatComponents.h.
    static const char* kFactionPresets[] = {
        "Neutral",
        "Player",
        "Enemy",
        "Environment",
    };
    static constexpr int kFactionPresetsCount =
        static_cast<int>(sizeof(kFactionPresets) / sizeof(kFactionPresets[0]));

    // Item 20: build a placeholder Sprite2D for "Add Component -> Sprite2D".
    // Uses whatever the sandbox already has wired up so the new sprite is
    // instantly visible. Passing pointers keeps this header-free.
    static HBE::Renderer::SpriteComponent2D MakeDefaultSprite(
        HBE::Renderer::Mesh* quadMesh,
        HBE::Renderer::Material* fallbackMat)
    {
        HBE::Renderer::SpriteComponent2D s{};
        s.mesh = quadMesh;
        s.material = fallbackMat;
        s.layer = 200;
        s.sortKey = 0.0f;
        s.sortOffsetY = 0.0f;
        s.uvRect[0] = 0.0f;
        s.uvRect[1] = 0.0f;
        s.uvRect[2] = 1.0f;
        s.uvRect[3] = 1.0f;
        return s;
    }
}

void GameLayer::onAttach(Application& app) {
    m_app = &app;
    m_console.setWindow(app.platform().getWindow());

    if (m_tileMapPath.empty())  m_tileMapPath = "maps/test_map.json";
    if (m_uiThemePath.empty())  m_uiThemePath = "ui/theme.json";
    if (m_spriteVsPath.empty()) m_spriteVsPath = "shaders/sprite.vert";
    if (m_spriteFsPath.empty()) m_spriteFsPath = "shaders/sprite.frag";

    HBE::Input::Initialize(&RegisterDefaultBindings);
    HBE::Input::Get().loadFromFile(HBE::Core::AssetPaths::ResolveUser(BINDINGS_LOGICAL));
    m_prefabs.loadDirectory("prefabs");
    registerScripts();
    registerAnimatorPresets();
    registerHitboxPresets();
    HBE::Core::LogInfo(std::string("CWD: ") + std::filesystem::current_path().string());

    m_camera.x = std::round(m_camera.x);
    m_camera.y = std::round(m_camera.y);
    m_camera.zoom = 1.0f;
    m_camera.viewportWidth = LOGICAL_WIDTH;
    m_camera.viewportHeight = LOGICAL_HEIGHT;

    m_text.setCullInset(8.0f);

    app.gl().setCamera(m_camera);
    app.gl().setClearColor(0.1f, 0.2f, 0.35f, 1.0f);

    buildSpritePipeline();

    auto& ppResources = m_app->resources();
    namespace ap = HBE::Core::AssetPaths;
    const std::string ppVs = ap::Resolve("shaders/pp_pass.vert");

    auto* bloomShader = ppResources.getOrCreateShaderFromFiles("pp_bloom",
        ppVs, ap::Resolve("shaders/pp_bloom.frag"));

    auto* crtShader = ppResources.getOrCreateShaderFromFiles("pp_crt",
        ppVs, ap::Resolve("shaders/pp_crt.frag"));

    auto* vigShader = ppResources.getOrCreateShaderFromFiles("pp_vignette",
        ppVs, ap::Resolve("shaders/pp_vignette.frag"));

    auto* gradeShader = ppResources.getOrCreateShaderFromFiles("pp_colorgrade",
        ppVs, ap::Resolve("shaders/pp_colorgrade.frag"));

    auto* pixelShader = ppResources.getOrCreateShaderFromFiles("pp_pixelate",
        ppVs, ap::Resolve("shaders/pp_pixelate.frag"));

    if (m_postProcess.initialize(static_cast<int>(LOGICAL_WIDTH), static_cast<int>(LOGICAL_HEIGHT))) {
        {
            HBE::Renderer::PostProcessEffect fx{};
            fx.name = "bloom";
            fx.shader = bloomShader;
            fx.enabled = false;
            fx.params[0] = 0.7f;
            fx.params[1] = 1.2f;
            m_postProcess.addEffect(std::move(fx));
        }
        {
            HBE::Renderer::PostProcessEffect fx{};
            fx.name = "colorgrade";
            fx.shader = gradeShader;
            fx.enabled = false;
            fx.params[0] = 0.0f;
            fx.params[1] = 1.0f;
            fx.params[2] = 1.0f;
            m_postProcess.addEffect(std::move(fx));
        }
        {
            HBE::Renderer::PostProcessEffect fx{};
            fx.name = "vignette";
            fx.shader = vigShader;
            fx.enabled = true;
            fx.params[0] = 0.75f;
            fx.params[1] = 0.45f;
            m_postProcess.addEffect(std::move(fx));
        }
        {
            HBE::Renderer::PostProcessEffect fx{};
            fx.name = "crt";
            fx.shader = crtShader;
            fx.enabled = false;
            fx.params[0] = 0.3f;
            fx.params[1] = 4.0f;
            m_postProcess.addEffect(std::move(fx));
        }
        {
            HBE::Renderer::PostProcessEffect fx{};
            fx.name = "pixelate";
            fx.shader = pixelShader;
            fx.enabled = false;
            fx.params[0] = 4.0f;
            m_postProcess.addEffect(std::move(fx));
        }

        m_fxBloom = m_postProcess.getEffect("bloom");
        m_fxColorGrade = m_postProcess.getEffect("colorgrade");
        m_fxVignette = m_postProcess.getEffect("vignette");
        m_fxCrt = m_postProcess.getEffect("crt");
        m_fxPixelate = m_postProcess.getEffect("pixelate");

        m_app->gl().setPostProcessStack(&m_postProcess);

        LogInfo("Post-process stack initialized.");
    }
    else {
        LogError("Post-process stack failed to initialize — running without effects.");
    }

    m_particles.initialize(app.resources(), m_quadMesh);
    m_particles.registerEffect("rain", makeRain());
    m_particles.registerEffect("player_dust", makePlayerDust());
    m_rainHandle = m_particles.spawnManaged("rain", m_camera.x, m_camera.y);

    HBE::Renderer::UI::UIStyle style;
    style.textScale = 1.0f;
    style.itemH = 34.0f;
    style.padding = 12.0f;
    style.spacing = 10.0f;
    m_ui.setStyle(style);

    std::string err;
    if (!HBE::Renderer::TileMapLoader::loadFromJsonFile(
        ap::Resolve(m_tileMapPath), m_tileMap, &err))
    {
        LogFatal("Failed to load tilemap: " + err);
        m_app->requestQuit();
        return;
    }

    if (!m_tileRenderer.build(m_app->renderer2D(), m_app->resources(), m_spriteShader, m_quadMesh, m_tileMap)) {
        LogFatal("Failed to build TileMapRenderer");
        m_app->requestQuit();
        return;
    }

    setupHotReloadWatches();
    hotReloadUITheme();

    app.audio().setMasterGain(1.0f);
    app.audio().setBusGain(HBE::Platform::Audio::Bus::Music, 0.65f);
    app.audio().setBusGain(HBE::Platform::Audio::Bus::SFX, 1.0f);
    app.audio().setBusGain(HBE::Platform::Audio::Bus::UI, 0.90f);
    app.audio().setBusGain(HBE::Platform::Audio::Bus::Ambient, 0.80f);

    const std::string testSoundPath = ap::Resolve("audio/test_sound.wav");
    //app.audio().loadSound("footstep", testSoundPath, true);
    //app.audio().loadSound("hit", testSoundPath, true);
    //app.audio().loadSound("ui_blip", testSoundPath, true);

    m_collisionLayer = m_tileMap.findLayer("Ground");
    if (!m_collisionLayer) {
        LogFatal("Tilemap missing collision layer named 'Ground'");
        m_app->requestQuit();
        return;
    }

    if (!HBE::Renderer::TileMapLoader::sampleTileTopColors(m_tileMap, m_tileTopColors)) {
        HBE::Core::LogInfo("TileMapLoader::sampleTileTopColors failed; player dust will use fallback color.");
    }

    m_scene.setTileCollisionContext(&m_tileMap, m_collisionLayer);

    {
        HBE::Renderer::Physics2DSettings phys{};
        phys.gravityY = -1800.0f;      
        phys.maxSubSteps = 4;          
        phys.maxStepDt = 1.0f / 120.0f;
        m_scene.setPhysics2DSettings(phys);
    }
    m_console.print("Console ready. Type 'help'.");

    m_console.registerCommand("help", "List commands", [this](const std::vector<std::string>&) {
        m_console.print("Commands:");
        m_console.print("  help");
        m_console.print("  clear");
        m_console.print("  cull [0/1]");
        m_console.print("  colliders [0/1]");
        m_console.print("  gravity <value>");
        m_console.print("  tp <x> <y>       (teleport player/soldier)");
        m_console.print("  reload_ui");
        m_console.print("  reload_shader");
        });

    m_console.registerCommand("clear", "Clear console output", [this](const std::vector<std::string>&) {
        m_console.clear();
        });

    m_console.registerCommand("cull", "cull [0/1] - enable/disable scene culling", [this](const std::vector<std::string>& args) {
        if (args.empty()) {
            m_console.print(std::string("cull = ") + (m_enableCulling ? "1" : "0"));
            return;
        }
        m_enableCulling = (args[0] != "0");
        m_scene.setCullingEnabled(m_enableCulling);
        m_console.print(std::string("cull set to ") + (m_enableCulling ? "1" : "0"));
        });

    m_console.registerCommand("colliders", "colliders [0/1] - show collider overlay", [this](const std::vector<std::string>& args) {
        if (args.empty()) {
            m_console.print(std::string("colliders = ") + (m_debugDraw ? "1" : "0"));
            return;
        }
        m_debugDraw = (args[0] != "0");
        m_console.print(std::string("colliders set to ") + (m_debugDraw ? "1" : "0"));
        });

    m_console.registerCommand("gravity", "gravity <value> - set physics gravityY", [this](const std::vector<std::string>& args) {
        if (args.size() < 1) {
            m_console.print("Usage: gravity <value>");
            return;
        }
        float g = std::stof(args[0]);
        auto s = m_scene.physics2DSettings();
        s.gravityY = g;
        m_scene.setPhysics2DSettings(s);
        m_console.print("gravityY set.");
        });

    m_console.registerCommand("tp", "tp <x> <y> - teleport player", [this](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            m_console.print("Usage: tp <x> <y>");
            return;
        }
        float x = std::stof(args[0]);
        float y = std::stof(args[1]);

        auto& reg = m_scene.registry();
        if (reg.valid(m_soldierEntity) && reg.has<HBE::Renderer::Transform2D>(m_soldierEntity)) {
            auto& tr = reg.get<HBE::Renderer::Transform2D>(m_soldierEntity);
            tr.posX = x;
            tr.posY = y;
            m_console.print("Teleported player.");
        }
        else {
            m_console.print("Player entity invalid.");
        }
        });

    m_console.registerCommand("reload_ui", "Hot reload UI theme", [this](const std::vector<std::string>&) {
        hotReloadUITheme();
        m_console.print("UI theme reloaded.");
        });

    m_console.registerCommand("reload_shader", "Hot reload sprite shader", [this](const std::vector<std::string>&) {
        hotReloadShader();
        m_console.print("Sprite shader reloaded.");
        });

    m_scene.setCullingEnabled(m_enableCulling);

    auto& reg = m_scene.registry();

    if (reg.valid(m_soldierEntity)) {
        reg.emplace<HBE::ECS::TagComponent>(m_soldierEntity, HBE::ECS::TagComponent{ "Player" });
    }
    if (reg.valid(m_goblinEntity)) {
        reg.emplace<HBE::ECS::TagComponent>(m_goblinEntity, HBE::ECS::TagComponent{ "Goblin" });
    }

    if (reg.valid(m_soldierEntity)) {
        const float pxScale = SPRITE_PIXEL_SCALE;

        HBE::ECS::Collider2D col{};
        col.halfW = 0.5f * (PLAYER_BODY_W_PX * pxScale);
        col.halfH = 0.5f * (PLAYER_BODY_H_PX * pxScale);
        col.offsetX = 0.0f;
        col.offsetY = (PLAYER_BODY_Y_OFFSET_PX * pxScale);

        if (!reg.has<HBE::ECS::Collider2D>(m_soldierEntity))
            reg.emplace<HBE::ECS::Collider2D>(m_soldierEntity, col);

        HBE::ECS::RigidBody2D rb{};
        rb.linearDamping = 0.0f;
        rb.isStatic = false;

        rb.useGravity = true;
        rb.gravityScale = 1.0f;
        rb.maxFallSpeed = -2200.0f;
        rb.maxStepUp = m_tileMap.worldTileH() * 0.35f;
        rb.enableOneWay = true;
        rb.enableSlopes = true;

        if (!reg.has<HBE::ECS::RigidBody2D>(m_soldierEntity))
            reg.emplace<HBE::ECS::RigidBody2D>(m_soldierEntity, rb);

        if (!reg.has<HBE::ECS::Script>(m_soldierEntity)) {
            HBE::ECS::Script sc = m_scripts.create("PlayerController", m_soldierEntity, m_scene);
            reg.emplace<HBE::ECS::Script>(m_soldierEntity, std::move(sc));
        }
    }

    if (reg.valid(m_goblinEntity)) {
        const float pxScale = SPRITE_PIXEL_SCALE;

        HBE::ECS::Collider2D col{};
        col.halfW = 0.5f * (GOBLIN_BODY_W_PX * pxScale);
        col.halfH = 0.5f * (GOBLIN_BODY_H_PX * pxScale);
        col.offsetX = 0.0f;
        col.offsetY = (GOBLIN_BODY_Y_OFFSET_PX * pxScale);

        if (!reg.has<HBE::ECS::Collider2D>(m_goblinEntity))
            reg.emplace<HBE::ECS::Collider2D>(m_goblinEntity, col);

        HBE::ECS::RigidBody2D rb{};
        rb.isStatic = true;
        rb.linearDamping = 0.0f;

        if (!reg.has<HBE::ECS::RigidBody2D>(m_goblinEntity))
            reg.emplace<HBE::ECS::RigidBody2D>(m_goblinEntity, rb);

        if (!reg.has<HBE::ECS::Script>(m_goblinEntity)) {
            HBE::ECS::Script sc = m_scripts.create("GoblinAI",
                m_goblinEntity,
                m_scene);
            reg.emplace<HBE::ECS::Script>(m_goblinEntity, std::move(sc));
        }
    }
    {
        auto& reg = m_scene.registry();

        // -- Player: Faction + Hurtbox --
        if (reg.valid(m_soldierEntity)) {
            if (!reg.has<HBE::ECS::FactionComponent>(m_soldierEntity)) {
                reg.emplace<HBE::ECS::FactionComponent>(
                    m_soldierEntity,
                    HBE::ECS::FactionComponent{ HBE::ECS::Faction::Player });
            }
            if (!reg.has<HBE::ECS::Hurtbox>(m_soldierEntity)) {
                HBE::ECS::Hurtbox hb{};
                // zero extents → CombatSystem falls back to Collider2D.
                reg.emplace<HBE::ECS::Hurtbox>(m_soldierEntity, hb);
            }
            if (!reg.has<HBE::ECS::Health>(m_soldierEntity)) {
                reg.emplace<HBE::ECS::Health>(
                    m_soldierEntity,
                    HBE::ECS::Health{ 5, 5 });
            }
        }

        // -- Goblin: Faction + Hurtbox + Health --
        if (reg.valid(m_goblinEntity)) {
            if (!reg.has<HBE::ECS::FactionComponent>(m_goblinEntity)) {
                reg.emplace<HBE::ECS::FactionComponent>(
                    m_goblinEntity,
                    HBE::ECS::FactionComponent{ HBE::ECS::Faction::Enemy });
            }
            if (!reg.has<HBE::ECS::Hurtbox>(m_goblinEntity)) {
                reg.emplace<HBE::ECS::Hurtbox>(m_goblinEntity,
                    HBE::ECS::Hurtbox{});
            }
            if (!reg.has<HBE::ECS::Health>(m_goblinEntity)) {
                reg.emplace<HBE::ECS::Health>(m_goblinEntity,
                    HBE::ECS::Health{ 3, 3 });
            }
        }

        if (auto* pTr = m_scene.getTransform(m_soldierEntity)) {
            m_demo.playerSpawnX = pTr->posX;
            m_demo.playerSpawnY = pTr->posY;
        }
        if (auto* gTr = m_scene.getTransform(m_goblinEntity)) {
            m_demo.goblinSpawnX = gTr->posX;
            m_demo.goblinSpawnY = gTr->posY;
        }
        m_demo.state = HBE::Sandbox::DemoState::Playing;
        m_demo.endBannerTimer = 0.0f;
    }

    LogInfo("GameLayer attached.");
}

void GameLayer::buildSpritePipeline() {
    auto& resources = m_app->resources();

    m_spriteShader = resources.getOrCreateShaderFromFiles(
        "sprite",
        HBE::Core::AssetPaths::Resolve(m_spriteVsPath),
        HBE::Core::AssetPaths::Resolve(m_spriteFsPath));

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
        if (!m_spriteShader) {
            LogFatal("GameLayer: failed to create sprite shader.");
            m_app->requestQuit();
            return;
        }
    }

    std::vector<float> quadVerts = {
        // tri 1
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f,

         // tri 2
          0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
         -0.5f,  0.5f, 0.0f,   0.0f, 1.0f,
         -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
    };

    m_quadMesh = resources.getOrCreateMeshPosUV("quad", quadVerts, 6);
    if (!m_quadMesh) {
        LogFatal("GameLayer: failed to create quad mesh.");
        m_app->requestQuit();
        return;
    }

    m_app->renderer2D().setSpriteQuadMesh(m_quadMesh);

    if (!m_debug.initialize(resources, m_quadMesh)) {
        LogFatal("GameLayer: DebugDraw2D init failed");
        m_app->requestQuit();
        return;
    }

    if (!m_text.initialize(m_app->resources(), m_spriteShader, m_quadMesh)) {
        LogFatal("GameLayer: TextRenderer2D init failed");
        m_app->requestQuit();
        return;
    }

    const std::string fontPath = HBE::Core::AssetPaths::Resolve("fonts/BoldPixels.ttf");
    if (!m_text.loadSDFont(m_app->resources(),
        "ui",
        fontPath,
        16.0f,
        1024, 1024,
        12))
    {
        LogError("FAILED to load font: " + fontPath);
    }
    else {
        m_text.setActiveFont("ui");
    }

    SpriteSheetDesc desc{};
    desc.frameWidth = 100;
    desc.frameHeight = 100;

    m_goblinSheet = SpriteRenderer2D::declareSpriteSheet(resources, "orc_sheet", HBE::Core::AssetPaths::Resolve("Orc.png"), desc);
    m_soldierSheet = SpriteRenderer2D::declareSpriteSheet(resources, "soldier_sheet", HBE::Core::AssetPaths::Resolve("Soldier.png"), desc);

    if (!m_goblinSheet.texture) {
        LogFatal("GameLayer: failed to load Orc.png (asset root: " + HBE::Core::AssetPaths::AssetRootString() + ")");
        m_app->requestQuit();
        return;
    }
    if (!m_soldierSheet.texture) {
        LogFatal("GameLayer: failed to load Soldier.png (asset root: " + HBE::Core::AssetPaths::AssetRootString() + ")");
        m_app->requestQuit();
        return;
    }

    m_goblinMaterial.shader = m_spriteShader;
    m_goblinMaterial.texture = m_goblinSheet.texture;

    m_soldierMaterial.shader = m_spriteShader;
    m_soldierMaterial.texture = m_soldierSheet.texture;

    RenderItem goblin{};
    goblin.mesh = m_quadMesh;
    goblin.material = &m_goblinMaterial;
    goblin.transform.posX = 1080.0f;
    goblin.transform.posY = 95.0f;
    goblin.transform.scaleX = desc.frameWidth * SPRITE_PIXEL_SCALE;
    goblin.transform.scaleY = desc.frameHeight * SPRITE_PIXEL_SCALE;
    goblin.layer = 100;
    goblin.sortKey = goblin.transform.posY;

    RenderItem soldier{};
    soldier.mesh = m_quadMesh;
    soldier.material = &m_soldierMaterial;
    soldier.transform.posX = 64.0f;
    soldier.transform.posY = 95.0f;
    soldier.transform.scaleX = desc.frameWidth * SPRITE_PIXEL_SCALE;
    soldier.transform.scaleY = desc.frameHeight * SPRITE_PIXEL_SCALE;

    soldier.layer = 200;
    soldier.sortKey = soldier.transform.posY;

    SpriteRenderer2D::setFrame(goblin, m_goblinSheet, 0, 0);
    SpriteRenderer2D::setFrame(soldier, m_soldierSheet, 0, 0);

    m_goblinEntity  = m_scene.createEntity(goblin);
    m_soldierEntity = m_scene.createEntity(soldier);

    if (auto* gAnim = m_scene.addSpriteAnimator(m_goblinEntity, &m_goblinSheet)) {
        m_animPresets.build("GoblinAnimator", m_goblinEntity, *gAnim, m_scene);
        gAnim->setState("Idle", true);
        SetAnimatorPreset(m_scene.registry(), m_goblinEntity, "GoblinAnimator");
    }

    if (auto* sAnim = m_scene.addSpriteAnimator(m_soldierEntity, &m_soldierSheet)) {
        m_animPresets.build("SoldierAnimator", m_soldierEntity, *sAnim, m_scene);
        sAnim->setState("Idle", true);
        SetAnimatorPreset(m_scene.registry(), m_soldierEntity, "SoldierAnimator");
    }
}

void GameLayer::onUpdate(float dt) {
    if (HBE::Input::ActionPressed(HBE::Input::Action::FullscreenToggle)) {

    }

    Transform2D* playerTr = m_scene.getTransform(m_soldierEntity);
    if (!playerTr) return;
    m_app->audio().setListenerPosition(playerTr->posX, playerTr->posY);

    if (KeyPressed(SDL_SCANCODE_T)) {
        HBE::Platform::Audio::PlayParams p;
        p.bus = HBE::Platform::Audio::Bus::SFX;
        p.gain = 1.0f;
        p.positional = true;
        p.worldX = playerTr->posX + 120.0f;
        p.worldY = playerTr->posY;
        p.minDistance = 10.0f;
        p.maxDistance = 350.0f;
        p.panRange = 240.0f;

        m_app->audio().playSoundEx("hit", p);
    }

    if (KeyPressed(SDL_SCANCODE_Y)) {
        m_app->audio().pauseBus(HBE::Platform::Audio::Bus::SFX);
    }

    if (KeyPressed(SDL_SCANCODE_U)) {
        m_app->audio().resumeBus(HBE::Platform::Audio::Bus::SFX);
    }

    m_uiAnimT += dt;
    m_statTimer += (double)dt;
    m_updateCount++;
    m_lastDt = dt;

    if (m_statTimer >= 0.5) {
        const double window = m_statTimer;

        m_fps = (float)((double)m_frameCount / window);
        m_ups = (float)((double)m_updateCount / window);

        m_frameCount = 0;
        m_updateCount = 0;
        m_statTimer = 0.0;
    }

    if (KeyPressed(SDL_SCANCODE_F5)) {
        saveSceneNow();
    }

    if (KeyPressed(SDL_SCANCODE_F9)) {
        std::string tilemapPath;
        std::string err;

        HBE::Renderer::SceneLoadCallbacks loadCb{};
        loadCb.mesh = [this](const std::string& key) -> HBE::Renderer::Mesh* {
            if (key == "quad") return m_quadMesh;
            return nullptr;
            };
        loadCb.material = [this](const std::string& key) -> HBE::Renderer::Material* {
            if (key == "goblin_mat") return &m_goblinMaterial;
            if (key == "soldier_mat") return &m_soldierMaterial;
            return nullptr;
            };
        loadCb.sheet = [this](const std::string& key) -> const HBE::Renderer::SpriteRenderer2D::SpriteSheetHandle* {
            if (key == "orc_sheet") return &m_goblinSheet;
            if (key == "soldier_sheet") return &m_soldierSheet;
            return nullptr;
            };

        loadCb.prefabs = &m_prefabs;
        loadCb.scripts = &m_scripts;
        loadCb.animators = &m_animPresets;

        namespace ap = HBE::Core::AssetPaths;
        std::string scenePath = ap::ResolveUser(SCENE_LOGICAL);
        {
            std::error_code ec;
            if (!std::filesystem::exists(scenePath, ec)) {
                scenePath = ap::Resolve(SCENE_LOGICAL);
            }
        }

        const bool ok = HBE::Renderer::SceneSerializer::loadFromFile(
            m_scene, scenePath, loadCb, &tilemapPath, &err);

        if (!ok) {
            LogError("Scene load FAILED: " + err);
        }
        else {
            LogInfo("Scene loaded: " + scenePath);

            if (!tilemapPath.empty()) {
                // The serializer already returned a logical path
                // (item 14 doc 08).
                m_tileMapPath = tilemapPath;
            }

            hotReloadTileMap();

            m_soldierEntity = {};
            m_goblinEntity = {};

            auto& reg = m_scene.registry();
            for (auto ent : reg.view<HBE::ECS::TagComponent>()) {
                const auto& tag = reg.get<HBE::ECS::TagComponent>(ent).tag;
                if (tag == "Player")  m_soldierEntity = ent;
                if (tag == "Goblin")  m_goblinEntity = ent;
            }
        }

        float instFps = (dt > 0.00001f) ? (1.0f / dt) : 0.0f;
        m_fpsHistory.push_back(instFps);
        if ((int)m_fpsHistory.size() > m_fpsHistoryMax) {
            m_fpsHistory.erase(m_fpsHistory.begin(), m_fpsHistory.begin() + (m_fpsHistory.size() - m_fpsHistoryMax));
        }
    }
    if (KeyPressed(SDL_SCANCODE_R)) {
        const bool allowRestart =
            (m_demo.state == HBE::Sandbox::DemoState::Playing) ||
            (m_demo.endBannerTimer >= HBE::Sandbox::demo::RESTART_DELAY);

        if (allowRestart) {
            HBE::Core::LogInfo("Demo: restart requested (R).");

            std::string err;
            if (!loadSceneNow(&err)) {
                LogError("Demo restart FAILED: " + err);
            }
            else {
                auto& reg = m_scene.registry();
                if (auto* pTr = m_scene.getTransform(m_soldierEntity)) {
                    m_demo.playerSpawnX = pTr->posX;
                    m_demo.playerSpawnY = pTr->posY;
                }
                if (auto* gTr = m_scene.getTransform(m_goblinEntity)) {
                    m_demo.goblinSpawnX = gTr->posX;
                    m_demo.goblinSpawnY = gTr->posY;
                }

                resetDemoState();
                spawnPopup(20.0f, 100.0f, "Restarted.",
                    HBE::Renderer::Color4{ 0.6f, 1.0f, 0.6f, 1.0f },
                    1.0f, 0.0f);
            }
        }
    }
    m_watcher.poll(dt);
    m_scene.update(dt, [&](const std::string& ev) {
        Transform2D* tr = m_scene.getTransform(m_soldierEntity);
        const float px = tr ? tr->posX : m_camera.x;
        const float py = tr ? tr->posY : m_camera.y;

        if (ev == "footstep") {
            spawnPopup(px, py - 30.0f, "step", Color4{ 0.8f,0.9f,1.0f,1.0f }, 0.35f, 35.0f);

            //HBE::Platform::Audio::PlayParams p;
            //p.bus = HBE::Platform::Audio::Bus::SFX;
            //p.gain = 0.55f;
            //p.positional = true;
            //p.worldX = px;
            //p.worldY = py;
            //p.minDistance = 24.0f;
            //p.maxDistance = 260.0f;
            //p.panRange = 220.0f;

            //m_app->audio().playSoundEx("footstep", p);
        }
        else if (ev == "hitframe") {
            if (m_scene.registry().valid(m_soldierEntity)) {
                HBE::ECS::spawnHitboxByName(
                    m_scene,
                    m_soldierEntity,
                    m_hitboxPresets,
                    "SoldierSlash");
            }
        }
    });

    {
        auto& reg = m_scene.registry();
        if (reg.has<HBE::ECS::RigidBody2D>(m_soldierEntity) &&
            reg.has<HBE::ECS::Collider2D>(m_soldierEntity))
        {
            const auto& body = reg.get<HBE::ECS::RigidBody2D>(m_soldierEntity);
            const auto& col  = reg.get<HBE::ECS::Collider2D>(m_soldierEntity);

            const bool grounded = body.grounded;
            const bool moving   = std::fabs(body.velX) > 60.0f;

            if (grounded && moving) {
                m_playerDustCooldown -= dt;
                if (m_playerDustCooldown <= 0.0f) {
                    const float feetX = playerTr->posX + col.offsetX;
                    const float feetY = playerTr->posY + col.offsetY - col.halfH;
                    spawnPlayerDustAtFeet(feetX, feetY);
                    m_playerDustCooldown = 0.14f;
                }
            }
            else {
                m_playerDustCooldown = 0.0f;
            }
        }
    }
    if (!m_rainPreWarmed) {
        m_camera.x = playerTr->posX;
        m_camera.y = playerTr->posY;
        m_app->gl().setCamera(m_camera);
    }

    if (m_particles.isAlive(m_rainHandle))
        m_particles.setPosition(m_rainHandle, m_camera.x, m_camera.y);

    if (!m_rainPreWarmed) {
        for (int i = 0; i < 60; ++i) {
            m_particles.update(1.0f / 30.0f);
        }
        m_rainPreWarmed = true;
    }

    m_particles.update(dt);

    // Camera lerp — feels smoother than snap. Item 22 will replace this
    // with a proper deadzone / look-ahead system.
    m_camera.x = LerpF(m_camera.x, playerTr->posX, HBE::Sandbox::demo::CAMERA_LERP);
    m_camera.y = LerpF(m_camera.y, playerTr->posY, HBE::Sandbox::demo::CAMERA_LERP);
    m_app->gl().setCamera(m_camera);

    updateDemoLogic(dt);

    for (auto& p : m_popups) {
        p.life -= dt;
        p.y += p.floatSpeed * dt;
    }

    m_popups.erase(
        std::remove_if(m_popups.begin(), m_popups.end(),
            [](const DebugPopup& p) { return p.life <= 0.0f; }),
        m_popups.end()
    );
}

void GameLayer::onRender() {
    m_frameCount++;
    m_text.beginFrame(m_frameCount);

    Renderer2D& r2d = m_app->renderer2D();
    m_ui.beginFrame(m_lastDt);

    r2d.beginScene(m_camera, HBE::Renderer::RenderPass::World);

    m_tileRenderer.draw(r2d, m_tileMap);

    m_scene.render(r2d);
    m_particles.render(r2d);

    if (m_debugDraw) {
        auto& reg = m_scene.registry();

        if (m_drawAllColliders) {
            for (auto e : reg.view<HBE::Renderer::Transform2D, HBE::ECS::Collider2D>()) {
                auto& tr = reg.get<HBE::Renderer::Transform2D>(e);
                auto& col = reg.get<HBE::ECS::Collider2D>(e);

                float cx = tr.posX + col.offsetX;
                float cy = tr.posY + col.offsetY;
                float w = col.halfW * 2.0f;
                float h = col.halfH * 2.0f;
                float r = 0.0f, g = 1.0f, b = 0.0f, a = 1.0f;

                if (col.isTrigger) { r = 1.0f; g = 1.0f; b = 0.2f; }
                if (reg.has<HBE::ECS::RigidBody2D>(e) && reg.get<HBE::ECS::RigidBody2D>(e).isStatic) { r = 0.2f; g = 0.9f; b = 1.0f; }

                m_debug.rect(r2d, cx, cy, w, h, r, g, b, a, false);
            }

            // -- Item 21: Hurtboxes (blue) --
            for (auto e : reg.view<HBE::ECS::Hurtbox, HBE::Renderer::Transform2D>()) {
                const auto& hb = reg.get<HBE::ECS::Hurtbox>(e);
                const auto& tr = reg.get<HBE::Renderer::Transform2D>(e);

                float hw = hb.halfW, hh = hb.halfH;
                float ox = hb.offsetX, oy = hb.offsetY;

                if (hw <= 0.0f || hh <= 0.0f) {
                    if (!reg.has<HBE::ECS::Collider2D>(e)) continue;
                    const auto& col = reg.get<HBE::ECS::Collider2D>(e);
                    hw = col.halfW; hh = col.halfH;
                    ox = col.offsetX; oy = col.offsetY;
                }

                m_debug.rect(r2d,
                    tr.posX + ox, tr.posY + oy,
                    hw * 2.0f, hh * 2.0f,
                    0.35f, 0.55f, 1.0f, hb.active ? 0.6f : 0.2f,
                    false);
            }

            // -- Item 21: Hitboxes (red). Facing mirrors CombatSystem math --
            for (auto e : reg.view<HBE::ECS::Hitbox>()) {
                const auto& hb = reg.get<HBE::ECS::Hitbox>(e);

                const HBE::Renderer::Transform2D* tr = nullptr;
                if (hb.follows && reg.valid(hb.owner)
                    && reg.has<HBE::Renderer::Transform2D>(hb.owner)) {
                    tr = &reg.get<HBE::Renderer::Transform2D>(hb.owner);
                }
                if (!tr && reg.has<HBE::Renderer::Transform2D>(e)) {
                    tr = &reg.get<HBE::Renderer::Transform2D>(e);
                }
                if (!tr) continue;

                const float facing = (tr->scaleX < 0.0f) ? -1.0f : +1.0f;

                m_debug.rect(r2d,
                    tr->posX + hb.offsetX * facing,
                    tr->posY + hb.offsetY,
                    hb.halfW * 2.0f, hb.halfH * 2.0f,
                    1.0f, 0.35f, 0.35f, hb.active ? 0.9f : 0.3f,
                    false);
            }
        }
    }

    for (const auto& p : m_popups) {
        float t = (p.maxLife > 0.0f) ? (p.life / p.maxLife) : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);

        Color4 c = p.color;
        c.a *= t;

        m_text.drawText(r2d, p.x, p.y, p.text, 1.0f, c);
    }

    r2d.endScene();

    HBE::Renderer::Camera2D uiCam{};
    uiCam.x = LOGICAL_WIDTH * 0.5f;
    uiCam.y = LOGICAL_HEIGHT * 0.5f;
    uiCam.zoom = 1.0f;
    uiCam.viewportWidth = LOGICAL_WIDTH;
    uiCam.viewportHeight = LOGICAL_HEIGHT;

    r2d.beginScene(uiCam, HBE::Renderer::RenderPass::UI);

    // Item 19 · demo HUD overlay (HP top-right, WIN/LOSE banner).
    drawDemoHUD(r2d);

    m_ui.bind(&m_app->renderer2D(), &m_debug, &m_text);

    using namespace HBE::Renderer::UI;

    UIRect tools;
    tools.x = 20.0f;
    tools.y = 20.0f;
    tools.w = 320.0f;
    tools.h = 280.0f;

    if (m_showDevTools) {
        auto savedStyle = m_ui.style();
        {
            auto s = savedStyle;
            s.itemH = 28.0f;
            s.padding = 10.0f;
            s.spacing = 6.0f;
            s.textScale = 0.90f;
            m_ui.setStyle(s);
        }

        m_ui.beginPanel("dev_tools", tools, "Dev Tools");

        m_ui.checkbox("chk_cull", "Scene Culling", m_enableCulling);
        m_scene.setCullingEnabled(m_enableCulling);

        m_ui.checkbox("chk_colliders", "Collision Overlay", m_debugDraw);
        m_ui.checkbox("chk_all_colliders", "Draw All Colliders", m_drawAllColliders);

        m_ui.checkbox("chk_fps_graph", "Show FPS Graph", m_showFpsGraph);

        m_ui.spacing(8.0f);

        if (m_ui.button("btn_reload_ui", "Reload UI")) hotReloadUITheme();
        if (m_ui.button("btn_reload_shader", "Reload Shader")) hotReloadShader();

        m_ui.spacing(2.0f);

        auto saved = m_ui.style();
        {
            auto s = saved;
            s.textScale = 0.78f;
            s.itemH = 18.0f;
            m_ui.setStyle(s);
        }

        m_ui.label("F1 all   F2 tools   F3 inspect   F4 console   F5 fps", true);

        m_ui.setStyle(saved);

        m_ui.spacing(8.0f);
        m_ui.label("Scene", true);

        if (m_ui.button("btn_scene_save", "F5  Save Scene")) {
            saveSceneNow();
            spawnPopup(20.0f, 100.0f, "Scene saved.",
                HBE::Renderer::Color4{ 0.7f, 1.0f, 0.7f, 1.0f },
                1.0f, 0.0f);
        }

        if (m_ui.button("btn_scene_load", "F9  Load Scene")) {
            std::string err;
            if (loadSceneNow(&err)) {
                spawnPopup(20.0f, 100.0f, "Scene loaded.",
                    HBE::Renderer::Color4{ 0.7f, 1.0f, 0.7f, 1.0f },
                    1.0f, 0.0f);
            }
            else {
                spawnPopup(20.0f, 100.0f, "Load FAILED: " + err,
                    HBE::Renderer::Color4{ 1.0f, 0.4f, 0.4f, 1.0f },
                    2.0f, 0.0f);
            }
        }

        m_ui.endPanel();

        UIRect ppRect{ 20.0f, LOGICAL_HEIGHT - 440.0f, 340.0f, 390.0f };
        if (m_ui.beginPanel("pp_panel", ppRect, "Post FX")) {

            auto checkbox = [&](const char* id, const char* label, HBE::Renderer::PostProcessEffect* fx) {
                if (!fx) return;
                m_ui.checkbox(id, label, fx->enabled);
            };

            auto slider = [&](const char* id, const char* label, float& val, float lo, float hi) {
                m_ui.sliderFloat(id, label, val, lo, hi);
            };

            checkbox("fx_bloom",      "Bloom",       m_fxBloom);
            if (m_fxBloom && m_fxBloom->enabled) {
                slider("fx_bloom_thresh", "  Threshold", m_fxBloom->params[0], 0.0f, 1.0f);
                slider("fx_bloom_int",    "  Intensity",  m_fxBloom->params[1], 0.0f, 3.0f);
            }

            checkbox("fx_grade",      "Color Grade",  m_fxColorGrade);
            if (m_fxColorGrade && m_fxColorGrade->enabled) {
                slider("fx_grade_br",  "  Brightness", m_fxColorGrade->params[0], -0.5f, 0.5f);
                slider("fx_grade_con", "  Contrast",   m_fxColorGrade->params[1],  0.0f, 3.0f);
                slider("fx_grade_sat", "  Saturation", m_fxColorGrade->params[2],  0.0f, 2.0f);
            }

            checkbox("fx_vig",        "Vignette",     m_fxVignette);
            if (m_fxVignette && m_fxVignette->enabled) {
                slider("fx_vig_rad",  "  Radius",   m_fxVignette->params[0], 0.3f, 1.2f);
                slider("fx_vig_soft", "  Softness", m_fxVignette->params[1], 0.0f, 1.0f);
            }

            checkbox("fx_crt",        "CRT",          m_fxCrt);
            if (m_fxCrt && m_fxCrt->enabled) {
                slider("fx_crt_scan", "  Scanlines", m_fxCrt->params[0], 0.0f,  1.0f);
                slider("fx_crt_curv", "  Curvature", m_fxCrt->params[1], 1.0f, 12.0f);
            }

            checkbox("fx_pixel",      "Pixelate",     m_fxPixelate);
            if (m_fxPixelate && m_fxPixelate->enabled) {
                slider("fx_pixel_sz", "  Pixel Size", m_fxPixelate->params[0], 1.0f, 16.0f);
            }

            m_ui.endPanel();
        }
    }
    if (m_showFpsGraph) {
        auto savedStyle = m_ui.style();
        {
            auto s = savedStyle;
            s.textScale = 0.85f;
            s.itemH = 22.0f;
            s.padding = 8.0f;
            s.spacing = 4.0f;
            m_ui.setStyle(s);
        }

        UIRect g;
        g.x = 20.0f;
        g.y = 305.0f;
        g.w = 260.0f;
        g.h = 50.0f;

        m_ui.beginPanel("fps_graph", g, "FPS");

        char buf[128];
        std::snprintf(buf, sizeof(buf), "FPS %.1f  UPS %.1f  dt %.3f", m_fps, m_ups, m_lastDt);
        m_ui.label(buf, true);

        m_ui.endPanel();
        m_ui.setStyle(savedStyle);
    }
    UIRect insp;
    insp.x = LOGICAL_WIDTH - 420.0f;
    insp.y = 20.0f;
    insp.w = 400.0f;
    insp.h = 520.0f;

    if (m_showInspector) {
        drawInspector();
    }

    r2d.endScene();

    r2d.beginScene(uiCam, HBE::Renderer::RenderPass::Overlay);

    UIRect con;
    con.x = 20.0f;
    con.y = LOGICAL_HEIGHT - 220.0f;
    con.w = LOGICAL_WIDTH - 40.0f;
    con.h = 170.0f;

    m_console.draw(m_ui, con, m_lastDt);

    r2d.endScene();

    m_ui.endFrame();
}

bool GameLayer::onEvent(HBE::Core::Event& e) {
    using namespace HBE::Core;

    m_ui.onEvent(e);

    if (e.type() == EventType::KeyPressed) {
        auto& ke = static_cast<KeyPressedEvent&>(e);

        if (ke.keyScancode == SDL_SCANCODE_GRAVE) {
            m_console.toggle();
            e.handled = true;
            return true;
        }

        if (m_console.isOpen()) {
            m_console.onEvent(e);
            return e.handled;
        }

        if (ke.keyScancode == SDL_SCANCODE_F1) {
            bool anyOn = (m_showDevTools || m_showInspector || m_showFpsGraph);
            m_showDevTools = !anyOn;
            m_showInspector = !anyOn;
            m_showFpsGraph = !anyOn;
            e.handled = true;
            return true;
        }

        if (ke.keyScancode == SDL_SCANCODE_F2) { m_showDevTools = !m_showDevTools; e.handled = true; return true; }
        if (ke.keyScancode == SDL_SCANCODE_F3) { m_showInspector = !m_showInspector; e.handled = true; return true; }
        if (ke.keyScancode == SDL_SCANCODE_F4) { m_console.toggle(); e.handled = true; return true; }
        if (ke.keyScancode == SDL_SCANCODE_F5) { m_showFpsGraph = !m_showFpsGraph; e.handled = true; return true; }
    }

    if (m_console.isOpen()) {
        m_console.onEvent(e);
        if (e.handled) return true;
    }

    if (e.type() == EventType::WindowResize) {
        if (m_postProcess.isInitialized()) {
            m_postProcess.resize(static_cast<int>(LOGICAL_WIDTH), static_cast<int>(LOGICAL_HEIGHT));
        }
        return false;
    }

    return false;
}

void GameLayer::spawnPopup(float x, float y, const std::string& text,
    const HBE::Renderer::Color4& color, float lifetimeSeconds, float floatUpSpeed)
{
    DebugPopup p;
    p.text = text;
    p.x = x;
    p.y = y;
    p.color = color;
    p.life = lifetimeSeconds;
    p.maxLife = lifetimeSeconds;
    p.floatSpeed = floatUpSpeed;

    m_popups.push_back(std::move(p));
}

void GameLayer::spawnPlayerDustAtFeet(float feetX, float feetY) {
    using namespace HBE::Renderer;

    float r = 0.82f, g = 0.72f, b = 0.55f;

    if (m_collisionLayer) {
        const float tw = m_tileMap.worldTileW();
        const float th = m_tileMap.worldTileH();

        if (tw > 0.0f && th > 0.0f) {
            const int tx = (int)std::floor(feetX / tw);
            const int ty0 = (int)std::floor((feetY - 0.5f) / th);
            int tileId = 0;
            for (int probe = 0; probe < 3 && tileId == 0; ++probe) {
                tileId = m_collisionLayer->at(tx, ty0 - probe);
            }
            if (tileId != 0) {
                auto it = m_tileTopColors.find(tileId);
                if (it != m_tileTopColors.end()) {
                    r = it->second[0];
                    g = it->second[1];
                    b = it->second[2];
                }
            }
        }
    }

    EmitterConfig cfg = makePlayerDust()[0];
    cfg.startR = r;         cfg.startG = g;         cfg.startB = b;
    cfg.endR   = r * 0.55f; cfg.endG   = g * 0.55f; cfg.endB   = b * 0.55f;
    m_particles.registerEffect("player_dust", cfg);

    m_particles.spawn("player_dust", feetX, feetY);
}

void GameLayer::setupHotReloadWatches() {
    HBE::Core::FileWatcher::Options opt{};
    opt.pollIntervalSeconds = 0.20f;
    opt.debounceSeconds = 0.25f;
    m_watcher.setOptions(opt);

    m_watcher.watchFile(AssetPaths::Resolve(m_spriteVsPath), [this](const std::string&) { hotReloadShader(); });
    m_watcher.watchFile(AssetPaths::Resolve(m_spriteFsPath), [this](const std::string&) { hotReloadShader(); });
    m_watcher.watchFile(AssetPaths::Resolve(m_tileMapPath), [this](const std::string&) { hotReloadTileMap(); });
    m_watcher.watchFile(AssetPaths::Resolve(m_uiThemePath), [this](const std::string&) { hotReloadUITheme(); });

    auto watchTex = [this](const std::string& logicalPath, const std::string& cacheKey, const std::string& popupLabel) {
        m_watcher.watchFile(AssetPaths::Resolve(logicalPath), [this, cacheKey, popupLabel](const std::string&) {
            m_app->resources().reloadTexture(cacheKey); spawnPopup(20.0f, 590.0f, "Texture reloaded: " + popupLabel, HBE::Renderer::Color4{ 0.3f, 1.0f, 0.3f, 1.0f }, 1.0f, 0.0f);
            });
        };

    watchTex("Orc.png", "orc_sheet", "Orc.png");
    watchTex("Soldier.png", "soldier_sheet", "Soldier.png");
    watchTex("tiles/tiles.png", "basic", "tiles.png");
}

void GameLayer::hotReloadShader() {
    if (!m_app) return;
    bool ok = m_app->resources().reloadShader("sprite");
    spawnPopup(20.0f, 680.0f,
        ok ? "Shader reloaded: sprite" : "Shader reload FAILED (see log)",
        ok ? HBE::Renderer::Color4{ 0.3f, 1.0f, 0.3f, 1.0f } : HBE::Renderer::Color4{ 1.0f, 0.3f, 0.3f, 1.0f },
        1.25f, 0.0f);
}

void GameLayer::hotReloadTileMap() {
    HBE::Renderer::TileMap newMap{};
    std::string err;
    namespace ap = HBE::Core::AssetPaths;
    const std::string tmPath = ap::Resolve(m_tileMapPath);
    auto newMapBool = TileMapLoader::loadFromJsonFile(tmPath, newMap, &err);

    if (!newMapBool) {
        LogError("TileMap hot-reload: failed to load " + tmPath + " (" + err + ")");
        spawnPopup(20.0f, 650.0f, "Tilemap reload FAILED: " + err,
            HBE::Renderer::Color4{ 1.0f, 0.3f, 0.3f, 1.0f }, 1.5f, 0.0f);
        return;
    }
    auto groundLayer = newMap.findLayer("Ground");
    if (!groundLayer) {
        LogError("TileMap hot-reload: missing required 'Ground' layer");
        spawnPopup(20.0f, 650.0f, "Tilemap reload FAILED: missing required 'Ground' layer", HBE::Renderer::Color4{ 1.0f, 0.3f, 0.3f, 1.0f }, 1.5f, 0.0f);
        return;
    }
    m_tileMap = std::move(newMap);
    m_tileRenderer.build(m_app->renderer2D(), m_app->resources(), m_spriteShader, m_quadMesh, m_tileMap);
    m_collisionLayer = m_tileMap.findLayer("Ground");

    if (m_collisionLayer) {
        m_scene.setTileCollisionContext(&m_tileMap, m_collisionLayer);
    }

    HBE::Renderer::TileMapLoader::sampleTileTopColors(m_tileMap, m_tileTopColors);

    m_scene.setTileCollisionContext(&m_tileMap, m_collisionLayer);

    LogInfo("TileMap hot-reloaded succesfully: " + m_tileMapPath);
    spawnPopup(20.0f, 650.0f, "TileMap hot-reloaded succesfully",
        HBE::Renderer::Color4{ 0.3f, 1.0f, 0.3f, 1.0f }, 1.25f, 0.0f);
}

void GameLayer::hotReloadUITheme() {
    HBE::Renderer::UI::UIStyle s = m_ui.style();
    std::string err;

    bool ok = HBE::Renderer::UI::UIThemeLoader::loadStyleFromJsonFile(
        HBE::Core::AssetPaths::Resolve(m_uiThemePath), s, &err);
    if (!ok) {
        spawnPopup(20.0f, 620.0f, "UI theme reload FAILED: " + err,
            HBE::Renderer::Color4{ 1.0f, 0.3f, 0.3f, 1.0f }, 1.5f, 0.0f);
        return;
    }

    m_ui.setStyle(s);

    spawnPopup(20.0f, 620.0f, "UI theme reloaded",
        HBE::Renderer::Color4{ 0.3f, 1.0f, 0.3f, 1.0f }, 1.25f, 0.0f);
}

void GameLayer::onDetach() {
    HBE::Input::Get().saveToFile(HBE::Core::AssetPaths::ResolveUser(BINDINGS_LOGICAL));
    m_particles.shutdown();
}

void GameLayer::registerScripts() {
    using HBE::ECS::Entity;
    using HBE::ECS::Script;
    using HBE::Renderer::Scene2D;

    m_scripts.registerScript("PlayerController",
        [this](Entity, Scene2D&) -> Script {
            Script sc{};
            sc.name = "PlayerController";
            sc.onUpdate = [this](Entity e, float dt) {
                if (m_console.isOpen()) {
                    auto& r = m_scene.registry();
                    if (r.has<HBE::ECS::RigidBody2D>(e)) {
                        auto& body = r.get<HBE::ECS::RigidBody2D>(e);
                        body.accelX = 0.0f;
                        body.accelY = 0.0f;
                        body.velX = 0.0f;
                    }
                    return;
                }
                auto& r = m_scene.registry();
                if (!r.has<HBE::ECS::RigidBody2D>(e) || !r.has<Transform2D>(e)) return;

                auto& body = r.get<HBE::ECS::RigidBody2D>(e);

                const float inputX = HBE::Input::AxisValue(HBE::Input::Axis::MoveX);
                const float inputY = HBE::Input::AxisValue(HBE::Input::Axis::MoveY);

                const bool Down = (inputY > 0.5f);
                const bool JumpPressed = HBE::Input::ActionPressed(HBE::Input::Action::Jump);
                const bool AttackPressed = HBE::Input::ActionPressed(HBE::Input::Action::Attack);

                const float moveSpeed = 520.0f;
                const float accelGround = 5200.0f;
                const float accelAir = 3200.0f;
                const float friction = 6200.0f;
                const float jumpVel = 780.0f;

                body.accelY = 0.0f;

                const float targetVX = inputX * moveSpeed;
                const float ax = body.grounded ? accelGround : accelAir;

                if (inputX != 0.0f) {
                    body.velX = Approach(body.velX, targetVX, ax * dt);
                    if (auto* tr = m_scene.getTransform(e)) {
                        const float mag = std::fabs(tr->scaleX);
                        tr->scaleX = (inputX < 0.0f) ? -mag : mag;
                    }
                }
                else if (body.grounded) {
                    body.velX = Approach(body.velX, 0.0f, friction * dt);
                }
                if (JumpPressed && body.grounded && !Down) {
                    body.velY = jumpVel;
                    body.grounded = false;
                }

                if (JumpPressed && body.grounded && Down) {
                    // DOWN + JUMP: temporarily disable one-way collision so the
                    // player falls through the platform they're standing on.
                    body.oneWayDisableTimer = 0.20f;
                    body.velY = std::min(body.velY, -120.0f);
                    body.grounded = false;
                }
                if (auto* sAnim = m_scene.getSpriteAnimator(e)) {
                    const bool moving = (std::fabs(body.velX) > 5.0f);
                    sAnim->setBool("moving", moving);
                    if (AttackPressed) sAnim->trigger("attack");
                }
                };
            return sc;
        });

    m_scripts.registerScript("GoblinAI",
        [this](Entity /*owner*/, Scene2D& /*scene*/) -> Script {
            Script sc{};
            sc.name = "GoblinAI";
            sc.onUpdate = [this](Entity e, float dt) {
                using namespace HBE::Sandbox;

                auto& reg = m_scene.registry();
                auto* tr = m_scene.getTransform(e);
                if (!tr) return;

                // Do nothing once dead so the fade-out doesn't move.
                if (reg.has<HBE::ECS::Health>(e) && reg.get<HBE::ECS::Health>(e).dead) {
                    if (auto* gAnim = m_scene.getSpriteAnimator(e)) {
                        gAnim->setBool("moving", false);
                    }
                    return;
                }

                // Patrol between spawnX +/- radius.
                const float leftBound = m_demo.goblinSpawnX - demo::GOBLIN_PATROL_RADIUS;
                const float rightBound = m_demo.goblinSpawnX + demo::GOBLIN_PATROL_RADIUS;

                // Direction lives in Transform2D.scaleX sign; -1 = left,
                // +1 = right. Preserve magnitude the same way PlayerController does.
                const float dirSign = (tr->scaleX >= 0.0f) ? +1.0f : -1.0f;
                tr->posX += dirSign * demo::GOBLIN_PATROL_SPEED * dt;

                if (tr->posX <= leftBound) {
                    tr->posX = leftBound;
                    const float mag = std::fabs(tr->scaleX);
                    tr->scaleX = +mag;   // face right
                }
                else if (tr->posX >= rightBound) {
                    tr->posX = rightBound;
                    const float mag = std::fabs(tr->scaleX);
                    tr->scaleX = -mag;   // face left
                }

                if (auto* gAnim = m_scene.getSpriteAnimator(e)) {
                    gAnim->setBool("moving", true);
                }
                };
            return sc;
        });

    HBE::Core::LogInfo("ScriptRegistry: registered "
        + std::to_string(m_scripts.size()) + " scripts(s).");
}

void GameLayer::registerAnimatorPresets() {
    using HBE::ECS::Entity;
    using HBE::Renderer::Scene2D;
    using HBE::Renderer::SpriteAnimationStateMachine;

    m_animPresets.registerPreset("GoblinAnimator",
        [](Entity, SpriteAnimationStateMachine& sm, Scene2D&) {
            sm.addClip({ "Idle",   0, 0, 6, 0.10f, true,  1.0f });
            sm.addClip({ "Run",    1, 0, 6, 0.08f, true,  1.0f });
            sm.addClip({ "Attack", 2, 0, 6, 0.07f, false, 1.0f });

            sm.addEvent("Run", 1, "footstep");
            sm.addEvent("Run", 4, "footstep");
            sm.addEvent("Attack", 3, "hitframe");

            sm.addState("Idle", "Idle");
            sm.addState("Run", "Run");
            sm.addState("Attack", "Attack");

            sm.addTransitionTrigger("*", "Attack", "attack");
            sm.addTransitionBool("Idle", "Run", "moving", true);
            sm.addTransitionBool("Run", "Idle", "moving", false);
            sm.addTransitionFinished("Attack", "Idle");
        });

    m_animPresets.registerPreset("SoldierAnimator",
        [](Entity, SpriteAnimationStateMachine& sm, Scene2D&) {
            sm.addClip({ "Idle",   0, 0, 6, 0.10f, true,  1.0f });
            sm.addClip({ "Run",    1, 0, 8, 0.10f, true,  1.0f });
            sm.addClip({ "Attack", 2, 0, 7, 0.07f, false, 1.0f });

            sm.addEvent("Run", 2, "footstep");
            sm.addEvent("Run", 6, "footstep");
            sm.addEvent("Attack", 4, "hitframe");

            sm.addState("Idle", "Idle");
            sm.addState("Run", "Run");
            sm.addState("Attack", "Attack");

            sm.addTransitionTrigger("*", "Attack", "attack");
            sm.addTransitionBool("Idle", "Run", "moving", true);
            sm.addTransitionBool("Run", "Idle", "moving", false);
            sm.addTransitionFinished("Attack", "Idle");
        });

    HBE::Core::LogInfo("AnimationPresetRegistry: registered "
        + std::to_string(m_animPresets.size()) + " preset(s).");
}

void GameLayer::resetDemoState() {
    using namespace HBE::Sandbox;

    auto& reg = m_scene.registry();

    // 1. Goblin: reset Health + snap back to spawn.
    if (reg.valid(m_goblinEntity)) {
        if (reg.has<HBE::ECS::Health>(m_goblinEntity)) {
            auto& gHp = reg.get<HBE::ECS::Health>(m_goblinEntity);
            gHp.hp = gHp.maxHp;
            gHp.invulnTimer = 0.0f;
            gHp.deathTimer = 0.0f;
            gHp.dead = false;
        }
        else {
            reg.emplace<HBE::ECS::Health>(m_goblinEntity, HBE::ECS::Health{ 3, 3 });
        }

        if (auto* gTr = m_scene.getTransform(m_goblinEntity)) {
            gTr->posX = m_demo.goblinSpawnX;
            gTr->posY = m_demo.goblinSpawnY;
            const float mag = std::fabs(gTr->scaleX);
            gTr->scaleX = -mag;
        }
    }
    if (reg.valid(m_soldierEntity)) {
        if (auto* pTr = m_scene.getTransform(m_soldierEntity)) {
            pTr->posX = m_demo.playerSpawnX;
            pTr->posY = m_demo.playerSpawnY;
            const float mag = std::fabs(pTr->scaleX);
            pTr->scaleX = +mag;
        }
        if (reg.has<HBE::ECS::RigidBody2D>(m_soldierEntity)) {
            auto& body = reg.get<HBE::ECS::RigidBody2D>(m_soldierEntity);
            body.velX = 0.0f;
            body.velY = 0.0f;
            body.accelX = 0.0f;
            body.accelY = 0.0f;
            body.grounded = false;
            body.oneWayDisableTimer = 0.0f;
        }
    }
    m_demo.state = DemoState::Playing;
    m_demo.endBannerTimer = 0.0f;

    if (auto* pTr = m_scene.getTransform(m_soldierEntity)) {
        m_camera.x = pTr->posX;
        m_camera.y = pTr->posY;
        m_app->gl().setCamera(m_camera);
    }

    HBE::Core::LogInfo("Demo: state reset.");
}

void GameLayer::updateDemoLogic(float dt) {
    using namespace HBE::Sandbox;

    auto& reg = m_scene.registry();

    for (auto e : reg.view<HBE::ECS::Health>()) {
        auto& h = reg.get<HBE::ECS::Health>(e);
        if (h.invulnTimer >= HBE::Sandbox::demo::HIT_IFRAMES_SEC - dt * 2.0f
            && h.invulnTimer > 0.0f
            && !h.dead) {
            if (auto* tr = m_scene.getTransform(e)) {
                spawnPopup(tr->posX, tr->posY + 60.0f, "-1",
                    HBE::Renderer::Color4{ 1.0f, 0.9f, 0.2f, 1.0f },
                    0.75f, 40.0f);
            }
        }
    }

    for (auto e : reg.view<HBE::ECS::Health>()) {
        auto& h = reg.get<HBE::ECS::Health>(e);
        if (h.dead && h.deathTimer <= 0.0f && h.hp == 0) {
            // Only run this once — deathTimer > 0 = already bootstrapped.
            h.deathTimer = HBE::Sandbox::demo::DEATH_FADE_SEC;
            if (auto* tr = m_scene.getTransform(e)) {
                spawnPopup(tr->posX, tr->posY + 90.0f, "DEFEATED",
                    HBE::Renderer::Color4{ 1.0f, 0.3f, 0.3f, 1.0f },
                    1.2f, 20.0f);
            }
        }
    }

    if (m_demo.state != DemoState::Playing) {
        m_demo.endBannerTimer += dt;
        return;
    }

    if (reg.valid(m_goblinEntity) && reg.has<HBE::ECS::Health>(m_goblinEntity)) {
        const auto& gHp = reg.get<HBE::ECS::Health>(m_goblinEntity);
        if (gHp.dead && gHp.deathTimer <= 0.0f) {
            m_demo.state = DemoState::Won;
            m_demo.endBannerTimer = 0.0f;
            HBE::Core::LogInfo("Demo: WIN.");
        }
    }

    if (reg.valid(m_soldierEntity) && reg.has<Transform2D>(m_soldierEntity)) {
        const auto& pTr = reg.get<Transform2D>(m_soldierEntity);
        if (pTr.posY < demo::LOSE_Y_THRESHOLD) {
            m_demo.state = DemoState::Lost;
            m_demo.endBannerTimer = 0.0f;
            HBE::Core::LogInfo("Demo: LOSE.");
        }
    }
}

void GameLayer::drawDemoHUD(HBE::Renderer::Renderer2D& r2d) {
    using namespace HBE::Sandbox;

    // Called from onRender inside a UI-space beginScene. Coordinates
    // are LOGICAL_WIDTH x LOGICAL_HEIGHT screen-space.

    auto& reg = m_scene.registry();

    // -- Goblin HP readout in the top-right --
    if (reg.valid(m_goblinEntity) && reg.has<HBE::ECS::Health>(m_goblinEntity)) {
        const auto& gHp = reg.get<HBE::ECS::Health>(m_goblinEntity);
        std::string hpText = "Goblin HP: " +
            std::to_string(gHp.hp) + " / " + std::to_string(gHp.maxHp);

        HBE::Renderer::Color4 hpColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        if (gHp.dead) hpColor = { 0.6f, 0.6f, 0.6f, 1.0f };
        else if (gHp.hp == 1) hpColor = { 1.0f, 0.35f, 0.25f, 1.0f };

        m_text.drawText(r2d,
            LOGICAL_WIDTH - 260.0f, 30.0f,
            hpText, 1.0f, hpColor);
    }

    // -- WIN / LOSE banner --
    if (m_demo.state == DemoState::Won) {
        m_text.drawText(r2d,
            LOGICAL_WIDTH * 0.5f - 90.0f, LOGICAL_HEIGHT * 0.5f - 20.0f,
            "YOU WIN", 2.5f,
            HBE::Renderer::Color4{ 0.4f, 1.0f, 0.5f, 1.0f });
        m_text.drawText(r2d,
            LOGICAL_WIDTH * 0.5f - 130.0f, LOGICAL_HEIGHT * 0.5f + 30.0f,
            "Press R to restart", 1.0f,
            HBE::Renderer::Color4{ 0.85f, 0.85f, 0.85f, 1.0f });
    }
    else if (m_demo.state == DemoState::Lost) {
        m_text.drawText(r2d,
            LOGICAL_WIDTH * 0.5f - 100.0f, LOGICAL_HEIGHT * 0.5f - 20.0f,
            "YOU LOSE", 2.5f,
            HBE::Renderer::Color4{ 1.0f, 0.4f, 0.4f, 1.0f });
        m_text.drawText(r2d,
            LOGICAL_WIDTH * 0.5f - 130.0f, LOGICAL_HEIGHT * 0.5f + 30.0f,
            "Press R to restart", 1.0f,
            HBE::Renderer::Color4{ 0.85f, 0.85f, 0.85f, 1.0f });
    }
}

void GameLayer::drawInspector() {
    using namespace HBE::Renderer::UI;

    UIRect insp;
    insp.x = LOGICAL_WIDTH - 420.0f;
    insp.y = 20.0f;
    insp.w = 400.0f;
    insp.h = 520.0f;

    m_ui.beginScrollPanel("inspector", insp, "Inspector");

    auto& reg = m_scene.registry();

    // -- Collect entities that have a tag; that covers every entity
    //    created via Scene2D::createEntity() or SceneSerializer, which
    //    is everything except transient bare-registry entities. --
    std::vector<HBE::ECS::Entity> ents;
    ents.reserve(64);
    for (auto e : reg.view<HBE::ECS::TagComponent>()) {
        ents.push_back(e);
    }

    // Clamp pagination if the list shrunk (delete pressed, scene reload).
    const int total = static_cast<int>(ents.size());
    if (m_inspectorFirstIndex >= total) m_inspectorFirstIndex = 0;
    if (m_inspectorFirstIndex < 0)      m_inspectorFirstIndex = 0;

    // -- Header: entity list controls --
    char hdr[64];
    std::snprintf(hdr, sizeof(hdr), "Entities  (%d total)", total);
    m_ui.label(hdr, true);

    // -- Item 20: entity lifecycle controls (Cycle / Create / Delete) --
    {
        // Clamp the cycle index in case the preset table changes size.
        if (m_inspectorCreateTagIdx < 0 ||
            m_inspectorCreateTagIdx >= kEntityTagPresetsCount) {
            m_inspectorCreateTagIdx = 0;
        }

        char cycleLbl[96];
        std::snprintf(cycleLbl, sizeof(cycleLbl),
            "Next tag: %s  (click to cycle)",
            kEntityTagPresets[m_inspectorCreateTagIdx]);
        if (m_ui.button("btn_cycle_tag_preset", cycleLbl)) {
            m_inspectorCreateTagIdx =
                (m_inspectorCreateTagIdx + 1) % kEntityTagPresetsCount;
        }

        char createLbl[96];
        std::snprintf(createLbl, sizeof(createLbl),
            "+ Create '%s' Entity",
            kEntityTagPresets[m_inspectorCreateTagIdx]);
        if (m_ui.button("btn_create_entity", createLbl)) {
            const char* tagName =
                kEntityTagPresets[m_inspectorCreateTagIdx];

            HBE::ECS::Entity newEnt = m_scene.createEntity();

            // Overwrite the placeholder tag stamped by createEntity()
            // with the preset name. Unconditional emplace matches the
            // documented "createEntity stamps a placeholder" behaviour.
            reg.emplace<HBE::ECS::TagComponent>(newEnt,
                HBE::ECS::TagComponent{ tagName });

            // Give the new entity a Transform2D at the camera centre so
            // it's immediately visible / clickable in the world.
            HBE::Renderer::Transform2D tr{};
            tr.posX = m_camera.x;
            tr.posY = m_camera.y;
            tr.scaleX = 100.0f;
            tr.scaleY = 100.0f;
            tr.rotation = 0.0f;
            reg.emplace<HBE::Renderer::Transform2D>(newEnt, tr);

            m_selectedEntity = newEnt;
            HBE::Core::LogInfo(std::string("Inspector: created '") +
                tagName + "' entity at camera centre.");
        }

        // Delete Selected — safely disarms our cached m_soldierEntity /
        // m_goblinEntity handles if the user nukes those.
        if (reg.valid(m_selectedEntity)) {
            std::string delLbl = "- Delete Selected (" +
                std::to_string((uint32_t)m_selectedEntity) + ")";
            if (m_ui.button("btn_delete_entity", delLbl.c_str())) {
                const HBE::ECS::Entity victim = m_selectedEntity;

                if (victim == m_soldierEntity) m_soldierEntity = {};
                if (victim == m_goblinEntity)  m_goblinEntity = {};

                reg.destroy(victim);
                m_selectedEntity = {};

                HBE::Core::LogInfo("Inspector: deleted entity " +
                    std::to_string((uint32_t)victim) + ".");

                // The `ents` snapshot above is now stale; any code below
                // that dereferences it would trip the Registry assertion.
                // Close the panel and bail — next frame re-snapshots.
                m_ui.endPanel();
                return;
            }
        }

        m_ui.spacing(6.0f);
    }

    // Prev / Next page buttons (only useful when list overflows).
    if (total > kInspectorPageSize) {
        char pageLbl[64];
        const int lastPageStart =
            ((total - 1) / kInspectorPageSize) * kInspectorPageSize;
        std::snprintf(pageLbl, sizeof(pageLbl),
            "Page %d / %d",
            (m_inspectorFirstIndex / kInspectorPageSize) + 1,
            (lastPageStart / kInspectorPageSize) + 1);
        m_ui.label(pageLbl, true);

        if (m_ui.button("btn_ent_prev", "< Prev")) {
            m_inspectorFirstIndex =
                std::max(0, m_inspectorFirstIndex - kInspectorPageSize);
        }
        if (m_ui.button("btn_ent_next", "Next >")) {
            const int cap = std::max(0, total - kInspectorPageSize);
            m_inspectorFirstIndex =
                std::min(cap, m_inspectorFirstIndex + kInspectorPageSize);
        }
    }

    // -- Entity list (one page) --
    const int end = std::min(total,
        m_inspectorFirstIndex + kInspectorPageSize);
    for (int i = m_inspectorFirstIndex; i < end; ++i) {
        auto e = ents[i];

        // Defensive: skip anything that got destroyed mid-frame (e.g.
        // by another system) so we never hit Storage::get on a dead
        // entity.
        if (!reg.valid(e)) continue;
        if (!reg.has<HBE::ECS::TagComponent>(e)) continue;

        std::string name = "Entity " + std::to_string((uint32_t)e);
        const auto& tag = reg.get<HBE::ECS::TagComponent>(e).tag;
        if (!tag.empty()) {
            name = tag + " (" + std::to_string((uint32_t)e) + ")";
        }

        std::string id = "ent_" + std::to_string((uint32_t)e);
        if (m_ui.button(id.c_str(), name.c_str())) {
            m_selectedEntity = e;
        }
    }

    m_ui.spacing(10.0f);

    if (reg.valid(m_selectedEntity)) {
        std::string header = "Selected: " + std::to_string((uint32_t)m_selectedEntity);
        m_ui.label(header.c_str(), false);
        m_ui.spacing(6.0f);

        if (reg.has<HBE::ECS::TagComponent>(m_selectedEntity)) {
            const auto& tag = reg.get<HBE::ECS::TagComponent>(m_selectedEntity).tag;
            std::string tagLine = "Tag: " + tag;
            m_ui.label(tagLine.c_str(), true);
        }

        if (reg.has<HBE::Renderer::PrefabRefComponent>(m_selectedEntity)) {
            const auto& pr = reg.get<HBE::Renderer::PrefabRefComponent>(m_selectedEntity);
            std::string prLbl = "Prefab: " + pr.name;
            m_ui.label(prLbl.c_str(), true);
        }

        // -- Item 20: Retag Selection cycle --
        {
            if (m_inspectorRetagIdx < 0 ||
                m_inspectorRetagIdx >= kEntityTagPresetsCount) {
                m_inspectorRetagIdx = 0;
            }

            char retagCycle[96];
            std::snprintf(retagCycle, sizeof(retagCycle),
                "Retag preset: %s  (click to cycle)",
                kEntityTagPresets[m_inspectorRetagIdx]);
            if (m_ui.button("btn_retag_cycle", retagCycle)) {
                m_inspectorRetagIdx =
                    (m_inspectorRetagIdx + 1) % kEntityTagPresetsCount;
            }

            char retagApply[96];
            std::snprintf(retagApply, sizeof(retagApply),
                "Apply Tag '%s'",
                kEntityTagPresets[m_inspectorRetagIdx]);
            if (m_ui.button("btn_retag_apply", retagApply)) {
                const char* newTag =
                    kEntityTagPresets[m_inspectorRetagIdx];
                reg.emplace<HBE::ECS::TagComponent>(m_selectedEntity,
                    HBE::ECS::TagComponent{ newTag });

                if (m_selectedEntity == m_soldierEntity &&
                    std::string(newTag) != "Player") {
                    m_soldierEntity = {};
                }
                if (m_selectedEntity == m_goblinEntity &&
                    std::string(newTag) != "Goblin") {
                    m_goblinEntity = {};
                }
                if (std::string(newTag) == "Player") {
                    m_soldierEntity = m_selectedEntity;
                }
                if (std::string(newTag) == "Goblin") {
                    m_goblinEntity = m_selectedEntity;
                }
            }
        }

        m_ui.spacing(6.0f);

        if (reg.has<HBE::Renderer::Transform2D>(m_selectedEntity)) {
            auto& tr = reg.get<HBE::Renderer::Transform2D>(m_selectedEntity);
            m_ui.label("Transform", true);

            m_ui.sliderFloat("tr_x", "posX", tr.posX, -5000.0f, 5000.0f, 1.0f);
            m_ui.sliderFloat("tr_y", "posY", tr.posY, -5000.0f, 5000.0f, 1.0f);
            constexpr float PI = 3.14159265358979323846f;
            m_ui.sliderFloat("tr_rot", "rotation (rad)", tr.rotation, -PI, PI, 0.01f);
            m_ui.sliderFloat("tr_sx", "scaleX", tr.scaleX, 0.1f, 10.0f, 0.1f);
            m_ui.sliderFloat("tr_sy", "scaleY", tr.scaleY, 0.1f, 10.0f, 0.1f);

            m_ui.spacing(6.0f);
        }

        if (reg.has<HBE::Renderer::SpriteComponent2D>(m_selectedEntity)) {
            auto& spr = reg.get<HBE::Renderer::SpriteComponent2D>(m_selectedEntity);
            m_ui.label("Sprite", true);
            m_ui.sliderInt("spr_layer", "layer", spr.layer, -10, 500);
            m_ui.sliderFloat("spr_sort", "sortKey", spr.sortKey, -5000.0f, 5000.0f, 1.0f);
            m_ui.sliderFloat("spr_sorty", "sortOffsetY", spr.sortOffsetY, -200.0f, 200.0f, 1.0f);

            // UV rect - u0, v0, u1, v1 (all in [0..1]).
            m_ui.sliderFloat("spr_u0", "uv u0", spr.uvRect[0], 0.0f, 1.0f, 0.001f);
            m_ui.sliderFloat("spr_v0", "uv v0", spr.uvRect[1], 0.0f, 1.0f, 0.001f);
            m_ui.sliderFloat("spr_u1", "uv u1", spr.uvRect[2], 0.0f, 1.0f, 0.001f);
            m_ui.sliderFloat("spr_v1", "uv v1", spr.uvRect[3], 0.0f, 1.0f, 0.001f);

            m_ui.checkbox("spr_visible", "visible", spr.visible);

            m_ui.spacing(6.0f);
        }

        if (reg.has<HBE::ECS::Collider2D>(m_selectedEntity)) {
            auto& col = reg.get<HBE::ECS::Collider2D>(m_selectedEntity);
            m_ui.label("Collider2D", true);
            m_ui.sliderFloat("col_hw", "halfW", col.halfW, 0.0f, 500.0f, 0.5f);
            m_ui.sliderFloat("col_hh", "halfH", col.halfH, 0.0f, 500.0f, 0.5f);
            m_ui.sliderFloat("col_ox", "offsetX", col.offsetX, -200.0f, 200.0f, 0.5f);
            m_ui.sliderFloat("col_oy", "offsetY", col.offsetY, -200.0f, 200.0f, 0.5f);
            m_ui.checkbox("col_trig", "isTrigger", col.isTrigger);
            m_ui.spacing(6.0f);
        }

        if (reg.has<HBE::ECS::RigidBody2D>(m_selectedEntity)) {
            auto& rb = reg.get<HBE::ECS::RigidBody2D>(m_selectedEntity);
            m_ui.label("RigidBody2D", true);

            m_ui.checkbox("rb_static", "isStatic", rb.isStatic);
            m_ui.checkbox("rb_grav", "useGravity", rb.useGravity);
            m_ui.sliderFloat("rb_gscale", "gravityScale", rb.gravityScale, 0.0f, 10.0f, 0.05f);

            m_ui.sliderFloat("rb_vx", "velX", rb.velX, -4000.0f, 4000.0f, 1.0f);
            m_ui.sliderFloat("rb_vy", "velY", rb.velY, -4000.0f, 4000.0f, 1.0f);
            m_ui.sliderFloat("rb_ax", "accelX", rb.accelX, -4000.0f, 4000.0f, 1.0f);
            m_ui.sliderFloat("rb_ay", "accelY", rb.accelY, -4000.0f, 4000.0f, 1.0f);

            m_ui.sliderFloat("rb_damp", "linearDamping", rb.linearDamping, 0.0f, 50.0f, 0.25f);
            m_ui.sliderFloat("rb_maxfall", "maxFallSpeed", rb.maxFallSpeed, -4000.0f, 0.0f, 25.0f);
            m_ui.sliderFloat("rb_step", "maxStepUp", rb.maxStepUp, 0.0f, 200.0f, 0.5f);

            m_ui.checkbox("rb_oneway", "enableOneWay", rb.enableOneWay);
            m_ui.checkbox("rb_slopes", "enableSlopes", rb.enableSlopes);

            // Read-only-ish: editable for debugging.
            m_ui.checkbox("rb_grounded_dbg", "grounded (dbg)", rb.grounded);

            m_ui.spacing(6.0f);
        }

        if (reg.has<HBE::ECS::Health>(m_selectedEntity)) {
            auto& hp = reg.get<HBE::ECS::Health>(m_selectedEntity);
            m_ui.label("Health", true);
            m_ui.sliderInt("hp_hp", "hp", hp.hp, 0, 100);
            m_ui.sliderInt("hp_maxhp", "maxHp", hp.maxHp, 1, 100);
            m_ui.sliderFloat("hp_iframe", "invulnTimer", hp.invulnTimer, 0.0f, 2.0f, 0.01f);
            m_ui.checkbox("hp_dead", "dead", hp.dead);
            m_ui.spacing(6.0f);
        }

        // -- Item 21: Faction editor --
        if (reg.has<HBE::ECS::FactionComponent>(m_selectedEntity)) {
            auto& fc = reg.get<HBE::ECS::FactionComponent>(m_selectedEntity);
            m_ui.label("Faction", true);

            int idx = static_cast<int>(fc.team);
            if (idx < 0 || idx >= kFactionPresetsCount) idx = 0;

            char label[64];
            std::snprintf(label, sizeof(label),
                "Team: %s (click to cycle)", kFactionPresets[idx]);
            if (m_ui.button("btn_faction_cycle", label)) {
                idx = (idx + 1) % kFactionPresetsCount;
                fc.team = static_cast<HBE::ECS::Faction>(idx);
            }
            m_ui.spacing(6.0f);
        }

        // -- Item 21: Hurtbox editor --
        if (reg.has<HBE::ECS::Hurtbox>(m_selectedEntity)) {
            auto& hb = reg.get<HBE::ECS::Hurtbox>(m_selectedEntity);
            m_ui.label("Hurtbox", true);
            m_ui.checkbox("hb_active", "active", hb.active);
            m_ui.sliderFloat("hb_hw",  "halfW",   hb.halfW,   0.0f, 200.0f, 0.5f);
            m_ui.sliderFloat("hb_hh",  "halfH",   hb.halfH,   0.0f, 200.0f, 0.5f);
            m_ui.sliderFloat("hb_ox",  "offsetX", hb.offsetX, -200.0f, 200.0f, 0.5f);
            m_ui.sliderFloat("hb_oy",  "offsetY", hb.offsetY, -200.0f, 200.0f, 0.5f);
            m_ui.spacing(6.0f);
        }

        // -- Item 21: Hitbox editor (transient, rarely selected) --
        if (reg.has<HBE::ECS::Hitbox>(m_selectedEntity)) {
            auto& h = reg.get<HBE::ECS::Hitbox>(m_selectedEntity);
            m_ui.label("Hitbox (transient)", true);
            std::string ownerLine = "owner: " + std::to_string((uint32_t)h.owner);
            m_ui.label(ownerLine.c_str(), false);
            m_ui.sliderFloat("hbx_life", "lifetime (s)", h.lifetime, 0.0f, 3.0f, 0.01f);
            m_ui.sliderInt("hbx_dmg",   "damage",       h.damage,   0, 20);
            m_ui.checkbox("hbx_active", "active",       h.active);
            m_ui.checkbox("hbx_ff",     "friendlyFire", h.friendlyFire);
            m_ui.spacing(6.0f);
        }

        // -- Item 20: Add Component menu --
        m_ui.spacing(4.0f);
        m_ui.label("Add Component", true);

        if (!reg.has<HBE::Renderer::Transform2D>(m_selectedEntity)) {
            if (m_ui.button("btn_add_transform", "+ Add Transform2D")) {
                HBE::Renderer::Transform2D tr{};
                tr.posX = m_camera.x;
                tr.posY = m_camera.y;
                tr.scaleX = 100.0f;
                tr.scaleY = 100.0f;
                reg.emplace<HBE::Renderer::Transform2D>(m_selectedEntity, tr);
            }
        }

        if (!reg.has<HBE::Renderer::SpriteComponent2D>(m_selectedEntity)) {
            if (m_ui.button("btn_add_sprite", "+ Add Sprite2D")) {
                reg.emplace<HBE::Renderer::SpriteComponent2D>(
                    m_selectedEntity,
                    MakeDefaultSprite(m_quadMesh, &m_soldierMaterial));
            }
        }

        if (!reg.has<HBE::ECS::Collider2D>(m_selectedEntity)) {
            if (m_ui.button("btn_add_collider", "+ Add Collider2D")) {
                HBE::ECS::Collider2D c{};
                c.halfW   = 16.0f;
                c.halfH   = 16.0f;
                c.offsetX = 0.0f;
                c.offsetY = 0.0f;
                c.isTrigger = false;
                reg.emplace<HBE::ECS::Collider2D>(m_selectedEntity, c);
            }
        }

        if (!reg.has<HBE::ECS::RigidBody2D>(m_selectedEntity)) {
            if (m_ui.button("btn_add_rigidbody", "+ Add RigidBody2D")) {
                HBE::ECS::RigidBody2D rb{};
                rb.isStatic     = true;
                rb.useGravity   = false;
                rb.linearDamping = 0.0f;
                rb.enableOneWay = true;
                rb.enableSlopes = true;
                reg.emplace<HBE::ECS::RigidBody2D>(m_selectedEntity, rb);
            }
        }

        if (!reg.has<HBE::ECS::Health>(m_selectedEntity)) {
            if (m_ui.button("btn_add_health", "+ Add Health")) {
                reg.emplace<HBE::ECS::Health>(m_selectedEntity,
                    HBE::ECS::Health{ 3, 3 });
            }
        }

        // -- Item 21: Add Faction / Hurtbox --
        if (!reg.has<HBE::ECS::FactionComponent>(m_selectedEntity)) {
            if (m_ui.button("btn_add_faction", "+ Add Faction")) {
                reg.emplace<HBE::ECS::FactionComponent>(m_selectedEntity,
                    HBE::ECS::FactionComponent{ HBE::ECS::Faction::Neutral });
            }
        }
        if (!reg.has<HBE::ECS::Hurtbox>(m_selectedEntity)) {
            if (m_ui.button("btn_add_hurtbox", "+ Add Hurtbox")) {
                reg.emplace<HBE::ECS::Hurtbox>(m_selectedEntity,
                    HBE::ECS::Hurtbox{});
            }
        }

        m_ui.spacing(4.0f);
        m_ui.label("Remove Component", true);

        if (reg.has<HBE::Renderer::Transform2D>(m_selectedEntity)) {
            if (m_ui.button("btn_rm_transform", "- Remove Transform2D")) {
                reg.remove<HBE::Renderer::Transform2D>(m_selectedEntity);
            }
        }
        if (reg.has<HBE::Renderer::SpriteComponent2D>(m_selectedEntity)) {
            if (m_ui.button("btn_rm_sprite", "- Remove Sprite2D")) {
                reg.remove<HBE::Renderer::SpriteComponent2D>(m_selectedEntity);
            }
        }
        if (reg.has<HBE::ECS::Collider2D>(m_selectedEntity)) {
            if (m_ui.button("btn_rm_collider", "- Remove Collider2D")) {
                reg.remove<HBE::ECS::Collider2D>(m_selectedEntity);
            }
        }
        if (reg.has<HBE::ECS::RigidBody2D>(m_selectedEntity)) {
            if (m_ui.button("btn_rm_rigidbody", "- Remove RigidBody2D")) {
                reg.remove<HBE::ECS::RigidBody2D>(m_selectedEntity);
            }
        }
        if (reg.has<HBE::ECS::Health>(m_selectedEntity)) {
            if (m_ui.button("btn_rm_health", "- Remove Health")) {
                reg.remove<HBE::ECS::Health>(m_selectedEntity);
            }
        }

        // -- Item 21: Remove Faction / Hurtbox --
        if (reg.has<HBE::ECS::FactionComponent>(m_selectedEntity)) {
            if (m_ui.button("btn_rm_faction", "- Remove Faction")) {
                reg.remove<HBE::ECS::FactionComponent>(m_selectedEntity);
            }
        }
        if (reg.has<HBE::ECS::Hurtbox>(m_selectedEntity)) {
            if (m_ui.button("btn_rm_hurtbox", "- Remove Hurtbox")) {
                reg.remove<HBE::ECS::Hurtbox>(m_selectedEntity);
            }
        }

        m_ui.spacing(6.0f);

        if (m_ui.button("btn_focus_player", "Select Player")) {
            m_selectedEntity = m_soldierEntity;
        }
        if (m_ui.button("btn_focus_goblin", "Select Goblin")) {
            m_selectedEntity = m_goblinEntity;
        }
    }
    else {
        m_ui.label("No entity selected.", true);
    }

    m_ui.endPanel();
}

bool GameLayer::saveSceneNow(std::string* outError) {
    HBE::Renderer::SceneSaveCallbacks saveCb{};
    saveCb.meshKey = [this](const HBE::Renderer::Mesh* m) -> std::string {
        if (m == m_quadMesh) return "quad";
        return "";
        };
    saveCb.materialKey = [this](const HBE::Renderer::Material* mat) -> std::string {
        if (mat == &m_goblinMaterial) return "goblin_mat";
        if (mat == &m_soldierMaterial) return "soldier_mat";
        return "";
        };
    saveCb.sheetKey = [this](const HBE::Renderer::SpriteRenderer2D::SpriteSheetHandle* sh) -> std::string {
        if (sh == &m_goblinSheet) return "orc_sheet";
        if (sh == &m_soldierSheet) return "soldier_sheet";
        return "";
        };
    saveCb.prefabs = &m_prefabs;

    namespace ap = HBE::Core::AssetPaths;
    const std::string scenePath = kSceneSaveGoesToUserData
        ? ap::ResolveUser(SCENE_LOGICAL)
        : ap::Resolve(SCENE_LOGICAL);

    if (kSceneSaveGoesToUserData) {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(scenePath).parent_path(), ec);
    }

    std::string err;
    const bool ok = HBE::Renderer::SceneSerializer::saveToFile(m_scene, scenePath, saveCb, m_tileMapPath, &err);

    if (!ok) {
        if (outError) *outError = err;
        LogError("Scene save FAILED: " + err);
        return false;
    }

    LogInfo("Scene saved: " + scenePath);
    return true;
}

bool GameLayer::loadSceneNow(std::string* outError) {
    std::string tilemapPath;
    std::string err;

    HBE::Renderer::SceneLoadCallbacks loadCb{};
    loadCb.mesh = [this](const std::string& key) -> HBE::Renderer::Mesh* {
        if (key == "quad") return m_quadMesh;
        return nullptr;
        };
    loadCb.material = [this](const std::string& key) -> HBE::Renderer::Material* {
        if (key == "goblin_mat") return &m_goblinMaterial;
        if (key == "solder_mat") return &m_soldierMaterial;
        return nullptr;
        };

    loadCb.prefabs = &m_prefabs;
    loadCb.scripts = &m_scripts;
    loadCb.animators = &m_animPresets;

    namespace ap = HBE::Core::AssetPaths;
    std::string scenePath = ap::ResolveUser(SCENE_LOGICAL);
    {
        std::error_code ec;
        if (!std::filesystem::exists(scenePath, ec)) {
            scenePath = ap::Resolve(SCENE_LOGICAL);
        }
    }

    const bool ok = HBE::Renderer::SceneSerializer::loadFromFile(m_scene, scenePath, loadCb, &tilemapPath, &err);

    if (!ok) {
        if (outError) *outError = err;
        LogError("Scene load FAILED: " + err);
        return false;
    }

    m_soldierEntity = {};
    m_goblinEntity = {};
    m_selectedEntity = {};

    auto& reg = m_scene.registry();
    for (auto ent : reg.view<HBE::ECS::TagComponent>()) {
        const auto& tag = reg.get<HBE::ECS::TagComponent>(ent).tag;
        if (tag == "Player") m_soldierEntity = ent;
        if (tag == "Goblin") m_goblinEntity = ent;
    }

    if (!tilemapPath.empty()) {
        m_tileMapPath = tilemapPath;
    }
    hotReloadTileMap();

    LogInfo("Scene loaded: " + scenePath);
    return true;
}

void GameLayer::registerHitboxPresets() {
    using namespace HBE::ECS;
    using namespace HBE::Sandbox::demo;

    HitboxPreset soldierSlash{};
    soldierSlash.halfW = 0.5f * ATTACK_REACH_PX;
    soldierSlash.halfH = 0.5f * ATTACK_HEIGHT_PX;
    soldierSlash.offsetX = 0.5f * (16.0f + ATTACK_REACH_PX);
    soldierSlash.offsetY = 2.0f;
    soldierSlash.damage = 1;
    soldierSlash.lifetime = 0.10f;
    soldierSlash.follows = true;
    soldierSlash.attackerFaction = Faction::Player;
    soldierSlash.canHit = { true, false, true, false };
    soldierSlash.knockbackX = 220.0f;
    soldierSlash.knockbackY = 220.0f;
    soldierSlash.invulnAfterHit = HIT_IFRAMES_SEC;
    m_hitboxPresets.registerPreset("SoldierSlash", soldierSlash);

    HitboxPreset goblinClaw{};
    goblinClaw.halfW = 20.0f;
    goblinClaw.halfH = 18.0f;
    goblinClaw.offsetX = 30.0f;
    goblinClaw.offsetY = 2.0f;
    goblinClaw.damage = 1;
    goblinClaw.lifetime = 0.12f;
    goblinClaw.attackerFaction = Faction::Enemy;
    goblinClaw.canHit = { true, true, false, false };
    goblinClaw.knockbackX = 260.0f;
    goblinClaw.knockbackY = 260.0f;
    m_hitboxPresets.registerPreset("GoblinClaw", goblinClaw);

    HBE::Core::LogInfo("HitboxPresetRegistry: registered "
    + std::to_string(m_hitboxPresets.size()) + " preset(s).");
}