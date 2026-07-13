#include "HBE/Renderer/SceneSerializer.h"

#include "HBE/Core/Log.h"
#include "HBE/Core/UUID.h"
#include "HBE/Core/AssetPaths.h"

#include "HBE/ECS/Components.h"
#include "HBE/ECS/ESCSComponents2D.h"
#include "HBE/ECS/RuntimeComponents.h"

#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/Transform2D.h"
#include "HBE/Renderer/Prefab.h"
#include "HBE/Renderer/PrefabLibrary.h"
#include "HBE/Renderer/ScriptRegistry.h"
#include "HBE/Renderer/AnimationPresetRegistry.h"

#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <json.hpp>

using json = nlohmann::json;

namespace HBE::Renderer {

    static bool readAllText(const std::string& path, std::string& out) {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        std::stringstream ss;
        ss << f.rdbuf();
        out = ss.str();
        return true;
    }

    static bool writeAllText(const std::string& path, const std::string& text) {
        std::ofstream f(path, std::ios::binary);
        if (!f.is_open()) return false;
        f.write(text.data(), (std::streamsize)text.size());
        return true;
    }

    static json toJsonTransform(const Transform2D& t) {
        return json{
            {"x", t.posX},
            {"y", t.posY},
            {"sx", t.scaleX},
            {"sy", t.scaleY},
            {"rot", t.rotation}
        };
    }

    static void fromJsonTransform(const json& j, Transform2D& t) {
        t.posX = j.value("x", 0.0f);
        t.posY = j.value("y", 0.0f);
        t.scaleX = j.value("sx", 1.0f);
        t.scaleY = j.value("sy", 1.0f);
        t.rotation = j.value("rot", 0.0f);
    }

    static json toJsonSprite(const SpriteComponent2D& s, const SceneSaveCallbacks& cb)
    {
        json j;
        j["layer"] = s.layer;
        j["sortOffsetY"] = s.sortOffsetY;
        j["uvRect"] = { s.uvRect[0], s.uvRect[1], s.uvRect[2], s.uvRect[3] };
        if (cb.meshKey) j["mesh"] = cb.meshKey(s.mesh);
        if (cb.materialKey) j["material"] = cb.materialKey(s.material);

        return j;
    }

    static bool fromJsonSprite(const json& j, SpriteComponent2D& s, const SceneLoadCallbacks& cb,
        std::string* outError)
    {
        const std::string meshKey = j.value("mesh", "");
        const std::string matKey = j.value("material", "");

        if (!cb.mesh || !cb.material) {
            if (outError) *outError = "SceneSerializer: load callbacks missing mesh/material resolvers.";
            return false;
        }

        s.mesh = cb.mesh(meshKey);
        s.material = cb.material(matKey);

        if (!s.mesh) {
            if (outError) *outError = "SceneSerializer: unknown mesh key: " + meshKey;
            return false;
        }
        if (!s.material) {
            if (outError) *outError = "SceneSerializer: unknown material key: " + matKey;
            return false;
        }

        s.layer = j.value("layer", 0);
        s.sortKey = 0.0f;
        s.sortOffsetY = j.value("sortOffsetY", 0.0f);

        if (j.contains("uvRect") && j["uvRect"].is_array() && j["uvRect"].size() == 4) {
            s.uvRect[0] = j["uvRect"][0].get<float>();
            s.uvRect[1] = j["uvRect"][1].get<float>();
            s.uvRect[2] = j["uvRect"][2].get<float>();
            s.uvRect[3] = j["uvRect"][3].get<float>();
        }
        else {
            s.uvRect[0] = 0.0f; s.uvRect[1] = 0.0f;
            s.uvRect[2] = 1.0f; s.uvRect[3] = 1.0f;
        }

        return true;
    }

