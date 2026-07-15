#pragma once

#include "HBE/Core/Layer.h"

namespace HBE::Core { class Application; }

namespace MegaX {

	class GameLayer : public HBE::Core::Layer {
	public:
		void onAttach(HBE::Core::Application& app) override;
		void onUpdate(float dt) override;
		void onRender() override;
	};
}