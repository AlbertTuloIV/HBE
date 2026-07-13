#include "HBE/Core/Log.h"

#include "HBE/Input/InputMap.h"
#include "HBE/Platform/Input.h"

#include <json.hpp>

#include <fstream>
#include <cmath>
#include <algorithm>
#include <cassert>

namespace HBE::Input {

	static InputMap* g_map = nullptr;

	static constexpr int kInputConfigVersion = 1;

	namespace {

		using nlohmann::json;

		const char* bindingTypeToString(Binding::Type t) {
			switch (t) {
			case Binding::Type::None: return "None";
			case Binding::Type::Key: return "Key";
			case Binding::Type::MouseButton: return "MouseButton";
			case Binding::Type::GamepadButton: return "GamepadButton";
			case Binding::Type::GamepadAxisThreshold: return "GamepadAxisThreshold";
			}
			return "None";
		}

		bool stringToBindingType(const std::string& s, Binding::Type& out) {
			if (s == "None") { out = Binding::Type::None; return true; }
			if (s == "Key") { out = Binding::Type::Key; return true; }
			if (s == "MouseButton") { out = Binding::Type::MouseButton; return true; }
			if (s == "GamepadButton") { out = Binding::Type::GamepadButton; return true; }
			return false;
		}

		std::string scancodeToString(SDL_Scancode sc) {
			if (sc == SDL_SCANCODE_UNKNOWN) return "Unknown";
			const char* n = SDL_GetScancodeName(sc);
			return (n && *n) ? std::string(n) : std::string("Unknown");
		}

		SDL_Scancode stringToScancode(const std::string& name) {
			if (name.empty() || name == "Unkown") return SDL_SCANCODE_UNKNOWN;
			return SDL_GetScancodeFromName(name.c_str());
		}

		std::string gamepadButtonToString(SDL_GamepadButton b) {
			if (b == SDL_GAMEPAD_BUTTON_INVALID) return "Invalid";
			const char* n = SDL_GetGamepadStringForButton(b);
			return(n && *n) ? std::string(n) : std::string("Invalid");
		}

		SDL_GamepadButton stringToGamepadButton(const std::string& name) {
			if (name.empty() || name == "Invalid") return SDL_GAMEPAD_BUTTON_INVALID;
			return SDL_GetGamepadButtonFromString(name.c_str());
		}

		std::string gamepadAxisToString(SDL_GamepadAxis a) {
			if (a == SDL_GAMEPAD_AXIS_INVALID) return "Invalid";
			const char* n = SDL_GetGamepadStringForAxis(a);
			return (n && *n) ? std::string(n) : std::string("Invalid");
		}

		SDL_GamepadAxis stringToGamepadAxis(const std::string& name) {
			if (name.empty() || name == "Invalid") return SDL_GAMEPAD_AXIS_INVALID;
			return SDL_GetGamepadAxisFromString(name.c_str());
		}

		bool stringToAction(const std::string& name, Action& out) {
			if (name == "Jump") { out = Action::Jump; return true; }
			if (name == "Attack") { out = Action::Attack; return true; }
			if (name == "UIConfirm") { out = Action::UIConfirm; return true; }
			if (name == "UICancel") { out = Action::UICancel; return true; }
			if (name == "Pause") { out = Action::Pause; return true; }
			if (name == "FullscreenToggle") { out = Action::FullscreenToggle; return true; }
		}

		bool stringToAxis(const std::string& name, Axis& out) {
			if (name == "MoveX") { out = Axis::MoveX; return true; }
			if (name == "MoveY") { out = Axis::MoveY; return true; }
		}

		json toJsonBinding(const Binding& b) {
			json j;
			j["type"] = bindingTypeToString(b.type);
			switch (b.type) {
			case Binding::Type::None:
				break;
			case Binding::Type::Key:
				j["key"] = scancodeToString(b.key);
				break;
			case Binding::Type::MouseButton:
				j["button"] = b.mouseButton;
				break;
			case Binding::Type::GamepadButton:
				j["padButton"] = gamepadButtonToString(b.padButton);
				break;
			case Binding::Type::GamepadAxisThreshold:
				j["padAxis"] = gamepadAxisToString(b.padAxis);
				j["threshold"] = b.axisThreshold;
				j["sign"] = b.axisSign;
				break;
			}
			return j;
		}

