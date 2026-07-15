#pragma once

#include "HBE/ECS/Registry.h"
#include "HBE/ECS/CombatComponents.h"

namespace HBE::Core { class EventBus; }

namespace HBE::ECS {
	// `bus` is optional: pass nullptr in tests or headless tools that
	// don't care about the events. When null, no events are published
	// but combat still resolves normally.
	void updateCombat(Registry& reg, float dt, HBE::Core::EventBus* bus = nullptr);
}