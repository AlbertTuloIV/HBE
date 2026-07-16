#include "IO/SceneIO.h"
#include "Model/Document.h"

#include <json.hpp>

#include <filesystem>
#include <fstream>

using nlohmann::json;
namespace fs = std::filesystem;

namespace hbmm::SceneIO {

    namespace {

        // Path of `target` relative to the folder containing `mapFile`, using
        // forward slashes. Falls back to the absolute path if no relative form
        // exists (e.g. different drive).
        std::string relativeToFile(const std::string& mapFile, const std::string& target) {
            if (target.empty()) return target;
            std::error_code ec;
            const fs::path baseDir = fs::path(mapFile).parent_path();
            fs::path rel = fs::relative(fs::path(target), baseDir, ec);
            if (ec || rel.empty()) return fs::path(target).generic_string();
            return rel.generic_string();
        }

    } // namespace

    bool SaveScene(const Document& doc, const std::string& path, std::string* err) {
        json j;
        j["hbmapmaker"] = 1;
        j["name"] = doc.name;
        j["width"] = doc.width;
        j["height"] = doc.height;
        j["tileW"] = doc.tileW;
        j["tileH"] = doc.tileH;
        j["tilePixelScale"] = doc.tilePixelScale;

        json jts = json::array();
        for (const auto& t : doc.tilesets) {
            json e;
            e["name"] = t.name;
            e["sourcePath"] = t.sourcePath;
            e["tileW"] = t.tileW;
            e["tileH"] = t.tileH;
            e["margin"] = t.margin;
            e["spacing"] = t.spacing;
            e["solidTiles"] = json::array();
            for (int id : t.solidTiles) e["solidTiles"].push_back(id);
            jts.push_back(std::move(e));
        }
        j["tilesets"] = std::move(jts);

        json jat = json::array();
        for (const auto& a : doc.animatedTiles) {
            json e;
            e["tilesetIndex"] = a.tilesetIndex;
            e["tileId"] = a.tileId;
            e["sheetPath"] = a.sheetPath;
            e["frameW"] = a.frameW;
            e["frameH"] = a.frameH;
            e["frames"] = a.frames;
            e["fps"] = a.fps;
            jat.push_back(std::move(e));
        }
        j["animatedTiles"] = std::move(jat);

        json jlayers = json::array();
        for (const auto& l : doc.layers) {
            json e;
            e["name"] = l.name;
            e["tilesetIndex"] = l.tilesetIndex;
            e["visible"] = l.visible;
            e["opacity"] = l.opacity;
            e["data"] = l.data;   // top-left origin, editor-native
            jlayers.push_back(std::move(e));
        }
        j["layers"] = std::move(jlayers);

        std::ofstream f(path);
        if (!f.is_open()) {
            if (err) *err = "Could not open '" + path + "' for writing.";
            return false;
        }
        f << j.dump(2);
        return true;
    }

