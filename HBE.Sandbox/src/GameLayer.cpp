#include "GameLayer.h"
#include "DevConsole.h"

#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"
#include "HBE/Core/Event.h"

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

#include "HBE/Renderer/SceneSerializer.h"
#include "HBE/ECS/RuntimeComponents.h" // IDComponent, TagComponent
#include "HBE/ECS/Components.h"

#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_gamepad.h>

#include <cmath>
#include <vector>
#include <string>
#include <filesystem>

#include <algorithm> // remove_if
#include <utility>   // move

using namespace HBE::Core;
using namespace HBE::Renderer;
using namespace HBE::Platform;

namespace {
    constexpr float SPRITE_PIXEL_SCALE = 4.0f;

    // --- COLLIDER SIZE IN PIXELS (tweak these) ---
    // These are NOT frame sizes, they are "body" sizes.
    constexpr float PLAYER_BODY_W_PX = 8.0f;
    constexpr float PLAYER_BODY_H_PX = 14.0f;
    constexpr float PLAYER_BODY_Y_OFFSET_PX = +0.5f;

    constexpr float GOBLIN_BODY_W_PX = 10.0f;
    constexpr float GOBLIN_BODY_H_PX = 14.0f;
    constexpr float GOBLIN_BODY_Y_OFFSET_PX = +0.5f;

    // Where user overrides live (next to exe while developing)
    constexpr const char* BINDINGS_FILE = "bindings.cfg";

    // Scene serialization file (relative to HBE.Sandbox CWD)
    constexpr const char* SCENE_PATH = "assets/scenes/sandbox.scene.json";

    static float Approach(float v, float target, float maxDelta) {
        if (v < target) return std::min(v + maxDelta, target);
        if (v > target) return std::max(v - maxDelta, target);
        return target;
    }

    // Game-owned default bindings (engine stays universal).
    static void RegisterDefaultBindings(HBE::Input::InputMap& map)
    {
        using namespace HBE::Input;

        // --- Actions ---
        map.bindAction(Action::Jump, Binding::Key(SDL_SCANCODE_SPACE), true);
        map.bindAction(Action::Jump, Binding::GamepadButton(SDL_GAMEPAD_BUTTON_SOUTH), false);

        // Attack default: E on keyboard, X/Square on controller
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

        // --- Axes ---
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
        moveY.negative = Binding::Key(SDL_SCANCODE_W); // up
        moveY.positive = Binding::Key(SDL_SCANCODE_S); // down
        moveY.negative2 = Binding::Key(SDL_SCANCODE_UP);
        moveY.positive2 = Binding::Key(SDL_SCANCODE_DOWN);
        moveY.useGamepadAxis = true;
        moveY.gamepadAxis = SDL_GAMEPAD_AXIS_LEFTY;
        moveY.deadzone = 0.20f;
        moveY.invert = false;
        moveY.scale = 1.0f;
        map.bindAxis(Axis::MoveY, moveY);
    }

    // NOTE:
    // If your Platform::Input API uses a different function name, adjust these two calls:
    // - Input::keyPressed(SDL_SCANCODE_F5)
    // - Input::keyPressed(SDL_SCANCODE_F9)
    //
    // Some codebases use Input::isKeyPressed or Input::getKeyDown etc.
    static bool KeyPressed(SDL_Scancode sc) {
        return HBE::Platform::Input::IsKeyPressed(sc);
    }
}

