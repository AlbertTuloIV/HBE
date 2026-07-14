#include "HBE/Core/Log.h"
#include "HBE/Core/UUID.h"
#include "HBE/ECS/RuntimeComponents.h"

#include "HBE/Renderer/Scene2D.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/Mesh.h"

#include "HBE/ECS/Components.h"
#include "HBE/ECS/RuntimeComponents.h"

#include "HBE/Renderer/TileMap.h"
#include "HBE/Renderer/TileCollision.h"

#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

namespace HBE::Renderer {

    using HBE::Core::LogError;

    EntityID Scene2D::createEntity() {
        auto e = m_reg.create();

        HBE::ECS::IDComponent id{};
        id.uuid = HBE::Core::NewUUID32();
        m_reg.emplace<HBE::ECS::IDComponent>(e, id);
        m_idToEntity.emplace(id.uuid, e);

        HBE::ECS::TagComponent tag{};
        tag.tag = std::string("Entity_") + id.uuid.substr(0, 6);
        m_reg.emplace<HBE::ECS::TagComponent>(e, tag);

        return e;
    }

    EntityID Scene2D::createEntity(const RenderItem& templateItem) {
        EntityID e = createEntity();

        m_reg.emplace<Transform2D>(e, templateItem.transform);

        SpriteComponent2D sprite;
        sprite.mesh = templateItem.mesh;
        sprite.material = templateItem.material;
        sprite.layer = templateItem.layer;
        sprite.sortKey = templateItem.sortKey;

        const float u0 = templateItem.uvRect[0];
        const float v0 = templateItem.uvRect[1];
        const float u1 = templateItem.uvRect[2];
        const float v1 = templateItem.uvRect[3];

        const bool allZero = (u0 == 0.0f && v0 == 0.0f && u1 == 0.0f && v1 == 0.0f);

        if (allZero) {
            sprite.uvRect[0] = 0.0f; sprite.uvRect[1] = 0.0f;
            sprite.uvRect[2] = 1.0f; sprite.uvRect[3] = 1.0f;
        }
        else {
            sprite.uvRect[0] = u0; sprite.uvRect[1] = v0;
            sprite.uvRect[2] = u1; sprite.uvRect[3] = v1;
        }

        m_reg.emplace<SpriteComponent2D>(e, sprite);

        return e;
    }

    bool Scene2D::tryAdoptId(EntityID e, const std::string& uuid, const std::string& tag) {
        if (auto it = m_idToEntity.find(uuid); it != m_idToEntity.end() && it->second != e) {
            HBE::Core::LogWarn("Scene2D: duplicate UUID '" + uuid + "' - new entity will be skipped by caller.");
            return false;
        }

        if (m_reg.has<HBE::ECS::IDComponent>(e)) {
            const auto& old = m_reg.get<HBE::ECS::IDComponent>(e);
            m_idToEntity.erase(old.uuid);
        }

        HBE::ECS::IDComponent id{};
        id.uuid = uuid;
        m_reg.emplace<HBE::ECS::IDComponent>(e, id);
        m_idToEntity[uuid] = e;

        HBE::ECS::TagComponent t{};
        t.tag = tag;
        m_reg.emplace<HBE::ECS::TagComponent>(e, t);

        return true;
    }

    Transform2D* Scene2D::getTransform(EntityID id) {
        if (!m_reg.valid(id)) return nullptr;
        if (!m_reg.has<Transform2D>(id)) return nullptr;
        return &m_reg.get<Transform2D>(id);
    }

    SpriteAnimationStateMachine* Scene2D::addSpriteAnimator(EntityID id, const SpriteRenderer2D::SpriteSheetHandle* sheet) {
        if (!m_reg.valid(id)) return nullptr;

        auto& anim = m_reg.emplace<AnimationComponent2D>(id);
        anim.sm.sheet = sheet;
        
        return &anim.sm;
    }

    SpriteAnimationStateMachine* Scene2D::getSpriteAnimator(EntityID id) {
        if (!m_reg.valid(id)) return nullptr;
        if (!m_reg.has<AnimationComponent2D>(id)) return nullptr;
        return &m_reg.get<AnimationComponent2D>(id).sm;
    }

    void Scene2D::setTileCollisionContext(const TileMap* map, const TileMapLayer* collisionLayer) {
        m_tileMap = map;
        m_collisionLayer = collisionLayer;
    }

    void Scene2D::removeEntity(EntityID id) {
        m_reg.destroy(id);
    }

    static inline float applyDamping(float v, float damping, float dt) {
        if (damping <= 0.0f) return v;
        const float k = 1.0f / (1.0f + damping * dt);
        return v * k;
    }

