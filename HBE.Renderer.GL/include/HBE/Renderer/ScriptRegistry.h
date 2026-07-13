#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "HBE/ECS/Registry.h"
#include "HBE/ECS/Components.h"

namespace HBE::Renderer {
	class Scene2D;

	class ScriptRegistry {
	public:
		using Factory = std::function<HBE::ECS::Script(HBE::ECS::Entity, Scene2D&)>;

		ScriptRegistry() = default;

		void registerScript(std::string name, Factory factory);

		bool has(const std::string& name) const;

		HBE::ECS::Script create(const std::string& name, HBE::ECS::Entity entity, Scene2D& scene) const;

		std::vector<std::string> names() const;

		int size() const { return static_cast<int>(m_factories.size()); }

		void clear();
	private:
		std::unordered_map<std::string, Factory> m_factories;
	};
}