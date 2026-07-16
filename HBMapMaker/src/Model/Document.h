#pragma once

// HBMapMaker document / data model.
//
// The editor works in its own coordinate convention (rows are TOP-LEFT origin,
// which is natural for editing) and converts to the engine's BOTTOM-LEFT origin
// only on export. Tile ids are 1-based and local to a tileset (0 = empty), which
// matches the engine's TileMapLoader.

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace hbmm {

    // An imported spritesheet sliced into a uniform grid of tiles.
    struct Tileset {
        std::string name = "tileset";
        std::string sourcePath;          // absolute path on disk (reloaded on open)

        int tileW = 32;                  // slice cell size (px in the atlas)
        int tileH = 32;
        int margin = 0;                  // border around the whole sheet
        int spacing = 0;                 // gap between tiles

        std::set<int> solidTiles;        // 1-based ids flagged solid (gameplay meta)

        // ---- runtime (not part of the on-disk identity; rebuilt from sourcePath)
        std::uint32_t glTexId = 0;       // GL texture id (owned by ResourceCache)
        int texW = 0;
        int texH = 0;

        int columns() const {
            if (tileW <= 0 || texW <= 0) return 0;
            const int usable = texW - 2 * margin + spacing;
            const int denom = tileW + spacing;
            return denom > 0 ? std::max(0, usable / denom) : 0;
        }
        int rows() const {
            if (tileH <= 0 || texH <= 0) return 0;
            const int usable = texH - 2 * margin + spacing;
            const int denom = tileH + spacing;
            return denom > 0 ? std::max(0, usable / denom) : 0;
        }
        int tileCount() const { return columns() * rows(); }

        // Top-left pixel of tile `index0` (0-based, left->right, top->bottom).
        void tilePixel(int index0, int& px, int& py) const {
            const int cols = columns();
            const int col = (cols > 0) ? (index0 % cols) : 0;
            const int row = (cols > 0) ? (index0 / cols) : 0;
            px = margin + col * (tileW + spacing);
            py = margin + row * (tileH + spacing);
        }
    };

    // An animation that overrides a static tile id in a tileset. Mirrors the
    // engine map "animatedTiles" schema. Frames play 0..frames-1 left->right,
    // top->bottom in the sheet.
    struct AnimatedTile {
        int tilesetIndex = 0;            // which tileset it overrides
        int tileId = 0;                  // 1-based id it replaces
        std::string sheetPath;           // absolute path on disk

        int frameW = 32;
        int frameH = 32;
        int frames = 8;
        float fps = 8.0f;

        // ---- runtime
        std::uint32_t glTexId = 0;
        int texW = 0;
        int texH = 0;

        int columns() const { return (frameW > 0 && texW > 0) ? texW / frameW : 0; }
    };

    // A grid layer. `data` is width*height, TOP-LEFT origin, row-major
    // (index = y*width + x). 0 = empty, else 1-based tile id in `tilesetIndex`.
    struct Layer {
        std::string name = "Layer";
        int tilesetIndex = 0;
        bool visible = true;
        float opacity = 1.0f;
        std::vector<int> data;

        int at(int x, int y, int w, int h) const {
            if (x < 0 || y < 0 || x >= w || y >= h) return 0;
            return data[(size_t)y * w + x];
        }
        void set(int x, int y, int w, int h, int v) {
            if (x < 0 || y < 0 || x >= w || y >= h) return;
            data[(size_t)y * w + x] = v;
        }
    };

    // The whole editable project ("Scene"): map dimensions, imported tilesets and
    // animated tiles, and the tile layers.
    struct Document {
        std::string name = "Untitled";

        int width = 40;                  // map size in tiles
        int height = 25;
        int tileW = 32;                  // world cell size (engine tileSize.w/h)
        int tileH = 32;
        float tilePixelScale = 1.0f;

        std::vector<Tileset> tilesets;
        std::vector<AnimatedTile> animatedTiles;
        std::vector<Layer> layers;

        // Path this document was last saved to / loaded from (native .hbscene).
        std::string scenePath;

        Layer makeEmptyLayer(const std::string& n, int tilesetIndex) const {
            Layer l;
            l.name = n;
            l.tilesetIndex = tilesetIndex;
            l.data.assign((size_t)width * height, 0);
            return l;
        }

        // Resize every layer to the current width/height, preserving overlap.
        void resizeLayers(int newW, int newH) {
            newW = std::max(1, newW);
            newH = std::max(1, newH);
            for (auto& l : layers) {
                std::vector<int> next((size_t)newW * newH, 0);
                const int cw = std::min(width, newW);
                const int ch = std::min(height, newH);
                for (int y = 0; y < ch; ++y)
                    for (int x = 0; x < cw; ++x)
                        next[(size_t)y * newW + x] = l.data[(size_t)y * width + x];
                l.data = std::move(next);
            }
            width = newW;
            height = newH;
        }
    };

}
