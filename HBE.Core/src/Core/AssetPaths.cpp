#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>   // SDL_free

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

namespace HBE::Core::AssetPaths {

    namespace fs = std::filesystem;

    namespace {

        // MSVC promotes std::getenv to a "deprecated / unsafe" error under
        // /sdl. Wrap it in a portable helper that uses _dupenv_s on MSVC and
        // std::getenv elsewhere. Returns an empty string if the variable is
        // unset.
        std::string GetEnvOrEmpty(const char* name) {
#ifdef _MSC_VER
            char*  value = nullptr;
            size_t sz    = 0;
            if (_dupenv_s(&value, &sz, name) != 0 || value == nullptr) {
                return {};
            }
            std::string result(value);
            free(value);
            return result;
#else
            const char* v = std::getenv(name);
            return v ? std::string(v) : std::string{};
#endif
        }

        bool             g_initialized = false;
        fs::path         g_assetRoot;
        fs::path         g_userDataRoot;
        std::string      g_assetRootStr;
        std::string      g_userDataRootStr;

        std::string normalizeSlashes(std::string_view in) {
            std::string out(in);
            std::replace(out.begin(), out.end(), '\\', '/');
            return out;
        }

        std::string stripLeadingSep(std::string_view in) {
            size_t i = 0;
            while (i < in.size() && (in[i] == '/' || in[i] == '\\')) ++i;
            return std::string(in.substr(i));
        }

        fs::path exeDir() {
            const char* base = SDL_GetBasePath();
            if (!base) return {};
            return fs::path(base);
        }

        std::optional<fs::path> tryAssetsUnder(const fs::path& parent,
            const std::string& folderName)
        {
            std::error_code ec;
            fs::path candidate = parent / folderName;
            if (fs::exists(candidate, ec) && fs::is_directory(candidate, ec)) {
                return fs::canonical(candidate, ec);
            }
            return std::nullopt;
        }

        std::optional<fs::path> searchLadder(const fs::path& start,
            const Config& cfg)
        {
            fs::path cur = start;
            for (int i = 0; i <= cfg.maxParentWalk; ++i) {
                if (auto hit = tryAssetsUnder(cur, cfg.assetFolderName))
                    return hit;

                for (const auto& sib : cfg.siblingProjectNames) {
                    if (auto hit = tryAssetsUnder(cur / sib, cfg.assetFolderName))
                        return hit;
                }

                if (!cur.has_parent_path() || cur == cur.parent_path()) break;
                cur = cur.parent_path();
            }
            return std::nullopt;
        }

        fs::path resolveUserDataRoot(const Config& cfg) {
            if (!cfg.forceUserDataRoot.empty()) {
                fs::path p(cfg.forceUserDataRoot);
                std::error_code ec;
                fs::create_directories(p, ec);
                return fs::weakly_canonical(p, ec);
            }

            char* pref = SDL_GetPrefPath(cfg.organization.c_str(),
                cfg.application.c_str());
            if (pref) {
                fs::path p(pref);
                SDL_free(pref);
                return p;
            }
            return exeDir();
        }
    }

    bool Initialize(const Config& cfg) {
        if (g_initialized) {
            LogWarn("AssetPaths::Initialize called twice — ignoring second call.");
            return true;
        }

        std::error_code ec;

        std::optional<fs::path> found;
        if (!cfg.forceAssetRoot.empty()) {
            fs::path p(cfg.forceAssetRoot);
            if (fs::exists(p, ec)) found = fs::canonical(p, ec);
            else LogWarn("AssetPaths: forceAssetRoot does not exist: " + p.string());
        }

        if (!found) {
            const std::string env = GetEnvOrEmpty("HBE_ASSET_ROOT");
            if (!env.empty()) {
                fs::path p(env);
                if (fs::exists(p, ec)) found = fs::canonical(p, ec);
                else LogWarn("AssetPaths: HBE_ASSET_ROOT does not exist: " + env);
            }
        }

        if (!found) found = searchLadder(exeDir(), cfg);

        if (!found) found = searchLadder(fs::current_path(ec), cfg);

        if (!found) {
            LogError("AssetPaths::Initialize: could not locate an '" +
                cfg.assetFolderName + "' folder. Searched from exe (" +
                exeDir().string() + ") and CWD (" +
                fs::current_path(ec).string() + ").");
            return false;
        }

        g_assetRoot = *found;
        g_userDataRoot = resolveUserDataRoot(cfg);
        g_assetRootStr = g_assetRoot.string();
        g_userDataRootStr = g_userDataRoot.string();
        g_initialized = true;

        LogInfo("AssetPaths: asset root    = " + g_assetRootStr);
        LogInfo("AssetPaths: user data root = " + g_userDataRootStr);
        return true;
    }

    bool IsInitialized() { return g_initialized; }

    const fs::path& AssetRoot() { return g_assetRoot; }
    const fs::path& UserDataRoot() { return g_userDataRoot; }

    std::string AssetRootString() { return g_assetRootStr; }
    std::string UserDataRootString() { return g_userDataRootStr; }

    std::string Resolve(std::string_view logicalRelPath) {
        if (!g_initialized) {
            LogError("AssetPaths::Resolve called before Initialize().");
            return std::string(logicalRelPath);
        }

        fs::path in(logicalRelPath);
        if (in.is_absolute()) return in.lexically_normal().string();

        const std::string clean = stripLeadingSep(normalizeSlashes(logicalRelPath));
        return (g_assetRoot / clean).lexically_normal().string();
    }

    std::string ResolveUser(std::string_view logicalRelPath) {
        if (!g_initialized) {
            LogError("AssetPaths::ResolveUser called before Initialize().");
            return std::string(logicalRelPath);
        }
        fs::path in(logicalRelPath);
        if (in.is_absolute()) return in.lexically_normal().string();

        const std::string clean = stripLeadingSep(normalizeSlashes(logicalRelPath));
        return (g_userDataRoot / clean).lexically_normal().string();
    }

    std::string ResolveRelativeTo(std::string_view baseFilePath,
        std::string_view embeddedRelPath)
    {
        fs::path emb(embeddedRelPath);
        if (emb.is_absolute()) return emb.lexically_normal().string();

        if (!embeddedRelPath.empty() &&
            (embeddedRelPath.front() == '/' || embeddedRelPath.front() == '\\'))
        {
            return Resolve(embeddedRelPath);
        }

        fs::path basePath(baseFilePath);
        std::error_code ec;
        if (!basePath.empty()) {
            fs::path baseDir = basePath.parent_path();
            fs::path candidate = (baseDir / emb).lexically_normal();
            if (fs::exists(candidate, ec)) return candidate.string();
        }

        return Resolve(embeddedRelPath);
    }

    bool Exists(std::string_view logicalRelPath) {
        std::error_code ec;
        return fs::exists(Resolve(logicalRelPath), ec);
    }

}