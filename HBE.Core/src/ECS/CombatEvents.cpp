#include "HBE/ECS/CombatEvents.h"

#include "HBE/Core/Log.h"
#include "HBE/Renderer/Scene2D.h"
#include "HBE/Renderer/Transform2D.h"

namespace HBE::ECS {

	Entity spawnHitbox(HBE::Renderer::Scene2D& scene, Entity attacker, const HitboxPreset& preset) {
		auto& reg = scene.registry();

		Entity e = reg.create();

		Hitbox hb{};
		hb.owner = attacker;
		hb.halfW = preset.halfW;
		hb.halfH = preset.halfH;
		hb.offsetX = preset.offsetX;
		hb.offsetY = preset.offsetY;
		hb.damage = preset.damage;
		hb.lifetime = preset.lifetime;
		hb.follows = preset.follows;
		hb.canHit = preset.canHit;
		hb.attackerFaction = preset.attackerFaction;
		hb.friendlyFire = preset.friendlyFire;
		hb.knockbackX = preset.knockbackX;
		hb.knockbackY = preset.knockbackY;
		hb.knockbackHorizontalUsesFacing = preset.knockbackHorizonatlUsesFacing;
		hb.invulnAfterHit = preset.invulnAfterHit;
		hb.active = true;

		reg.emplace<Hitbox>(e, std::move(hb));

		if (!preset.follows) {
			HBE::Renderer::Transform2D tr{};
			if (reg.has<HBE::Renderer::Transform2D>(attacker)) {
				tr = reg.get<HBE::Renderer::Transform2D>(attacker);
				tr.scaleX = 1.0f;
				tr.scaleY = 1.0f;
			}
			reg.emplace<HBE::Renderer::Transform2D>(e, tr);
		}
		return e;
	}

	Entity spawnHitboxByName(HBE::Renderer::Scene2D& scene, Entity attacker, const HitboxPresetRegistry& registry, const std::string& presetName) {
		const HitboxPreset* p = registry.get(presetName);
		if (!p) {
			HBE::Core::LogWarn("spawnHitbox: unkown preset '"
				+ presetName + "' (skipping).");
			return Null;
		}
		return spawnHitbox(scene, attacker, *p);
	}
}
