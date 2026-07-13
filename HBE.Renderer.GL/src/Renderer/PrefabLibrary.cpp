#include "HBE/Renderer/PrefabLibrary.h"

#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace HBE::Renderer {
	namespace fs = std::filesystem;

	static bool readAllText(const fs::path& p, std::string& out) {
		std::ifstream in(p, std::ios::binary);
		if (!in) return false;
		std::stringstream ss;
		ss << in.rdbuf();
		out = ss.str();
		return true;
	}

	void PrefabLibrary::clear() {
		m_prefabs.clear();
	}

	void PrefabLibrary::addOrReplace(PrefabDefinition def) {
		if (def.name.empty()) {
			HBE::Core::LogWarn("PrefabLibrary::addOrReplace: skipping prefab with empty name (source: " + def.sourcePath + ")");
			return;
		}
		m_prefabs[def.name] = std::move(def);
	}

	const PrefabDefinition* PrefabLibrary::get(const std::string& name) const {
		auto it = m_prefabs.find(name);
		return it == m_prefabs.end() ? nullptr : &it->second;
	}

	std::vector<std::string> PrefabLibrary::names() const {
		std::vector<std::string> out;
		out.reserve(m_prefabs.size());
		for (const auto& [k, _] : m_prefabs) out.push_back(k);
		std::sort(out.begin(), out.end());
		return out;
	}

	int PrefabLibrary::loadDirectory(std::string_view directoryLogicalPath) {
		clear();

		const std::string rooted = HBE::Core::AssetPaths::Resolve(directoryLogicalPath);
		std::error_code ec;

		if (!fs::exists(rooted, ec)) {
			HBE::Core::LogInfo("PrefabLibrary: no prefab directory at '" + rooted + "' -- library is empty.");
			return 0;
		}
		if (!fs::is_directory(rooted, ec)) {
			HBE::Core::LogWarn("PrefabLibrary: '" + rooted + "' exists but is not a directory. Skipping.");
			return 0;
		}

		int loaded = 0;
		int failed = 0;

		for (const auto& entry : fs::recursive_directory_iterator(rooted, ec)) {
			if (ec) {
				HBE::Core::LogWarn("PrefabLibrary: filesystem erorr while walking '" + rooted + "': " + ec.message());
				ec.clear();
				continue;
			}
			if (!entry.is_regular_file()) continue;

			const std::string filename = entry.path().filename().string();

			if (filename.size() < 12) continue;
			if (filename.compare(filename.size() - 12, 12, ".prefab.json") != 0) continue;

			std::string text;
			if (!readAllText(entry.path(), text)) {
				HBE::Core::LogWarn("PrefabLibrary: could not read '" + entry.path().string() + "'.");
				++failed;
				continue;
			}

			nlohmann::json root;
			try {
				root = nlohmann::json::parse(text);
			}
			catch (const std::exception& e) {
				HBE::Core::LogWarn("PrefabLibrary: parse error in '" + entry.path().string() + "': " + e.what());
				++failed;
				continue;
			}
			if (!root.is_object()) {
				HBE::Core::LogWarn("PrefabLibrary: '" + entry.path().string() + "' is not a JSON object.");
				++failed;
				continue;
			}

			const int version = root.value("version", 0);
			if (version <= 0 || version > kPrefabVersion) {
				HBE::Core::LogWarn("PrefabLibrary: '" + entry.path().string() + "' has unsupported version " + std::to_string(version) + " (current is " + std::to_string(kPrefabVersion) + "). Skipping.");
				++failed;
				continue;
			}

			PrefabDefinition def{};
			def.name = root.value("name", "");
			def.tag = root.value("tag", "");
			def.sourcePath = entry.path().string();

			if (def.name.empty()) {
				HBE::Core::LogWarn("PrefabLibrary: '" + def.sourcePath + "' has no 'name' field. Skipping.");
				++failed;
				continue;
			}

			if (const auto it = root.find("components"); it != root.end() && it->is_object()) {
				def.components = *it;
			}
			else {
				HBE::Core::LogWarn("PrefabLibarry: '" + def.sourcePath + "' has no 'components' object. Treating as empty prefab.");
				def.components = nlohmann::json::object();
			}

			if (m_prefabs.contains(def.name)) {
				HBE::Core::LogWarn("PrefabLibarry: duplicate prefab name '" + def.name + "' -- '" + def.sourcePath + "' overrides earlier definition from '" + m_prefabs[def.name].sourcePath + "'.");
			}

			m_prefabs[def.name] = std::move(def);
			++loaded;
		}

		HBE::Core::LogInfo("PrefabLibrary: loaded " + std::to_string(loaded) + " prefab(s) from '" + rooted + "'" + (failed ? (" (" + std::to_string(failed) + " skipped)") : ""));
		return loaded;
	}
}