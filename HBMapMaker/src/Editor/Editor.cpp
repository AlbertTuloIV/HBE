#include "Editor/Editor.h"

#include "IO/SceneIO.h"
#include "Platform/FileDialog.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Texture2D.h"
#include "HBE/Core/Log.h"

#include <imgui.h>
#include <imgui_internal.h>   // DockBuilder*

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <utility>

using HBE::Renderer::Texture2D;

namespace hbmm {

    namespace {
        std::string baseName(const std::string& path) {
            return std::filesystem::path(path).stem().string();
        }

        // ImGui's OpenGL3 texture backend can leave GL_UNPACK_ROW_LENGTH (and
        // other pixel-store state) set to non-default values. The engine's
        // Texture2D::loadFromFile calls glTexImage2D assuming a tightly-packed
        // client buffer, so a stale row length makes the driver read past the
        // end of the pixel data (access violation in the GL driver). Reset the
        // unpack state before handing off to the engine loader.
        void resetPixelStoreForUpload() {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        }
    }

    Editor::Editor(HBE::Renderer::ResourceCache& resources)
        : m_resources(resources) {
        newScene();
    }

    // ------------------------------------------------------------------ helpers

    Layer* Editor::activeLayer() {
        if (m_activeLayer < 0 || m_activeLayer >= (int)m_doc.layers.size()) return nullptr;
        return &m_doc.layers[m_activeLayer];
    }

    const AnimatedTile* Editor::findAnimated(int tilesetIndex, int tileId) const {
        for (const auto& a : m_doc.animatedTiles)
            if (a.tilesetIndex == tilesetIndex && a.tileId == tileId) return &a;
        return nullptr;
    }

    int Editor::currentAnimFrame(const AnimatedTile& a) const {
        if (a.frames <= 0) return 0;
        long long ticks = (long long)std::floor(m_animClock * a.fps);
        return (int)(((ticks % a.frames) + a.frames) % a.frames);
    }

    void Editor::subUV(int texW, int texH, int px, int py, int w, int h,
                       ImVec2& uv0, ImVec2& uv1) {
        const float fw = texW > 0 ? (float)texW : 1.0f;
        const float fh = texH > 0 ? (float)texH : 1.0f;
        uv0 = ImVec2((float)px / fw, 1.0f - (float)py / fh);
        uv1 = ImVec2((float)(px + w) / fw, 1.0f - (float)(py + h) / fh);
    }

    // ------------------------------------------------------------------ textures

    void Editor::loadTilesetTexture(Tileset& t, int /*index*/) {
        if (t.sourcePath.empty()) return;
        resetPixelStoreForUpload();
        Texture2D* tex = m_resources.getOrCreateTextureFromFile("tsimg:" + t.sourcePath, t.sourcePath);
        if (!tex) { m_status = "Failed to load tileset: " + t.sourcePath; return; }
        t.glTexId = tex->getID();
        t.texW = tex->getWidth();
        t.texH = tex->getHeight();
    }

    void Editor::loadAnimatedTexture(AnimatedTile& a, int /*index*/) {
        if (a.sheetPath.empty()) return;
        resetPixelStoreForUpload();
        Texture2D* tex = m_resources.getOrCreateTextureFromFile("animimg:" + a.sheetPath, a.sheetPath);
        if (!tex) { m_status = "Failed to load animated sheet: " + a.sheetPath; return; }
        a.glTexId = tex->getID();
        a.texW = tex->getWidth();
        a.texH = tex->getHeight();
    }

    void Editor::reloadAllTextures() {
        for (int i = 0; i < (int)m_doc.tilesets.size(); ++i) loadTilesetTexture(m_doc.tilesets[i], i);
        for (int i = 0; i < (int)m_doc.animatedTiles.size(); ++i) loadAnimatedTexture(m_doc.animatedTiles[i], i);
        m_status = "Textures reloaded.";
    }

    // ------------------------------------------------------------------ file ops

    void Editor::newScene() {
        m_doc = Document{};
        m_doc.layers.push_back(m_doc.makeEmptyLayer("Ground", 0));
        m_activeLayer = 0;
        m_selectedTileset = -1;
        m_selectedTileId = 0;
        m_undo.clear();
        m_redo.clear();
        m_hasSel = false;
        m_viewInit = false;
        m_status = "New scene.";
    }

    void Editor::openScene() {
        std::string path = FileDialog::OpenFile("Open Scene", "HBMapMaker Scene\0*.hbscene\0All Files\0*.*\0");
        if (path.empty()) return;
        std::string err;
        Document loaded;
        if (!SceneIO::LoadScene(loaded, path, &err)) { m_status = "Open failed: " + err; return; }
        m_doc = std::move(loaded);
        m_activeLayer = m_doc.layers.empty() ? 0 : 0;
        m_selectedTileset = m_doc.tilesets.empty() ? -1 : 0;
        m_selectedTileId = 0;
        m_undo.clear(); m_redo.clear();
        m_hasSel = false; m_viewInit = false;
        reloadAllTextures();
        m_status = "Opened " + path;
    }

