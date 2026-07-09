#pragma once
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>

namespace HBE::Core {
	namespace AssetPaths {

		struct Config {
			std::string organization = "HBE";
			std::string application = "HonestlyBadEngine";
			std::string forceAssetRoot;
			std::string forceUserDataRoot;
			std::string assetFolderName = "assets";
			std::vector<std::string> siblingProjectNames = { "HBE.Sandbox" };

			int maxParentWalk = 5;
		};

		bool Initialize(const Config& cfg = {});

		bool IsInitialized();

		const std::filesystem::path& AssetRoot();
		const std::filesystem::path& UserDataRoot();

		std::string AssetRootString();
		std::string UserDataRootString();

		std::string Resolve(std::string_view logicalRelPath);
		std::string ResolveUser(std::string_view logicalRelPath);
		std::string ResolveRelativeTo(std::string_view baseFilePath, std::string_view embeddedRelPath);

		bool Exists(std::string_view logicalRelPath);
	}
}