    void Scene2D::update(float dt, const SpriteAnimationStateMachine::EventCallback& onAnimEvent) {
        for (auto e : m_reg.view<HBE::ECS::Script>()) {
            auto& sc = m_reg.get<HBE::ECS::Script>(e);

            // One-time create
            if (!m_reg.has<HBE::ECS::ScriptRuntimeState>(e)) {
                auto& rt = m_reg.emplace<HBE::ECS::ScriptRuntimeState>(e);
                rt.created = false;
            }

            auto& rt = m_reg.get<HBE::ECS::ScriptRuntimeState>(e);
            if (!rt.created) {
                rt.created = true;
                if (sc.onCreate) sc.onCreate(e);
            }

            if (sc.onUpdate) sc.onUpdate(e, dt);
        }

        const bool canTileCollide = (m_tileMap != nullptr && m_collisionLayer != nullptr);

        for (auto e : m_reg.view<HBE::ECS::RigidBody2D>()) {
            auto& rb = m_reg.get<HBE::ECS::RigidBody2D>(e);
            rb.grounded = false;

            if (rb.oneWayDisableTimer > 0.0f) {
                rb.oneWayDisableTimer = std::max(0.0f, rb.oneWayDisableTimer - dt);
            }
        }

        int steps = 1;
        float stepDt = dt;

        if (m_physics.maxSubSteps > 0 && m_physics.maxStepDt > 0.0f) {
            const int desired = (int)std::ceil(dt / m_physics.maxStepDt);
            steps = std::clamp(desired, 1, m_physics.maxSubSteps);
            stepDt = (steps > 0) ? (dt / (float)steps) : dt;
        }

        for (int step = 0; step < steps; ++step) {
            for (auto e : m_reg.view<Transform2D, HBE::ECS::RigidBody2D>()) {
                auto& tr = m_reg.get<Transform2D>(e);
                auto& rb = m_reg.get<HBE::ECS::RigidBody2D>(e);

                if (rb.isStatic) continue;

                rb.velX += rb.accelX * stepDt;
                rb.velY += rb.accelY * stepDt;

                if (rb.useGravity) {
                    rb.velY += m_physics.gravityY * rb.gravityScale * stepDt;
                }

                if (rb.maxFallSpeed != 0.0f) {
                    rb.velY = std::max(rb.velY, rb.maxFallSpeed);
                }

                rb.velX = applyDamping(rb.velX, rb.linearDamping, stepDt);
                rb.velY = applyDamping(rb.velY, rb.linearDamping, stepDt);

                const bool hasCollider = m_reg.has<HBE::ECS::Collider2D>(e);

                if (canTileCollide && hasCollider) {
                    auto& col = m_reg.get<HBE::ECS::Collider2D>(e);

                    HBE::Renderer::AABB box;
                    box.w = col.halfW * 2.0f;
                    box.h = col.halfH * 2.0f;
                    box.cx = tr.posX + col.offsetX;
                    box.cy = tr.posY + col.offsetY;

                    const float prevBottom = box.cy - box.h * 0.5f;

                    const bool allowOneWay = rb.enableOneWay && (rb.oneWayDisableTimer <= 0.0f);

                    HBE::Renderer::MoveResult2D res =
                        HBE::Renderer::TileCollision::moveAndCollideEx(
                            *m_tileMap, *m_collisionLayer,
                            box, rb.velX, rb.velY, stepDt,
                            rb.maxStepUp,
                            allowOneWay,
                            rb.enableSlopes,
                            prevBottom
                        );

                    if (res.grounded) rb.grounded = true;

                    tr.posX = box.cx - col.offsetX;
                    tr.posY = box.cy - col.offsetY;
                }
                else {
                    tr.posX += rb.velX * stepDt;
                    tr.posY += rb.velY * stepDt;
                }
            }
        }
                
        struct WorldAABB {
            float cx, cy;
            float hx, hy;
        };

        auto makeAABB = [&](HBE::ECS::Entity e) -> WorldAABB {
            const auto& tr = m_reg.get<Transform2D>(e);
            const auto& col = m_reg.get<HBE::ECS::Collider2D>(e);

            WorldAABB a;
            a.cx = tr.posX + col.offsetX;
            a.cy = tr.posY + col.offsetY;
            a.hx = col.halfW;
            a.hy = col.halfH;
            return a;
            };

        auto overlap = [&](const WorldAABB& a, const WorldAABB& b, float& outDx, float& outDy) -> bool {
            const float dx = b.cx - a.cx;
            const float px = (a.hx + b.hx) - std::fabs(dx);
            if (px <= 0.0f) return false;

            const float dy = b.cy - a.cy;
            const float py = (a.hy + b.hy) - std::fabs(dy);
            if (py <= 0.0f) return false;

            if (px < py) {
                outDx = (dx < 0.0f) ? +px : -px;
                outDy = 0.0f;
            }
            else {
                outDx = 0.0f;
                outDy = (dy < 0.0f) ? +py : -py;
            }
            return true;
            };

        std::vector<HBE::ECS::Entity> statics;
        statics.reserve(128);

        for (auto e : m_reg.view<Transform2D, HBE::ECS::Collider2D>()) {
            bool isStatic = true;

            if (m_reg.has<HBE::ECS::RigidBody2D>(e)) {
                isStatic = m_reg.get<HBE::ECS::RigidBody2D>(e).isStatic;
            }

            if (isStatic) {
                statics.push_back(e);
            }
        }

        for (auto e : m_reg.view<Transform2D, HBE::ECS::RigidBody2D, HBE::ECS::Collider2D>()) {
            auto& tr = m_reg.get<Transform2D>(e);
            auto& rb = m_reg.get<HBE::ECS::RigidBody2D>(e);

            if (rb.isStatic) continue;

            for (int pass = 0; pass < 2; ++pass) {
                bool anyResolved = false;

                for (auto s : statics) {
                    if (s == e) continue;

                    float pushX = 0.0f, pushY = 0.0f;

                    WorldAABB a = makeAABB(e);
                    WorldAABB b = makeAABB(s);

                    if (!overlap(a, b, pushX, pushY)) continue;

                    tr.posX += pushX;
                    tr.posY += pushY;

                    if (pushX != 0.0f) rb.velX = 0.0f;
                    if (pushY != 0.0f) rb.velY = 0.0f;

                    anyResolved = true;
                }

                if (!anyResolved) break;
            }
        }

        for (auto e : m_reg.view<AnimationComponent2D, SpriteComponent2D>()) {
            auto& anim = m_reg.get<AnimationComponent2D>(e).sm;
            auto& spr = m_reg.get<SpriteComponent2D>(e);

            anim.update(dt, onAnimEvent);

            RenderItem tmp;
            tmp.mesh = spr.mesh;
            tmp.material = spr.material;
            tmp.layer = spr.layer;
            tmp.sortKey = spr.sortKey;
            std::memcpy(tmp.uvRect, spr.uvRect, sizeof(tmp.uvRect));

            anim.apply(tmp);

            std::memcpy(spr.uvRect, tmp.uvRect, sizeof(tmp.uvRect));
        }
    }

