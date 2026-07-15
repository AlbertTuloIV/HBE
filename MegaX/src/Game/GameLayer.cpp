#include "Game/GameLayer.h"

#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"

namespace MegaX {

	void GameLayer::onAttach(HBE::Core::Application& app) {
		(void)app;
		HBE::Core::LogInfo("MegaX GameLayer attached.");
	}

	void GameLayer::onUpdate(float dt) {
		(void)(dt);
	}

	void GameLayer::onRender() {

	}
}