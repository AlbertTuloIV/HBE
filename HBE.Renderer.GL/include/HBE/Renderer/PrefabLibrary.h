#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "HBE/Renderer/Prefab.h"

namespace HBE::Renderer {
	class PrefabLibrary {
	public:
		PrefabLibrary() = default;
		int loadDirectory(std::string_view directoryLogicalPath);

		void addOrReplace(PrefabDefinition def);

		const PrefabDefinition* get(const std::string& name) const;

		bool has(const std::string& name) const { return get(name) != nullptr; }

		std::vector<std::string> names() const;

		int size() const { return static_cast<int>(m_prefabs.size()); }

		void clear();
	private:
		std::unordered_map<std::string, PrefabDefinition> m_prefabs;
	};
}