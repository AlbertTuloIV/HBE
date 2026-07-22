#include "Game/EnemyManager.h"

#include "Game/Bullet.h"
#include "Game/Effects.h"

#include "HBE/Renderer/DebugDraw2D.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Core/Log.h"

#include <algorithm>
#include <cmath>

using namespace HBE::Renderer;

namespace MegaX {

	bool EnemyManager::init(ResourceCache& resources, Mesh* quadMesh, GLShader* spriteShader) {
		if (!quadMesh || !spriteShader) {
			HBE::Core::LogError("EnemyManager::init: quadMesh or spriteShader is null.");
			return false;
		}
		m_resources = &resources;
		m_quadMesh = quadMesh;
		m_spriteShader = spriteShader;
		return true;
	}

	Enemy* EnemyManager::spawn(float x, float groundY, int facing) {
		if (!m_resources || !m_quadMesh || !m_spriteShader) {
			HBE::Core::LogError("EnemyManager::spawn called before init.");
			return nullptr;
		}
		m_enemies.emplace_back();
		Enemy& e = m_enemies.back();
		if (!e.init(*m_resources, m_quadMesh, m_spriteShader)) {
			m_enemies.pop_back();
			return nullptr;
		}
		e.spawn(x, groundY, facing);
		return &e;
	}

	int EnemyManager::checkBulletHits(BulletManager& bullets, Effects* effects, int damagePerBullet) {
		if (m_enemies.empty()) return 0;

		int hits = 0;
		auto& list = bullets.bullets();

		for (auto& b : list) {
			if (!b.alive) continue;

			for (auto& e : m_enemies) {
				if (!e.hurtboxActive()) continue;

				const AABB h = e.hurtbox();
				const float dx = std::fabs(b.x - h.cx);
				const float dy = std::fabs(b.y - h.cy);
				if (dx > h.w * 0.5f || dy > h.h * 0.5f) continue;

				if (e.takeDamage(damagePerBullet)) {
					b.alive = false;
					++hits;
					if (effects) {
						effects->spawnBulletImpact(b.x, b.y, 0);
					}
					break;
				}
			}
		}
		return hits;
	}

	void EnemyManager::update(float dt) {
		for (auto& e : m_enemies) e.update(dt);

		m_enemies.erase(
			std::remove_if(m_enemies.begin(), m_enemies.end(),
				[](const Enemy& e) {return e.isFinished(); }),
			m_enemies.end());
	}

	void EnemyManager::render(Renderer2D& r2d) {
		for (auto& e : m_enemies) e.render(r2d);
	}

	void EnemyManager::debugDrawBoxes(DebugDraw2D& dbg, Renderer2D& r2d) const {
		for (const auto& e : m_enemies) {
			if (e.isFinished()) continue;
			const AABB h = e.hurtbox();
			if (e.isDying()) {
				dbg.rect(r2d, h.cx, h.cy, h.w, h.h, 0.7f, 0.7f, 0.7f, 1.0f, false);
			}
			else {
				dbg.rect(r2d, h.cx, h.cy, h.w, h.h, 0.25f, 1.0f, 0.35f, 1.0f, false);
			}

			if (e.hitboxActive()) {
				const AABB k = e.hitbox();
				dbg.rect(r2d, k.cx, k.cy, k.w, k.h, 1.09f, 0.35f, 0.35f, 1.0f, false);
			}
		}
	}

	int EnemyManager::aliveCount() const {
		int n = 0;
		for (const auto& e : m_enemies) if (e.isAlive()) ++n;
		return n;
	}
}