void GameLayer::onAttach(Application& app) {
    m_app = &app;
    m_console.setWindow(app.platform().getWindow());
    // Ensure paths are initialized (prevents empty-string watches if header defaults differ)
    if (m_tileMapPath.empty())  m_tileMapPath = "assets/maps/test_map.json";
    if (m_uiThemePath.empty())  m_uiThemePath = "assets/ui/theme.json";
    if (m_spriteVsPath.empty()) m_spriteVsPath = "assets/shaders/sprite.vert";
    if (m_spriteFsPath.empty()) m_spriteFsPath = "assets/shaders/sprite.frag";

    // ------------------------------------------------------------
    // Input Mapping Layer:
    // - Game defines defaults here
    // - Player overrides loaded from bindings.cfg
    // ------------------------------------------------------------
    HBE::Input::Initialize(&RegisterDefaultBindings);
    HBE::Input::Get().loadFromFile(BINDINGS_FILE); // safe if missing (returns false)

    HBE::Core::LogInfo(std::string("CWD: ") + std::filesystem::current_path().string());

    // camera setup (logical resolution)
    m_camera.x = std::round(m_camera.x);
    m_camera.y = std::round(m_camera.y);
    m_camera.zoom = 1.0f;
    m_camera.viewportWidth = LOGICAL_WIDTH;
    m_camera.viewportHeight = LOGICAL_HEIGHT;

    m_text.setCullInset(8.0f);

    app.gl().setCamera(m_camera);
    app.gl().setClearColor(0.1f, 0.2f, 0.35f, 1.0f);

    buildSpritePipeline();

    // ── Particle System ────────────────────────────────────────────────────────
    m_particles.initialize(app.resources(), m_quadMesh);

    m_particles.registerEffect("rain", makeRain());
    m_particles.registerEffect("player_dust", makePlayerDust());

    // Rain is always active. Emitter is a rectangle centred on the camera each frame so drops
    // spawn throughout the visible area (not just at the top edge). The first onUpdate frame
    // pre-warms it so the world starts already populated with drops at every height.
    m_rainHandle = m_particles.spawnManaged("rain", m_camera.x, m_camera.y);

    HBE::Renderer::UI::UIStyle style;
    style.textScale = 1.0f;
    style.itemH = 34.0f;
    style.padding = 12.0f;
    style.spacing = 10.0f;
    m_ui.setStyle(style);

    // load Tilemap
    std::string err;
    if (!HBE::Renderer::TileMapLoader::loadFromJsonFile(m_tileMapPath, m_tileMap, &err)) {
        LogFatal("Failed to load tilemap: " + err);
        m_app->requestQuit();
        return;
    }

    // build tile renderer (uses same sprite shader + quad mesh)
    if (!m_tileRenderer.build(m_app->renderer2D(), m_app->resources(), m_spriteShader, m_quadMesh, m_tileMap)) {
        LogFatal("Failed to build TileMapRenderer");
        m_app->requestQuit();
        return;
    }

    setupHotReloadWatches();
    hotReloadUITheme(); // apply theme at startup (optional, but nice)

    // Load test sound
        // ---------------------------------------------------------------------
    // Audio bootstrap
    // For now we only have test_sound.wav in the repo, so reuse it for
    // multiple logical events until you drop in real assets.
    // ---------------------------------------------------------------------
    app.audio().setMasterGain(1.0f);
    app.audio().setBusGain(HBE::Platform::Audio::Bus::Music, 0.65f);
    app.audio().setBusGain(HBE::Platform::Audio::Bus::SFX, 1.0f);
    app.audio().setBusGain(HBE::Platform::Audio::Bus::UI, 0.90f);
    app.audio().setBusGain(HBE::Platform::Audio::Bus::Ambient, 0.80f);

    app.audio().loadSound("footstep", "assets/audio/test_sound.wav", true);
    app.audio().loadSound("hit", "assets/audio/test_sound.wav", true);
    app.audio().loadSound("ui_blip", "assets/audio/test_sound.wav", true);

    // When you add a real music file later, swap the path below.
    // Example:
    // app.audio().loadMusic("bgm_main", "assets/audio/forest_theme.ogg");
    // app.audio().playMusic("bgm_main", -1, 0.8f);

    // choose collision layer by name
    m_collisionLayer = m_tileMap.findLayer("Ground");
    if (!m_collisionLayer) {
        LogFatal("Tilemap missing collision layer named 'Ground'");
        m_app->requestQuit();
        return;
    }

    // Sample the top-of-tile colors used to tint the player-dust particles.
    if (!HBE::Renderer::TileMapLoader::sampleTileTopColors(m_tileMap, m_tileTopColors)) {
        HBE::Core::LogInfo("TileMapLoader::sampleTileTopColors failed; player dust will use fallback color.");
    }

    // Hook Scene2D physics/collision to this tilemap layer
    m_scene.setTileCollisionContext(&m_tileMap, m_collisionLayer);

    // Physics-lite tuning (gravity + substeps)
    {
        HBE::Renderer::Physics2DSettings phys{};
        phys.gravityY = -1800.0f;         // world units / s^2 (negative = down)
        phys.maxSubSteps = 4;             // stability
        phys.maxStepDt = 1.0f / 120.0f;   // stability
        m_scene.setPhysics2DSettings(phys);
    }

    // -------------------------
    // Dev Console commands
    // -------------------------
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

    // make sure scene matches current toggle
    m_scene.setCullingEnabled(m_enableCulling);

    // -----------------------------
    // ECS: attach gameplay components + scripts
    // -----------------------------
    auto& reg = m_scene.registry();

    // Tag your initial entities (helps scene save find �Player�/�Goblin�)
    if (reg.valid(m_soldierEntity)) {
        if (!reg.has<HBE::ECS::TagComponent>(m_soldierEntity))
            reg.emplace<HBE::ECS::TagComponent>(m_soldierEntity, HBE::ECS::TagComponent{ "Player" });
    }
    if (reg.valid(m_goblinEntity)) {
        if (!reg.has<HBE::ECS::TagComponent>(m_goblinEntity))
            reg.emplace<HBE::ECS::TagComponent>(m_goblinEntity, HBE::ECS::TagComponent{ "Goblin" });
    }

    // Player (soldier): Collider + Rigidbody + Script controller
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

        // ---- PLATFORMER DEFAULTS ----
        rb.useGravity = true;
        rb.gravityScale = 1.0f;
        rb.maxFallSpeed = -2200.0f; // clamp falling speed (negative down)
        rb.maxStepUp = m_tileMap.worldTileH() * 0.35f;
        rb.enableOneWay = true;
        rb.enableSlopes = true;

        if (!reg.has<HBE::ECS::RigidBody2D>(m_soldierEntity))
            reg.emplace<HBE::ECS::RigidBody2D>(m_soldierEntity, rb);

        HBE::ECS::Script sc{};
        sc.name = "PlayerController";
        sc.onUpdate = [this](HBE::ECS::Entity e, float dt) {
            if (m_console.isOpen()) {
                // Optional: kill movement so player doesn't keep sliding
                auto& r = m_scene.registry();
                if (r.has<HBE::ECS::RigidBody2D>(e)) {
                    auto& body = r.get<HBE::ECS::RigidBody2D>(e);
                    body.accelX = 0.0f;
                    body.accelY = 0.0f;
                    body.velX = 0.0f;
                    // don't zero velY if you want gravity to keep acting while typing
                }
                return;
            }
            auto& r = m_scene.registry();
            if (!r.has<HBE::ECS::RigidBody2D>(e) || !r.has<Transform2D>(e)) return;

            auto& body = r.get<HBE::ECS::RigidBody2D>(e);

            // -------- INPUT (mapped) --------
            const float inputX = HBE::Input::AxisValue(HBE::Input::Axis::MoveX);
            const float inputY = HBE::Input::AxisValue(HBE::Input::Axis::MoveY);

            const bool Down = (inputY > 0.5f);
            const bool JumpPressed = HBE::Input::ActionPressed(HBE::Input::Action::Jump);
            const bool AttackPressed = HBE::Input::ActionPressed(HBE::Input::Action::Attack);

            // -------- TUNING --------
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
            }
            else if (body.grounded) {
                body.velX = Approach(body.velX, 0.0f, friction * dt);
            }

            if (JumpPressed && body.grounded && !Down) {
                body.velY = jumpVel;
                body.grounded = false;
            }

            if (JumpPressed && Down) {
                body.oneWayDisableTimer = 0.20f;
                body.velY = std::min(body.velY, -120.0f);
                body.grounded = false;
            }

            if (auto* sAnim = m_scene.getSpriteAnimator(e)) {
                const bool moving = (std::fabs(body.velX) > 5.0f);
                sAnim->setBool("moving", moving);

                if (AttackPressed) {
                    sAnim->trigger("attack");
                }
            }
            };

        if (!reg.has<HBE::ECS::Script>(m_soldierEntity))
            reg.emplace<HBE::ECS::Script>(m_soldierEntity, std::move(sc));
    }

    // Goblin: Collider + STATIC Rigidbody + optional script
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

        HBE::ECS::Script sc{};
        sc.name = "GoblinTest";
        sc.onUpdate = [this](HBE::ECS::Entity e, float dt) {
            (void)dt;
            if (auto* gAnim = m_scene.getSpriteAnimator(e)) {
                gAnim->setBool("moving", false);
                if (HBE::Input::ActionPressed(HBE::Input::Action::UIConfirm)) {
                    gAnim->trigger("attack");
                }
            }
            };

        if (!reg.has<HBE::ECS::Script>(m_goblinEntity))
            reg.emplace<HBE::ECS::Script>(m_goblinEntity, std::move(sc));
    }

    LogInfo("GameLayer attached.");
}

