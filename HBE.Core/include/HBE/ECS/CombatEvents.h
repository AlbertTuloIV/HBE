#pragma once

#include <string>
#include <unordered_map>

#include "HBE/ECS/CombatComponents.h"

namespace HBE::Renderer { class Scene2D; }

namespace HBE::ECS {
	struct HitboxPreset {
		float halfW = 40.0f;
		float halfH = 40.0f;
		float offsetX = 40.0f;
		float offsetY = 0.0f;

		int damage = 1;
		float lifetime = 0.10f;
		bool follows = true;

		std::array<bool, static_cast<std::size_t>(Faction::_Count)> canHit{
			true, true, true, false
		};
		Faction attackerFaction = Faction::Neutral;
		bool friendlyFire = false;

		float knockbackX = 260.0f;
		float knockbackY = 220.0f;
		bool knockbackHorizonatlUsesFacing = true;

		float invulnAfterHit = 0.35f;
	};

	class HitboxPresetRegistry {
	public:
		void registerPreset(const std::string& name, const HitboxPreset& p) {
			m_presets[name] = p;
		}
		const HitboxPreset* get(const std::string& name) const {
			auto it = m_presets.find(name);
			return(it == m_presets.end()) ? nullptr : &it->second;
		}
		bool has(const std::string& name) const {
			return m_presets.find(name) != m_presets.end();
		}
		std::size_t size() { return m_presets.size(); }
	private:
		std::unordered_map<std::string, HitboxPreset> m_presets;
	};

	Entity spawnHitbox(HBE::Renderer::Scene2D& scene, Entity attacker, const HitboxPreset& preset);

	Entity spawnHitboxByName(HBE::Renderer::Scene2D& scene, Entity attacker, const HitboxPresetRegistry& registry, const std::string& presetName);
}