    void Scene2D::render(Renderer2D& renderer) {
        const Camera2D* cam = renderer.activeCamera();

        float viewL = -1e9f, viewR = 1e9f, viewB = -1e9f, viewT = 1e9f;
        bool canCull = false;

        if (cam) {
            const float zoom = (cam->zoom > 0.0001f) ? cam->zoom : 0.0001f;
            const float halfW = 0.5f * cam->viewportWidth / zoom;
            const float halfH = 0.5f * cam->viewportHeight / zoom;

            viewL = cam->x - halfW;
            viewR = cam->x + halfW;
            viewB = cam->y - halfH;
            viewT = cam->y + halfH;

            canCull = m_cullingEnabled;
        }

        for (auto e : m_reg.view<Transform2D, SpriteComponent2D>()) {
            auto& tr = m_reg.get<Transform2D>(e);
            auto& spr = m_reg.get<SpriteComponent2D>(e);

            if (!spr.visible) continue;

            if (canCull) {
                const float pad = 0.25f * std::max(std::fabs(tr.scaleX), std::fabs(tr.scaleY));

                const float minX = tr.posX - 0.5f * std::fabs(tr.scaleX) - pad;
                const float maxX = tr.posX + 0.5f * std::fabs(tr.scaleX) + pad;
                const float minY = tr.posY - 0.5f * std::fabs(tr.scaleY) - pad;
                const float maxY = tr.posY + 0.5f * std::fabs(tr.scaleY) + pad;

                if (maxX < viewL || minX > viewR || maxY < viewB || minY > viewT)
                    continue;
            }

            RenderItem item;
            item.transform = tr;
            item.mesh = spr.mesh;
            item.material = spr.material;
            item.layer = spr.layer;
            item.sortKey = spr.sortKey;
            std::memcpy(item.uvRect, spr.uvRect, sizeof(item.uvRect));

            renderer.draw(item);
        }
    }

    void Scene2D::clear() {
        m_reg = HBE::ECS::Registry{};
        m_tileMap = nullptr;
        m_collisionLayer = nullptr;
        m_idToEntity.clear();
    }

} // namespace HBE::Renderer