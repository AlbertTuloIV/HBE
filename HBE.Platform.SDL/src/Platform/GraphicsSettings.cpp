#include "HBE/Platform/GraphicsSettings.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace HBE::Platform {

	namespace {
		std::string Trim(const std::string& s) {
			std::string out = s;
			auto notSpace = [](unsigned char c) { return !std::isspace(c); };

			out.erase(out.begin(), std::find_if(out.begin(), out.end(), notSpace));
			out.erase(std::find_if(out.rbegin(), out.rend(), notSpace).base(), out.end());
			return out;
		}

		bool ToBool(const std::string& v, bool defaultVal) {
			std::string s = v;
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {return std::tolower(c); });

			if (s == "true" || s == "1" || s == "yes" || s == "on")
				return true;
			if (s == "false" || s == "0" || s == "no" || s == "off")
				return false;
			return defaultVal;
		}

		WindowMode ParseWindowMode(const std::string& v, WindowMode defaultVal) {
			std::string s = v;
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {return std::tolower(c); });

			if (s == "windowed") return WindowMode::Windowed;
			if (s == "fullscreen_desktop" || s == "borderless") return WindowMode::FullscreenDesktop;

			return defaultVal;
		}

		std::string WindowModeToString(WindowMode m) {
			switch (m) {
			case WindowMode::Windowed: return "windowed";
			case WindowMode::FullscreenDesktop: return "fullscreen_desktop";
			default: return "windowed";
			}
		}
	}

	bool LoadGraphicsSettings(GraphicsSettings& out, const std::string& path) {
		std::ifstream in(path);
		if (!in.is_open()) {
			// no file yet: keep defaults
			return false;
		}

		GraphicsSettings result = out; // start from existing defaults
		
		std::string line;
		while (std::getline(in, line)) {
			line = Trim(line);
			if (line.empty() || line[0] == '#' || line[0] == ';')
				continue;

			auto eqPos = line.find('=');
			if (eqPos == std::string::npos) continue;

			std::string key = Trim(line.substr(0, eqPos));
			std::string val = Trim(line.substr(eqPos + 1));

			if (key == "width") {
				try {
					result.width = std::max(1, std::stoi(val));
				}
				catch (const std::exception&) {
					//skip invalid value keep default
				}
			}
			else if (key == "height") {
				try {
					result.height = std::max(1, std::stoi(val));
				}
				catch (const std::exception&) {
					// skip invalid value keep default
				}
			}
			else if (key == "mode") {
				result.mode = ParseWindowMode(val, result.mode);
			}
			else if (key == "vsync") {
				result.vsync = ToBool(val, result.vsync);
			}
			else if (key == "target_fps") {
				try {
					result.targetFPS = std::max(0, std::stoi(val));
				}
				catch (const std::exception&) {
					// skip invalid value, keep default.
				}
			}
		}
		out = result;
		return true;
	}

	bool SaveGraphicsSettings(const GraphicsSettings& gs, const std::string& path) {
		std::ofstream out(path, std::ios::trunc);
		if (!out.is_open()) {
			return false;
		}

		out << "# HonestlyBadEngine graphics settings\n";
		out << "width=" << gs.width << "\n";
		out << "height=" << gs.height << "\n";
		out << "mode=" << WindowModeToString(gs.mode) << "\n";
		out << "vsync=" << (gs.vsync ? "true" : "false") << "\n";
		out << "target_fps=" << gs.targetFPS << "\n";

		return true;
	}
}