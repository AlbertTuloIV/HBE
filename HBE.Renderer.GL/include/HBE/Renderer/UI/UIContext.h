#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "HBE/Renderer/UI/UIRect.h"
#include "HBE/Renderer/UI/UIStyle.h"

#include "HBE/Core/Event.h"
#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/DebugDraw2D.h"
#include "HBE/Renderer/TextRenderer2D.h"

namespace HBE::Renderer::UI {
	class UIContext {
	public:
		void setStyle(const UIStyle& style) { m_style = style; }
		UIStyle& style() { return m_style; }
		const UIStyle& style() const { return m_style; }

		// Call once per frame BEFORE building widgets
		void beginFrame(float dt);
		void endFrame();

		// Feed events from layer
		void onEvent(HBE::Core::Event& e);

		// ---- Layout Helpers ----
		bool beginPanel(const char* id, const UIRect& rect, const char* title = nullptr);
		// Scrollable variant: enables mouse-wheel scrolling and consumes overflow.
		// The `id` must be non-null and stable across frames.
		bool beginScrollPanel(const char* id, const UIRect& rect, const char* title = nullptr);
		void endPanel();

		void spacing(float h);
		void label(const char* text, bool muted = false);

		// Auto-layout button inside current panel
		bool button(const char* id, const char* text);

		// Manual button (no layout)
		bool buttonRect(const char* id, const UIRect& rect, const char* text);

		// checkbox (label to right0 returns true if value changed this frame
		bool checkbox(const char* id, const char* label, bool& value);

		// a button that toggles a bool (returns true if toggled)
		bool toggleButton(const char* id, const char* text, bool& value);

		// sliders
		bool sliderFloat(const char* id, const char* label, float& value, float min, float max, float step = 0.0f);
		bool sliderInt(const char* id, const char* label, int& value, int min, int max);

		// expose mouse
		float mouseX() const { return m_mouseX; }
		float mouseY() const { return m_mouseY; }
		float wheelY() const { return m_wheelY; }

		// set render dependencies each frame (keeps UIContext lightweight)
		void bind(HBE::Renderer::Renderer2D* r2d, HBE::Renderer::DebugDraw2D* debug, HBE::Renderer::TextRenderer2D* text);

	private:
		struct PanelState {
			UIRect outer{};
			UIRect content{};
			float cursorY = 0.0f; // top-down cursor inside content
			float cursorX = 0.0f;
			// scroll bookkeeping
			std::uint32_t id = 0;
			float startCursorY = 0.0f;   // cursor position right after title reservation
			float scrollOffset = 0.0f;   // pixels the content is shifted UP by
			bool scrollable = false;
		};

		// Rendering
		HBE::Renderer::Renderer2D* m_r2d = nullptr;
		HBE::Renderer::DebugDraw2D* m_debug = nullptr;
		HBE::Renderer::TextRenderer2D* m_text = nullptr;

		UIStyle m_style{};

		// Input state (Logical coords)
		float m_mouseX = 0.0f;
		float m_mouseY = 0.0f;
		bool m_mouseInViewport = false;

		bool m_mouseDownL = false;
		bool m_mouseDownR = false;
		bool m_mouseDownM = false;

		bool m_mousePressedL = false;
		bool m_mouseReleasedL = false;

		bool m_mousePressedL_queued = false;
		bool m_mouseReleasedL_queued = false;

		float m_wheelY = 0.0f;

		// Immediate-mode interaction
		std::uint32_t m_hot = 0;
		std::uint32_t m_active = 0;

		// Persistent widget state keyed by hashed id
		std::unordered_map<std::uint32_t, bool> m_boolState;

		// Persistent scroll offset per scrollable panel id
		std::unordered_map<std::uint32_t, float> m_panelScroll;
		// Last-frame measured content height per scrollable panel id
		std::unordered_map<std::uint32_t, float> m_panelContentH;

		// Panel stack
		std::vector<PanelState> m_panels;

		// Utilities
		static std::uint32_t hashId(const char* s);
		bool isMouseDownPrimary() const { return m_mouseDownL; }

		// Draw Helpers
		void drawFilledRect(const UIRect& r, const HBE::Renderer::Color4& c);
		void drawBorderRect(const UIRect& r, const HBE::Renderer::Color4& c);

		bool buttonInternal(std::uint32_t id, const UIRect& rect, const char* text);
	};
}
