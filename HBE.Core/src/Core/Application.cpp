#include "HBE/Core/Application.h"

#include "HBE/Core/Log.h"
#include "HBE/Core/Time.h"
#include "HBE/Core/Event.h"
#include "HBE/Platform/Input.h"
#include "HBE/Input/InputMap.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_scancode.h>

namespace HBE::Core {

	using HBE::Core::LogError;
	using HBE::Core::LogInfo;
	using HBE::Core::LogFatal;

	Application::~Application() {
		m_layers.clear();
		// SDLPlatform destructur already calls shutdown()
	}

	bool Application::initialize(const HBE::Platform::WindowConfig& windowCfg) {
		if (m_initialized) return true;

		m_windowCfg = windowCfg;

		if (!m_platform.initialize(windowCfg)) {
			LogError("Application: SDLPlatform init failed.");
			return false;
		}

		if (!m_gl.initialize(m_platform)) {
			LogError("Application: GLRenderer init failed.");
			return false;
		}

		if (!m_audio.initialize()) {
			LogError("Application: Audio init failed.");
			return false;
		}

		recalcViewportAndNotify();

		m_initialized = true;
		LogInfo("Application initialized.");
		return true;
	}

	static void ComputeLetterboxViewport(int windowW, int windowH, int logicalW, int logicalH, int& outX, int& outY, int& outW, int& outH) {
		if (windowW <= 0 || windowH <= 0 || logicalW <= 0 || logicalH <= 0) {
			outX = 0; outY = 0; outW = windowW; outH = windowH;
			return;
		}

		const float windowAspect = static_cast<float>(windowW) / static_cast<float>(windowH);
		const float logicalAspect = static_cast<float>(logicalW) / static_cast<float>(logicalH);

		if (windowAspect > logicalAspect) {
			// window is wider -> pillarbox
			outH = windowH;
			outW = static_cast<int>(outH * logicalAspect + 0.5f);
			outX = (windowW - outW) / 2;
			outY = 0;
		}
		else {
			// window is taller -> letterbox
			outW = windowW;
			outH = static_cast<int>(outW / logicalAspect + 0.5f);
			outX = 0;
			outY = (windowH - outH) / 2;
		}
	}

	void Application::pushLayer(std::unique_ptr<Layer> layer) {
		m_layers.pushLayer(std::move(layer), *this);
	}

	void Application::pushOverlay(std::unique_ptr<Layer>overlay) {
		m_layers.pushOverlay(std::move(overlay), *this);
	}

	bool Application::screenToLogical(int screenX, int screenY, Vec2& outLogical) const {
		// screenX/screenY are in window pixel coords (top-left origin in SDL mouse events)
		// Our OpenGL viewport uses bottom-left origin, but letterbox math is pixel-rect based.
		// We'll treat inputs as top-left and convert to viewport local first.

		// Convert to "viewport-local" coordinates in window space (top-left origin)
		const int vpLeft = m_vpX;
		const int vpTop = m_vpY; // NOTE: m_vpY is bottom-left for glViewport math
		// but our ComputeLetterboxViewport gave us vpY in window pixels from bottom.
		// We need a consistent convention.

		// Fix the convention: our viewport rect is in OpenGL coordinates (origin bottom-left).
		// SDL mouse events are top-left origin. Convert screenY to bottom-left origin:
		const int screenY_fromBottom = m_winH - 1 - screenY;

		// Check inside viewport rect (in bottom-left origin space)
		if (screenX < m_vpX || screenX >= (m_vpX + m_vpW) ||
			screenY_fromBottom < m_vpY || screenY_fromBottom >= (m_vpY + m_vpH)) {
			return false; // in black bars
		}

		// viewport-local (0..vpW, 0..vpH), bottom-left origin
		const int localX = screenX - m_vpX;
		const int localY = screenY_fromBottom - m_vpY;

		const float nx = static_cast<float>(localX) / static_cast<float>(m_vpW);
		const float ny = static_cast<float>(localY) / static_cast<float>(m_vpH);

		// map to logical coords (bottom-left origin)
		outLogical.x = nx * static_cast<float>(m_logicalW);
		outLogical.y = ny * static_cast<float>(m_logicalH);

		return true;
	}