    void Editor::saveScene(bool saveAs) {
        std::string path = m_doc.scenePath;
        if (saveAs || path.empty())
            path = FileDialog::SaveFile("Save Scene", "HBMapMaker Scene\0*.hbscene\0All Files\0*.*\0", "hbscene");
        if (path.empty()) return;
        std::string err;
        if (!SceneIO::SaveScene(m_doc, path, &err)) { m_status = "Save failed: " + err; return; }
        m_doc.scenePath = path;
        m_status = "Saved " + path;
    }

    void Editor::exportMap() {
        std::string path = FileDialog::SaveFile("Export Engine Map JSON", "Map JSON\0*.json\0All Files\0*.*\0", "json");
        if (path.empty()) return;
        std::string err;
        if (!SceneIO::ExportEngineMap(m_doc, path, &err)) { m_status = "Export failed: " + err; return; }
        m_status = "Exported engine map: " + path;
    }

    void Editor::importTileset() {
        std::string path = FileDialog::OpenFile("Import Tileset (PNG)", "Images\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0");
        if (path.empty()) return;
        m_tsPendingPath = path;
        m_tsTileW = m_doc.tileW; m_tsTileH = m_doc.tileH;
        m_tsMargin = 0; m_tsSpacing = 0;
        m_openTilesetModal = true;
    }

    void Editor::importAnimatedTile() {
        std::string path = FileDialog::OpenFile("Import Animated Sheet (PNG)", "Images\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0");
        if (path.empty()) return;
        m_animPendingPath = path;
        m_animTileset = (m_selectedTileset >= 0) ? m_selectedTileset : 0;
        m_animTileId = (m_selectedTileId > 0) ? m_selectedTileId : 1;
        m_animFW = m_doc.tileW; m_animFH = m_doc.tileH;
        m_animFrames = 8; m_animFps = 8.0f;
        m_openAnimModal = true;
    }

    // ------------------------------------------------------------------ edit ops

    void Editor::pushUndo() {
        m_undo.push_back(Snapshot{ m_doc.width, m_doc.height, m_doc.layers });
        if (m_undo.size() > 80) m_undo.erase(m_undo.begin());
        m_redo.clear();
    }

    void Editor::undo() {
        if (m_undo.empty()) return;
        m_redo.push_back(Snapshot{ m_doc.width, m_doc.height, m_doc.layers });
        Snapshot s = std::move(m_undo.back()); m_undo.pop_back();
        m_doc.width = s.width; m_doc.height = s.height; m_doc.layers = std::move(s.layers);
        if (m_activeLayer >= (int)m_doc.layers.size()) m_activeLayer = (int)m_doc.layers.size() - 1;
        m_status = "Undo.";
    }

    void Editor::redo() {
        if (m_redo.empty()) return;
        m_undo.push_back(Snapshot{ m_doc.width, m_doc.height, m_doc.layers });
        Snapshot s = std::move(m_redo.back()); m_redo.pop_back();
        m_doc.width = s.width; m_doc.height = s.height; m_doc.layers = std::move(s.layers);
        if (m_activeLayer >= (int)m_doc.layers.size()) m_activeLayer = (int)m_doc.layers.size() - 1;
        m_status = "Redo.";
    }

    void Editor::paintCell(int tx, int ty, int tileId) {
        Layer* l = activeLayer();
        if (l) l->set(tx, ty, m_doc.width, m_doc.height, tileId);
    }

    void Editor::floodFill(int tx, int ty, int newId) {
        Layer* l = activeLayer();
        if (!l) return;
        const int W = m_doc.width, H = m_doc.height;
        if (tx < 0 || ty < 0 || tx >= W || ty >= H) return;
        const int target = l->at(tx, ty, W, H);
        if (target == newId) return;
        std::vector<std::pair<int, int>> st;
        st.push_back({ tx, ty });
        while (!st.empty()) {
            auto [cx, cy] = st.back(); st.pop_back();
            if (cx < 0 || cy < 0 || cx >= W || cy >= H) continue;
            if (l->at(cx, cy, W, H) != target) continue;
            l->set(cx, cy, W, H, newId);
            st.push_back({ cx + 1, cy }); st.push_back({ cx - 1, cy });
            st.push_back({ cx, cy + 1 }); st.push_back({ cx, cy - 1 });
        }
    }