void GameLayer::buildSpritePipeline() {
    auto& resources = m_app->resources();

    m_spriteShader = resources.getOrCreateShaderFromFiles("sprite", m_spriteVsPath, m_spriteFsPath);

    // Fallback (only if files missing)
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

    // quad mesh (pos + uv) as 2 triangles (6 verts) for the current Mesh API
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

    // Tell Renderer2D which mesh identifies a sprite-quad, so anything drawn with
    // this mesh flows through the batch (uses the batch VAO with per-vertex color)
    // instead of falling back to the mesh's own VAO (which lacks aColor).
    m_app->renderer2D().setSpriteQuadMesh(m_quadMesh);

    // Debug draw setup (uses the same quad mesh)
    if (!m_debug.initialize(resources, m_quadMesh)) {
        LogFatal("GameLayer: DebugDraw2D init failed");
        m_app->requestQuit();
        return;
    }

    // Text renderer
    if (!m_text.initialize(m_app->resources(), m_spriteShader, m_quadMesh)) {
        LogFatal("GameLayer: TextRenderer2D init failed");
        m_app->requestQuit();
        return;
    }

    // Font
    if (!m_text.loadSDFont(m_app->resources(),
        "ui",
        "assets/fonts/BoldPixels.ttf",
        16.0f,
        1024, 1024,
        12))
    {
        LogError("FAILED to load font: assets/fonts/BoldPixels.ttf");
    }
    else {
        m_text.setActiveFont("ui");
    }

    // Sprite sheet
    SpriteSheetDesc desc{};
    desc.frameWidth = 100;
    desc.frameHeight = 100;

    m_goblinSheet = SpriteRenderer2D::declareSpriteSheet(resources, "orc_sheet", "assets/Orc.png", desc);
    m_soldierSheet = SpriteRenderer2D::declareSpriteSheet(resources, "soldier_sheet", "assets/Soldier.png", desc);

    if (!m_goblinSheet.texture) {
        LogFatal("GameLayer: failed to load assets/Orc.png");
        m_app->requestQuit();
        return;
    }
    if (!m_soldierSheet.texture) {
        LogFatal("GameLayer: failed to load assets/Soldier.png");
        m_app->requestQuit();
        return;
    }

    // materials
    m_goblinMaterial.shader = m_spriteShader;
    m_goblinMaterial.texture = m_goblinSheet.texture;

    m_soldierMaterial.shader = m_spriteShader;
    m_soldierMaterial.texture = m_soldierSheet.texture;

    // entities
    RenderItem goblin{};
    goblin.mesh = m_quadMesh;
    goblin.material = &m_goblinMaterial;
    goblin.transform.posX = 1080.0f;
    goblin.transform.posY = 95.0f;
    goblin.transform.scaleX = desc.frameWidth * SPRITE_PIXEL_SCALE;
    goblin.transform.scaleY = desc.frameHeight * SPRITE_PIXEL_SCALE;
    goblin.layer = 100; // base entity layer
    goblin.sortKey = goblin.transform.posY;

    RenderItem soldier{};
    soldier.mesh = m_quadMesh;
    soldier.material = &m_soldierMaterial;
    soldier.transform.posX = 64.0f;
    soldier.transform.posY = 95.0f;
    soldier.transform.scaleX = desc.frameWidth * SPRITE_PIXEL_SCALE;
    soldier.transform.scaleY = desc.frameHeight * SPRITE_PIXEL_SCALE;

    // Make player render above goblin:
    soldier.layer = 200;
    soldier.sortKey = soldier.transform.posY;

    SpriteRenderer2D::setFrame(goblin, m_goblinSheet, 0, 0);
    SpriteRenderer2D::setFrame(soldier, m_soldierSheet, 0, 0);

    m_goblinEntity  = m_scene.createEntity(goblin);
    m_soldierEntity = m_scene.createEntity(soldier);

    // -----------------------------
    // Per-entity anim SM setup
    // -----------------------------

    // Goblin
    if (auto* gAnim = m_scene.addSpriteAnimator(m_goblinEntity, &m_goblinSheet)) {
        gAnim->addClip({ "Idle",   0, 0, 6, 0.10f, true,  1.0f });
        gAnim->addClip({ "Run",    1, 0, 6, 0.08f, true,  1.0f });
        gAnim->addClip({ "Attack", 2, 0, 6, 0.07f, false, 1.0f });

        gAnim->addEvent("Run", 1, "footstep");
        gAnim->addEvent("Run", 4, "footstep");
        gAnim->addEvent("Attack", 3, "hitframe");

        gAnim->addState("Idle", "Idle");
        gAnim->addState("Run", "Run");
        gAnim->addState("Attack", "Attack");

        gAnim->addTransitionTrigger("*", "Attack", "attack");
        gAnim->addTransitionBool("Idle", "Run", "moving", true);
        gAnim->addTransitionBool("Run", "Idle", "moving", false);
        gAnim->addTransitionFinished("Attack", "Idle");

        gAnim->setState("Idle", true);
    }

    // Soldier
    if (auto* sAnim = m_scene.addSpriteAnimator(m_soldierEntity, &m_soldierSheet)) {
        sAnim->addClip({ "Idle",   0, 0, 6, 0.10f, true,  1.0f });
        sAnim->addClip({ "Run",    1, 0, 8, 0.10f, true,  1.0f });
        sAnim->addClip({ "Attack", 2, 0, 7, 0.07f, false, 1.0f });

        sAnim->addEvent("Run", 2, "footstep");
        sAnim->addEvent("Run", 6, "footstep");
        sAnim->addEvent("Attack", 4, "hitframe");

        sAnim->addState("Idle", "Idle");
        sAnim->addState("Run", "Run");
        sAnim->addState("Attack", "Attack");

        sAnim->addTransitionTrigger("*", "Attack", "attack");
        sAnim->addTransitionBool("Idle", "Run", "moving", true);
        sAnim->addTransitionBool("Run", "Idle", "moving", false);
        sAnim->addTransitionFinished("Attack", "Idle");

        sAnim->setState("Idle", true);
    }
}

