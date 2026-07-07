#pragma once
#include <string>
#include <vector>

namespace HBE::Renderer {
	// pure data that describes how one emitter behaves
	// create instances of this in game code
	// p[ass them to ParticleSystem::registerEffect() - no engine code to modify
	struct EmitterConfig {
		// -- Emission --------------------------------------------
		// how many particles per second to emit continuously.
		// set to 0 to use bursts only
		float emissionRate = 20.f;

		// how long emitter stays active (seconds)
		// -1 = run forever until you call stop()
		float duration = 1.f;

		bool loop = false;

		// -- Bursts ------------------------------------------------------
		// a burst fires a batch of particles at a specific moment in time.
		// add multiple bursts to the same emitter for complex patterns.
		struct Burst {
			float time = 0.f;		// seconds after emitter start
			float count = 10;		// particles to emit at once
			int maxRepeats = 1;		// how many times to fire (-1 = unlimited)
			float interval = 0.1f;	// seconds between repeats
		};
		std::vector<Burst> bursts;

		// -- Particle Lifetime----------------------------------------------
		float lifetimeMin = 0.4f;
		float lifetimeMax = 0.9f;

		// -- Spawn Shape ---------------------------------------------------
		// controls the random offset from the emitter origin at spawn time
		enum class Shape {
			Point,		// all particles come from the exact emitter position
			Circle,		// random point inside a filled circle
			Ring,		// random point inside a circle edge
			Rect,		// random point inside a rectangle
			Line,		// random point along a horizontal line
		};

		Shape shape = Shape::Point;
		float shapeRadius = 0.f;	// Circle: 0..radius	| Ring: exactly Radius
		float shapeWidth = 0.f;		// Rect: full width		| Line: full length
		float shapeHeight = 0.f;	// Rect: full height

		// -- Velocity ------------------------------------------------------------
		float speedMin = 60.f;
		float speedMax = 140.f;

		// launch direction range (degrees, 0 = right, 90 = up in +Y-up space)
		// 0..360 = all directions. 60..120 = upward cone. 90..90 = straight up.
		float dirMin = 0.f;
		float dirMax = 360.f;

		// optional constant velocity added to every new particle.
		// useful for wind or inheritting emitter motion.
		float inheritVX = 0.f;
		float inheritVY = 0.f;

		// -- Physics ----------------------------------------------------------
		// constant acceleration (world units / s^2) applied every frame.
		// engine uses +Y = up so use a negative gravityY to pull particles down.
		float gravityX = 0.f;
		float gravityY = 0.f;

		// velocity damping fraction per second. 0 = none, 0.95 = heavy drag.
		float drag = 0.f;

		// -- Rotation ----------------------------------------------------------
		float startRotMin = 0.f, startRotMax = 0.f;	// initial rotation (rads)
		float rotVelMin = 0.f, rotVelMax = 0.f;		// spin speed (rads / s)

		// Size Over LifeTime --------------------------------------------------
		float startSizeMin = 6.f, startSizeMax = 10.f;
		float endSizeMin = 0.f, endSizeMax = 0.f;

		// -- Color Over Lifetime ----------------------------------------------
		// each particle interpolates from its start color to end R/G/B/A at death
		float startR = 1.f, startG = 1.f, startB = 1.f, startA = 1.f;
		float endR = 1.f, endG = 1.f, endB = 1.f, endA = 0.f;

		// optional second start color for per-particle variance.
		float startR2 = -1.f, startG2 = -1.f, startB2 = -1.f, startA2 = -1.f;

		// -- Coordinate Space ------------------------------------------------
		// true = particles are placed in world space and stay when emitter moves.
		//			Best for explosions, hit sparks, weather.
		// false = particles are relative to the emitter (follow it).
		//			Best for auras, shields, attached trails.
		bool worldSpace = true;

		// -- Performance -----------------------------------------------------
		// pre-allocated pool size. new particles are silently dropped if full.
		// size this just above the maximum live particles you expect.
		int maxParticles = 200;

		// -- Rendering Hints -------------------------------------------------
		// Draw layer. Higher = drawn on top. Sprites are at 50 in current sandbox.
		int sortLayer = 200;

		// additive blend hint. with the default DebugDraw2D renderer this is 
		// passed thorugh to any custom render callback provided.
		bool additiveBlend = false;

		// optional name for debugging / logging output.
		std::string name;
	};

	// an effect is one or more EmitterConfig Layers running at the same time.
	// a single config is the common case; multiple configs layer effects
	// (e.g. explosions = sparks + smoke + flash).
	using EffectDef = std::vector<EmitterConfig>;
}