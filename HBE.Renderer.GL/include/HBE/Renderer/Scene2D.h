#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#include "HBE/ECS/Registry.h"
#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Transform2D.h"
#include "HBE/Renderer/SpriteAnimationStateMachine.h"
#include "HBE/ECS/ESCSComponents2D.h"

namespace HBE::Renderer {

    class Renderer2D;
    class TileMap;
    struct TileMapLayer;

    // simple opaque handle to an entity in the scene
    using EntityID = HBE::ECS::Entity;
    inline constexpr EntityID InvalidEntityID = HBE::ECS::Null;

    struct Physics2DSettings {
        // World-space gravity (negative = down if +Y is up)
        float gravityY = -1800.0f;

        // Sub-stepping helps stability when dt spikes or velocities get high.
        // 0 disables and uses a single step per frame.
        int maxSubSteps = 4;

        // Upper bound per substep (seconds). Smaller = more stable.
        float maxStepDt = 1.0f / 120.0f;
    };

    class Scene2D {
    public:
        Scene2D() = default;

        // Create an entity by copying a template RenderItem
        EntityID createEntity();
        EntityID createEntity(const RenderItem& templateItem);

        // Access
        Transform2D* getTransform(EntityID id);

        // Sprite animation access (optional per entity)
        // If you call this, the entity will be updated automatically by Scene2D::update().
        SpriteAnimationStateMachine* addSpriteAnimator(EntityID id, const SpriteRenderer2D::SpriteSheetHandle* sheet);
        SpriteAnimationStateMachine* getSpriteAnimator(EntityID id);

        // Physics/tile collision context
        // if set, entities with Transform2D + RigidBody2D + Collider2D will collide against the tile layer
        void setTileCollisionContext(const TileMap* map, const TileMapLayer* collisionLayer);

        void setCullingEnabled(bool enabled) { m_cullingEnabled = enabled; }

        // Physics settings
        void setPhysics2DSettings(const Physics2DSettings& s) { m_physics = s; }
        const Physics2DSettings& physics2DSettings() const { return m_physics; }

        // remove
        void removeEntity(EntityID id);

        // update animations (call once per frame in your layer)
        void update(float dt, const SpriteAnimationStateMachine::EventCallback& onAnimEvent = {});

        // render all active entities
        void render(Renderer2D& renderer);

        // Clear all entities/components from the scene
        void clear();

        // Expose registry if you want to go full ECS later (optional but useful)
        HBE::ECS::Registry& registry() { return m_reg; }
        const HBE::ECS::Registry& registry() const { return m_reg; }

        bool cullingEnabled() const { return m_cullingEnabled; }
        bool tryAdoptId(EntityID e, const std::string& uuid, const std::string& tag);

    private:
        HBE::ECS::Registry m_reg;

        Physics2DSettings m_physics{};

        // optional tile collision pointers (not owned)
        const TileMap* m_tileMap = nullptr;
        const TileMapLayer* m_collisionLayer = nullptr;

        bool m_cullingEnabled = true;

        std::unordered_map<std::string, HBE::ECS::Entity> m_idToEntity;
    };

} // namespace HBE::Renderer