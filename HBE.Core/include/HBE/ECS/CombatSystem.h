#pragma once

#include "HBE/ECS/Registry.h"
#include "HBE/ECS/CombatComponents.h"

namespace HBE::ECS {
	void updateCombat(Registry& reg, float dt);
}