void GameLayer::onUpdate(float dt) {
    // fullscreen toggle stays in layer for now
    if (HBE::Input::ActionPressed(HBE::Input::Action::FullscreenToggle)) {
        // (left blank like your current file)
    }

    // Controlled entity (for camera follow)
    Transform2D* playerTr = m_scene.getTransform(m_soldierEntity);
    if (!playerTr) return;

    // Keep the audio listener centered on the player/camera.
    m_app->audio().setListenerPosition(playerTr->posX, playerTr->posY);

    // Quick audio test keys
    if (KeyPressed(SDL_SCANCODE_T)) {
        HBE::Platform::Audio::PlayParams p;
        p.bus = HBE::Platform::Audio::Bus::SFX;
        p.gain = 1.0f;
        p.positional = true;
        p.worldX = playerTr->posX + 120.0f; // sound slightly to the right
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

    // ---- stats ----
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

    // -------------------------
    // Scene Save/Load hotkeys
    // -------------------------
    // NOTE: adjust KeyPressed() mapping if your Platform::Input uses different names.
    if (KeyPressed(SDL_SCANCODE_F5)) {
        std::string err;

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

        const bool ok = HBE::Renderer::SceneSerializer::saveToFile(
            m_scene, SCENE_PATH, saveCb, m_tileMapPath, &err);

        if (ok) LogInfo(std::string("Scene saved: ") + SCENE_PATH);
        else    LogError("Scene save FAILED: " + err);
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

        // Bind scripts by name (re-attach lambdas after loading)
        loadCb.bindScript = [this](HBE::ECS::Entity e, const std::string& name, HBE::Renderer::Scene2D& scene) {
            auto& reg = scene.registry();
            if (!reg.has<HBE::ECS::Script>(e)) return;

            auto& sc = reg.get<HBE::ECS::Script>(e);
            sc.name = name;

            if (name == "PlayerController") {
                sc.onUpdate = [this](HBE::ECS::Entity ent, float dt) {
                    auto& r = m_scene.registry();
                    if (!r.has<HBE::ECS::RigidBody2D>(ent) || !r.has<Transform2D>(ent)) return;

                    auto& body = r.get<HBE::ECS::RigidBody2D>(ent);

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
                    }
                    else if (body.grounded) {
                        body.velX = Approach(body.velX, 0.0f, friction * dt);
                    }

                    if (JumpPressed && body.grounded && !Down) {
                        body.velY = jumpVel;
                        body.grounded = false;
                    }

                    if (JumpPressed && Down) {
                        body.oneWayDisableTimer = 0.20f;
                        body.velY = std::min(body.velY, -120.0f);
                        body.grounded = false;
                    }

                    if (auto* sAnim = m_scene.getSpriteAnimator(ent)) {
                        const bool moving = (std::fabs(body.velX) > 5.0f);
                        sAnim->setBool("moving", moving);
                        if (AttackPressed) sAnim->trigger("attack");
                    }
                    };
            }
            else if (name == "GoblinTest") {
                sc.onUpdate = [this](HBE::ECS::Entity ent, float dt) {
                    (void)dt;
                    if (auto* gAnim = m_scene.getSpriteAnimator(ent)) {
                        gAnim->setBool("moving", false);
                        if (HBE::Input::ActionPressed(HBE::Input::Action::UIConfirm)) {
                            gAnim->trigger("attack");
                        }
                    }
                    };
            }
            };

        // Animator preset builder (rebuild the SM from a preset name)
        loadCb.buildAnimatorPreset = [this](HBE::ECS::Entity e,
            const std::string& preset,
            HBE::Renderer::SpriteAnimationStateMachine& sm,
            HBE::Renderer::Scene2D& scene)
            {
                (void)e; (void)scene;

                if (preset == "GoblinAnimator") {
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
                }

                if (preset == "SoldierAnimator") {
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
                }
            };

        const bool ok = HBE::Renderer::SceneSerializer::loadFromFile(
            m_scene, SCENE_PATH, loadCb, &tilemapPath, &err);

        if (!ok) {
            LogError("Scene load FAILED: " + err);
        }
        else {
            LogInfo(std::string("Scene loaded: ") + SCENE_PATH);

            // Re-hook tilemap if scene file requested a different one
            if (!tilemapPath.empty()) {
                m_tileMapPath = tilemapPath;
            }

            // Rebuild tilemap (and collision context) using your existing hot-reload path
            hotReloadTileMap();

            // Re-find key entities by tag so camera follows correctly
            m_soldierEntity = {};
            m_goblinEntity = {};

            auto& reg = m_scene.registry();
            for (auto ent : reg.view<HBE::ECS::TagComponent>()) {
                const auto& tag = reg.get<HBE::ECS::TagComponent>(ent).tag;
                if (tag == "Player")  m_soldierEntity = ent;
                if (tag == "Goblin")  m_goblinEntity = ent;
            }
        }

        // FPS history (instant)
        float instFps = (dt > 0.00001f) ? (1.0f / dt) : 0.0f;
        m_fpsHistory.push_back(instFps);
        if ((int)m_fpsHistory.size() > m_fpsHistoryMax) {
            m_fpsHistory.erase(m_fpsHistory.begin(), m_fpsHistory.begin() + (m_fpsHistory.size() - m_fpsHistoryMax));
        }
    }

    // Hot reload poll
    m_watcher.poll(dt);

    // Tick scripts + physics + animators.
    // Catch animation events here.
    m_scene.update(dt, [&](const std::string& ev) {
        Transform2D* tr = m_scene.getTransform(m_soldierEntity);
        const float px = tr ? tr->posX : m_camera.x;
        const float py = tr ? tr->posY : m_camera.y;

        if (ev == "footstep") {
            spawnPopup(px, py - 30.0f, "step", Color4{ 0.8f,0.9f,1.0f,1.0f }, 0.35f, 35.0f);

            HBE::Platform::Audio::PlayParams p;
            p.bus = HBE::Platform::Audio::Bus::SFX;
            p.gain = 0.55f;
            p.positional = true;
            p.worldX = px;
            p.worldY = py;
            p.minDistance = 24.0f;
            p.maxDistance = 260.0f;
            p.panRange = 220.0f;

            m_app->audio().playSoundEx("footstep", p);
        }
        else if (ev == "hitframe") {
            spawnPopup(px, py + 50.0f, "HIT!", Color4{ 1.0f,0.3f,0.2f,1.0f }, 0.55f, 25.0f);

            HBE::Platform::Audio::PlayParams p;
            p.bus = HBE::Platform::Audio::Bus::SFX;
            p.gain = 0.9f;
            p.positional = true;
            p.worldX = px;
            p.worldY = py;
            p.minDistance = 18.0f;
            p.maxDistance = 320.0f;
            p.panRange = 260.0f;

            m_app->audio().playSoundEx("hit", p);
        }
        });

    // Player dust: while grounded and moving, kick up tinted dust at the feet.
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
                    m_playerDustCooldown = 0.14f; // ~7 puffs per second while running
                }
            }
            else {
                m_playerDustCooldown = 0.0f; // fire immediately next time we start moving
            }
        }
    }

    // Keep rain centred on the camera so the spawn rectangle always covers the viewport.
    // On the first update, snap the camera to the player first so the pre-warm samples the
    // right area — otherwise the emitter (and all pre-warmed drops) end up at the world origin.
    if (!m_rainPreWarmed) {
        m_camera.x = playerTr->posX;
        m_camera.y = playerTr->posY;
        m_app->gl().setCamera(m_camera);
    }

    if (m_particles.isAlive(m_rainHandle))
        m_particles.setPosition(m_rainHandle, m_camera.x, m_camera.y);

    if (!m_rainPreWarmed) {
        // Simulate ~2 seconds so the world starts already populated with drops at every height.
        for (int i = 0; i < 60; ++i) {
            m_particles.update(1.0f / 30.0f);
        }
        m_rainPreWarmed = true;
    }

    m_particles.update(dt);

    // camera follow
    m_camera.x = playerTr->posX;
    m_camera.y = playerTr->posY;
    m_app->gl().setCamera(m_camera);

    // debug popup aging / movement
    for (auto& p : m_popups) {
        p.life -= dt;
        p.y += p.floatSpeed * dt;
    }

    // remove dead
    m_popups.erase(
        std::remove_if(m_popups.begin(), m_popups.end(),
            [](const DebugPopup& p) { return p.life <= 0.0f; }),
        m_popups.end()
    );
}

