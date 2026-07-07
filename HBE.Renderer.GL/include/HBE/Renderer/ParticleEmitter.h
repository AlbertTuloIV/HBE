#pragma once
#include "HBE/Renderer/ParticleTypes.h"
#include "HBE/Renderer/EmitterConfig.h"

#include <functional>
#include <vector>

namespace HBE::Renderer {
	class Renderer2D;

	using ParticleRenderFn = std::function<void(
		float x, float y,
		float size,
		float rotation,
		float r, float g, float b, float a,
		bool additive
		)>;

	class ParticleEmitter {
	public:
		EmitterConfig config;

		float worldX = 0.f; // current world pos (set by ParticleSystem)
		float worldY = 0.f;

		bool active = true;	// false = stop spawning; existing particles still live
		bool alive = true;	// false = fully done, safe to remove

		void init();
		void update(float dt);
		void render(const ParticleRenderFn& fn) const;
		bool isDone() const;
		int liveCount() const;
		void stop() { active = false; }
		void burst(int count);
		void reset();

	private:
		float m_elapsed = 0.f;
		float m_emitAccum = 0.f;

		struct BurstState {
			int repeatsDone = 0;
			float nextTrigger = 0.f;
		};

		std::vector<BurstState> m_burstStates;
		std::vector<Particle> m_particles;

		void spawnOne();
		void processBursts();
	};
}