#pragma once

#include <string>

namespace hbmm {

    struct Document;

    namespace SceneIO {

        // Native editor project format (.hbscene). Round-trips everything needed
        // to continue editing later. Texture GL handles are NOT restored here;
        // the editor reloads textures from each tileset's sourcePath afterwards.
        bool SaveScene(const Document& doc, const std::string& path, std::string* err);
        bool LoadScene(Document& doc, const std::string& path, std::string* err);

        // Export an engine-compatible tile map JSON (TileMapLoader schema).
        // Flips row order to the engine's bottom-left origin and writes texture /
        // sheet paths relative to the exported file's folder.
        bool ExportEngineMap(const Document& doc, const std::string& path, std::string* err);

    }

}