void GameLayer::onRender() {
    m_frameCount++;

    // Reset per-frame text renderer state
    m_text.beginFrame(m_frameCount);

    Renderer2D& r2d = m_app->renderer2D();
    m_ui.beginFrame(m_lastDt);

    // -------------------------
    // PASS 1: WORLD
    // -------------------------
    r2d.beginScene(m_camera, HBE::Renderer::RenderPass::World);

    // draw map
    m_tileRenderer.draw(r2d, m_tileMap);

    // --- DAMAGE TEXT (WORLD SPACE) ---
    HBE::Renderer::TextRenderer2D::TextAnim dmg{};
    dmg.t = std::fmod(m_uiAnimT, 1.0f);
    dmg.duration = 1.0f;
    dmg.autoExpire = true;
    dmg.fadeIn = true;
    dmg.fadeInTime = 0.05f;
    dmg.fadeOut = true;
    dmg.fadeOutTime = 0.35f;
    dmg.velY = 40.0f;
    dmg.startScale = 1.2f;
    dmg.endScale = 1.0f;

    Transform2D* trg = m_scene.getTransform(m_goblinEntity);
    if (trg) {
        m_text.drawTextAnimated(r2d,
            trg->posX, trg->posY + 40.0f,
            "-25",
            1.0f,
            { 1.0f, 0.0f, 0.0f, 1.0f },
            TextRenderer2D::TextAlignH::Center,
            TextRenderer2D::TextAlignV::Baseline,
            0.0f,
            1.0f,
            dmg);
    }

    // draw sprites/entities
    m_scene.render(r2d);

    // ── Particles (above sprites at layer 200) ─────────────────────────────────
    m_particles.render(r2d);

    // debug draw (WORLD)
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

                // color: trigger=yellow, dynamic=green, static=cyan-ish
                float r = 0.0f, g = 1.0f, b = 0.0f, a = 1.0f;

                if (col.isTrigger) { r = 1.0f; g = 1.0f; b = 0.2f; }
                if (reg.has<HBE::ECS::RigidBody2D>(e) && reg.get<HBE::ECS::RigidBody2D>(e).isStatic) { r = 0.2f; g = 0.9f; b = 1.0f; }

                m_debug.rect(r2d, cx, cy, w, h, r, g, b, a, false);
            }
        }
    }

    // world popups
    for (const auto& p : m_popups) {
        float t = (p.maxLife > 0.0f) ? (p.life / p.maxLife) : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);

        Color4 c = p.color;
        c.a *= t;

        m_text.drawText(r2d, p.x, p.y, p.text, 1.0f, c);
    }

    r2d.endScene();

    // -------------------------
    // PASS 2: UI (widgets + panels)
    // -------------------------
    HBE::Renderer::Camera2D uiCam{};
    uiCam.x = LOGICAL_WIDTH * 0.5f;
    uiCam.y = LOGICAL_HEIGHT * 0.5f;
    uiCam.zoom = 1.0f;
    uiCam.viewportWidth = LOGICAL_WIDTH;
    uiCam.viewportHeight = LOGICAL_HEIGHT;

    r2d.beginScene(uiCam, HBE::Renderer::RenderPass::UI);

    // Bind UI renderer dependencies now that we�re in UI space
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

        m_ui.endPanel();
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
        g.h = 50.0f; // <- actually small now

        m_ui.beginPanel("fps_graph", g, "FPS");

        // Show current (single line only)
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
        m_ui.beginPanel("inspector", insp, "Inspector");

        auto& reg = m_scene.registry();

        m_ui.label("Entities", true);

        // live list (Transform-driven)
        int shown = 0;
        for (auto e : reg.view<HBE::Renderer::Transform2D>()) {
            if (shown++ >= 12) { // keep it readable for now
                m_ui.label("... (more omitted)", true);
                break;
            }

            std::string name = "Entity " + std::to_string((uint32_t)e);
            if (reg.has<HBE::ECS::TagComponent>(e)) {
                const auto& tag = reg.get<HBE::ECS::TagComponent>(e).tag;
                if (!tag.empty()) name = tag + " (" + std::to_string((uint32_t)e) + ")";
            }

            std::string id = "ent_" + std::to_string((uint32_t)e);
            if (m_ui.button(id.c_str(), name.c_str())) {
                m_selectedEntity = e;
            }
        }

        m_ui.spacing(10.0f);

        // selected details
        if (reg.valid(m_selectedEntity)) {
            std::string header = "Selected: " + std::to_string((uint32_t)m_selectedEntity);
            m_ui.label(header.c_str(), false);
            m_ui.spacing(6.0f);

            // TAG
            if (reg.has<HBE::ECS::TagComponent>(m_selectedEntity)) {
                const auto& tag = reg.get<HBE::ECS::TagComponent>(m_selectedEntity).tag;
                std::string tagLine = "Tag: " + tag;
                m_ui.label(tagLine.c_str(), true);
            }

            // TRANSFORM
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

            // SPRITE
            if (reg.has<HBE::Renderer::SpriteComponent2D>(m_selectedEntity)) {
                auto& spr = reg.get<HBE::Renderer::SpriteComponent2D>(m_selectedEntity);
                m_ui.label("Sprite", true);
                m_ui.sliderInt("spr_layer", "layer", spr.layer, -10, 50);
                m_ui.sliderFloat("spr_sort", "sortKey", spr.sortKey, -5000.0f, 5000.0f, 1.0f);
                m_ui.sliderFloat("spr_sorty", "sortOffsetY", spr.sortOffsetY, -200.0f, 200.0f, 1.0f);
                m_ui.spacing(6.0f);
            }

            // COLLIDER
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

            // RIGIDBODY
            if (reg.has<HBE::ECS::RigidBody2D>(m_selectedEntity)) {
                auto& rb = reg.get<HBE::ECS::RigidBody2D>(m_selectedEntity);
                m_ui.label("RigidBody2D", true);
                m_ui.checkbox("rb_static", "isStatic", rb.isStatic);
                m_ui.checkbox("rb_grav", "useGravity", rb.useGravity);
                m_ui.sliderFloat("rb_vx", "velX", rb.velX, -4000.0f, 4000.0f, 1.0f);
                m_ui.sliderFloat("rb_vy", "velY", rb.velY, -4000.0f, 4000.0f, 1.0f);
                m_ui.sliderFloat("rb_damp", "linearDamping", rb.linearDamping, 0.0f, 50.0f, 0.25f);
                m_ui.spacing(6.0f);
            }

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

    r2d.endScene();

    // -------------------------
    // PASS 3: OVERLAY (dev console — always on top of world + UI)
    // -------------------------
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

    // UI can still receive mouse events
    m_ui.onEvent(e);

    // Toggle console with ~ (grave)
    if (e.type() == EventType::KeyPressed) {
        auto& ke = static_cast<KeyPressedEvent&>(e);
       
        // Console Toggle (Grave/tilde) should work even when console is open
        if (ke.keyScancode == SDL_SCANCODE_GRAVE) {
            m_console.toggle();
            e.handled = true;
            return true;
        }

        // if console is open, let it consume key events FIRST
        // Don't process other hotkeys while typing in console
        if (m_console.isOpen()) {
            m_console.onEvent(e);
            return e.handled; // Stops here, do not process F1-F5 while console is open
        }

        // only process dev hotkeys if console is CLOSED
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

    // Fallback dust color if we can't sample the tile below the player.
    float r = 0.82f, g = 0.72f, b = 0.55f;

    if (m_collisionLayer) {
        const float tw = m_tileMap.worldTileW();
        const float th = m_tileMap.worldTileH();

        if (tw > 0.0f && th > 0.0f) {
            const int tx = (int)std::floor(feetX / tw);
            // Sample slightly below the feet so we hit the tile the player is standing on rather
            // than the empty space they occupy. Then walk down a couple of tiles in case the very
            // top row is a one-way / slope with a transparent leading edge.
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

    // Re-register player_dust with the tinted colors. ParticleSystem copies the config on spawn,
    // so this cheap swap changes only the next burst's colors.
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

    // Shader hot reload
    m_watcher.watchFile(m_spriteVsPath, [this](const std::string&) { hotReloadShader(); });
    m_watcher.watchFile(m_spriteFsPath, [this](const std::string&) { hotReloadShader(); });

    // Tilemap hot reload
    m_watcher.watchFile(m_tileMapPath, [this](const std::string&) { hotReloadTileMap(); });

    // UI theme hot reload
    m_watcher.watchFile(m_uiThemePath, [this](const std::string&) { hotReloadUITheme(); });

    // Texture hot reload (these names match your SpriteRenderer2D declare calls)
    m_watcher.watchFile("assets/Orc.png", [this](const std::string& p) { hotReloadTextureByPath(p); });
    m_watcher.watchFile("assets/Soldier.png", [this](const std::string& p) { hotReloadTextureByPath(p); });
    m_watcher.watchFile("assets/tiles/tiles.png", [this](const std::string& p) { hotReloadTextureByPath(p); });
}

void GameLayer::hotReloadShader() {
    if (!m_app) return;
    bool ok = m_app->resources().reloadShader("sprite");

    // (world-space popup: may be off-screen depending on camera)
    spawnPopup(20.0f, 680.0f,
        ok ? "Shader reloaded: sprite" : "Shader reload FAILED (see log)",
        ok ? HBE::Renderer::Color4{ 0.3f, 1.0f, 0.3f, 1.0f } : HBE::Renderer::Color4{ 1.0f, 0.3f, 0.3f, 1.0f },
        1.25f, 0.0f);
}

void GameLayer::hotReloadTileMap() {
    // Load New Map    
    HBE::Renderer::TileMap newMap{};
    std::string err;
    auto newMapBool = TileMapLoader::loadFromJsonFile(m_tileMapPath, newMap, &err);
    if (!newMapBool) {
        LogError("TileMap hot-reload: failed to load " + m_tileMapPath);
        spawnPopup(20.0f, 650.0f, "Tilemap reload FAILED: failed to load " + m_tileMapPath, HBE::Renderer::Color4{ 1.0f, 0.3f, 0.3f, 1.0f }, 1.5f,0.0f);
        return;
    }

    // Check for required layers before committing
    auto groundLayer = newMap.findLayer("Ground");
    if (!groundLayer) {
        LogError("TileMap hot-reload: missing required 'Ground' layer");
        spawnPopup(20.0f, 650.0f, "Tilemap reload FAILED: missing required 'Ground' layer", HBE::Renderer::Color4{ 1.0f, 0.3f, 0.3f, 1.0f }, 1.5f, 0.0f);
        return;
    }
    
    // Validations passed, safe to commit changes
    m_tileMap = std::move(newMap);

    // these should succeed
    m_tileRenderer.build(m_app->renderer2D(), m_app->resources(), m_spriteShader, m_quadMesh, m_tileMap);
    m_collisionLayer = m_tileMap.findLayer("Ground");

    // update collision context
    if (m_collisionLayer) {
        m_scene.setTileCollisionContext(&m_tileMap, m_collisionLayer);
    }

    // Re-sample tile top colors for the new map
    HBE::Renderer::TileMapLoader::sampleTileTopColors(m_tileMap, m_tileTopColors);

    m_scene.setTileCollisionContext(&m_tileMap, m_collisionLayer);

    LogInfo("TileMap hot-reloaded succesfully: " + m_tileMapPath);
    spawnPopup(20.0f, 650.0f, "TileMap hot-reloaded succesfully",
        HBE::Renderer::Color4{ 0.3f, 1.0f, 0.3f, 1.0f }, 1.25f, 0.0f);
}

void GameLayer::hotReloadUITheme() {
    HBE::Renderer::UI::UIStyle s = m_ui.style();
    std::string err;

    bool ok = HBE::Renderer::UI::UIThemeLoader::loadStyleFromJsonFile(m_uiThemePath, s, &err);
    if (!ok) {
        spawnPopup(20.0f, 620.0f, "UI theme reload FAILED: " + err,
            HBE::Renderer::Color4{ 1.0f, 0.3f, 0.3f, 1.0f }, 1.5f, 0.0f);
        return;
    }

    m_ui.setStyle(s);

    spawnPopup(20.0f, 620.0f, "UI theme reloaded",
        HBE::Renderer::Color4{ 0.3f, 1.0f, 0.3f, 1.0f }, 1.25f, 0.0f);
}

void GameLayer::hotReloadTextureByPath(const std::string& path) {
    if (!m_app) return;

    // Map file path -> cache name you used when loading
    if (path == "assets/Orc.png") {
        m_app->resources().reloadTexture("orc_sheet");
        spawnPopup(20.0f, 590.0f, "Texture reloaded: Orc.png",
            HBE::Renderer::Color4{ 0.3f, 1.0f, 0.3f, 1.0f }, 1.0f, 0.0f);
    }
    else if (path == "assets/Soldier.png") {
        m_app->resources().reloadTexture("soldier_sheet");
        spawnPopup(20.0f, 590.0f, "Texture reloaded: Soldier.png",
            HBE::Renderer::Color4{ 0.3f, 1.0f, 0.3f, 1.0f }, 1.0f, 0.0f);
    }
    else if (path == "assets/tiles/tiles.png") {
        // If your TileMapRenderer uses a different cache key, adjust this:
        m_app->resources().reloadTexture("basic");

        spawnPopup(20.0f, 590.0f, "Texture reloaded: tiles.png",
            HBE::Renderer::Color4{ 0.3f, 1.0f, 0.3f, 1.0f }, 1.0f, 0.0f);
    }
}

void GameLayer::onDetach() {
    m_particles.shutdown();
}