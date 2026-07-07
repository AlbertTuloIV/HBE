#pragma once
#include <string>
#include <array>
#include <unordered_map>
#include "HBE/Renderer/TileMap.h"

namespace HBE::Renderer {

	class TileMapLoader {
	public:
		static bool loadFromJsonFile(const std::string& path, TileMap& outMap, std::string* outError = nullptr);

		// Sample the average color of the topmost `topRowsToSample` rows of pixels of each tile in
		// tileset 0. Fully-transparent pixels are skipped. Only tiles with at least one visible pixel
		// are added to `outColors` (keyed by 1-based tile ID). Returns true if the tileset image
		// was loaded successfully.
		static bool sampleTileTopColors(
			const TileMap& map,
			std::unordered_map<int, std::array<float, 4>>& outColors,
			int topRowsToSample = 3);
	};
}