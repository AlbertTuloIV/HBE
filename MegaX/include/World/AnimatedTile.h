#pragma once

#include <string>
#include <vector>

#include "HBE/Renderer/Material.h"

namespace HBE::Renderer { class Texture2D; }

namespace MegaX {

	struct AnimatedTileInstance {
		int tileX = 0;
		int tileY = 0;
	};

	struct AnimatedTile {

		int tilesetIndex = 0;
		int tileId = 0;

		std::string sheetPath;

		int frameW = 32;
		int frameH = 32;
		int frameCount = 8;
		float fps = 8.0f;

		HBE::Renderer::Texture2D* texture = nullptr;
		int texW = 0;
		int texH = 0;

		HBE::Renderer::Material material{};
		std::vector<AnimatedTileInstance> instances{};
	};
}