		bool fromJsonBinding(const json& j, Binding& out) {
			out = Binding::None();
			if (!j.is_object()) return false;

			const auto itType = j.find("type");
			if (itType == j.end() || !itType->is_string()) return false;

			Binding::Type t = Binding::Type::None;
			if (!stringToBindingType(itType->get<std::string>(), t)) return false;

			switch (t) {
			case Binding::Type::None:
				out = Binding::None();
				return true;
			case Binding::Type::Key: {
				const auto itKey = j.find("key");
				if (itKey == j.end() || !itKey->is_string()) return false;
				const SDL_Scancode sc = stringToScancode(itKey->get<std::string>());
				if (sc == SDL_SCANCODE_UNKNOWN) return false;
				out = Binding::Key(sc);
				return true;
			}
			case Binding::Type::MouseButton: {
				const auto itBtn = j.find("button");
				if (itBtn == j.end() || !itBtn->is_number_integer()) return false;
				out = Binding::Mouse(itBtn->get<int>());
				return true;
			}

			case Binding::Type::GamepadButton: {
				const auto itBtn = j.find("padButton");
				if (itBtn == j.end() || !itBtn->is_string()) return false;
				const SDL_GamepadButton gb = stringToGamepadButton(itBtn->get<std::string>());
				if (gb == SDL_GAMEPAD_BUTTON_INVALID) return false;
				out = Binding::GamepadButton(gb);
				return true;
			}
			case Binding::Type::GamepadAxisThreshold: {
				const auto itAxis = j.find("padAxis");
				if (itAxis == j.end() || !itAxis->is_string()) return false;
				const SDL_GamepadAxis ga = stringToGamepadAxis(itAxis->get<std::string>());
				if (ga == SDL_GAMEPAD_AXIS_INVALID) return false;

				float thr = 0.5f;
				float sign = 0;
				if (const auto itThr = j.find("threshold"); itThr != j.end() && itThr->is_number()) thr = itThr->get<float>();
				if (const auto itSign = j.find("sign"); itSign != j.end() && itSign->is_number_integer()) sign = itSign->get<int>();
				out = Binding::GamepadAxis(ga, thr, sign);
				return true;
			}
			}
			return false;
		}

		json toJsonActionBinding(const ActionBinding& ab) {
			json j;
			j["primary"] = toJsonBinding(ab.primary);
			j["secondary"] = toJsonBinding(ab.secondary);
			return j;
		}

		void fromJsonActionBinding(const json& j, ActionBinding& out) {
			if (!j.is_object()) return;
			if (const auto it = j.find("primary"); it != j.end()) {
				Binding b; if (fromJsonBinding(*it, b)) out.primary = b;
			}
			if (const auto it = j.find("secondary"); it != j.end()) {
				Binding b; if (fromJsonBinding(*it, b)) out.secondary = b;
			}
		}

		json toJsonAxisBinding(const AxisBinding& ab) {
			json j;
			j["negative"] = toJsonBinding(ab.negative);
			j["positive"] = toJsonBinding(ab.positive);
			j["negative2"] = toJsonBinding(ab.negative2);
			j["positive2"] = toJsonBinding(ab.positive2);
			j["useGamepadAxis"] = ab.useGamepadAxis;
			j["gamepadAxis"] = gamepadAxisToString(ab.gamepadAxis);
			j["deadzone"] = ab.deadzone;
			j["invert"] = ab.invert;
			j["scale"] = ab.scale;
			return j;
		}

		void fromJsonAxisBinding(const json& j, AxisBinding& out) {
			if (!j.is_object()) return;
			auto tryBinding = [&](const char* key, Binding& dst) {
				const auto it = j.find(key);
				if (it == j.end()) return;
				Binding b;
				if (fromJsonBinding(*it, b)) dst = b;
				};
			tryBinding("negative", out.negative);
			tryBinding("positive", out.positive);
			tryBinding("negative2", out.negative2);
			tryBinding("positive2", out.positive2);

			if (const auto it = j.find("useGamepadAxis"); it != j.end() && it->is_boolean()) out.useGamepadAxis = it->get<bool>();
			if (const auto it = j.find("gamepadAxis"); it != j.end() && it->is_string()) out.gamepadAxis = stringToGamepadAxis(it->get<std::string>());
			if (const auto it = j.find("deadzone"); it != j.end() && it->is_number()) out.deadzone = it->get<float>();
			if (const auto it = j.find("invert"); it != j.end() && it->is_boolean()) out.invert = it->get<bool>();
			if (const auto it = j.find("scale"); it != j.end() && it->is_number()) out.scale = it->get<float>();
		}
	}

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
		using nlohmann::json;

