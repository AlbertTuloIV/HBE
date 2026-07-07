#include "HBE/Renderer/ParticleSystem.h"
#include "HBE/Renderer/Scene2D.h"
#include "HBE/Renderer/Transform2D.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Renderer2D.h"

#include <algorithm>

namespace HBE::Renderer {
	// -- helpers
	ParticleSystem::Effect* ParticleSystem::findEffect(ParticleHandle h) {
		for (auto& e : m_effects) {
			if (e.handle == h) return &e;
		}
		return nullptr;
	}
	const ParticleSystem::Effect* ParticleSystem::findEffect(ParticleHandle h) const {
		for (const auto& e : m_effects) {
			if (e.handle == h) return &e;
		}
		return nullptr;
	}

	// -- initialize / shutdown
	bool ParticleSystem::initialize(ResourceCache& cache, Mesh* quadMesh) {
		m_initialized = m_debugDraw.initialize(cache, quadMesh);
		return m_initialized;
	}

	void ParticleSystem::shutdown() {
		clear();
		m_initialized = false;
	}

	// -- registration
	void ParticleSystem::registerEffect(const std::string& name, const EffectDef& def) {
		m_library[name] = def;
	}
	void ParticleSystem::registerEffect(const std::string& name, const EmitterConfig& single) {
		m_library[name] = { single };
	}

	// --internal spawn
	ParticleHandle ParticleSystem::spawnInternal(const std::string& name, float x, float y,
		bool managed, bool attached, Scene2D* scene, EntityID entity, float offsetX, float offsetY) {
		auto it = m_library.find(name);
		if (it == m_library.end()) return kInvalidParticle;

		Effect effect;
		effect.managed = managed;
		effect.attached = attached;
		effect.scene = scene;
		effect.entity = entity;
		effect.offsetX = offsetX;
		effect.offsetY = offsetY;
		effect.handle = managed ? m_nextHandle++ : kInvalidParticle;

		for (const auto& cfg : it->second) {
			ParticleEmitter em;
			em.config = cfg;
			em.worldX = x;
			em.worldY = y;
			em.init();
			effect.emitters.push_back(std::move(em));
		}

		m_effects.push_back(std::move(effect));
		return m_effects.back().handle;
	}

	// -- public spawn
	void ParticleSystem::spawn(const std::string& name, float x, float y) {
		spawnInternal(name, x, y, false, false, nullptr, InvalidEntityID, 0.f, 0.f);
	}

	ParticleHandle ParticleSystem::spawnManaged(const std::string& name, float x, float y) {
		return spawnInternal(name, x, y, true, false, nullptr, InvalidEntityID, 0.f, 0.f);
	}

	ParticleHandle ParticleSystem::spawnAttached(const std::string& name, Scene2D* scene, EntityID entity, float offsetX, float offsetY) {
		float sx = 0.f, sy = 0.f;
		if (scene) {
			if (auto* xform = scene->getTransform(entity)) {
				sx = xform->posX;
				sy = xform->posY;
			}
		}
		return spawnInternal(name, sx + offsetX, sy + offsetY, true, true, scene, entity, offsetX, offsetY);
	}

	// -- control
	void ParticleSystem::stop(ParticleHandle h) {
		if (auto* e = findEffect(h)) {
			for (auto& em : e->emitters) em.stop();
		}
	}

	void ParticleSystem::kill(ParticleHandle h) {
		m_effects.erase(
			std::remove_if(m_effects.begin(), m_effects.end(), [h](const Effect& e) { return e.handle == h; }),
			m_effects.end());
	}

	void ParticleSystem::setPosition(ParticleHandle h, float x, float y) {
		if (auto* e = findEffect(h)) {
			for (auto& em : e->emitters) {
				em.worldX = x + e->offsetX;
				em.worldY = y + e->offsetY;
			}
		}
	}

	bool ParticleSystem::isAlive(ParticleHandle h) const {
		return findEffect(h) != nullptr;
	}

	// -- update
	void ParticleSystem::update(float dt) {
		for (auto& effect : m_effects) {
			if (effect.attached && effect.scene) {
				if (auto* xform = effect.scene->getTransform(effect.entity)) {
					for (auto& em : effect.emitters) {
						em.worldX = xform->posX + effect.offsetX;
						em.worldY = xform->posY + effect.offsetY;
					}
				}
			}
			for (auto& em : effect.emitters) em.update(dt);
		}

		m_effects.erase(std::remove_if(m_effects.begin(), m_effects.end(), [](const Effect& e) {
			for (const auto& em : e.emitters) {
				if (em.alive) return false;
			}
			return true;
			}),
		m_effects.end());
	}

	// -- render
	void ParticleSystem::render(Renderer2D& r2d) const {
		render(r2d, [&](float x, float y, float size, float /*rotation*/,
			float r, float g, float b, float a, bool /*additive*/)
			{
				m_debugDraw.rect(r2d, x, y, size, size, r, g, b, a, true);
			});
	}

	void ParticleSystem::render(Renderer2D& r2d, const ParticleRenderFn& fn) const {
		for (const auto& effect : m_effects)
			for (const auto& em : effect.emitters)
				em.render(fn);
	}

	// -- utility
	void ParticleSystem::clear() {
		m_effects.clear();
	}

	int ParticleSystem::totalLiveParticles() const {
		int n = 0;
		for (const auto& e : m_effects)
			for (const auto& em : e.emitters)
				n += em.liveCount();
		return n;
	}

	int ParticleSystem::activeEffectCount() const {
		return static_cast<int>(m_effects.size());
	}
}