	Application::Vec2 Application::logicalToScreen(float logicalX, float logicalY) const {
		Vec2 out{};

		const float nx = (m_logicalW > 0) ? (logicalX / static_cast<float>(m_logicalW)) : 0.f;
		const float ny = (m_logicalH > 0) ? (logicalY / static_cast<float>(m_logicalH)) : 0.f;

		const float px = static_cast<float>(m_vpX) + nx * static_cast<float>(m_vpW);
		const float py_fromBottom = static_cast<float>(m_vpY) + ny * static_cast<float>(m_vpH);

		// convert bottom-left origin to top-left origin
		const float py_topLeft = static_cast<float>(m_winH - 1) - py_fromBottom;

		out.x = px;
		out.y = py_topLeft;
		return out;
	}

	void Application::toggleFullscreen() {
		if (m_windowCfg.mode == HBE::Platform::WindowMode::Windowed)
			m_windowCfg.mode = HBE::Platform::WindowMode::FullscreenDesktop;
		else
			m_windowCfg.mode = HBE::Platform::WindowMode::Windowed;

		m_platform.applyGraphicsSettings(m_windowCfg);

		// Mode changes often alter pixel size; recompute viewport and notify layers
		recalcViewportAndNotify();
	}

	void Application::handleSDLEvent(const SDL_Event& e) {
		// Always feed Input first
		HBE::Platform::Input::HandleEvent(e);

		// Feed mapping layer too (rebinding capture). Safe if never initialized.
		HBE::Input::HandleEvent(e);

		switch (e.type) {
			// -------- Keyboard Events ----------
		case SDL_EVENT_KEY_DOWN: {
			const int sc = static_cast<int>(e.key.scancode);
			const bool repeat = (e.key.repeat != 0);

			KeyPressedEvent kev(sc, repeat);

			// Global fullscreen toggle (ignore repeats)
			if (!repeat && e.key.scancode == SDL_SCANCODE_F11) {
				toggleFullscreen();
				kev.handled = true;
			}

			// Dispatch to layers (even if handled, up to you; here we stop if handled)
			if (!kev.handled) {
				m_layers.dispatchEvent(kev);
			}
			break;
		}
		case SDL_EVENT_WINDOW_FOCUS_LOST: {
			// Pause gameplay-ish sounds when the app loses focus.
			// Keep it simple for now: pause SFX + Ambient, leave Music alone if you prefer.
			m_audio.pauseBus(HBE::Platform::Audio::Bus::SFX);
			m_audio.pauseBus(HBE::Platform::Audio::Bus::Ambient);
			break;
		}
		case SDL_EVENT_WINDOW_FOCUS_GAINED: {
			m_audio.resumeBus(HBE::Platform::Audio::Bus::SFX);
			m_audio.resumeBus(HBE::Platform::Audio::Bus::Ambient);
			break;
		}
							   // -------- Window Resize Events ----------
							   // SDL3 has multiple size-related events. These are the ones you�ll commonly see.
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
			recalcViewportAndNotify();
			break;
		}
												// -------- Mouse Events ----------
		case SDL_EVENT_MOUSE_MOTION: {
			// window coords (top-left origin)
			const float mx = e.motion.x;
			const float my = e.motion.y;
			const float dx = e.motion.xrel;
			const float dy = e.motion.yrel;

			Vec2 logical{};
			const bool inVp = screenToLogical(static_cast<int>(mx), static_cast<int>(my), logical);

			MouseMovedEvent mev(mx, my, dx, dy, inVp, logical.x, logical.y);
			m_layers.dispatchEvent(mev);
			break;
		}
		case SDL_EVENT_MOUSE_BUTTON_DOWN: {
			const int button = static_cast<int>(e.button.button);
			const int clicks = static_cast<int>(e.button.clicks);
			const float mx = e.button.x;
			const float my = e.button.y;

			Vec2 logical{};
			const bool inVp = screenToLogical(static_cast<int>(mx), static_cast<int>(my), logical);

			MouseButtonPressedEvent mb(button, clicks, mx, my, inVp, logical.x, logical.y);
			m_layers.dispatchEvent(mb);
			break;
		}
		case SDL_EVENT_MOUSE_BUTTON_UP: {
			const int button = static_cast<int>(e.button.button);
			const float mx = e.button.x;
			const float my = e.button.y;

			Vec2 logical{};
			const bool inVp = screenToLogical(static_cast<int>(mx), static_cast<int>(my), logical);

			MouseButtonReleasedEvent mb(button, mx, my, inVp, logical.x, logical.y);
			m_layers.dispatchEvent(mb);
			break;
		}
		case SDL_EVENT_MOUSE_WHEEL: {
			// SDL convention: +y = away from user
			float wx = e.wheel.x;
			float wy = e.wheel.y;

			const float mx = e.wheel.mouse_x;
			const float my = e.wheel.mouse_y;

			Vec2 logical{};
			const bool inVp = screenToLogical(static_cast<int>(mx), static_cast<int>(my), logical);

			MouseScrolledEvent ms(wx, wy, mx, my, inVp, logical.x, logical.y);
			m_layers.dispatchEvent(ms);
			break;
		}
		case SDL_EVENT_TEXT_INPUT: {
			// SDL gives UTF-8 text in e.text.text
			TextInputEvent tev(e.text.text);
			m_layers.dispatchEvent(tev);
			break;
		}
		default:
			break;
		}
	}

	void Application::recalcViewport() {
		int w = 0, h = 0;
		SDL_GetWindowSizeInPixels(m_platform.getWindow(), &w, &h);

		if (w <= 0 || h <= 0) {
			w = m_windowCfg.width > 0 ? m_windowCfg.width : 1280;
			h = m_windowCfg.height > 0 ? m_windowCfg.height : 720;
		}

		m_winW = w;
		m_winH = h;

		ComputeLetterboxViewport(m_winW, m_winH, m_logicalW, m_logicalH, m_vpX, m_vpY, m_vpW, m_vpH);

		// Apply viewport rect (letterboxed)
		// We'll extend GLRenderer to accept x/y/w/h below.
		m_gl.setViewportRect(m_vpX, m_vpY, m_vpW, m_vpH);
	}

	void Application::recalcViewportAndNotify() {
		recalcViewport();
		WindowResizeEvent wev(m_winW, m_winH, m_vpX, m_vpY, m_vpW, m_vpH);
		m_layers.dispatchEvent(wev);
	}


	void Application::syncWindowSizeAndViewport() {
		int w = 0, h = 0;
		SDL_GetWindowSizeInPixels(m_platform.getWindow(), &w, &h);

		if (w <= 0 || h <= 0) {
			w = 1280;
			h = 720;
		}

		m_winW = w;
		m_winH = h;


		// Note: GLRenderer::resizeViewport early-outs if not initialized but at this point it is.
		m_gl.resizeViewport(m_winW, m_winH);
	}

	void Application::run() {
		if (!m_initialized) {
			LogError("Application::run called before initialize()");
			return;
		}

		m_running = true;

		double prevTime = GetTimeSeconds();

		while (m_running) {
			HBE::Platform::Input::NewFrame();

			// mapping layer needs per-frame update too (edge detection for axis-threshold)
			HBE::Input::NewFrame();

			// Pump SDL events through platform
			bool quit = m_platform.pumpEvents([this](const SDL_Event& e) {
				this->handleSDLEvent(e);
				});

			if (quit) {
				m_running = false;
				break;
			}

			// dt
			double now = GetTimeSeconds();
			float dt = static_cast<float>(now - prevTime);
			if (dt < 0.0f) dt = 0.0f;
			if (dt > 0.25f) dt = 0.25f;
			prevTime = now;

			// update
			for (auto& layer : m_layers) {
				if (layer) layer->onUpdate(dt);
			}
			// keep audio spatialization / finished-track cleanup fresh
			m_audio.update(dt);

			// 1) Clear the whole window (black bars)
			m_gl.beginFrameFullWindow(m_winW, m_winH);

			// 2) Render scene only inside the letterboxed viewport
			m_gl.beginFrameInViewport(m_vpX, m_vpY, m_vpW, m_vpH);

			for (auto& layer : m_layers) {
				if (layer) layer->onRender();
			}

			m_gl.endFrame(m_platform);

			m_platform.delayMillis(1);
		}

		LogInfo("Application exiting run loop.");
	}

}