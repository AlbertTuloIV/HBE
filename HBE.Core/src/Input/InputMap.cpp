#include "HBE/Core/Log.h"

#include "HBE/Input/InputMap.h"
#include "HBE/Platform/Input.h"

#include <fstream>
#include <cmath>
#include <algorithm>
#include <cassert>

namespace HBE::Input {

	static InputMap* g_map = nullptr;

	static inline int AxisKey(SDL_GamepadAxis a) {
		return static_cast<int>(a);
	}

	static inline float ApplyDeadzone(float v, float dz) {
		if (std::fabs(v) < dz) return 0.0f;
		const float sign = (v < 0.0f) ? -1.0f : 1.0f;
		const float mag = (std::fabs(v) - dz) / (1.0f - dz);
		return sign * std::clamp(mag, 0.0f, 1.0f);
	}

	// ---------------- InputMap ----------------
	void InputMap::clear() {
		m_actions.clear();
		m_axes.clear();
		m_prevAxis.clear();
		m_currAxis.clear();
		m_rebinding = false;
	}

	void InputMap::newFrame() {
		// carry current -> prev
		m_prevAxis = m_currAxis;

		// recompute only the axes used by threshold bindings
		m_currAxis.clear();

		for (const auto& [act, ab] : m_actions) {
			const Binding* bs[2] = { &ab.primary, &ab.secondary };
			for (auto* b : bs) {
				if (b->type == Binding::Type::GamepadAxisThreshold) {
					const int k = AxisKey(b->padAxis);
					m_currAxis[k] = Platform::Input::GetGamepadAxis(b->padAxis);
				}
			}
		}
	}

	void InputMap::handleEvent(const SDL_Event& e) {
		if (!m_rebinding) return;

		Binding captured = Binding::None();
		bool gotOne = false;

		switch (e.type) {
		case SDL_EVENT_KEY_DOWN:
			if (e.key.scancode != SDL_SCANCODE_UNKNOWN) {
				captured = Binding::Key(e.key.scancode);
				gotOne = true;
			}
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if (e.button.button != 0) {
				captured = Binding::Mouse(static_cast<int>(e.button.button));
				gotOne = true;
			}
			break;

		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			captured = Binding::GamepadButton(static_cast<SDL_GamepadButton>(e.gbutton.button));
			gotOne = true;
			break;

		case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
			// bind axis threshold if pushed strongly
			const float raw = static_cast<float>(e.gaxis.value);

			float norm = 0.0f;
			if (raw >= 0.0f) norm = raw / 32767.0f;
			else norm = raw / 32768.0f;

			norm = std::clamp(norm, -1.0f, 1.0f);

			if (std::fabs(norm) >= 0.7f) {
				const int sign = (norm < 0.0f) ? -1 : +1;
				captured = Binding::GamepadAxis(static_cast<SDL_GamepadAxis>(e.gaxis.axis), 0.5f, sign);
				gotOne = true;
			}
			break;
		}

		default:
			break;
		}

