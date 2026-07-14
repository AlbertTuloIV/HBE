#include "HBE/Renderer/UI/UIContext.h"

#include <cstring>
#include <cstdio>
#include <cmath>

namespace HBE::Renderer::UI{

	// FNV-1a 32-bit
	std::uint32_t UIContext::hashId(const char* s) {
		if (!s) return 0;
		std::uint32_t h = 2166136261u;
		while (*s) {
			h ^= (unsigned char)(*s++);
			h *= 16777619u;
		}
		return h;
	}

	static float clampf(float v, float lo, float hi) {
		if (v < lo) return lo;
		if (v > hi) return hi;
		return v;
	}

	static float invLerp(float a, float b, float v) {
		if (a == b) return 0.0f;
		return (v - a) / (b - a);
	}

	static float lerp(float a, float b, float t) {
		return a + (b - a) * t;
	}

	static float snapStep(float v, float step) {
		if (step <= 0.0f) return v;
		return std::round(v / step) * step;
	}

	void UIContext::bind(HBE::Renderer::Renderer2D* r2d, HBE::Renderer::DebugDraw2D* debug, HBE::Renderer::TextRenderer2D* text) {
		m_r2d = r2d;
		m_debug = debug;
		m_text = text;
	}

	void UIContext::beginFrame(float /*dt*/) {
		m_hot = 0;

		// Consume queued edge events into per-frame flags
		m_mousePressedL = m_mousePressedL_queued;
		m_mouseReleasedL = m_mouseReleasedL_queued;

		// Clear queue for next frame
		m_mousePressedL_queued = false;
		m_mouseReleasedL_queued = false;

		// NOTE: do NOT clear m_wheelY here. SDL events are pumped BEFORE
		// beginFrame, so wheel deltas accumulated during the pump would be
		// discarded before any panel could consume them. We clear in endFrame.
		m_panels.clear();
	}

	void UIContext::endFrame() {
		// if primary mouse is up, release active
		if (!m_mouseDownL) {
			m_active = 0;
		}
		// clear wheel accumulator so next frame starts fresh
		m_wheelY = 0.0f;
	}

	void UIContext::onEvent(HBE::Core::Event& e) {
		using namespace HBE::Core;

		if (e.type() == EventType::MouseMoved) {
			auto& ev = static_cast<MouseMovedEvent&>(e);
			m_mouseX = ev.logicalX;
			m_mouseY = ev.logicalY;
			m_mouseInViewport = ev.inViewport;
		}
		else if (e.type() == EventType::MouseButtonPressed) {
			auto& ev = static_cast<MouseButtonPressedEvent&>(e);
			m_mouseX = ev.logicalX;
			m_mouseY = ev.logicalY;
			m_mouseInViewport = ev.inViewport;

			if (!m_mouseInViewport) return;

			if (ev.button == 1) { // Left
				m_mouseDownL = true;
				m_mousePressedL_queued = true;
			}
			else if (ev.button == 3) { m_mouseDownR = true; }
			else if (ev.button == 2) { m_mouseDownM = true; }
		}
		else if (e.type() == EventType::MouseButtonReleased) {
			auto& ev = static_cast<MouseButtonReleasedEvent&>(e);
			m_mouseX = ev.logicalX;
			m_mouseY = ev.logicalY;
			m_mouseInViewport = ev.inViewport;

			if (ev.button == 1) {
				m_mouseDownL = false;
				m_mouseReleasedL_queued = true;
			}
			else if (ev.button == 3) { m_mouseDownR = false; }
			else if (ev.button == 2) { m_mouseDownM = false; }
		}
		else if (e.type() == EventType::MouseScrolled) {
			auto& ev = static_cast<MouseScrolledEvent&>(e);
			m_mouseX = ev.logicalX;
			m_mouseY = ev.logicalY;
			m_mouseInViewport = ev.inViewport;

			if (!m_mouseInViewport) return;
			m_wheelY += ev.wheelY;
		}
	}

	void UIContext::drawFilledRect(const UIRect& r, const HBE::Renderer::Color4& c) {
		if (!m_r2d || !m_debug) return;
		const float cx = r.x + r.w * 0.5f;
		const float cy = r.y + r.h * 0.5f;
		m_debug->rect(*m_r2d, cx, cy, r.w, r.h, c.r, c.g, c.b, c.a, true);
	}

