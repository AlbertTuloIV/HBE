#include "HBE/Renderer/TileMapLoader.h"
#include "HBE/Core/Log.h"

#include <fstream>
#include <sstream>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <json.hpp>
#include <stb_image.h>

using json = nlohmann::json;

namespace HBE::Renderer {

    static bool readAllText(const std::string& path, std::string& out) {
        std::ifstream f(path);
        if (!f.is_open()) return false;
        std::stringstream ss;
        ss << f.rdbuf();
        out = ss.str();
        return true;
    }

    bool TileMapLoader::loadFromJsonFile(const std::string& path, TileMap& outMap, std::string* outError) {
        std::string text;
        if (!readAllText(path, text)) {
            if (outError) *outError = "TileMapLoader: could not open: " + path;
            return false;
        }

        json j;
        try {
            j = json::parse(text);
        }
        catch (...) {
            if (outError) *outError = "TileMapLoader: invalid JSON";
            return false;
        }

        TileMap map{};
        map.version = j.value("version", 1);

        if (j.contains("tileSize")) {
            map.tileSizeW = j["tileSize"].value("w", 16);
            map.tileSizeH = j["tileSize"].value("h", 16);
        }

        map.tilePixelScale = j.value("tilePixelScale", 1.0f);
        if (map.tilePixelScale <= 0.f) map.tilePixelScale = 1.f;

        // Tilesets
        if (!j.contains("tilesets") || !j["tilesets"].is_array()) {
            if (outError) *outError = "TileMapLoader: missing tilesets";
            return false;
        }

        for (auto& ts : j["tilesets"]) {
            TileMapTileset t{};
            t.name = ts.value("name", "");
            t.texturePath = ts.value("texture", "");
            t.tileW = ts.value("tileW", map.tileSizeW);
            t.tileH = ts.value("tileH", map.tileSizeH);
            t.margin = ts.value("margin", 0);
            t.spacing = ts.value("spacing", 0);

            // Solid tiles (array of tile IDs)
            if (ts.contains("solidTiles") && ts["solidTiles"].is_array()) {
                for (auto& v : ts["solidTiles"])
                    t.solidTiles.insert((int)v);
            }

            // One-way platform tiles (array of tile IDs)
            if (ts.contains("oneWayTiles") && ts["oneWayTiles"].is_array()) {
                for (auto& v : ts["oneWayTiles"])
                    t.oneWayTiles.insert((int)v);
            }

            // Slopes:
            // Supported JSON shapes:
            // 1) "slopes": { "12": "left", "13": "right" }
            // 2) "slopes": [ { "id": 12, "type": "left" }, ... ]
            if (ts.contains("slopes")) {
                auto parseSlopeType = [](const std::string& s) -> SlopeType {
                    std::string t = s;
                    for (auto& c : t) c = (char)std::tolower(c);
                    if (t == "left" || t == "leftup" || t == "/" || t == "lu") return SlopeType::LeftUp;
                    if (t == "right" || t == "rightup" || t == "\\" || t == "ru") return SlopeType::RightUp;
                    return SlopeType::None;
                    };

                const auto& slopes = ts["slopes"];
                if (slopes.is_object()) {
                    for (auto it = slopes.begin(); it != slopes.end(); ++it) {
                        int id = 0;
                        try { id = std::stoi(it.key()); }
                        catch (...) { continue; }

                        if (it.value().is_number_integer()) {
                            int v = (int)it.value();
                            if (v == 1) t.slopes[id] = SlopeType::LeftUp;
                            else if (v == 2) t.slopes[id] = SlopeType::RightUp;
                        }
                        else if (it.value().is_string()) {
                            SlopeType st = parseSlopeType((std::string)it.value());
                            if (st != SlopeType::None) t.slopes[id] = st;
                        }
                    }
                }
                else if (slopes.is_array()) {
                    for (auto& s : slopes) {
                        int id = s.value("id", 0);
                        if (id <= 0) continue;

                        if (s.contains("type") && s["type"].is_string()) {
                            SlopeType st = parseSlopeType(s.value("type", ""));
                            if (st != SlopeType::None) t.slopes[id] = st;
                        }
                        else if (s.contains("type") && s["type"].is_number_integer()) {
                            int v = s["type"];
                            if (v == 1) t.slopes[id] = SlopeType::LeftUp;
                            else if (v == 2) t.slopes[id] = SlopeType::RightUp;
                        }
                    }
                }
            }

            map.tilesets.push_back(std::move(t));
        }

        // Layers
        if (!j.contains("layers") || !j["layers"].is_array()) {
            if (outError) *outError = "TileMapLoader: missing layers";
            return false;
        }

        for (auto& layer : j["layers"]) {
            TileMapLayer l{};
            l.name = layer.value("name", "");
            l.w = layer.value("w", 0);
            l.h = layer.value("h", 0);
            l.tilesetIndex = layer.value("tileset", 0);

            if (l.w <= 0 || l.h <= 0) {
                if (outError) *outError = "TileMapLoader: invalid layer size";
                return false;
            }

            if (!layer.contains("data") || !layer["data"].is_array()) {
                if (outError) *outError = "TileMapLoader: layer missing data";
                return false;
            }

            l.data.reserve(l.w * l.h);
            for (auto& v : layer["data"])
                l.data.push_back((int)v);

            if ((int)l.data.size() != l.w * l.h) {
                if (outError) *outError = "TileMapLoader: layer data size mismatch";
                return false;
            }

            map.layers.push_back(std::move(l));
        }

        outMap = std::move(map);
        return true;
    }

