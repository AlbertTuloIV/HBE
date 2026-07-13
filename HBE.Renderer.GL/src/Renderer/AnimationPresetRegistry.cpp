#include "HBE/Renderer/AnimationPresetRegistry.h"

#include "HBE/Core/Log.h"
#include "HBE/Renderer/SpriteAnimationStateMachine.h"

#include <algorithm>
#include <utility>

namespace HBE::Renderer {

	void AnimationPresetRegistry::registerPreset(std::string name, Builder builder) {
		if (name.empty()) {
			HBE::Core::LogWarn("AnimationPresetRegistry::registerPrest: refusing to register a preset with an empty name.");
			return;
		}
		if (!builder) {
			HBE::Core::LogWarn("AnimationPresetRegistry::registerPreset: refusing to register '" + name + "' with a null builder.");
			return;
		}
		if (m_builders.contains(name)) {
			HBE::Core::LogWarn("AnimationPresetRegistry: replacing existing builder for preset '" + name + "'.");
		}
		m_builders[std::move(name)] = std::move(builder);
	}

	bool AnimationPresetRegistry::has(const std::string& name) const {
		return m_builders.find(name) != m_builders.end();
	}

	void AnimationPresetRegistry::build(const std::string& name, HBE::ECS::Entity entity, SpriteAnimationStateMachine& sm, Scene2D& scene) const {
		auto it = m_builders.find(name);
		if (it == m_builders.end()) {
			HBE::Core::LogWarn("AnimationPresetRegsitry::build: no builder registered for preset '" + name + "'. State machine left as-is.");
			return;
		}
		it->second(entity, sm, scene);
	}

	std::vector<std::string> AnimationPresetRegistry::names() const {
		std::vector<std::string> out;
		out.reserve(m_builders.size());
		for (const auto& [k, _] : m_builders) out.push_back(k);
		std::sort(out.begin(), out.end());
		return out;
	}
	void AnimationPresetRegistry::clear() {
		m_builders.clear();
	}
}