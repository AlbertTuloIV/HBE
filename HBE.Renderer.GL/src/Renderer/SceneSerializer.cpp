#include "HBE/Renderer/SceneSerializer.h"

#include "HBE/Core/Log.h"
#include "HBE/Core/UUID.h"
#include "HBE/Core/AssetPaths.h"

#include "HBE/ECS/RuntimeComponents.h"
#include "HBE/ECS/Components.h"

#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/Transform2D.h"
#include "HBE/ECS/ESCSComponents2D.h"

#include <fstream>
#include <sstream>
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

    static json toJsonSprite(const SpriteComponent2D& s,
        const SceneSaveCallbacks& cb)
    {
        json j;
        j["layer"] = s.layer;
        j["sortKey"] = s.sortKey;
        j["sortOffsetY"] = s.sortOffsetY;

        j["uvRect"] = { s.uvRect[0], s.uvRect[1], s.uvRect[2], s.uvRect[3] };

        if (cb.meshKey) j["mesh"] = cb.meshKey(s.mesh);
        if (cb.materialKey) j["material"] = cb.materialKey(s.material);

        return j;
    }

    static bool fromJsonSprite(const json& j, SpriteComponent2D& s,
        const SceneLoadCallbacks& cb,
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
        s.sortKey = j.value("sortKey", 0.0f);
        s.sortOffsetY = j.value("sortOffsetY", 0.0f);

        if (j.contains("uvRect") && j["uvRect"].is_array() && j["uvRect"].size() == 4) {
            s.uvRect[0] = j["uvRect"][0].get<float>();
            s.uvRect[1] = j["uvRect"][1].get<float>();
            s.uvRect[2] = j["uvRect"][2].get<float>();
            s.uvRect[3] = j["uvRect"][3].get<float>();
        }
        else {
            // default visible
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
            {"velX", r.velX}, {"velY", r.velY},
            {"accelX", r.accelX}, {"accelY", r.accelY},
            {"linearDamping", r.linearDamping},
            {"isStatic", r.isStatic},
            {"useGravity", r.useGravity},
            {"gravityScale", r.gravityScale},
            {"grounded", r.grounded},
            {"maxStepUp", r.maxStepUp},
            {"enableOneWay", r.enableOneWay},
            {"oneWayDisableTimer", r.oneWayDisableTimer},
            {"enableSlopes", r.enableSlopes},
            {"maxFallSpeed", r.maxFallSpeed}
        };
    }

    static void fromJsonRigidBody(const json& j, HBE::ECS::RigidBody2D& r) {
        r.velX = j.value("velX", 0.0f);
        r.velY = j.value("velY", 0.0f);
        r.accelX = j.value("accelX", 0.0f);
        r.accelY = j.value("accelY", 0.0f);
        r.linearDamping = j.value("linearDamping", 0.0f);
        r.isStatic = j.value("isStatic", false);

        r.useGravity = j.value("useGravity", false);
        r.gravityScale = j.value("gravityScale", 1.0f);
        r.grounded = j.value("grounded", false);

        r.maxStepUp = j.value("maxStepUp", 0.0f);

        r.enableOneWay = j.value("enableOneWay", true);
        r.oneWayDisableTimer = j.value("oneWayDisableTimer", 0.0f);

        r.enableSlopes = j.value("enableSlopes", true);
        r.maxFallSpeed = j.value("maxFallSpeed", 0.0f);
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
        root["version"] = 1;
        // Store the tilemap reference as a LOGICAL path so scene files remain
        // portable across machines and don't embed developer-specific
        // absolute paths. GameLayer already hands us a logical path since
        // step 3 of item 13; strip is a no-op on a clean path and defensive
        // against accidental absolute inputs.
        // TODO(item 13 cleanup): once no scenes on disk carry an "assets/"
        // prefix, StripLegacyAssetsPrefix can be removed in item 14.
        root["tilemap"] = HBE::Core::AssetPaths::StripLegacyAssetsPrefix(tilemapPath);

        json ents = json::array();

        // Driver: entities that have Transform2D
        for (auto e : reg.view<Transform2D>()) {

            // Ensure stable ID exists
            if (!reg.has<HBE::ECS::IDComponent>(e)) {
                HBE::ECS::IDComponent id{};
                id.uuid = HBE::Core::NewUUID32();
                reg.emplace<HBE::ECS::IDComponent>(e, id);
            }

            const auto& id = reg.get<HBE::ECS::IDComponent>(e);

            json ej;
            ej["uuid"] = id.uuid;

            // Tag
            if (reg.has<HBE::ECS::TagComponent>(e)) {
                ej["name"] = reg.get<HBE::ECS::TagComponent>(e).tag;
            }
            else {
                ej["name"] = std::string("Entity_") + id.uuid.substr(0, 6);
            }

            json comps;

            // Transform
            comps["Transform2D"] = toJsonTransform(reg.get<Transform2D>(e));

            // Sprite
            if (reg.has<SpriteComponent2D>(e)) {
                comps["Sprite2D"] = toJsonSprite(reg.get<SpriteComponent2D>(e), cb);
            }

            // Collider
            if (reg.has<HBE::ECS::Collider2D>(e)) {
                comps["Collider2D"] = toJsonCollider(reg.get<HBE::ECS::Collider2D>(e));
            }

            // Rigidbody
            if (reg.has<HBE::ECS::RigidBody2D>(e)) {
                comps["RigidBody2D"] = toJsonRigidBody(reg.get<HBE::ECS::RigidBody2D>(e));
            }

            // Script name only
            if (reg.has<HBE::ECS::Script>(e)) {
                const auto& sc = reg.get<HBE::ECS::Script>(e);
                comps["Script"] = json{ {"name", sc.name} };
            }

            // Animator preset (we cannot serialize the whole state machine safely yet)
            if (reg.has<AnimationComponent2D>(e)) {
                json aj;
                aj["preset"] = "UNSPECIFIED";
                aj["sheet"] = "";
                aj["state"] = "Idle";

                // If caller can reverse-map sheet pointer -> key
                const auto& anim = reg.get<AnimationComponent2D>(e);
                if (cb.sheetKey && anim.sm.sheet) {
                    aj["sheet"] = cb.sheetKey(anim.sm.sheet);
                }

                // If you store preset name somewhere later, swap this out.
                comps["Animator"] = aj;
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

        const int version = root.value("version", 1);
        (void)version;

        if (outTilemapPath) {
            // Normalize to a LOGICAL path (no leading "assets/", no absolute
            // prefix). GameLayer stores m_tileMapPath as a logical path and
            // re-resolves at every use site; storing an absolute path here
            // would break the moment the game is moved to a different asset
            // root.
            // TODO(item 13 cleanup): drop StripLegacyAssetsPrefix in item 14
            // once no scenes on disk carry an "assets/" prefix.
            const std::string embedded = root.value("tilemap", "");
            *outTilemapPath = embedded.empty()
                ? std::string{}
                : HBE::Core::AssetPaths::StripLegacyAssetsPrefix(embedded);
        }

        if (!root.contains("entities") || !root["entities"].is_array()) {
            if (outError) *outError = "SceneSerializer: missing entities array.";
            return false;
        }

        // Clear current scene
        scene.clear();
        auto& reg = scene.registry();

        // Create entities
        for (auto& ej : root["entities"]) {
            const std::string uuid = ej.value("uuid", "");
            const std::string name = ej.value("name", "");

            const json comps = ej.value("components", json::object());

            HBE::ECS::Entity e = reg.create();

            // ID + Tag
            {
                HBE::ECS::IDComponent id{};
                id.uuid = uuid.empty() ? HBE::Core::NewUUID32() : uuid;
                reg.emplace<HBE::ECS::IDComponent>(e, id);

                HBE::ECS::TagComponent tag{};
                tag.tag = name.empty() ? std::string("Entity_") + id.uuid.substr(0, 6) : name;
                reg.emplace<HBE::ECS::TagComponent>(e, tag);
            }

            // Transform
            if (comps.contains("Transform2D")) {
                Transform2D t{};
                fromJsonTransform(comps["Transform2D"], t);
                reg.emplace<Transform2D>(e, t);
            }
            else {
                // every entity needs Transform2D
                reg.emplace<Transform2D>(e, Transform2D{});
            }

            // Sprite
            if (comps.contains("Sprite2D")) {
                SpriteComponent2D s{};
                std::string err;
                if (!fromJsonSprite(comps["Sprite2D"], s, cb, &err)) {
                    if (outError) *outError = err;
                    return false;
                }
                reg.emplace<SpriteComponent2D>(e, s);
            }

            // Collider
            if (comps.contains("Collider2D")) {
                HBE::ECS::Collider2D c{};
                fromJsonCollider(comps["Collider2D"], c);
                reg.emplace<HBE::ECS::Collider2D>(e, c);
            }

            // Rigidbody
            if (comps.contains("RigidBody2D")) {
                HBE::ECS::RigidBody2D r{};
                fromJsonRigidBody(comps["RigidBody2D"], r);
                reg.emplace<HBE::ECS::RigidBody2D>(e, r);
            }

            // Script (name -> bindScript callback sets lambdas)
            if (comps.contains("Script")) {
                const std::string scriptName = comps["Script"].value("name", "");
                if (!scriptName.empty()) {
                    HBE::ECS::Script sc{};
                    sc.name = scriptName;
                    reg.emplace<HBE::ECS::Script>(e, sc);

                    if (cb.bindScript) {
                        cb.bindScript(e, scriptName, scene);
                    }
                }
            }

            // Animator preset (preset + sheet key)
            if (comps.contains("Animator")) {
                const std::string preset = comps["Animator"].value("preset", "");
                const std::string sheetKey = comps["Animator"].value("sheet", "");
                const std::string state = comps["Animator"].value("state", "Idle");

                if (cb.sheet && cb.buildAnimatorPreset && !sheetKey.empty() && !preset.empty()) {
                    const auto* sheet = cb.sheet(sheetKey);
                    if (sheet) {
                        auto* sm = scene.addSpriteAnimator(e, sheet);
                        if (sm) {
                            cb.buildAnimatorPreset(e, preset, *sm, scene);
                            sm->setState(state, true);
                        }
                    }
                }
            }
        }

        HBE::Core::LogInfo("Scene loaded: " + path);
        return true;
    }

}