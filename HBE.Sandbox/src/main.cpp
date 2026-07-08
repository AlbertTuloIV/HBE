#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"

#include "GameLayer.h"

using namespace HBE::Core;
using namespace HBE::Platform;

int main() {
	SetLogLevel(LogLevel::Trace);

	WindowConfig cfg;
	cfg.title = "HBE Sandbox - OpenGL";
	cfg.width = 1280;
	cfg.height = 720;
	cfg.useOpenGL = true;
	cfg.mode = WindowMode::Windowed;
	cfg.vsync = true;

	HBE::Core::AssetPaths::Config assetCfg{};
	assetCfg.organization = "HBE";
	assetCfg.application = "HonestlyBadEngine";

	Application app;
	if (!app.initialize(cfg, assetCfg)) {
		LogFatal("SandBox: app.initialize failed.");
		return -1;
	}

	app.pushLayer(std::make_unique<GameLayer>());
	app.run();

	return 0;
}