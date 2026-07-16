#include "HBE/Platform/SDLPlatform.h"
#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#include <SDL3/SDL.h>

#include "Editor/Editor.h"

using namespace HBE;

int main() {
    Core::SetLogLevel(Core::LogLevel::Info);

    Platform::WindowConfig cfg;
    cfg.title = "HBMapMaker";
    cfg.width = 1600;
    cfg.height = 900;
    cfg.useOpenGL = true;
    cfg.mode = Platform::WindowMode::Windowed;
    cfg.vsync = true;

    Platform::SDLPlatform platform;
    if (!platform.initialize(cfg)) {
        Core::LogError("HBMapMaker: platform init failed.");
        return -1;
    }

    Renderer::GLRenderer gl;
    if (!gl.initialize(platform)) {
        Core::LogError("HBMapMaker: GL init failed.");
        return -1;
    }

    // Not strictly required (textures load by absolute path) but keeps engine
    // asset helpers/log happy.
    Core::AssetPaths::Config assetCfg{};
    assetCfg.organization = "HBMapMaker";
    assetCfg.application = "HBMapMaker";
    assetCfg.siblingProjectNames = { "HBMapMaker" };
    Core::AssetPaths::Initialize(assetCfg);

    Renderer::ResourceCache resources;

    // ---- Dear ImGui ----
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "HBMapMaker_imgui.ini";
    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForOpenGL(platform.getWindow(), platform.getGLContext())) {
        Core::LogError("HBMapMaker: ImGui SDL3 backend init failed.");
        return -1;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        Core::LogError("HBMapMaker: ImGui OpenGL3 backend init failed.");
        return -1;
    }

    hbmm::Editor editor(resources);
    Core::LogInfo("HBMapMaker started.");

    Uint64 last = SDL_GetPerformanceCounter();
    bool running = true;
    while (running) {
        platform.pumpEvents([&](const SDL_Event& e) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                e.window.windowID == SDL_GetWindowID(platform.getWindow()))
                running = false;
        });

        const Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)((now - last) / (double)SDL_GetPerformanceFrequency());
        last = now;
        if (dt > 0.25f) dt = 0.25f;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        editor.update(dt);
        editor.draw();
        if (editor.wantsQuit()) running = false;

        ImGui::Render();

        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(platform.getWindow(), &w, &h);
        if (w <= 0 || h <= 0) { w = platform.currentWidth(); h = platform.currentHeight(); }

        gl.beginFrameFullWindow(w, h);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        gl.endFrame(platform);   // swaps buffers
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    platform.shutdown();
    return 0;
}
