#pragma once
#include "HBE/Renderer/EmitterConfig.h"
#include "HBE/Renderer/ParticleEmitter.h"
#include "HBE/Renderer/DebugDraw2D.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/Scene2D.h"   // provides EntityID, InvalidEntityID, Scene2D

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// forward declarations for types not pulled in by Scene2D.h
namespace HBE::Renderer {
	class Renderer2D;
	class ResourceCache;
	class Mesh;
	class GLShader;
	class Texture2D;
}

namespace HBE::Renderer {

	using ParticleHandle = std::uint32_t;
	inline constexpr ParticleHandle kInvalidParticle = 0u;

	// EntityID and InvalidEntityID come from Scene2D.h

    // ─────────────────────────────────────────────────────────────────────────
	// ParticleSystem
	//
	// Lives in your game layer alongside Scene2D.
	// Typical usage (in GameLayer):
	//
	//   onAttach:
	//       m_particles.initialize(app.resources(), m_quadMesh);
	//       m_particles.registerEffect("hit_sparks", makeHitSparks());
	//       m_particles.registerEffect("rain", makeRain());
	//
	//   onUpdate(dt):
	//       m_particles.update(dt);
	//
	//   onRender (inside beginScene/endScene):
	//       m_particles.render(r2d);
	// ─────────────────────────────────────────────────────────────────────────

	class ParticleSystem {
	public:
		ParticleSystem() = default;
		~ParticleSystem() = default;

		ParticleSystem(const ParticleSystem&) = delete;
		ParticleSystem& operator=(const ParticleSystem&) = delete;

		bool initialize(ResourceCache& cache, Mesh* quadMesh);
		void shutdown();

		// -- Effect Registration
		void registerEffect(const std::string& name, const EffectDef& def);
		void registerEffect(const std::string& name, const EmitterConfig& single);

		// -- Spawning 
		void spawn(const std::string& name, float x, float y);

		ParticleHandle spawnManaged(const std::string& name, float x, float y);
		ParticleHandle spawnAttached(const std::string& name,
			Scene2D* scene,
			EntityID entity,
			float offsetX = 0.f,
			float offsetY = 0.f);

		// -- Control
		void stop(ParticleHandle h);
		void kill(ParticleHandle h);
		void setPosition(ParticleHandle h, float x, float y);
		bool isAlive(ParticleHandle h) const;

		// Simulation
		void update(float dt);

		// -- Rendering
		void render(Renderer2D& r2d) const;

		void render(Renderer2D& r2d, const ParticleRenderFn& fn) const;

		// -- Utility
		void clear();
		int totalLiveParticles() const;
		int activeEffectCount() const;

	private:
		struct Effect {
			ParticleHandle handle = kInvalidParticle;
			bool managed = false;
			bool attached = false;
			Scene2D* scene = nullptr;
			EntityID entity = InvalidEntityID;
			float offsetX = 0.f;
			float offsetY = 0.f;
			std::vector<ParticleEmitter> emitters;
		};

		std::unordered_map<std::string, EffectDef> m_library;
		std::vector<Effect> m_effects;
		ParticleHandle m_nextHandle = 1u;

		mutable DebugDraw2D m_debugDraw{};
		bool m_initialized = false;

		// Resources for batched rendering. Populated in initialize() by looking up the
		// sprite shader from ResourceCache and creating a 1x1 white texture.
		Mesh*      m_quadMesh     = nullptr;
		GLShader*  m_spriteShader = nullptr;
		Texture2D* m_whiteTex     = nullptr;
		mutable Material m_matAlpha{};    // sprite shader, white tex, alpha blend
		mutable Material m_matAdditive{}; // sprite shader, white tex, additive blend

		Effect* findEffect(ParticleHandle h);
		const Effect* findEffect(ParticleHandle h) const;

		ParticleHandle spawnInternal(const std::string& name,
			float x, float y,
			bool managed, bool attached,
			Scene2D* scene,
			EntityID entity,
			float offsetX, float offsetY);
	};
}