	void UIContext::drawBorderRect(const UIRect& r, const HBE::Renderer::Color4& c) {
		if (!m_r2d || !m_debug) return;

		const float t = 2.0f; // border thickness

		// top
		m_debug->rect(*m_r2d,
			r.x + r.w * 0.5f, r.y + r.h - t * 0.5f,
			r.w, t,
			c.r, c.g, c.b, c.a,
			true);

		// bottom
		m_debug->rect(*m_r2d,
			r.x + r.w * 0.5f, r.y + t * 0.5f,
			r.w, t,
			c.r, c.g, c.b, c.a,
			true);

		// left
		m_debug->rect(*m_r2d,
			r.x + t * 0.5f, r.y + r.h * 0.5f,
			t, r.h,
			c.r, c.g, c.b, c.a,
			true);

		// right
		m_debug->rect(*m_r2d,
			r.x + r.w - t * 0.5f, r.y + r.h * 0.5f,
			t, r.h,
			c.r, c.g, c.b, c.a,
			true);
	}


	bool UIContext::beginPanel(const char* id, const UIRect& rect, const char* title) {
		(void)id; // reserved (later: dragging, collapse, etc..)

		// Draw panel
		drawFilledRect(rect, m_style.panelBg);
		drawBorderRect(rect, m_style.panelBorder);

		PanelState p;
		p.outer = rect;
		p.content = rect;
		p.content.x += m_style.padding;
		p.content.y += m_style.padding;
		p.content.w -= m_style.padding * 2.0f;
		p.content.h -= m_style.padding * 2.0f;

		// top-down cursor inside content
		p.cursorX = p.content.x;
		p.cursorY = p.content.y + p.content.h; // top

		m_panels.push_back(p);

		if (title && title[0] && m_text && m_r2d) {
			// title near top-left inside panel
			float tx = p.content.x;
			float ty = (p.content.y + p.content.h) - 2.0f; // baseline ish
			m_text->drawTextAligned(*m_r2d, tx, ty, title,
				m_style.textScale, m_style.text,
				HBE::Renderer::TextRenderer2D::TextAlignH::Left, HBE::Renderer::TextRenderer2D::TextAlignV::Baseline);

			// reserve space
			p.cursorY -= (m_style.itemH * 0.8f);
			m_panels.back() = p;
		}
		// record starting cursor for content-height measurement
		m_panels.back().startCursorY = m_panels.back().cursorY;
		return true;
	}

	bool UIContext::beginScrollPanel(const char* id, const UIRect& rect, const char* title) {
		// share layout with beginPanel, then apply persistent scroll offset
		beginPanel(id, rect, title);

		if (!id || !id[0]) return true;
		const std::uint32_t hid = hashId(id);

		auto& p = m_panels.back();
		p.id = hid;
		p.scrollable = true;

		auto it = m_panelScroll.find(hid);
		float offset = (it == m_panelScroll.end()) ? 0.0f : it->second;

		// pre-clamp against last-frame content height (if we have one)
		auto ith = m_panelContentH.find(hid);
		if (ith != m_panelContentH.end()) {
			float maxScroll = ith->second - p.content.h;
			if (maxScroll < 0.0f) maxScroll = 0.0f;
			if (offset < 0.0f) offset = 0.0f;
			if (offset > maxScroll) offset = maxScroll;
		}
		else if (offset < 0.0f) {
			offset = 0.0f;
		}

		p.scrollOffset = offset;
		// shift ALL subsequent widgets UP by offset (they'll be drawn higher off-panel)
		// i.e., raise the starting cursorY so content appears further down in the stack
		p.cursorY += offset;
		p.startCursorY = p.cursorY;
		return true;
	}

