#include "HBE/Renderer/ScriptRegistry.h"

#include "HBE/Core/Log.h"

#include <algorithm>
#include <utility>

namespace HBE::Renderer {

	void ScriptRegistry::registerScript(std::string name, Factory factory) {
		if (name.empty()) {
			HBE::Core::LogWarn("ScriptRegistry::registerScript: refusing to register a "
				"script with an empty name.");
			return;
		}
		if (!factory) {
			HBE::Core::LogWarn(
				"ScriptRegistry::registerScript: refusing to register '"
				+ name + "' with a null factory.");
			return;
		}
		if (m_factories.contains(name)) {
			HBE::Core::LogWarn(
				"ScriptRegistry: replacing existing factory for script '"
				+ name + "'.");
		}
		m_factories[std::move(name)] = std::move(factory);
	}

	bool ScriptRegistry::has(const std::string& name) const {
		return m_factories.find(name) != m_factories.end();
	}

	HBE::ECS::Script ScriptRegistry::create(const std::string& name, HBE::ECS::Entity entity, Scene2D& scene) const {
		auto it = m_factories.find(name);
		if (it == m_factories.end()) {
			HBE::Core::LogWarn(
				"ScriptRegistry::create: no factory registered for '"
				+ name + "'. Returning empty script.");
			return {};
		}

		HBE::ECS::Script s = it->second(entity, scene);
		if (s.name.empty()) {
			s.name = name;
		}
		return s;
	}

	std::vector<std::string> ScriptRegistry::names() const {
		std::vector<std::string> out;
		out.reserve(m_factories.size());
		for (const auto& [k, _] : m_factories) out.push_back(k);
		std::sort(out.begin(), out.end());
		return out;
	}

	void ScriptRegistry::clear() {
		m_factories.clear();
	}
}