    bool TileMapLoader::sampleTileTopColors(
        const TileMap& map,
        std::unordered_map<int, std::array<float, 4>>& outColors,
        int topRowsToSample)
    {
        outColors.clear();
        if (map.tilesets.empty()) return false;
        if (topRowsToSample <= 0) topRowsToSample = 1;

        const auto& ts = map.tilesets[0];
        if (ts.tileW <= 0 || ts.tileH <= 0) return false;

        // Sample from the raw image top-down so raw pixel (x,0) is the visual top of the atlas.
        stbi_set_flip_vertically_on_load(0);

        int w = 0, h = 0, ch = 0;
        unsigned char* data = stbi_load(ts.texturePath.c_str(), &w, &h, &ch, 4);
        if (!data) return false;

        const int strideX = ts.tileW + ts.spacing;
        const int strideY = ts.tileH + ts.spacing;
        const int usableW = w - 2 * ts.margin;
        const int usableH = h - 2 * ts.margin;
        const int cols = (strideX > 0) ? std::max(1, usableW / strideX) : 1;
        const int rows = (strideY > 0) ? std::max(1, usableH / strideY) : 1;

        const int sampleRows = std::min(topRowsToSample, ts.tileH);

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const int atlasIdx = r * cols + c;
                const int tileId = atlasIdx + 1; // tile IDs are 1-based

                const int px0 = ts.margin + c * strideX;
                const int py0 = ts.margin + r * strideY;

                double ar = 0.0, ag = 0.0, ab = 0.0;
                int count = 0;

                for (int dy = 0; dy < sampleRows; ++dy) {
                    for (int dx = 0; dx < ts.tileW; ++dx) {
                        const int x = px0 + dx;
                        const int y = py0 + dy;
                        if (x < 0 || y < 0 || x >= w || y >= h) continue;
                        const int idx = (y * w + x) * 4;
                        const unsigned char a = data[idx + 3];
                        if (a < 8) continue; // skip transparent pixels
                        ar += data[idx + 0];
                        ag += data[idx + 1];
                        ab += data[idx + 2];
                        ++count;
                    }
                }

                if (count > 0) {
                    std::array<float, 4> col{};
                    col[0] = (float)(ar / count) / 255.0f;
                    col[1] = (float)(ag / count) / 255.0f;
                    col[2] = (float)(ab / count) / 255.0f;
                    col[3] = 1.0f;
                    outColors.emplace(tileId, col);
                }
            }
        }

        stbi_image_free(data);
        return true;
    }

}