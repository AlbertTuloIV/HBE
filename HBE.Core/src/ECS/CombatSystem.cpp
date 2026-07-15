#include "HBE/ECS/CombatSystem.h"

#include <algorithm>
#include <cmath>

#include "HBE/ECS/Components.h"
#include "HBE/Renderer/Transform2D.h"

namespace HBE::ECS {

	namespace {
		struct AABB {
			float cx = 0.0f;
			float cy = 0.0f;
			float hx = 0.0f;
			float hy = 0.0f;
		};

		bool aabbOverlap(const AABB& a, const AABB& b) {
			return std::fabs(a.cx - b.cx) <= (a.hx + b.hx)
				&& std::fabs(a.cy - b.cy) <= (a.hy + b.hy);
		}

		AABB hurtboxAABB(Registry& reg, Entity e, const Hurtbox& hb) {
			AABB a;
			const auto* tr = reg.tryGet<HBE::Renderer::Transform2D>(e);
			if (!tr) return a;

			if (hb.halfW > 0.0f && hb.halfH > 0.0f) {
				a.cx = tr->posX + hb.offsetX;
				a.cy = tr->posY + hb.offsetY;
				a.hx = hb.halfW;
				a.hy = hb.halfH;
				return a;
			}

			if (reg.has<Collider2D>(e)) {
				const auto& col = reg.get<Collider2D>(e);
				a.cx = tr->posX + col.offsetX;
				a.cy = tr->posY + col.offsetY;
				a.hx = col.halfW;
				a.hy = col.halfH;
			}
			return a;
		}

		AABB hitboxAABB(Registry& reg, Entity hitboxEntity, const Hitbox& hb) {
			AABB a;

			const HBE::Renderer::Transform2D* tr = nullptr;
			if (hb.follows && reg.valid(hb.owner)) {
				tr = reg.tryGet<HBE::Renderer::Transform2D>(hb.owner);
			}
			if (!tr) {
				tr = reg.tryGet<HBE::Renderer::Transform2D>(hitboxEntity);
			}
			if (!tr) return a;

			float facing = 1.0f;
			if (tr->scaleX < 0.0f) facing = -1.0f;

			a.cx = tr->posX + hb.offsetX * facing;
			a.cy = tr->posY + hb.offsetY;
			a.hx = hb.halfW;
			a.hy = hb.halfH;
			return a;
		}
	}

	void updateCombat(Registry& reg, float dt) {
		for (auto e : reg.view<Health>()) {
			auto& h = reg.get<Health>(e);
			if (h.invulnTimer > 0.0f) h.invulnTimer = std::max(0.0f, h.invulnTimer - dt);
			if (h.dead && h.deathTimer > 0.0f) h.deathTimer = std::max(0.0f, h.deathTimer - dt);
		}

		std::vector<Entity> toDestroy;
		toDestroy.reserve(8);

		for (auto e : reg.view<Hitbox>()) {
			auto& hb = reg.get<Hitbox>(e);
			hb.lifetime -= dt;
			if (hb.lifetime <= 0.0f) {
				toDestroy.push_back(e);
			}
		}

		struct VictimEntry {
			Entity entity;
			AABB aabb;
			Faction faction;
			bool hasHealth;
			bool hasFaction;
		};
		std::vector<VictimEntry> victims;
		victims.reserve(32);

		for (auto e : reg.view<Hurtbox>()) {
			const auto& hb = reg.get<Hurtbox>(e);
			if (!hb.active) continue;

			VictimEntry v;
			v.entity = e;
			v.aabb = hurtboxAABB(reg, e, hb);
			if (v.aabb.hx <= 0.0f || v.aabb.hy <= 0.0f) continue;

			v.hasHealth = reg.has<Health>(e);
			v.hasFaction = reg.has<FactionComponent>(e);
			v.faction = v.hasFaction
				? reg.get<FactionComponent>(e).team
				: Faction::Neutral;
			victims.push_back(v);
		}

		for (auto he : reg.view<Hitbox>()) {
			auto& hit = reg.get<Hitbox>(he);
			if (!hit.active) continue;

			const AABB hitAABB = hitboxAABB(reg, he, hit);
			if (hitAABB.hx <= 0.0f || hitAABB.hy <= 0.0f) continue;

			for (const auto& v : victims) {
				if (v.entity == hit.owner) continue;
				if (std::find(hit.alreadyHit.begin(),
					hit.alreadyHit.end(),
					v.entity) != hit.alreadyHit.end()) {
					continue;
				}

				const std::size_t idx = static_cast<std::size_t>(v.faction);
				if (idx >= hit.canHit.size() || !hit.canHit[idx]) continue;

				if (!hit.friendlyFire && hit.attackerFaction != Faction::Neutral && hit.attackerFaction == v.faction) {
					continue;
				}

				if (v.hasHealth) {
					const auto& h = reg.get<Health>(v.entity);
					if (h.dead) continue;
					if (h.invulnTimer > 0.0f) continue;
				}

				if (!aabbOverlap(hitAABB, v.aabb)) continue;

				if (v.hasHealth) {
					auto& h = reg.get<Health>(v.entity);
					h.hp = std::max(0, h.hp - hit.damage);
					h.invulnTimer = hit.invulnAfterHit;
					if (h.hp <= 0 && !h.dead) {
						h.dead = true;
					}
				}

				float kx = hit.knockbackX;
				float ky = hit.knockbackY;

				if (hit.knockbackHorizontalUsesFacing) {
					const float ax =
						reg.has<HBE::Renderer::Transform2D>(hit.owner)
						? reg.get<HBE::Renderer::Transform2D>(hit.owner).posX
						: hitAABB.cx;
					kx = (v.aabb.cx >= ax) ? +std::fabs(kx) : -std::fabs(kx);
				}

				if (kx != 0.0f || ky != 0.0f){
					if (!reg.has<Knockback>(v.entity)) {
						reg.emplace<Knockback>(v.entity, Knockback{});
					}
					auto& kb = reg.get<Knockback>(v.entity);
					kb.velX += kx;
					kb.velY += ky;
				}
				hit.alreadyHit.push_back(v.entity);
			}
		}
		for (auto e : toDestroy) {
			reg.destroy(e);
		}
	}
}