		if (gotOne) {
			bindAction(m_rebindAction, captured, m_rebindPrimary);
			cancelRebind();
		}
	}

	void InputMap::beginRebind(Action a, bool primary) {
		m_rebinding = true;
		m_rebindPrimary = primary;
		m_rebindAction = a;
	}

	void InputMap::cancelRebind() {
		m_rebinding = false;
	}

	void InputMap::bindAction(Action a, const Binding& b, bool primary) {
		auto& ab = m_actions[a];
		if (primary) ab.primary = b;
		else ab.secondary = b;
	}

	void InputMap::bindAxis(Axis ax, const AxisBinding& b) {
		m_axes[ax] = b;
	}

	const ActionBinding& InputMap::getActionBinding(Action a) const {
		static ActionBinding empty{};
		auto it = m_actions.find(a);
		return (it != m_actions.end()) ? it->second : empty;
	}

	const AxisBinding& InputMap::getAxisBinding(Axis ax) const {
		static AxisBinding empty{};
		auto it = m_axes.find(ax);
		return (it != m_axes.end()) ? it->second : empty;
	}

	bool InputMap::bindingDown(const Binding& b) const {
		using Platform::Input::MouseButton;

		switch (b.type) {
		case Binding::Type::Key:
			return Platform::Input::IsKeyDown(b.key);

		case Binding::Type::MouseButton:
			return Platform::Input::IsMouseDown(static_cast<MouseButton>(b.mouseButton));

		case Binding::Type::GamepadButton:
			return Platform::Input::IsGamepadButtonDown(b.padButton);

		case Binding::Type::GamepadAxisThreshold: {
			const float v = currentAxisForThresholdBinding(b);
			if (b.axisSign == 0) return std::fabs(v) >= b.axisThreshold;
			if (b.axisSign < 0) return v <= -b.axisThreshold;
			return v >= b.axisThreshold;
		}

		default:
			return false;
		}
	}

	bool InputMap::bindingPressed(const Binding& b) const {
		using Platform::Input::MouseButton;

		switch (b.type) {
		case Binding::Type::Key:
			return Platform::Input::IsKeyPressed(b.key);

		case Binding::Type::MouseButton:
			return Platform::Input::IsMousePressed(static_cast<MouseButton>(b.mouseButton));

		case Binding::Type::GamepadButton:
			return Platform::Input::IsGamepadButtonPressed(b.padButton);

		case Binding::Type::GamepadAxisThreshold: {
			const float cur = currentAxisForThresholdBinding(b);
			const float prev = prevAxisForThresholdBinding(b);

			const bool now =
				(b.axisSign == 0) ? (std::fabs(cur) >= b.axisThreshold)
				: (b.axisSign < 0) ? (cur <= -b.axisThreshold)
				: (cur >= b.axisThreshold);

			const bool was =
				(b.axisSign == 0) ? (std::fabs(prev) >= b.axisThreshold)
				: (b.axisSign < 0) ? (prev <= -b.axisThreshold)
				: (prev >= b.axisThreshold);

			return now && !was;
		}

		default:
			return false;
		}
	}

	bool InputMap::bindingReleased(const Binding& b) const {
		using Platform::Input::MouseButton;

		switch (b.type) {
		case Binding::Type::Key:
			return Platform::Input::IsKeyReleased(b.key);

		case Binding::Type::MouseButton:
			return Platform::Input::IsMouseReleased(static_cast<MouseButton>(b.mouseButton));

		case Binding::Type::GamepadButton:
			return Platform::Input::IsGamepadButtonReleased(b.padButton);

		case Binding::Type::GamepadAxisThreshold: {
			const float cur = currentAxisForThresholdBinding(b);
			const float prev = prevAxisForThresholdBinding(b);

			const bool now =
				(b.axisSign == 0) ? (std::fabs(cur) >= b.axisThreshold)
				: (b.axisSign < 0) ? (cur <= -b.axisThreshold)
				: (cur >= b.axisThreshold);

			const bool was =
				(b.axisSign == 0) ? (std::fabs(prev) >= b.axisThreshold)
				: (b.axisSign < 0) ? (prev <= -b.axisThreshold)
				: (prev >= b.axisThreshold);

			return !now && was;
		}

		default:
			return false;
		}
	}

	float InputMap::currentAxisForThresholdBinding(const Binding& b) const {
		if (b.type != Binding::Type::GamepadAxisThreshold) return 0.0f;

		const int k = AxisKey(b.padAxis);
		auto it = m_currAxis.find(k);
		if (it != m_currAxis.end()) return it->second;

		return Platform::Input::GetGamepadAxis(b.padAxis);
	}

	float InputMap::prevAxisForThresholdBinding(const Binding& b) const {
		if (b.type != Binding::Type::GamepadAxisThreshold) return 0.0f;

		const int k = AxisKey(b.padAxis);
		auto it = m_prevAxis.find(k);
		if (it != m_prevAxis.end()) return it->second;

		return 0.0f;
	}

	float InputMap::axisValueForBinding(const AxisBinding& b) const {
		float digital = 0.0f;

		if (bindingDown(b.negative) || bindingDown(b.negative2)) digital -= 1.0f;
		if (bindingDown(b.positive) || bindingDown(b.positive2)) digital += 1.0f;

		float analog = 0.0f;
		if (b.useGamepadAxis && Platform::Input::HasGamepad() && b.gamepadAxis != SDL_GAMEPAD_AXIS_INVALID) {
			analog = Platform::Input::GetGamepadAxis(b.gamepadAxis);
			analog = ApplyDeadzone(analog, b.deadzone);
			if (b.invert) analog *= -1.0f;
			analog *= b.scale;
			analog = std::clamp(analog, -1.0f, 1.0f);
		}

		return (std::fabs(analog) > std::fabs(digital)) ? analog : digital;
	}

	bool InputMap::actionDown(Action a) const {
		const auto& ab = getActionBinding(a);
		return bindingDown(ab.primary) || bindingDown(ab.secondary);
	}

	bool InputMap::actionPressed(Action a) const {
		const auto& ab = getActionBinding(a);
		return bindingPressed(ab.primary) || bindingPressed(ab.secondary);
	}

	bool InputMap::actionReleased(Action a) const {
		const auto& ab = getActionBinding(a);
		return bindingReleased(ab.primary) || bindingReleased(ab.secondary);
	}

	float InputMap::axis(Axis ax) const {
		const auto& b = getAxisBinding(ax);
		return axisValueForBinding(b);
	}

	// ---------------- Persistence (stub-safe) ----------------
	bool InputMap::saveToFile(const std::string& path) const {
		// If you already wrote a serializer, keep it.
		// For now, this is a stub that creates a file to prove the workflow.
		std::ofstream out(path);
		if (!out) return false;

		out << "# bindings.cfg (stub)\n";
		out << "# TODO: implement full serialization\n";
		return true;
	}

	bool InputMap::loadFromFile(const std::string& path) {
		// Stub: just check existence.
		std::ifstream in(path);
		if (!in) return false;
		return true;
	}

	const char* InputMap::actionName(Action a) {
		switch (a) {
		case Action::Jump: return "Jump";
		case Action::Attack: return "Attack";
		case Action::UIConfirm: return "UIConfirm";
		case Action::UICancel: return "UICancel";
		case Action::Pause: return "Pause";
		case Action::FullscreenToggle: return "FullscreenToggle";
		default: return "Unknown";
		}
	}

	const char* InputMap::axisName(Axis a) {
		switch (a) {
		case Axis::MoveX: return "MoveX";
		case Axis::MoveY: return "MoveY";
		default: return "Unknown";
		}
	}

	// ---------------- Global API ----------------
	void Initialize(DefaultBindingsFn defaultsProvider) {
		if (!g_map) g_map = new InputMap();
		g_map->clear();

		if (defaultsProvider) {
			defaultsProvider(*g_map);
		}
	}

	void Shutdown() {
		delete g_map;
		g_map = nullptr;
	}

	void NewFrame() {
		if (g_map) g_map->newFrame();
	}

	void HandleEvent(const SDL_Event& e) {
		if (g_map) g_map->handleEvent(e);
	}

	bool ActionDown(Action a) {
		return g_map ? g_map->actionDown(a) : false;
	}

	bool ActionPressed(Action a) {
		return g_map ? g_map->actionPressed(a) : false;
	}

	bool ActionReleased(Action a) {
		return g_map ? g_map->actionReleased(a) : false;
	}

	float AxisValue(Axis a) {
		return g_map ? g_map->axis(a) : 0.0f;
	}

	void BeginRebind(Action a, bool primary) {
		if (g_map) g_map->beginRebind(a, primary);
	}

	void CancelRebind() {
		if (g_map) g_map->cancelRebind();
	}

	bool IsRebinding() {
		return g_map ? g_map->isRebinding() : false;
	}

	InputMap& Get() {
		if (!g_map) {
			HBE::Core::LogFatal("HBE::Input::Get() called before Initialize() or after Shutdown()");
			assert(false && "HBE::Input::Get() called before Initialize()");
		}
		return *g_map;
	}

} // namespace HBE::Input