    static json toJsonCollider(const HBE::ECS::Collider2D& c) {
        return json{
            {"halfW", c.halfW},
            {"halfH", c.halfH},
            {"offsetX", c.offsetX},
            {"offsetY", c.offsetY},
            {"isTrigger", c.isTrigger}
        };
    }

    static void fromJsonCollider(const json& j, HBE::ECS::Collider2D& c) {
        c.halfW = j.value("halfW", 0.5f);
        c.halfH = j.value("halfH", 0.5f);
        c.offsetX = j.value("offsetX", 0.0f);
        c.offsetY = j.value("offsetY", 0.0f);
        c.isTrigger = j.value("isTrigger", false);
    }

    static json toJsonRigidBody(const HBE::ECS::RigidBody2D& r) {
        return json{
            {"linearDamping", r.linearDamping},
            {"isStatic", r.isStatic},
            {"useGravity", r.useGravity},
            {"gravityScale", r.gravityScale},
            {"maxStepUp", r.maxStepUp},
            {"enableOneWay", r.enableOneWay},
            {"enableSlopes", r.enableSlopes},
            {"maxFallSpeed", r.maxFallSpeed}
        };
    }

    static void fromJsonRigidBody(const json& j, HBE::ECS::RigidBody2D& r) {
        r.velX = 0.0f;
        r.velY = 0.0f;
        r.accelX = 0.0f;
        r.accelY = 0.0f;
        r.grounded = false;
        r.oneWayDisableTimer = 0.0f;
        r.linearDamping = j.value("linearDamping", 0.0f);
        r.isStatic = j.value("isStatic", false);
        r.useGravity = j.value("useGravity", false);
        r.gravityScale = j.value("gravityScale", 1.0f);
        r.maxStepUp = j.value("maxStepUp", 0.0f);
        r.enableOneWay = j.value("enableOneWay", true);
        r.enableSlopes = j.value("enableSlopes", true);
        r.maxFallSpeed = j.value("maxFallSpeed", 0.0f);
    }

    static void migrateV1toV2(json& root) {
        if (!root.contains("entities") || !root["entities"].is_array()) return;

        for (auto& ej : root["entities"]) {
            if (!ej.contains("components") || !ej["components"].is_object()) continue;
            json& comps = ej["components"];

            if (comps.contains("RigidBody2D") && comps["RigidBody2D"].is_object()) {
                for (const char* key : {"velX", "velY", "accelX", "accelY",
                                        "grounded", "oneWayDisableTimer"}) {
                    comps["RigidBody2D"].erase(key);
                }
            }

            if (comps.contains("Sprite2D") && comps["Sprite2D"].is_object()) {
                comps["Sprite2D"].erase("sortKey");
            }

            if (comps.contains("Animator") && comps["Animator"].is_object()) {
                if (comps["Animator"].value("preset", "") == "UNSPECIFIED") {
                    comps["Animator"]["preset"] = "";
                }
            }
        }

        root["version"] = 2;
    }

    static void migrateV2toV3(json& root) {
        root["version"] = 3;
    }

    static void migrateToLatest(json& root) {
        const int version = root.value("version", 1);
        if (version < 2) migrateV1toV2(root);
        if (version < 3) migrateV2toV3(root);
    }

    struct SceneComponentSchema {
        std::string key;
        bool required = false;

        std::function<bool(HBE::ECS::Entity, HBE::ECS::Registry&, const SceneSaveCallbacks&, json&)> save;
        std::function<bool(HBE::ECS::Entity, HBE::ECS::Registry&, Scene2D&, const SceneLoadCallbacks&, const json&, std::string*)> load;
        std::function<void(HBE::ECS::Entity, HBE::ECS::Registry&)> installDefault;
    };

