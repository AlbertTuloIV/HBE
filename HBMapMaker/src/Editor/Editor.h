#pragma once

#include "Model/Document.h"

#include <imgui.h>

#include <cstdint>
#include <string>
#include <vector>

namespace HBE::Renderer { class ResourceCache; }

namespace hbmm {

    // The whole map editor: owns the document + editor state, builds all ImGui
    // panels each frame, and handles the drawing tools. Renders the map canvas
    // with ImGui draw lists (screen-space pan/zoom).
    class Editor {
    public:
        explicit Editor(HBE::Renderer::ResourceCache& resources);

        void update(float dt);   // advance animation clock
        void draw();             // build one frame of UI
        bool wantsQuit() const { return m_quit; }

    private:
        enum class Tool { Brush, Eraser, Rect, Bucket, Select, Picker };

        // ---- panels
        void drawMenuBar();
        void drawDockspaceLayout();
        void drawToolbar();
        void drawTilesetPanel();
        void drawLayersPanel();
        void drawAnimatedPanel();
        void drawPropertiesPanel();
        void drawCanvas();
        void drawStatusBar();

        // ---- file actions
        void newScene();
        void openScene();
        void saveScene(bool saveAs);
        void exportMap();
        void importTileset();
        void importAnimatedTile();

        // ---- texture management
        void reloadAllTextures();
        void loadTilesetTexture(Tileset& t, int index);
        void loadAnimatedTexture(AnimatedTile& a, int index);

        // ---- edit ops
        void pushUndo();
        void undo();
        void redo();
        void paintCell(int tx, int ty, int tileId);
        void floodFill(int tx, int ty, int newId);
        void fillRectTiles(int x0, int y0, int x1, int y1, int tileId);
        void copySelection();
        void pasteAt(int tx, int ty);
        void deleteSelection();

        // Ensures the layer that will receive paint is bound to the currently
        // selected tileset (engine maps allow one tileset per layer): rebinds an
        // empty active layer, else switches to / creates a matching layer.
        Layer* ensurePaintLayer();

        // ---- helpers
        Layer* activeLayer();
        const AnimatedTile* findAnimated(int tilesetIndex, int tileId) const;
        int currentAnimFrame(const AnimatedTile& a) const;
        static void subUV(int texW, int texH, int px, int py, int w, int h,
                          ImVec2& uv0, ImVec2& uv1);

        HBE::Renderer::ResourceCache& m_resources;
        Document m_doc;

        int  m_activeLayer = 0;
        int  m_selectedTileset = -1;
        int  m_selectedTileId = 0;   // 1-based; 0 = empty/none
        Tool m_tool = Tool::Brush;

        // canvas view
        ImVec2 m_pan{ 0.0f, 0.0f };
        float  m_zoom = 1.5f;
        bool   m_showGrid = true;
        bool   m_viewInit = false;

        // interaction
        bool m_stroking = false;
        int  m_dragX0 = 0, m_dragY0 = 0;
        bool m_boxDrag = false;
        int  m_hoverX = -1, m_hoverY = -1;

        // selection (tile coords, inclusive)
        bool m_hasSel = false;
        int  m_selX0 = 0, m_selY0 = 0, m_selX1 = 0, m_selY1 = 0;

        // clipboard
        int m_clipW = 0, m_clipH = 0;
        std::vector<int> m_clip;

        // undo / redo
        struct Snapshot { int width; int height; std::vector<Layer> layers; };
        std::vector<Snapshot> m_undo;
        std::vector<Snapshot> m_redo;

        float m_animClock = 0.0f;
        std::string m_status = "Ready.";
        bool m_quit = false;
        bool m_layoutBuilt = false;

        // pending "new scene" modal fields
        bool m_openNewModal = false;
        int  m_newW = 40, m_newH = 25, m_newTileW = 32, m_newTileH = 32;

        // Properties-panel map-size edit buffer. Re-seeded from the document
        // only when its size changes externally (not every frame), so typing
        // isn't clobbered before the user clicks "Resize Map".
        int m_propW = 40, m_propH = 25;
        int m_propSeenW = -1, m_propSeenH = -1;

        // pending "add animated tile" modal fields
        bool m_openAnimModal = false;
        std::string m_animPendingPath;
        int m_animTileset = 0, m_animTileId = 1, m_animFW = 32, m_animFH = 32, m_animFrames = 8;
        float m_animFps = 8.0f;

        // pending "add tileset" slice fields (applied to last import)
        bool m_openTilesetModal = false;
        std::string m_tsPendingPath;
        int m_tsTileW = 32, m_tsTileH = 32, m_tsMargin = 0, m_tsSpacing = 0;
    };

}