	void UIContext::endPanel() {
		if (m_panels.empty()) return;

		PanelState p = m_panels.back();

		if (p.scrollable && p.id != 0) {
			// measure content stack height (independent of scrollOffset shift)
			float contentH = p.startCursorY - p.cursorY;
			if (contentH < 0.0f) contentH = 0.0f;
			m_panelContentH[p.id] = contentH;

			// consume wheel if mouse is over the panel outer rect
			const bool hovered = m_mouseInViewport && p.outer.contains(m_mouseX, m_mouseY);
			if (hovered && m_wheelY != 0.0f) {
				const float step = m_style.itemH * 1.25f; // pixels per wheel notch
				float newOffset = p.scrollOffset - m_wheelY * step;

				float maxScroll = contentH - p.content.h;
				if (maxScroll < 0.0f) maxScroll = 0.0f;
				if (newOffset < 0.0f) newOffset = 0.0f;
				if (newOffset > maxScroll) newOffset = maxScroll;

				m_panelScroll[p.id] = newOffset;
				m_wheelY = 0.0f; // consume so world-camera zoom doesn't also react
			}
			else {
				// keep stored offset clamped even when not scrolling this frame
				float maxScroll = contentH - p.content.h;
				if (maxScroll < 0.0f) maxScroll = 0.0f;
				float clamped = p.scrollOffset;
				if (clamped < 0.0f) clamped = 0.0f;
				if (clamped > maxScroll) clamped = maxScroll;
				m_panelScroll[p.id] = clamped;
			}
		}

		m_panels.pop_back();
	}

	void UIContext::label(const char* text, bool muted) {
		if (!text) return;
		if (m_panels.empty() || !m_text || !m_r2d) return;

		auto& p = m_panels.back();
		const float x = p.cursorX;
		const float yTop = p.cursorY;

		// draw label
		m_text->drawTextAligned(*m_r2d, x, yTop, text,
			m_style.textScale, muted ? m_style.textMuted : m_style.text,
			HBE::Renderer::TextRenderer2D::TextAlignH::Left,
			HBE::Renderer::TextRenderer2D::TextAlignV::Baseline);

		// Advance cursor
		p.cursorY -= (m_style.itemH * 0.7f);
	}

	bool UIContext::button(const char* id, const char* text) {
		if (m_panels.empty()) return false;
		auto& p = m_panels.back();

		// build rect using top-down layout
		UIRect r;
		r.x = p.cursorX;
		r.w = p.content.w;
		r.h = m_style.itemH;
		r.y = p.cursorY - r.h; // bc cursrY is top

		// advance cursor
		p.cursorY -= (r.h + m_style.spacing);

		return buttonRect(id, r, text);
	}

	bool UIContext::buttonRect(const char* id, const UIRect& rect, const char* text) {
		if (!id) return false;
		const std::uint32_t hid = hashId(id);
		return buttonInternal(hid, rect, text);
	}

	bool UIContext::buttonInternal(std::uint32_t id, const UIRect& rect, const char* text) {
		if (!m_r2d || !m_debug || !m_text) return false;

		// only interact if mouse is inside viewport
		const bool hovered = m_mouseInViewport && rect.contains(m_mouseX, m_mouseY);

		if (hovered) m_hot = id;

		// Primary press begins active
		if (hovered && m_mousePressedL) {
			m_active = id;
		}

		bool clicked = false;

		// release triggers click if still hovered and was active
		if (m_mouseReleasedL) {
			if (hovered && m_active == id) {
				clicked = true;
			}
		}

		// pick color by state
		HBE::Renderer::Color4 bg = m_style.btnIdle;
		if (hovered) bg = m_style.btnHover;
		if (m_active == id && isMouseDownPrimary()) bg = m_style.btnDown;

		drawFilledRect(rect, bg);
		drawBorderRect(rect, m_style.btnBorder);

		// Measure text block
		auto layout = m_text->measureText(text, m_style.textScale, 0.0f);

		// Center X (we can still use drawTextAligned for horizontal centering)
		const float cx = rect.x + rect.w * 0.5f;

		// Compute baseline Y for vertical centering.
		// drawTextAligned expects y to be a BASELINE when alignV == Baseline.
		float approxLineH = (layout.lineCount > 0) ? (layout.height / (float)layout.lineCount) : layout.height;
		float baselineFudge = approxLineH * m_style.baselineFudgeMul;  // tweak in UIStyle.h: baselineFudgeMul

		const float baselineY = rect.y + (rect.h + layout.height) * 0.5f - baselineFudge;

		m_text->drawTextAligned(*m_r2d,
			cx, baselineY,
			text,
			m_style.textScale,
			m_style.text,
			HBE::Renderer::TextRenderer2D::TextAlignH::Center,
			HBE::Renderer::TextRenderer2D::TextAlignV::Baseline,
			0.0f,
			1.25f);



		return clicked;
	}

