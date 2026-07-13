#pragma once

#include <string>
#include <functional>

#include "HBE/Renderer/Scene2D.h"

namespace HBE::Renderer {

    class Mesh;
    class Material;
    class PrefabLibrary;
    class ScriptRegistry;

    struct SceneSaveCallbacks {
        std::function<std::string(const Mesh*)> meshKey;
        std::function<std::string(const Material*)> materialKey;
        std::function<std::string(const SpriteRenderer2D::SpriteSheetHandle*)> sheetKey;

        const PrefabLibrary* prefabs = nullptr;
    };

    struct SceneLoadCallbacks {
        std::function<Mesh* (const std::string&)> mesh;
        std::function<Material* (const std::string&)> material;
        std::function<const SpriteRenderer2D::SpriteSheetHandle* (const std::string&)> sheet;
        std::function<bool(HBE::ECS::Entity e, const std::string& scriptName, Scene2D& scene)> bindScript;
        std::function<void(HBE::ECS::Entity e, const std::string& presetName, SpriteAnimationStateMachine& sm, Scene2D& scene)> buildAnimatorPreset;

        const PrefabLibrary* prefabs = nullptr;
        const ScriptRegistry* scripts = nullptr;
    };

    class SceneSerializer {
    public:
        static constexpr int kSceneVersion = 3;
        static bool saveToFile(Scene2D& scene, const std::string& path, const SceneSaveCallbacks& cb, const std::string& tilemapPath, std::string* outError = nullptr);
        static bool loadFromFile(Scene2D& scene, const std::string& path, const SceneLoadCallbacks& cb, std::string* outTilemapPath = nullptr, std::string* outError = nullptr);
    };

}