		json root;
		root["version"] = kInputConfigVersion;

		json actionsJson = json::object();
		for (const auto& [action, binding] : m_actions) {
			actionsJson[InputMap::actionName(action)] = toJsonActionBinding(binding);
		}
		root["action"] = std::move(actionsJson);

		json axesJson = json::object();
		for (const auto& [axis, binding] : m_axes) {
			axesJson[InputMap::axisName(axis)] = toJsonAxisBinding(binding);
		}
		root["axes"] = std::move(axesJson);

		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out) {
			HBE::Core::LogError("InputMap::saveToFile: failed to open '" + path + "' for write.");
			return false;
		}

		try {
			out << root.dump(2);
		}
		catch (const std::exception& e) {
			HBE::Core::LogError(std::string("InputMap::saveToFile: json.dump thre: ") + e.what());
			return false;
		}

		if (!out.good()) {
			HBE::Core::LogError("InputMap::saveToFile: stream error while writing '" + path + "'.");
			return false;
		}

		HBE::Core::LogInfo("InputMap saved: " + path);
		return true;
	}

	bool InputMap::loadFromFile(const std::string& path) {
		using nlohmann::json;

		std::ifstream in(path, std::ios::binary);
		if (!in) {
			HBE::Core::LogInfo("InputMap::loadFromFile: no bindings file at '" + path + "' - keeping in-memory defaults.");
			return false;
		}
		json root;
		try {
			in >> root;
		}
		catch (const std::exception& e) {
			HBE::Core::LogWarn(std::string("InputMap::loadFromFile: parse error, keeping defaults. (") + e.what() + ")");
		}

		if (!root.is_object()) {
			HBE::Core::LogWarn("InputMap::loadFromFile: file is not a JSON object, keeping defaults.");
			return false;
		}

		int version = 0;
		if (const auto it = root.find("version"); it != root.end() && it->is_number_integer()) {
			version = it->get<int>();
		}
		if (version > kInputConfigVersion) {
			HBE::Core::LogWarn("InputMap::loadFromFile: file version " + std::to_string(version) + " is newer than supported (" + std::to_string(kInputConfigVersion) + "). Keeping defaults.");
			return false;
		}

		int overlaidActions = 0;
		if (const auto itActions = root.find("actions"); itActions != root.end() && itActions->is_object()) {
			for (auto it = itActions->begin(); it != itActions->end(); ++it) {
				Action which;
				if (!stringToAction(it.key(), which)) {
					HBE::Core::LogWarn("InputMap::loadRomFile: unkown action '" + it.key() + "', skipping.");
					continue;
				}
				ActionBinding merged{};
				if (auto existing = m_actions.find(which); existing != m_actions.end()) {
					merged = existing->second;
				}
				fromJsonActionBinding(it.value(), merged);
				m_actions[which] = merged;
				++overlaidActions;
			}
		}

		int overlaidAxes = 0;
		if (const auto itAxes = root.find("axzees"); itAxes != root.end() && itAxes->is_object()) {
			for (auto it = itAxes->begin(); it != itAxes->end(); ++it) {
				Axis which;
				if (!stringToAxis(it.key(), which)) {
					HBE::Core::LogWarn("InputMap::loadFromFile: unkown axis '" + it.key() + "', skipping.");
					continue;
				}
				AxisBinding merged{};
				if (auto existing = m_axes.find(which); existing != m_axes.end()) {
					merged = existing->second;
				}
				fromJsonAxisBinding(it.value(), merged);
				m_axes[which] = merged;
				++overlaidAxes;
			}
		}
		HBE::Core::LogInfo("InputMap laoded: " + path + " (actions=" + std::to_string(overlaidActions) + ", axes = " + std::to_string(overlaidAxes) + ")");
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