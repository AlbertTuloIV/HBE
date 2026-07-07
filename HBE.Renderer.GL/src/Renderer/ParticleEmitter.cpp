#include "HBE/Renderer/ParticleEmitter.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace HBE::Renderer {
	// -- RNG helpers
	static std::mt19937& rng() {
		static std::mt19937 gen{ std::random_device{}() };
		return gen;
	}
	static float randF(float lo, float hi) {
		if (lo >= hi) return lo;
		return std::uniform_real_distribution<float>(lo, hi)(rng());
	}
	static float randF01() { return randF(0.f, 1.f); }

	static constexpr float kDeg2Rad = 3.14159265f / 180.f;

	// -- init / reset
	void ParticleEmitter::init() {
		m_particles.assign(static_cast<std::size_t>(config.maxParticles), Particle{});
		for (auto& p : m_particles) p.alive = false;

		m_burstStates.resize(config.bursts.size());
		for (std::size_t i = 0; i < config.bursts.size(); ++i) {
			m_burstStates[i].repeatsDone = 0;
			m_burstStates[i].nextTrigger = config.bursts[i].time;
		}
		m_elapsed = 0.f;
		m_emitAccum = 0.f;
		active = true;
		alive = true;
	}

	void ParticleEmitter::reset() {
		for (auto& p : m_particles) p.alive = false;
		for (std::size_t i = 0; i < config.bursts.size(); ++i) {
			m_burstStates[i].repeatsDone = 0;
			m_burstStates[i].nextTrigger = config.bursts[i].time;
		}
		m_elapsed = 0.f;
		m_emitAccum = 0.f;
		active = true;
		alive = true;
	}

	// -- spawn one particle
	void ParticleEmitter::spawnOne() {
		Particle* slot = nullptr;
		for (auto& p : m_particles) {
			if (!p.alive) { slot = &p; break; }
		}
		if (!slot) return;

		Particle& p = *slot;
		p.alive = true;
		p.maxLife = randF(config.lifetimeMin, config.lifetimeMax);
		p.life = p.maxLife;

		// -- position offset from shape
		float ox = 0.f, oy = 0.f;
		switch (config.shape) {
			case EmitterConfig::Shape::Circle: {
				float a = randF(0.f, 360.f) * kDeg2Rad;
				float r = randF(0.f, config.shapeRadius);
				ox = std::cos(a) * r; oy = std::sin(a) * r;
				break;
			}
			case EmitterConfig::Shape::Ring: {
				float a = randF(0.f, 360.f) * kDeg2Rad;
				ox = std::cos(a) * config.shapeRadius;
				oy = std::sin(a) * config.shapeRadius;
				break;
			}
			case EmitterConfig::Shape::Rect:
				ox = randF(-config.shapeWidth * 0.5f, config.shapeWidth * 0.5f);
				oy = randF(-config.shapeHeight * 0.5f, config.shapeHeight * 0.5f);
				break;
			case EmitterConfig::Shape::Line:
				ox = randF(-config.shapeWidth * 0.5f, config.shapeWidth * 0.5f);
				break;
			default: break;
		}

		if (config.worldSpace) {
			p.x = worldX + ox; p.y = worldY + oy;
		}
		else {
			p.x = ox; p.y = oy;
		}

		// -- velocity
		float speed = randF(config.speedMin, config.speedMax);
		float rad = randF(config.dirMin, config.dirMax) * kDeg2Rad;
		p.vx = std::cos(rad) * speed + config.inheritVX;
		p.vy = std::sin(rad) * speed + config.inheritVY;

		// -- size
		p.startSize = randF(config.startSizeMin, config.startSizeMax);
		p.endSize = randF(config.endSizeMin, config.endSizeMax);
		p.size = p.startSize;

		// -- rotation
		p.rotation = randF(config.startRotMin, config.startRotMax);
		p.rotVel = randF(config.rotVelMin, config.rotVelMax);

		// -- color
		if (config.startR2 >= 0.f) {
			float t = randF01();
			p.sr = config.startR + t * (config.startR2 - config.startR);
			p.sg = config.startG + t * (config.startG2 - config.startG);
			p.sb = config.startB + t * (config.startB2 - config.startB);
			p.sa = config.startA + t * (config.startA2 - config.startA);
		}
		else {
			p.sr = config.startR; p.sg = config.startG;
			p.sb = config.startB; p.sa = config.startA;
		}
		p.er = config.endR; p.eg = config.endG;
		p.eb = config.endB; p.ea = config.endA;
	}

	// -- manual burst
	void ParticleEmitter::burst(int count) {
		for (int i = 0; i < count; ++i) spawnOne();
	}

	// -- timed burst processing
	void ParticleEmitter::processBursts() {
		for (std::size_t i = 0; i < config.bursts.size(); ++i) {
			const auto& bcfg = config.bursts[i];
			auto& bstate = m_burstStates[i];

			if (m_elapsed < bstate.nextTrigger) continue;

			bool unlimited = (bcfg.maxRepeats < 0);
			bool canRepeat = unlimited || (bstate.repeatsDone < bcfg.maxRepeats);
			if (!canRepeat) continue;

			burst(bcfg.count);
			++bstate.repeatsDone;
			bstate.nextTrigger = m_elapsed + bcfg.interval;
		}
	}

	// -- update
	void ParticleEmitter::update(float dt) {
		if (!alive) return;

		m_elapsed += dt;
		if (active) {
			processBursts();

			if (config.emissionRate > 0.f) {
				bool infinite = (config.duration < 0.f);
				bool inTime = infinite || (m_elapsed <= config.duration);

				if (!inTime && config.loop) {
					m_elapsed = std::fmod(m_elapsed, config.duration);
					inTime = true;
					for (std::size_t i = 0; i < config.bursts.size(); ++i) {
						m_burstStates[i].repeatsDone = 0;
						m_burstStates[i].nextTrigger = config.bursts[i].time;
					}
				}

				if (inTime) {
					m_emitAccum += config.emissionRate * dt;
					while (m_emitAccum >= 1.f) {
						spawnOne();
						m_emitAccum -= 1.f;
					}
				}
			}
		}

		for (auto& p : m_particles) {
			if (!p.alive) continue;

			p.life -= dt;
			if (p.life <= 0.f) { p.alive = false; continue; }

			p.vx += config.gravityX * dt;
			p.vy += config.gravityY * dt;

			if (config.drag > 0.f) {
				float f = std::max(0.f, 1.f - config.drag * dt);
				p.vx *= f; p.vy *= f;
			}

			if (config.worldSpace) {
				p.x += p.vx * dt;
				p.y += p.vy * dt;
			}

			p.rotation += p.rotVel * dt;

			float t = 1.f - (p.life / p.maxLife);
			p.size = p.startSize + (p.endSize - p.startSize) * t;
		}

		if (isDone()) alive = false;
	}

	// -- render
	void ParticleEmitter::render(const ParticleRenderFn& fn) const {
		if (!fn) return;
		for (const auto& p : m_particles) {
			if (!p.alive) continue;

			float t = 1.f - (p.life / p.maxLife);
			float r = p.sr + (p.er - p.sr) * t;
			float g = p.sg + (p.eg - p.sg) * t;
			float b = p.sb + (p.eb - p.sb) * t;
			float a = p.sa + (p.ea - p.sa) * t;

			float rx = config.worldSpace ? p.x : p.x + worldX;
			float ry = config.worldSpace ? p.y : p.y + worldY;

			fn(rx, ry, p.size, p.rotation, r, g, b, a, config.additiveBlend);
		}
	}

	// -- isDone
	bool ParticleEmitter::isDone() const {
		if (config.loop && active) return false;
		if (active && config.duration < 0.f) return false;
		if (active && m_elapsed <= config.duration + config.lifetimeMax) return false;

		for (const auto& p : m_particles) {
			if (p.alive) return false;
		}

		return true;
	}

	int ParticleEmitter::liveCount() const {
		int n = 0;
		for (const auto& p : m_particles) n += p.alive ? 1 : 0;
		return n;
	}
}