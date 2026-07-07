#pragma once

namespace HBE::Renderer {

	// one simulated particle, plain data no methods
	// used internally by ParticleEmitter; game code never touches this directly
	struct Particle {
		float x = 0.f, y = 0.f;		// world-space position
		float vx = 0.f, vy = 0.f;	// velocity (world units / second)

		float life = 0.f;			// remaining lifetime (seconds)
		float maxLife = 1.f;		// starting lifetime

		float startSize = 8.f;		// size at spawn
		float endSize = 0.f;		// size at death
		float size = 8.f;			// current computed size

		float rotation = 0.f;		// current rotation (radians)
		float rotVel = 0.f;			// rotation speed (radians / s)

		// per-particle start/end colors; lerped over the particle's lifetime
		float sr = 1.f, sg = 1.f, sb = 1.f, sa = 1.f; // start RGBA
		float er = 1.f, eg = 1.f, eb = 1.f, ea = 0.f; // end RGBA

		bool alive = false;
	};
}