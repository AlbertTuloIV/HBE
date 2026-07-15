#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"

#include "Game/GameLayer.h"

#include <memory>

using namespace HBE::Core;
using namespace HBE::Platform;

int main() {
	SetLogLevel(LogLevel::Info);

	WindowConfig cfg;
	cfg.title = "MegaX";
	cfg.width = 1280;
	cfg.height = 720;
	cfg.useOpenGL = true;
	cfg.mode = WindowMode::Windowed;
	cfg.vsync = true;

	AssetPaths::Config assetCfg{};
	assetCfg.organization = "MegaX";
	assetCfg.application = "MegaX";
	assetCfg.siblingProjectNames = { "MegaX" };

	Application app;
	if (!app.initialize(cfg, assetCfg)) {
		return -1;
	}

	app.pushLayer(std::make_unique<MegaX::GameLayer>());
	app.run();

	return 0;
}