    bool LoadScene(Document& doc, const std::string& path, std::string* err) {
        std::ifstream f(path);
        if (!f.is_open()) {
            if (err) *err = "Could not open '" + path + "'.";
            return false;
        }
        json j;
        try { f >> j; }
        catch (const std::exception& e) {
            if (err) *err = std::string("JSON parse error: ") + e.what();
            return false;
        }

        Document d;
        d.name = j.value("name", "Untitled");
        d.width = j.value("width", 40);
        d.height = j.value("height", 25);
        d.tileW = j.value("tileW", 32);
        d.tileH = j.value("tileH", 32);
        d.tilePixelScale = j.value("tilePixelScale", 1.0f);
        d.scenePath = path;

        if (j.contains("tilesets") && j["tilesets"].is_array()) {
            for (const auto& e : j["tilesets"]) {
                Tileset t;
                t.name = e.value("name", "tileset");
                t.sourcePath = e.value("sourcePath", "");
                t.tileW = e.value("tileW", 32);
                t.tileH = e.value("tileH", 32);
                t.margin = e.value("margin", 0);
                t.spacing = e.value("spacing", 0);
                if (e.contains("solidTiles") && e["solidTiles"].is_array())
                    for (const auto& v : e["solidTiles"]) t.solidTiles.insert((int)v);
                d.tilesets.push_back(std::move(t));
            }
        }

        if (j.contains("animatedTiles") && j["animatedTiles"].is_array()) {
            for (const auto& e : j["animatedTiles"]) {
                AnimatedTile a;
                a.tilesetIndex = e.value("tilesetIndex", 0);
                a.tileId = e.value("tileId", 0);
                a.sheetPath = e.value("sheetPath", "");
                a.frameW = e.value("frameW", 32);
                a.frameH = e.value("frameH", 32);
                a.frames = e.value("frames", 8);
                a.fps = e.value("fps", 8.0f);
                d.animatedTiles.push_back(std::move(a));
            }
        }

        if (j.contains("layers") && j["layers"].is_array()) {
            for (const auto& e : j["layers"]) {
                Layer l;
                l.name = e.value("name", "Layer");
                l.tilesetIndex = e.value("tilesetIndex", 0);
                l.visible = e.value("visible", true);
                l.opacity = e.value("opacity", 1.0f);
                if (e.contains("data") && e["data"].is_array())
                    for (const auto& v : e["data"]) l.data.push_back((int)v);
                l.data.resize((size_t)d.width * d.height, 0);
                d.layers.push_back(std::move(l));
            }
        }

        doc = std::move(d);
        return true;
    }

    bool ExportEngineMap(const Document& doc, const std::string& path, std::string* err) {
        json j;
        j["version"] = 1;
        j["tileSize"] = { {"w", doc.tileW}, {"h", doc.tileH} };
        j["tilePixelScale"] = doc.tilePixelScale;

        json jts = json::array();
        for (const auto& t : doc.tilesets) {
            json e;
            e["name"] = t.name;
            e["texture"] = relativeToFile(path, t.sourcePath);
            e["tileW"] = t.tileW;
            e["tileH"] = t.tileH;
            e["margin"] = t.margin;
            e["spacing"] = t.spacing;
            e["solidTiles"] = json::array();
            for (int id : t.solidTiles) e["solidTiles"].push_back(id);
            jts.push_back(std::move(e));
        }
        j["tilesets"] = std::move(jts);

        json jlayers = json::array();
        for (const auto& l : doc.layers) {
            json e;
            e["name"] = l.name;
            e["w"] = doc.width;
            e["h"] = doc.height;
            e["tileset"] = l.tilesetIndex;

            // Flip rows: editor is TOP-LEFT origin, engine is BOTTOM-LEFT.
            json data = json::array();
            for (int engineRow = 0; engineRow < doc.height; ++engineRow) {
                const int editorRow = doc.height - 1 - engineRow;
                for (int x = 0; x < doc.width; ++x)
                    data.push_back(l.data[(size_t)editorRow * doc.width + x]);
            }
            e["data"] = std::move(data);
            jlayers.push_back(std::move(e));
        }
        j["layers"] = std::move(jlayers);

        json jat = json::array();
        for (const auto& a : doc.animatedTiles) {
            json e;
            e["tileset"] = a.tilesetIndex;
            e["tileId"] = a.tileId;
            e["sheet"] = relativeToFile(path, a.sheetPath);
            e["frameW"] = a.frameW;
            e["frameH"] = a.frameH;
            e["frames"] = a.frames;
            e["fps"] = a.fps;
            jat.push_back(std::move(e));
        }
        j["animatedTiles"] = std::move(jat);

        std::ofstream f(path);
        if (!f.is_open()) {
            if (err) *err = "Could not open '" + path + "' for writing.";
            return false;
        }
        f << j.dump(2);
        return true;
    }

}
