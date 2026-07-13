#pragma once

#include <string>
#include <json.hpp>

namespace HBE::Renderer {
	inline constexpr int kPrefabVersion = 1;

	struct PrefabDefinition {
		std::string name;
		std::string tag;
		nlohmann::json components;
		std::string sourcePath;
	};

	struct PrefabRefComponent {
		std::string name;
	};
}