    static const std::vector<SceneComponentSchema>& componentSchemas() {
        static const std::vector<SceneComponentSchema> table = [] {
            std::vector<SceneComponentSchema> t;

            // -- PrefabRef (name-only; instance provenance) --
            t.push_back({
                "__prefab_ref__",
                false,
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   const SceneSaveCallbacks&, json&) -> bool {
                        (void)e; (void)reg;
                        return false;
                    },
                    [](HBE::ECS::Entity, HBE::ECS::Registry&,
                       Scene2D&, const SceneLoadCallbacks&,
                       const json&, std::string*) -> bool {
                            return true;
                        },
                        nullptr
                });

            // -- Transform2D (required) --
            t.push_back({
                "Transform2D",
                true,
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   const SceneSaveCallbacks&, json& out) -> bool {
                    if (!reg.has<Transform2D>(e)) return false;
                    out = toJsonTransform(reg.get<Transform2D>(e));
                    return true;
                },
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   Scene2D&, const SceneLoadCallbacks&,
                   const json& j, std::string*) -> bool {
                    Transform2D t{};
                    fromJsonTransform(j, t);
                    reg.emplace<Transform2D>(e, t);
                    return true;
                },
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg) {
                    if (!reg.has<Transform2D>(e))
                        reg.emplace<Transform2D>(e, Transform2D{});
                }
            });

            // -- Sprite2D --
            t.push_back({
                "Sprite2D",
                false,
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   const SceneSaveCallbacks& cb, json& out) -> bool {
                    if (!reg.has<SpriteComponent2D>(e)) return false;
                    out = toJsonSprite(reg.get<SpriteComponent2D>(e), cb);
                    return true;
                },
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   Scene2D&, const SceneLoadCallbacks& cb,
                   const json& j, std::string* outError) -> bool {
                    SpriteComponent2D s{};
                    std::string err;
                    if (!fromJsonSprite(j, s, cb, &err)) {
                        if (outError) *outError = err;
                        return false;
                    }
                    reg.emplace<SpriteComponent2D>(e, s);
                    return true;
                },
                nullptr
            });

            // -- Collider2D --
            t.push_back({
                "Collider2D",
                false,
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   const SceneSaveCallbacks&, json& out) -> bool {
                    if (!reg.has<HBE::ECS::Collider2D>(e)) return false;
                    out = toJsonCollider(reg.get<HBE::ECS::Collider2D>(e));
                    return true;
                },
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   Scene2D&, const SceneLoadCallbacks&,
                   const json& j, std::string*) -> bool {
                    HBE::ECS::Collider2D c{};
                    fromJsonCollider(j, c);
                    reg.emplace<HBE::ECS::Collider2D>(e, c);
                    return true;
                },
                nullptr
            });

            // -- RigidBody2D --
            t.push_back({
                "RigidBody2D",
                false,
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   const SceneSaveCallbacks&, json& out) -> bool {
                    if (!reg.has<HBE::ECS::RigidBody2D>(e)) return false;
                    out = toJsonRigidBody(reg.get<HBE::ECS::RigidBody2D>(e));
                    return true;
                },
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   Scene2D&, const SceneLoadCallbacks&,
                   const json& j, std::string*) -> bool {
                    HBE::ECS::RigidBody2D r{};
                    fromJsonRigidBody(j, r);
                    reg.emplace<HBE::ECS::RigidBody2D>(e, r);
                    return true;
                },
                nullptr
            });

            // -- Script (name-only; onUpdate re-bound by callback) --
            t.push_back({
                "Script",
                false,
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   const SceneSaveCallbacks&, json& out) -> bool {
                    if (!reg.has<HBE::ECS::Script>(e)) return false;
                    const auto& sc = reg.get<HBE::ECS::Script>(e);
                    out = json{ {"name", sc.name} };
                    return true;
                },
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   Scene2D& scene, const SceneLoadCallbacks& cb,
                   const json& j, std::string*) -> bool {
                    const std::string scriptName = j.value("name", "");
                    if (scriptName.empty()) return true;
                    if (cb.scripts && cb.scripts->has(scriptName)) {
                        HBE::ECS::Script sc = cb.scripts->create(scriptName, e, scene);
                        // create() guarantees sc.name is set even if the
                        // factory forgot; belt-and-braces:
                        if (sc.name.empty()) sc.name = scriptName;
                        reg.emplace<HBE::ECS::Script>(e, std::move(sc));
                        return true;
                    }
                    HBE::ECS::Script sc{};
                    sc.name = scriptName;
                    reg.emplace<HBE::ECS::Script>(e, sc);

                    if (cb.bindScript) {
                        const bool recognized = cb.bindScript(e, scriptName, scene);
                        if (!recognized) {
                            HBE::Core::LogWarn(
                                "SceneSerializer: script '" + scriptName +
                                "' is not recognized by the current binder. "
                                "Component attached with name preserved; "
                                "onUpdate is null.");
                        }
                        return true;
                    }

                    HBE::Core::LogWarn(
                        "SceneSerializer: no ScriptRegistry or bindScript "
                        "callback provided; script '" + scriptName +
                        "' will not run.");
                    return true;
                },
                nullptr
                });

            t.push_back({
                "Animator",
                false,
                [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   const SceneSaveCallbacks& cb, json& out) -> bool {
                    if (!reg.has<AnimationComponent2D>(e)) return false;
                    const auto& anim = reg.get<AnimationComponent2D>(e);

                    json aj;
                    if (reg.has<HBE::ECS::AnimatorPresetComponent>(e)) {
                        aj["preset"] = reg
                            .get<HBE::ECS::AnimatorPresetComponent>(e).preset;
                    }
                    else {
                        aj["preset"] = "";
                        HBE::Core::LogWarn("SceneSerializer: entity has "
                            "AnimationComponent2D but no "
                            "AnimatorPresetComponent - state machine will "
                            "not survive reload.");
                    }
                    aj["sheet"] = "";
                    if (cb.sheetKey && anim.sm.sheet)
                        aj["sheet"] = cb.sheetKey(anim.sm.sheet);
                    aj["state"] = anim.sm.getState().empty() ? "Idle"
                        : anim.sm.getState();

                    out = aj;
                    return true;
                },
                   [](HBE::ECS::Entity e, HBE::ECS::Registry& reg,
                   Scene2D& scene, const SceneLoadCallbacks& cb,
                   const json& j, std::string*) -> bool {
                    const std::string preset = j.value("preset", "");
                    const std::string sheetKey = j.value("sheet", "");
                    const std::string state = j.value("state", "Idle");

                    if (sheetKey.empty()) return true;
                    if (!cb.sheet) return true;

                    const auto* sheet = cb.sheet(sheetKey);
                    if (!sheet) {
                        HBE::Core::LogWarn(
                            "SceneSerializer: unknown sheet key '" +
                            sheetKey + "'; animator skipped.");
                        return true;
                    }

                    auto* sm = scene.addSpriteAnimator(e, sheet);
                    if (!sm) return true;

                    if (preset.empty()) {
                        return true;
                    }
                    if (cb.animators && cb.animators->has(preset)) {
                        cb.animators->build(preset, e, *sm, scene);
                        sm->setState(state, true);

                        HBE::ECS::AnimatorPresetComponent ap{};
                        ap.preset = preset;
                        reg.emplace<HBE::ECS::AnimatorPresetComponent>(e, ap);
                        return true;
                    }
                    if (cb.buildAnimatorPreset) {
                        cb.buildAnimatorPreset(e, preset, *sm, scene);
                        sm->setState(state, true);

                        HBE::ECS::AnimatorPresetComponent ap{};
                        ap.preset = preset;
                        reg.emplace<HBE::ECS::AnimatorPresetComponent>(e, ap);
                        return true;
                    }

                    HBE::Core::LogWarn(
                        "SceneSerializer: no AnimationPresetRegistry or "
                        "buildAnimatorPreset callback; preset '" + preset +
                        "' will not be rebuilt.");
                    return true;
                },
                nullptr
            });

            return t;
        }();
        return table;
    }

    bool SceneSerializer::saveToFile(Scene2D& scene,
        const std::string& path,
        const SceneSaveCallbacks& cb,
        const std::string& tilemapPath,
        std::string* outError)
    {
        auto& reg = scene.registry();

        if (!cb.meshKey || !cb.materialKey) {
            if (outError) *outError = "SceneSerializer::saveToFile requires meshKey/materialKey callbacks.";
            return false;
        }

        json root;
        root["version"] = SceneSerializer::kSceneVersion;
        root["tilemap"] = tilemapPath;

        json ents = json::array();

        const auto& schemas = componentSchemas();

        for (auto e : reg.view<Transform2D>()) {

            if (!reg.has<HBE::ECS::IDComponent>(e)) {
                HBE::Core::LogWarn("SceneSerializer::saveToFile: entity created "
                    "without going through Scene2D::createEntity � assigning "
                    "a UUID now, but this entity will get a *new* UUID next "
                    "session. Fix the call site.");
                HBE::ECS::IDComponent id{};
                id.uuid = HBE::Core::NewUUID32();
                reg.emplace<HBE::ECS::IDComponent>(e, id);
            }

            const auto& id = reg.get<HBE::ECS::IDComponent>(e);

            json ej;
            ej["uuid"] = id.uuid;
            ej["name"] = reg.has<HBE::ECS::TagComponent>(e)
                ? reg.get<HBE::ECS::TagComponent>(e).tag
                : (std::string("Entity_") + id.uuid.substr(0, 6));

            const PrefabDefinition* prefabDef = nullptr;
            if (cb.prefabs && reg.has<PrefabRefComponent>(e)) {
                const auto& ref = reg.get<PrefabRefComponent>(e);
                if (!ref.name.empty()) {
                    prefabDef = cb.prefabs->get(ref.name);
                    if (prefabDef) {
                        ej["prefab"] = ref.name;
                    }
                    else {
                        HBE::Core::LogWarn(
                            "SceneSerializer::saveToFile: entity '" +
                            ej["name"].get<std::string>() +
                            "' references prefab '" + ref.name +
                            "' but the library has no such name; writing "
                            "fully inline.");
                    }
                }
            }
            json comps = json::object();
            for (const auto& s : schemas) {
                if (s.key.rfind("__", 0) == 0) continue;

                json cj;
                if (!s.save(e, reg, cb, cj)) continue;

                if (prefabDef) {
                    const auto& base = prefabDef->components;
                    const auto baseIt = base.find(s.key);
                    if (baseIt != base.end() && *baseIt == cj) {
                        continue;
                    }
                }
                comps[s.key] = std::move(cj);
            }

            ej["components"] = comps;
            ents.push_back(ej);
        }

        root["entities"] = ents;

        const std::string text = root.dump(2);
        if (!writeAllText(path, text)) {
            if (outError) *outError = "SceneSerializer: failed to write: " + path;
            return false;
        }

        HBE::Core::LogInfo("Scene saved: " + path);
        return true;
    }

    bool SceneSerializer::loadFromFile(Scene2D& scene,
        const std::string& path,
        const SceneLoadCallbacks& cb,
        std::string* outTilemapPath,
        std::string* outError)
    {
        std::string text;
        if (!readAllText(path, text)) {
            if (outError) *outError = "SceneSerializer: could not open: " + path;
            return false;
        }

        json root;
        try {
            root = json::parse(text);
        }
        catch (...) {
            if (outError) *outError = "SceneSerializer: invalid JSON: " + path;
            return false;
        }

        const int fileVersion = root.value("version", 1);
        if (fileVersion > SceneSerializer::kSceneVersion) {
            if (outError) *outError = "SceneSerializer: scene file version " +
                std::to_string(fileVersion) + " is newer than engine (" +
                std::to_string(SceneSerializer::kSceneVersion) +
                "). Update the engine or downgrade the file.";
            return false;
        }
        if (fileVersion < SceneSerializer::kSceneVersion) {
            HBE::Core::LogInfo("SceneSerializer: migrating scene from v" +
                std::to_string(fileVersion) + " to v" +
                std::to_string(SceneSerializer::kSceneVersion));
            migrateToLatest(root);
        }

        if (outTilemapPath) {
            *outTilemapPath = root.value("tilemap", "");
        }

        if (!root.contains("entities") || !root["entities"].is_array()) {
            if (outError) *outError = "SceneSerializer: missing entities array.";
            return false;
        }

        scene.clear();
        auto& reg = scene.registry();

        const auto& schemas = componentSchemas();

        std::unordered_map<std::string, const SceneComponentSchema*> byName;
        byName.reserve(schemas.size());
        for (const auto& s : schemas) byName.emplace(s.key, &s);

        for (auto& ej : root["entities"]) {
            const std::string uuid       = ej.value("uuid", "");
            const std::string name       = ej.value("name", "");
            const std::string prefabName = ej.value("prefab", "");
            const json overrides         = ej.value("components", json::object());

            HBE::ECS::Entity e = reg.create();

            json comps = json::object();
            const PrefabDefinition* prefabDef = nullptr;

            if (!prefabName.empty()) {
                if (cb.prefabs) {
                    prefabDef = cb.prefabs->get(prefabName);
                }
                if (prefabDef) {
                    comps = prefabDef->components;   // deep copy
                }
                else {
                    HBE::Core::LogWarn(
                        "SceneSerializer::loadFromFile: entity '" +
                        (name.empty() ? uuid : name) +
                        "' references prefab '" + prefabName +
                        "' but " +
                        (cb.prefabs ? std::string("the library has no such name.")
                                    : std::string("no prefab library was provided.")) +
                        " Falling back to scene-only components.");
                }
            }

            if (prefabDef) {
                comps.merge_patch(overrides);
            }
            else {
                comps = overrides;
            }

            const std::string chosenUuid = uuid.empty()
                ? HBE::Core::NewUUID32() : uuid;

            std::string chosenTag;
            if (!name.empty())                              chosenTag = name;
            else if (prefabDef && !prefabDef->tag.empty())  chosenTag = prefabDef->tag;
            else chosenTag = std::string("Entity_") + chosenUuid.substr(0, 6);

            if (!scene.tryAdoptId(e, chosenUuid, chosenTag)) {
                HBE::Core::LogWarn("SceneSerializer::loadFromFile: skipping "
                    "entity '" + chosenTag + "' - UUID " + chosenUuid +
                    " already taken.");
                reg.destroy(e);
                continue;
            }

            if (prefabDef) {
                reg.emplace<PrefabRefComponent>(e, PrefabRefComponent{ prefabName });
            }

            for (auto it = comps.begin(); it != comps.end(); ++it) {
                const std::string& key = it.key();
                auto lookup = byName.find(key);
                if (lookup == byName.end()) {
                    HBE::Core::LogWarn(
                        "SceneSerializer: unknown component '" + key +
                        "' on entity '" + chosenTag + "' - ignored.");
                    continue;
                }

                std::string err;
                if (!lookup->second->load(e, reg, scene, cb, it.value(), &err)) {
                    if (outError) *outError = err;
                    return false;
                }
            }

            for (const auto& s : schemas) {
                if (!s.required) continue;
                if (comps.contains(s.key)) continue;
                if (s.installDefault) {
                    HBE::Core::LogWarn("SceneSerializer: entity '" + chosenTag +
                        "' missing required component '" + s.key +
                        "' - using default.");
                    s.installDefault(e, reg);
                }
            }
        }

        HBE::Core::LogInfo("Scene loaded: " + path);
        return true;
    }

}
