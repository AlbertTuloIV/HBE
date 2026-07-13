#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "HBE/ECS/Registry.h";

namespace HBE::Renderer {

	class Scene2D;
	class SpriteAnimationStateMachine;

	class AnimationPresetRegistry {
	public:
		using Builder = std::function<void(HBE::ECS::Entity, SpriteAnimationStateMachine&, Scene2D&)>;

		AnimationPresetRegistry() = default;

		void registerPreset(std::string name, Builder builder);

		bool has(const std::string& name) const;

		void build(const std::string& name, HBE::ECS::Entity entity, SpriteAnimationStateMachine& sm, Scene2D& scene) const;

		std::vector<std::string> names() const;

		int size() const { return static_cast<int>(m_builders.size()); }

		void clear();
	private:
		std::unordered_map<std::string, Builder> m_builders;
	};
}