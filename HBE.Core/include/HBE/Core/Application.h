#pragma once

#include "HBE/Core/LayerStack.h"
#include "HBE/Core/AssetPaths.h"

#include "HBE/Platform/SDLPlatform.h"
#include "HBE/Platform/Audio.h"
#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/ResourceCache.h"

namespace HBE::Core {

	class Application {
	public:
		Application() = default;
		~Application();

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		struct Vec2 {
			float x = 0.f;
			float y = 0.f;
		};

		bool initialize(const HBE::Platform::WindowConfig& windowCfg, const HBE::Core::AssetPaths::Config& assetCfg = {});

		// screen pixels -> logical coords (0..logicalW, 0..logicalH)
		// returns false if the screen point is in the black bars
		bool screenToLogical(int screenX, int screenY, Vec2& outLogical) const;

		// logical coords -> screen pixels
		Vec2 logicalToScreen(float logicalX, float logicalY) const;

		void run();
		void requestQuit() { m_running = false; }

		void pushLayer(std::unique_ptr<Layer> layer);
		void pushOverlay(std::unique_ptr<Layer> overlay);

		// Logical render size for letterboxing
		void setLogicalSize(int w, int h) { m_logicalW = w; m_logicalH = h; recalcViewportAndNotify(); }

		// accessors for layers
		HBE::Platform::SDLPlatform& platform() { return m_platform; }
		HBE::Platform::Audio& audio() { return m_audio; }
		HBE::Renderer::GLRenderer& gl() { return m_gl; }
		HBE::Renderer::Renderer2D& renderer2D() { return m_renderer2D; }
		HBE::Renderer::ResourceCache& resources() { return m_resources; }

		const std::filesystem::path& assetRoot() const { return HBE::Core::AssetPaths::AssetRoot(); }
		const std::filesystem::path& userDataRoot() const { return HBE::Core::AssetPaths::UserDataRoot(); }

		int windowWidthPixels() const { return m_winW; }
		int windowHeightPixels() const { return m_winH; }

		int viewportX() const { return m_vpX; }
		int viewportY() const { return m_vpY; }
		int viewportW() const { return m_vpW; }
		int viewportH() const { return m_vpH; }

	private:
		// store config so app can toggle mode
		HBE::Platform::WindowConfig m_windowCfg{};

		bool m_initialized = false;
		bool m_running = false;

		HBE::Platform::SDLPlatform m_platform;
		HBE::Platform::Audio m_audio;

		HBE::Renderer::GLRenderer m_gl;
		HBE::Renderer::Renderer2D m_renderer2D{ m_gl };
		HBE::Renderer::ResourceCache m_resources;

		LayerStack m_layers;

		int m_winW = 0;
		int m_winH = 0;

		int m_logicalW = 1280;
		int m_logicalH = 720;

		int m_vpX = 0;
		int m_vpY = 0;
		int m_vpW = 0;
		int m_vpH = 0;

		void syncWindowSizeAndViewport();

		void handleSDLEvent(const SDL_Event& e);
		void toggleFullscreen();

		void recalcViewport(); // compute letterbox rect + apply to GL
		void recalcViewportAndNotify(); // recalc + dispatch WindowResizeEvent
	};
}