	bool UIContext::checkbox(const char* id, const char* label, bool& value) {
		if (m_panels.empty() || !m_r2d || !m_debug || !m_text) return false;

		auto& p = m_panels.back();

		// Layout row (full width, one item tall)
		UIRect row;
		row.x = p.cursorX;
		row.w = p.content.w;
		row.h = m_style.itemH;
		row.y = p.cursorY - row.h;

		// advance cursor
		p.cursorY -= (row.h + m_style.spacing);

		// checkbox square
		const float boxSize = row.h * 0.60f;
		const float boxPadY = (row.h - boxSize) * 0.5f;

		UIRect box;
		box.x = row.x;
		box.y = row.y + boxPadY;
		box.w = boxSize;
		box.h = boxSize;

		// label rect to the right
		const float labelPadX = 10.0f;

		UIRect labelRect;
		labelRect.x = box.x + box.w + labelPadX;
		labelRect.y = row.y;
		labelRect.w = row.w - (box.w + labelPadX);
		labelRect.h = row.h;

		const std::uint32_t hid = hashId(id);

		// seed persistent state once
		auto it = m_boolState.find(hid);
		if (it == m_boolState.end()) {
			m_boolState[hid] = value;
		}
		else {
			value = it->second; // keep caller value synced.
		}

		// Interaction over whole row
		const bool hovered = m_mouseInViewport && row.contains(m_mouseX, m_mouseY);
		if (hovered) m_hot = hid;

		if (hovered && m_mousePressedL) {
			m_active = hid;
		}

		bool changed = false;
		if (m_mouseReleasedL) {
			if (hovered && m_active == hid) {
				value = !value;
				m_boolState[hid] = value;
				changed = true;
			}
		}

		// draw row hover highlight (subtle)
		if (hovered) {
			drawFilledRect(row, HBE::Renderer::Color4{ 0.22f, 0.22f, 0.27f, 0.35f });
		}

		// Draw Box
		drawFilledRect(box, HBE::Renderer::Color4{ 0.12f, 0.12f, 0.16f, 0.95f });
		drawBorderRect(box, m_style.btnBorder);

		// fill inner if checked
		if (value) {
			const float inset = 4.0f;
			UIRect inner;
			inner.x = box.x + inset;
			inner.y = box.y + inset;
			inner.w = box.w - inset * 2.0f;
			inner.h = box.h - inset * 2.0f;

			// green check fill 
			drawFilledRect(inner, HBE::Renderer::Color4{ 0.20f, 0.85f, 0.35f, 1.0f });
		}

		// Draw label (vertically centered baseline like button)
		auto layout = m_text->measureText(label, m_style.textScale, 0.0f);

		float approxLineH = (layout.lineCount > 0) ? (layout.height / (float)layout.lineCount) : layout.height;
		float baselineFudge = approxLineH * m_style.baselineFudgeMul;
		const float baselineY = labelRect.y + (labelRect.h + layout.height) * 0.5f - baselineFudge;

		m_text->drawTextAligned(*m_r2d, labelRect.x, baselineY, label,
			m_style.textScale, m_style.text,
			HBE::Renderer::TextRenderer2D::TextAlignH::Left,
			HBE::Renderer::TextRenderer2D::TextAlignV::Baseline,
			0.0f,
			1.25f);

		return changed;
	}

	bool UIContext::toggleButton(const char* id, const char* text, bool& value) {
		const std::uint32_t hid = hashId(id);

		// seed / sync state
		auto it = m_boolState.find(hid);
		if (it == m_boolState.end()) {
			m_boolState[hid] = value;
		}
		else {
			value = it->second;
		}

		// use normal button interaction
		bool pressed = button(id, text);
		if (pressed) {
			value = !value;
			m_boolState[hid] = value;
			return true;
		}
		return false;
	}

