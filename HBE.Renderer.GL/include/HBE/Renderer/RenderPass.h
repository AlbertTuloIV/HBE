#pragma once
#include <cstdint>

namespace HBE::Renderer {
	enum class RenderPass : std::uint8_t {
		World = 0,
		UI = 1,
		Overlay = 2,

		Count
	};

	namespace Layers {
		constexpr int TileBackground = 0;
		constexpr int TileWorld = 10;
		constexpr int Sprites = 50;
		constexpr int WorldFX = 100;
		constexpr int WorldOverlay = 150;
		constexpr int Particles = 200;

		constexpr int UIBackground = 0;
		constexpr int UIWidgets = 50;
		constexpr int UIText = 100;

		constexpr int OverlaydDebug = 250;
	}

	enum class BlendMode : std::uint8_t {
		Alpha = 0,
		Additive = 1,
		Opaque = 2,

		Invalid = 0xFF
	};
}