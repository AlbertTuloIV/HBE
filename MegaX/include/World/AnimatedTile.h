#pragma once
#include <string>

namespace HBE::Renderer { class Texture2D; }

namespace MegaX {
	struct AnimatedTile {
		int tilesetIndex = 0;
		int tileId = 0;
		std::string sheetPath;

		int frameW = 32;
		int frameH = 32;
		int frameCount = 8;
		float fps = 8.0f;

		HBE::Renderer::Texture2D* texture = nullptr;
	};
}