	bool UIContext::sliderFloat(const char* id, const char* label, float& value, float min, float max, float step) {
		if (m_panels.empty() || !m_r2d || !m_debug || !m_text) return false;
		if (max <= min) return false;

		auto& p = m_panels.back();

		// Layout row
		UIRect row;
		row.x = p.cursorX;
		row.w = p.content.w;
		row.h = m_style.itemH;
		row.y = p.cursorY - row.h;

		p.cursorY -= (row.h + m_style.spacing);

		// split row: label left, slider track right
		const float labelW = row.w * 0.42f;
		const float gap = 10.0f;

		UIRect labelRect{ row.x, row.y, labelW, row.h };

		UIRect trackRect;
		trackRect.x = row.x + labelW + gap;
		trackRect.y = row.y + row.h * 0.35f;
		trackRect.w = row.w - labelW - gap;
		trackRect.h = row.h * 0.30f;

		// Interaction area should be bigger than the thin track
		UIRect hitRect;
		hitRect.x = trackRect.x;
		hitRect.y = row.y;
		hitRect.w = trackRect.w;
		hitRect.h = row.h;

		const std::uint32_t hid = hashId(id);

		// Seed persistent state
		auto it = m_boolState.find(hid);

		bool hovered = m_mouseInViewport && hitRect.contains(m_mouseX, m_mouseY);
		if (hovered) m_hot = hid;

		// capture on press
		if (hovered && m_mousePressedL) {
			m_active = hid;
		}

		bool changed = false;

		// While active and mouse held, update value based on mouseX
		if (m_active == hid && m_mouseDownL && m_mouseInViewport) {
			float t = invLerp(trackRect.x, trackRect.x + trackRect.w, m_mouseX);
			t = clampf(t, 0.0f, 1.0f);

			float newV = lerp(min, max, t);
			newV = snapStep(newV, step);
			newV = clampf(newV, min, max);

			if (newV != value) {
				value = newV;
				changed = true;
			}
		}

		 // draw label
		{
			auto layout = m_text->measureText(label, m_style.textScale, 0.0f);
			float approxLineH = (layout.lineCount > 0) ? (layout.height / (float)layout.lineCount) : layout.height;
			float baseLineY = labelRect.y + (labelRect.h + layout.height) * 0.5f - (approxLineH * m_style.baselineFudgeMul);

			m_text->drawTextAligned(*m_r2d, labelRect.x, baseLineY, label, m_style.textScale, m_style.textMuted,
				TextRenderer2D::TextAlignH::Left,
				TextRenderer2D::TextAlignV::Baseline,
				0.0f,
				1.25f);
		}

		// track
		drawFilledRect(trackRect, m_style.sliderTrack);
		drawBorderRect(trackRect, m_style.btnBorder);

		// fill + knob
		float tVal = invLerp(min, max, value);
		tVal = clampf(tVal, 0.0f, 1.0f);

		UIRect fill = trackRect;
		fill.w = trackRect.w * tVal;

		if (fill.w > 0.0f) drawFilledRect(fill, m_style.sliderFill);

		// knob is a small rect
		const float knobW = row.h * 0.22f;
		const float knobH = row.h * 0.55f;
		float knobX = trackRect.x + trackRect.w * tVal - knobW * 0.5f;

		UIRect knob;
		knob.x = knobX;
		knob.y = row.y + (row.h - knobH) * 0.5f;
		knob.w = knobW;
		knob.h = knobH;

		bool knobHover = m_mouseInViewport && hitRect.contains(m_mouseX, m_mouseY);
		drawFilledRect(knob, knobHover ? m_style.sliderKnobHover : m_style.sliderKnob);
		drawBorderRect(knob, m_style.btnBorder);

		// value text on the far right of the row
		{
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%.2f", value);

			UIRect valueRect;
			valueRect.x = trackRect.x;
			valueRect.y = row.y;
			valueRect.w = trackRect.w;
			valueRect.h = row.h;

			auto layout = m_text->measureText(buf, m_style.textScale, 0.0f);
			float approxLineH = (layout.lineCount > 0) ? (layout.height / (float)layout.lineCount) : layout.height;
			float baselineY = valueRect.y + (valueRect.h + layout.height) * 0.5f - (approxLineH * m_style.baselineFudgeMul);

			m_text->drawTextAligned(*m_r2d, valueRect.x + valueRect.w, baselineY, buf, m_style.textScale, m_style.text,
				TextRenderer2D::TextAlignH::Right,
				TextRenderer2D::TextAlignV::Baseline,
				0.0f,
				1.25f);
		}
		
		return changed;
	}

	bool UIContext::sliderInt(const char* id, const char* label, int& value, int min, int max) {
		float v = (float)value;
		bool changed = sliderFloat(id, label, v, (float)min, (float)max, 1.0f);
		int iv = (int)std::round(v);
		iv = (iv < min) ? min : (iv > max ? max : iv);

		if (iv != value) {
			value = iv;
			return true;
		}
		return changed;
	}

	void UIContext::spacing(float h) {
		if (m_panels.empty()) return;
		m_panels.back().cursorY -= h;
	}

}