    void Editor::fillRectTiles(int x0, int y0, int x1, int y1, int tileId) {
        Layer* l = activeLayer();
        if (!l) return;
        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);
        x0 = std::max(0, x0); y0 = std::max(0, y0);
        x1 = std::min(m_doc.width - 1, x1); y1 = std::min(m_doc.height - 1, y1);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                l->set(x, y, m_doc.width, m_doc.height, tileId);
    }

    void Editor::copySelection() {
        Layer* l = activeLayer();
        if (!l || !m_hasSel) return;
        int x0 = std::min(m_selX0, m_selX1), x1 = std::max(m_selX0, m_selX1);
        int y0 = std::min(m_selY0, m_selY1), y1 = std::max(m_selY0, m_selY1);
        m_clipW = x1 - x0 + 1; m_clipH = y1 - y0 + 1;
        m_clip.assign((size_t)m_clipW * m_clipH, 0);
        for (int y = 0; y < m_clipH; ++y)
            for (int x = 0; x < m_clipW; ++x)
                m_clip[(size_t)y * m_clipW + x] = l->at(x0 + x, y0 + y, m_doc.width, m_doc.height);
        m_status = "Copied " + std::to_string(m_clipW) + "x" + std::to_string(m_clipH) + " tiles.";
    }

    void Editor::pasteAt(int tx, int ty) {
        Layer* l = activeLayer();
        if (!l || m_clip.empty()) return;
        pushUndo();
        for (int y = 0; y < m_clipH; ++y)
            for (int x = 0; x < m_clipW; ++x) {
                int v = m_clip[(size_t)y * m_clipW + x];
                if (v != 0) l->set(tx + x, ty + y, m_doc.width, m_doc.height, v);
            }
        m_status = "Pasted.";
    }

    void Editor::deleteSelection() {
        if (!m_hasSel) return;
        pushUndo();
        fillRectTiles(m_selX0, m_selY0, m_selX1, m_selY1, 0);
        m_status = "Cleared selection.";
    }

    Layer* Editor::ensurePaintLayer() {
        if (m_selectedTileset < 0 || m_selectedTileset >= (int)m_doc.tilesets.size())
            return activeLayer();

        Layer* l = activeLayer();
        if (l && l->tilesetIndex == m_selectedTileset) return l;

        const std::string& tsName = m_doc.tilesets[m_selectedTileset].name;

        // Rebind the active layer if it has no tiles yet.
        if (l) {
            const bool empty = std::all_of(l->data.begin(), l->data.end(),
                                           [](int v) { return v == 0; });
            if (empty) {
                l->tilesetIndex = m_selectedTileset;
                m_status = "Layer '" + l->name + "' now uses tileset '" + tsName + "'.";
                return l;
            }
        }

        // Otherwise switch to an existing layer bound to this tileset...
        for (int i = 0; i < (int)m_doc.layers.size(); ++i) {
            if (m_doc.layers[i].tilesetIndex == m_selectedTileset) {
                m_activeLayer = i;
                m_status = "Switched to layer '" + m_doc.layers[i].name + "' for tileset '" + tsName + "'.";
                return &m_doc.layers[i];
            }
        }

        // ...or create a new one.
        m_doc.layers.push_back(m_doc.makeEmptyLayer(tsName + " Layer", m_selectedTileset));
        m_activeLayer = (int)m_doc.layers.size() - 1;
        m_status = "Created layer '" + m_doc.layers.back().name + "' for tileset '" + tsName + "'.";
        return &m_doc.layers.back();
    }

    // ------------------------------------------------------------------ frame

    void Editor::update(float dt) { m_animClock += dt; }

    void Editor::draw() {
        // Global keyboard shortcuts (skip when typing in a field).
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput) {
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) undo();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) redo();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) copySelection();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) saveScene(false);
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deleteSelection();
            if (ImGui::IsKeyPressed(ImGuiKey_B)) m_tool = Tool::Brush;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) m_tool = Tool::Eraser;
            if (ImGui::IsKeyPressed(ImGuiKey_G)) m_tool = Tool::Bucket;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) m_tool = Tool::Rect;
            if (ImGui::IsKeyPressed(ImGuiKey_M)) m_tool = Tool::Select;
            if (ImGui::IsKeyPressed(ImGuiKey_I)) m_tool = Tool::Picker;
        }

        drawMenuBar();
        drawDockspaceLayout();

        drawToolbar();
        drawTilesetPanel();
        drawLayersPanel();
        drawPropertiesPanel();
        drawAnimatedPanel();
        drawCanvas();
        drawStatusBar();

        // ---- modals -------------------------------------------------------
        if (m_openTilesetModal) { ImGui::OpenPopup("Slice Tileset"); m_openTilesetModal = false; }
        if (ImGui::BeginPopupModal("Slice Tileset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("File: %s", m_tsPendingPath.c_str());
            ImGui::InputInt("Tile Width", &m_tsTileW);
            ImGui::InputInt("Tile Height", &m_tsTileH);
            ImGui::InputInt("Margin", &m_tsMargin);
            ImGui::InputInt("Spacing", &m_tsSpacing);
            m_tsTileW = std::max(1, m_tsTileW); m_tsTileH = std::max(1, m_tsTileH);
            m_tsMargin = std::max(0, m_tsMargin); m_tsSpacing = std::max(0, m_tsSpacing);
            if (ImGui::Button("Add Tileset", ImVec2(140, 0))) {
                Tileset t;
                t.name = baseName(m_tsPendingPath);
                t.sourcePath = m_tsPendingPath;
                t.tileW = m_tsTileW; t.tileH = m_tsTileH;
                t.margin = m_tsMargin; t.spacing = m_tsSpacing;
                m_doc.tilesets.push_back(std::move(t));
                loadTilesetTexture(m_doc.tilesets.back(), (int)m_doc.tilesets.size() - 1);
                m_selectedTileset = (int)m_doc.tilesets.size() - 1;
                m_selectedTileId = m_doc.tilesets.back().tileCount() > 0 ? 1 : 0;
                m_status = "Imported tileset " + m_doc.tilesets.back().name;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (m_openAnimModal) { ImGui::OpenPopup("Add Animated Tile"); m_openAnimModal = false; }
        if (ImGui::BeginPopupModal("Add Animated Tile", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("File: %s", m_animPendingPath.c_str());
            ImGui::InputInt("Overrides Tileset #", &m_animTileset);
            ImGui::InputInt("Overrides Tile Id (1-based)", &m_animTileId);
            ImGui::InputInt("Frame Width", &m_animFW);
            ImGui::InputInt("Frame Height", &m_animFH);
            ImGui::InputInt("Frame Count", &m_animFrames);
            ImGui::InputFloat("FPS", &m_animFps);
            m_animTileset = std::clamp(m_animTileset, 0, std::max(0, (int)m_doc.tilesets.size() - 1));
            m_animTileId = std::max(1, m_animTileId);
            m_animFW = std::max(1, m_animFW); m_animFH = std::max(1, m_animFH);
            m_animFrames = std::max(1, m_animFrames); m_animFps = std::max(0.0f, m_animFps);
            if (ImGui::Button("Add", ImVec2(140, 0))) {
                AnimatedTile a;
                a.tilesetIndex = m_animTileset; a.tileId = m_animTileId;
                a.sheetPath = m_animPendingPath;
                a.frameW = m_animFW; a.frameH = m_animFH;
                a.frames = m_animFrames; a.fps = m_animFps;
                m_doc.animatedTiles.push_back(std::move(a));
                loadAnimatedTexture(m_doc.animatedTiles.back(), (int)m_doc.animatedTiles.size() - 1);
                m_status = "Added animated tile.";
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (m_openNewModal) { ImGui::OpenPopup("New Scene"); m_openNewModal = false; }
        if (ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputInt("Map Width (tiles)", &m_newW);
            ImGui::InputInt("Map Height (tiles)", &m_newH);
            ImGui::InputInt("Tile Width (px)", &m_newTileW);
            ImGui::InputInt("Tile Height (px)", &m_newTileH);
            m_newW = std::max(1, m_newW); m_newH = std::max(1, m_newH);
            m_newTileW = std::max(1, m_newTileW); m_newTileH = std::max(1, m_newTileH);
            if (ImGui::Button("Create", ImVec2(140, 0))) {
                newScene();
                m_doc.width = m_newW; m_doc.height = m_newH;
                m_doc.tileW = m_newTileW; m_doc.tileH = m_newTileH;
                for (auto& l : m_doc.layers) l.data.assign((size_t)m_doc.width * m_doc.height, 0);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    void Editor::drawMenuBar() {
        if (!ImGui::BeginMainMenuBar()) return;
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene...")) m_openNewModal = true;
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) openScene();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) saveScene(false);
            if (ImGui::MenuItem("Save Scene As...")) saveScene(true);
            ImGui::Separator();
            if (ImGui::MenuItem("Import Tileset...")) importTileset();
            if (ImGui::MenuItem("Import Animated Sheet...")) importAnimatedTile();
            ImGui::Separator();
            if (ImGui::MenuItem("Export Engine Map JSON...")) exportMap();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) m_quit = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_undo.empty())) undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !m_redo.empty())) redo();
            ImGui::Separator();
            if (ImGui::MenuItem("Copy Selection", "Ctrl+C", false, m_hasSel)) copySelection();
            if (ImGui::MenuItem("Delete Selection", "Del", false, m_hasSel)) deleteSelection();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Show Grid", "", &m_showGrid);
            if (ImGui::MenuItem("Reset Camera")) m_viewInit = false;
            if (ImGui::MenuItem("Reset Layout")) m_layoutBuilt = false;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    void Editor::drawDockspaceLayout() {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGuiID dockId = ImGui::DockSpaceOverViewport(0, vp, ImGuiDockNodeFlags_PassthruCentralNode);
        if (m_layoutBuilt) return;
        m_layoutBuilt = true;

        ImGui::DockBuilderRemoveNode(dockId);
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);

        ImGuiID center = dockId;
        ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
        ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.22f, nullptr, &center);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.26f, nullptr, &center);
        ImGuiID leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.55f, nullptr, &left);
        ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.5f, nullptr, &right);

        ImGui::DockBuilderDockWindow("Tilesets", left);
        ImGui::DockBuilderDockWindow("Palette", leftBottom);
        ImGui::DockBuilderDockWindow("Layers", right);
        ImGui::DockBuilderDockWindow("Properties", rightBottom);
        ImGui::DockBuilderDockWindow("Tools", bottom);
        ImGui::DockBuilderDockWindow("Animated Tiles", bottom);
        ImGui::DockBuilderDockWindow("Canvas", center);
        ImGui::DockBuilderFinish(dockId);
    }

    void Editor::drawToolbar() {
        ImGui::Begin("Tools");
        auto toolRadio = [&](const char* label, Tool t) {
            if (ImGui::RadioButton(label, m_tool == t)) m_tool = t;
        };
        toolRadio("Brush (B)", Tool::Brush);
        toolRadio("Eraser (E)", Tool::Eraser);
        toolRadio("Rect Fill (R)", Tool::Rect);
        toolRadio("Bucket (G)", Tool::Bucket);
        toolRadio("Select (M)", Tool::Select);
        toolRadio("Picker (I)", Tool::Picker);
        ImGui::Separator();
        ImGui::Checkbox("Show Grid", &m_showGrid);
        ImGui::SliderFloat("Zoom", &m_zoom, 0.1f, 8.0f, "%.2fx");
        ImGui::Separator();
        ImGui::Text("Selected: tileset %d, tile %d", m_selectedTileset, m_selectedTileId);
        if (!m_clip.empty())
            ImGui::Text("Clipboard: %dx%d", m_clipW, m_clipH);
        ImGui::End();
    }

    void Editor::drawTilesetPanel() {
        ImGui::Begin("Tilesets");
        if (ImGui::Button("Import Tileset...")) importTileset();
        ImGui::SameLine();
        if (ImGui::Button("Reload Textures")) reloadAllTextures();
        ImGui::Separator();

        for (int i = 0; i < (int)m_doc.tilesets.size(); ++i) {
            auto& t = m_doc.tilesets[i];
            ImGui::PushID(i);
            bool sel = (m_selectedTileset == i);
            if (ImGui::Selectable(t.name.c_str(), sel)) m_selectedTileset = i;
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f);
            if (ImGui::SmallButton("X")) {
                // Remove tileset; keep it simple (does not remap layer indices).
                m_doc.tilesets.erase(m_doc.tilesets.begin() + i);
                if (m_selectedTileset >= (int)m_doc.tilesets.size())
                    m_selectedTileset = (int)m_doc.tilesets.size() - 1;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        if (m_selectedTileset >= 0 && m_selectedTileset < (int)m_doc.tilesets.size()) {
            auto& t = m_doc.tilesets[m_selectedTileset];
            ImGui::Separator();
            ImGui::Text("Slice: %s (%dx%d px)", t.name.c_str(), t.texW, t.texH);
            bool ch = false;
            ch |= ImGui::InputInt("Tile W", &t.tileW);
            ch |= ImGui::InputInt("Tile H", &t.tileH);
            ch |= ImGui::InputInt("Margin", &t.margin);
            ch |= ImGui::InputInt("Spacing", &t.spacing);
            if (ch) {
                t.tileW = std::max(1, t.tileW); t.tileH = std::max(1, t.tileH);
                t.margin = std::max(0, t.margin); t.spacing = std::max(0, t.spacing);
            }
            ImGui::Text("Grid: %d x %d  (%d tiles)", t.columns(), t.rows(), t.tileCount());
        }
        ImGui::End();

        // ---- palette (pick a tile) ----
        ImGui::Begin("Palette");
        if (m_selectedTileset >= 0 && m_selectedTileset < (int)m_doc.tilesets.size()) {
            auto& t = m_doc.tilesets[m_selectedTileset];
            if (t.glTexId == 0) {
                ImGui::TextUnformatted("Texture not loaded. Click 'Reload Textures'.");
            } else {
                // Eraser swatch.
                if (ImGui::Button(m_selectedTileId == 0 ? "[Eraser]" : "Eraser")) m_selectedTileId = 0;
                ImGui::Separator();

                const float maxDim = (float)std::max(t.tileW, t.tileH);
                const float scale = maxDim > 0 ? (32.0f / maxDim) : 1.0f;
                const ImVec2 sz((float)t.tileW * scale, (float)t.tileH * scale);
                const float avail = ImGui::GetContentRegionAvail().x;
                const int perRow = std::max(1, (int)(avail / (sz.x + 6.0f)));

                const int count = t.tileCount();
                for (int idx = 0; idx < count; ++idx) {
                    int px, py; t.tilePixel(idx, px, py);
                    ImVec2 uv0, uv1; subUV(t.texW, t.texH, px, py, t.tileW, t.tileH, uv0, uv1);
                    ImGui::PushID(idx);
                    bool clicked = ImGui::ImageButton("tile", (ImTextureID)t.glTexId, sz, uv0, uv1);
                    if (m_selectedTileId == idx + 1) {
                        ImGui::GetWindowDrawList()->AddRect(
                            ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                            IM_COL32(255, 210, 40, 255), 0.0f, 0, 2.5f);
                    }
                    if (clicked) { m_selectedTileId = idx + 1; if (m_tool == Tool::Eraser) m_tool = Tool::Brush; }
                    ImGui::PopID();
                    if ((idx + 1) % perRow != 0) ImGui::SameLine();
                }
            }
        } else {
            ImGui::TextUnformatted("Import or select a tileset.");
        }
        ImGui::End();
    }

    void Editor::drawLayersPanel() {
        ImGui::Begin("Layers");
        if (ImGui::Button("Add")) {
            pushUndo();
            m_doc.layers.push_back(m_doc.makeEmptyLayer("Layer " + std::to_string(m_doc.layers.size() + 1),
                                                        std::max(0, m_selectedTileset)));
            m_activeLayer = (int)m_doc.layers.size() - 1;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete") && m_doc.layers.size() > 1) {
            pushUndo();
            m_doc.layers.erase(m_doc.layers.begin() + m_activeLayer);
            m_activeLayer = std::min(m_activeLayer, (int)m_doc.layers.size() - 1);
        }
        ImGui::SameLine();
        if (ImGui::Button("Up") && m_activeLayer > 0) {
            std::swap(m_doc.layers[m_activeLayer], m_doc.layers[m_activeLayer - 1]); m_activeLayer--;
        }
        ImGui::SameLine();
        if (ImGui::Button("Down") && m_activeLayer < (int)m_doc.layers.size() - 1) {
            std::swap(m_doc.layers[m_activeLayer], m_doc.layers[m_activeLayer + 1]); m_activeLayer++;
        }
        ImGui::Separator();

        // Draw top-most layer first for a natural stack view.
        for (int i = (int)m_doc.layers.size() - 1; i >= 0; --i) {
            auto& l = m_doc.layers[i];
            ImGui::PushID(i);
            ImGui::Checkbox("##vis", &l.visible);
            ImGui::SameLine();
            bool sel = (m_activeLayer == i);
            if (ImGui::Selectable(l.name.c_str(), sel)) m_activeLayer = i;
            ImGui::PopID();
        }

        if (m_activeLayer >= 0 && m_activeLayer < (int)m_doc.layers.size()) {
            auto& l = m_doc.layers[m_activeLayer];
            ImGui::Separator();
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s", l.name.c_str());
            if (ImGui::InputText("Name", buf, sizeof(buf))) l.name = buf;

            // per-layer tileset selector
            if (!m_doc.tilesets.empty()) {
                const char* cur = (l.tilesetIndex >= 0 && l.tilesetIndex < (int)m_doc.tilesets.size())
                    ? m_doc.tilesets[l.tilesetIndex].name.c_str() : "(none)";
                if (ImGui::BeginCombo("Tileset", cur)) {
                    for (int i = 0; i < (int)m_doc.tilesets.size(); ++i)
                        if (ImGui::Selectable(m_doc.tilesets[i].name.c_str(), l.tilesetIndex == i))
                            l.tilesetIndex = i;
                    ImGui::EndCombo();
                }
            }
        }
        ImGui::End();
    }

    void Editor::drawPropertiesPanel() {
        ImGui::Begin("Properties");
        ImGui::TextUnformatted("Map");
        // Re-seed the edit fields only when the document size changes externally
        // (new scene, load, resize) so per-frame reassignment doesn't clobber
        // the value the user is typing before they press "Resize Map".
        if (m_doc.width != m_propSeenW || m_doc.height != m_propSeenH) {
            m_propW = m_doc.width; m_propH = m_doc.height;
            m_propSeenW = m_doc.width; m_propSeenH = m_doc.height;
        }
        ImGui::InputInt("Width", &m_propW);
        ImGui::InputInt("Height", &m_propH);
        if (ImGui::Button("Resize Map")) {
            pushUndo();
            m_doc.resizeLayers(std::max(1, m_propW), std::max(1, m_propH));
            m_propSeenW = m_doc.width; m_propSeenH = m_doc.height;
            m_viewInit = false;
        }
        ImGui::InputInt("Tile W (px)", &m_doc.tileW);
        ImGui::InputInt("Tile H (px)", &m_doc.tileH);
        m_doc.tileW = std::max(1, m_doc.tileW); m_doc.tileH = std::max(1, m_doc.tileH);
        ImGui::InputFloat("Tile Pixel Scale", &m_doc.tilePixelScale);

        ImGui::Separator();
        ImGui::TextUnformatted("Selected tile");
        if (m_selectedTileset >= 0 && m_selectedTileset < (int)m_doc.tilesets.size() && m_selectedTileId > 0) {
            auto& t = m_doc.tilesets[m_selectedTileset];
            bool solid = t.solidTiles.count(m_selectedTileId) > 0;
            if (ImGui::Checkbox("Solid (collision)", &solid)) {
                if (solid) t.solidTiles.insert(m_selectedTileId);
                else t.solidTiles.erase(m_selectedTileId);
            }
        } else {
            ImGui::TextDisabled("None");
        }
        ImGui::End();
    }

    void Editor::drawAnimatedPanel() {
        ImGui::Begin("Animated Tiles");
        if (ImGui::Button("Import Animated Sheet...")) importAnimatedTile();
        ImGui::Separator();
        for (int i = 0; i < (int)m_doc.animatedTiles.size(); ++i) {
            auto& a = m_doc.animatedTiles[i];
            ImGui::PushID(i);
            ImGui::Text("#%d  tileset %d, tile %d", i, a.tilesetIndex, a.tileId);
            if (a.glTexId != 0) {
                int frame = currentAnimFrame(a);
                int cols = a.columns();
                int fx = cols > 0 ? (frame % cols) * a.frameW : 0;
                int fy = cols > 0 ? (frame / cols) * a.frameH : 0;
                ImVec2 uv0, uv1; subUV(a.texW, a.texH, fx, fy, a.frameW, a.frameH, uv0, uv1);
                ImGui::SameLine();
                ImGui::Image((ImTextureID)a.glTexId, ImVec2(28, 28), uv0, uv1);
            }
            ImGui::SetNextItemWidth(90);
            ImGui::InputInt("frames", &a.frames); a.frames = std::max(1, a.frames);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90);
            ImGui::InputFloat("fps", &a.fps); a.fps = std::max(0.0f, a.fps);
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                m_doc.animatedTiles.erase(m_doc.animatedTiles.begin() + i);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
            ImGui::Separator();
        }
        ImGui::End();
    }

    void Editor::drawStatusBar() {
        // Small always-on info line docked with the Tools window area is enough;
        // show it as an overlay at the bottom of the main viewport.
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImVec2 pos(vp->WorkPos.x + 8, vp->WorkPos.y + vp->WorkSize.y - 24);
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("##status", nullptr, flags)) {
            ImGui::Text("%s   |   hover (%d, %d)   |   %dx%d map", m_status.c_str(),
                        m_hoverX, m_hoverY, m_doc.width, m_doc.height);
        }
        ImGui::End();
    }

    // ------------------------------------------------------------------ canvas

    void Editor::drawCanvas() {
        ImGui::Begin("Canvas");

        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        if (canvasSize.x < 32) canvasSize.x = 32;
        if (canvasSize.y < 32) canvasSize.y = 32;

        const float cellW = m_doc.tileW * m_zoom;
        const float cellH = m_doc.tileH * m_zoom;

        if (!m_viewInit) {
            m_pan.x = (canvasSize.x - m_doc.width * cellW) * 0.5f;
            m_pan.y = (canvasSize.y - m_doc.height * cellH) * 0.5f;
            m_viewInit = true;
        }

        ImGui::InvisibleButton("##canvas", canvasSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
        const bool hovered = ImGui::IsItemHovered();
        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

        // background
        dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                          IM_COL32(30, 30, 34, 255));

        // pan (middle-drag) & zoom (wheel, cursor-anchored)
        if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            m_pan.x += io.MouseDelta.x; m_pan.y += io.MouseDelta.y;
        }
        if (hovered && io.MouseWheel != 0.0f) {
            const float oldZoom = m_zoom;
            m_zoom = std::clamp(m_zoom * (io.MouseWheel > 0 ? 1.1f : 1.0f / 1.1f), 0.1f, 8.0f);
            const float ocw = m_doc.tileW * oldZoom, och = m_doc.tileH * oldZoom;
            const float wx = (io.MousePos.x - (canvasPos.x + m_pan.x)) / ocw;
            const float wy = (io.MousePos.y - (canvasPos.y + m_pan.y)) / och;
            m_pan.x = io.MousePos.x - canvasPos.x - wx * (m_doc.tileW * m_zoom);
            m_pan.y = io.MousePos.y - canvasPos.y - wy * (m_doc.tileH * m_zoom);
        }

        const ImVec2 origin(canvasPos.x + m_pan.x, canvasPos.y + m_pan.y);
        const float ncw = m_doc.tileW * m_zoom, nch = m_doc.tileH * m_zoom;

        auto tileTL = [&](int tx, int ty) {
            return ImVec2(origin.x + tx * ncw, origin.y + ty * nch);
        };

        // map bounds background
        dl->AddRectFilled(origin, ImVec2(origin.x + m_doc.width * ncw, origin.y + m_doc.height * nch),
                          IM_COL32(18, 18, 20, 255));

        // visible tile range (cull)
        int minTX = std::max(0, (int)std::floor((canvasPos.x - origin.x) / ncw));
        int minTY = std::max(0, (int)std::floor((canvasPos.y - origin.y) / nch));
        int maxTX = std::min(m_doc.width, (int)std::ceil((canvasPos.x + canvasSize.x - origin.x) / ncw));
        int maxTY = std::min(m_doc.height, (int)std::ceil((canvasPos.y + canvasSize.y - origin.y) / nch));

        // draw layers bottom-to-top
        for (const auto& l : m_doc.layers) {
            if (!l.visible) continue;
            if (l.tilesetIndex < 0 || l.tilesetIndex >= (int)m_doc.tilesets.size()) continue;
            const Tileset& ts = m_doc.tilesets[l.tilesetIndex];
            if (ts.glTexId == 0) continue;
            for (int ty = minTY; ty < maxTY; ++ty) {
                for (int tx = minTX; tx < maxTX; ++tx) {
                    int id = l.at(tx, ty, m_doc.width, m_doc.height);
                    if (id <= 0) continue;
                    ImVec2 p0 = tileTL(tx, ty);
                    ImVec2 p1(p0.x + ncw, p0.y + nch);

                    // animated override?
                    const AnimatedTile* anim = findAnimated(l.tilesetIndex, id);
                    if (anim && anim->glTexId != 0) {
                        int frame = currentAnimFrame(*anim);
                        int cols = anim->columns();
                        int fx = cols > 0 ? (frame % cols) * anim->frameW : 0;
                        int fy = cols > 0 ? (frame / cols) * anim->frameH : 0;
                        ImVec2 uv0, uv1; subUV(anim->texW, anim->texH, fx, fy, anim->frameW, anim->frameH, uv0, uv1);
                        dl->AddImage((ImTextureID)anim->glTexId, p0, p1, uv0, uv1);
                    } else {
                        int px, py; ts.tilePixel(id - 1, px, py);
                        ImVec2 uv0, uv1; subUV(ts.texW, ts.texH, px, py, ts.tileW, ts.tileH, uv0, uv1);
                        dl->AddImage((ImTextureID)ts.glTexId, p0, p1, uv0, uv1);
                    }
                }
            }
        }

        // grid
        if (m_showGrid && ncw >= 4.0f && nch >= 4.0f) {
            const ImU32 gcol = IM_COL32(255, 255, 255, 30);
            for (int x = 0; x <= m_doc.width; ++x)
                dl->AddLine(ImVec2(origin.x + x * ncw, origin.y),
                            ImVec2(origin.x + x * ncw, origin.y + m_doc.height * nch), gcol);
            for (int y = 0; y <= m_doc.height; ++y)
                dl->AddLine(ImVec2(origin.x, origin.y + y * nch),
                            ImVec2(origin.x + m_doc.width * ncw, origin.y + y * nch), gcol);
        }
        // map border
        dl->AddRect(origin, ImVec2(origin.x + m_doc.width * ncw, origin.y + m_doc.height * nch),
                    IM_COL32(255, 255, 255, 90));

        // hover tile
        m_hoverX = m_hoverY = -1;
        if (hovered) {
            int tx = (int)std::floor((io.MousePos.x - origin.x) / ncw);
            int ty = (int)std::floor((io.MousePos.y - origin.y) / nch);
            if (tx >= 0 && ty >= 0 && tx < m_doc.width && ty < m_doc.height) {
                m_hoverX = tx; m_hoverY = ty;
                ImVec2 p0 = tileTL(tx, ty);
                dl->AddRect(p0, ImVec2(p0.x + ncw, p0.y + nch), IM_COL32(255, 220, 40, 220), 0.0f, 0, 2.0f);
            }
        }

        // ---- tool interaction ----
        const bool inMap = (m_hoverX >= 0 && m_hoverY >= 0);
        if (hovered && inMap) {
            const int paintId = (m_tool == Tool::Eraser) ? 0 : m_selectedTileId;

            if (m_tool == Tool::Brush || m_tool == Tool::Eraser) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    pushUndo();
                    if (m_tool == Tool::Brush && m_selectedTileId > 0) ensurePaintLayer();
                    m_stroking = true;
                }
                if (m_stroking && ImGui::IsMouseDown(ImGuiMouseButton_Left)) paintCell(m_hoverX, m_hoverY, paintId);
            } else if (m_tool == Tool::Bucket) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    pushUndo();
                    if (m_selectedTileId > 0) ensurePaintLayer();
                    floodFill(m_hoverX, m_hoverY, paintId);
                }
            } else if (m_tool == Tool::Picker) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    Layer* l = activeLayer();
                    if (l) {
                        int id = l->at(m_hoverX, m_hoverY, m_doc.width, m_doc.height);
                        if (id > 0) { m_selectedTileId = id; m_selectedTileset = l->tilesetIndex; m_tool = Tool::Brush; }
                    }
                }
            } else if (m_tool == Tool::Rect || m_tool == Tool::Select) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    m_boxDrag = true; m_dragX0 = m_hoverX; m_dragY0 = m_hoverY;
                }
            }
        }

        // box drag preview + commit (works even if released outside map, clamps)
        if (m_boxDrag) {
            int cx = std::clamp(m_hoverX < 0 ? m_dragX0 : m_hoverX, 0, m_doc.width - 1);
            int cy = std::clamp(m_hoverY < 0 ? m_dragY0 : m_hoverY, 0, m_doc.height - 1);
            int x0 = std::min(m_dragX0, cx), x1 = std::max(m_dragX0, cx);
            int y0 = std::min(m_dragY0, cy), y1 = std::max(m_dragY0, cy);
            ImVec2 a = tileTL(x0, y0), b = tileTL(x1 + 1, y1 + 1);
            ImU32 col = (m_tool == Tool::Select) ? IM_COL32(80, 170, 255, 200) : IM_COL32(120, 255, 120, 200);
            dl->AddRect(a, b, col, 0.0f, 0, 2.0f);
            dl->AddRectFilled(a, b, (col & 0x00FFFFFF) | 0x22000000);

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (m_tool == Tool::Rect) {
                    pushUndo();
                    if (m_selectedTileId > 0) ensurePaintLayer();
                    fillRectTiles(x0, y0, x1, y1, (m_selectedTileId));
                } else { // Select
                    m_hasSel = true;
                    m_selX0 = x0; m_selY0 = y0; m_selX1 = x1; m_selY1 = y1;
                }
                m_boxDrag = false;
            }
        }

        // stop stroking on release
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) m_stroking = false;

        // paste with right-click when clipboard present
        if (hovered && inMap && !m_clip.empty() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            pasteAt(m_hoverX, m_hoverY);

        // persistent selection outline
        if (m_hasSel) {
            ImVec2 a = tileTL(std::min(m_selX0, m_selX1), std::min(m_selY0, m_selY1));
            ImVec2 b = tileTL(std::max(m_selX0, m_selX1) + 1, std::max(m_selY0, m_selY1) + 1);
            dl->AddRect(a, b, IM_COL32(80, 170, 255, 230), 0.0f, 0, 2.0f);
        }

        dl->PopClipRect();
        ImGui::End();
    }

}
