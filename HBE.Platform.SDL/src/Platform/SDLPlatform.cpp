#include "HBE/Platform/SDLPlatform.h"
#include "HBE/Platform/Input.h"

#include "HBE/Core/Log.h"
#include "HBE/Core/Time.h"
#include <SDL3/SDL_scancode.h>

namespace HBE::Platform {

    using HBE::Core::LogError;
    using HBE::Core::LogInfo;
    using HBE::Core::LogTrace;
    using HBE::Core::LogFatal;
    using HBE::Core::LogWarn;

    SDLPlatform::~SDLPlatform() {
        shutdown();
    }

    bool SDLPlatform::initialize(const WindowConfig& config) {
        if (m_initialized) {
            return true;
        }

        SDL_ClearError();

        int rc = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

        // SDL_Init: 0 = success, <0 = error, >0 = already initialized
        if (rc < 0) {
            std::string err = SDL_GetError();
            LogError(
                "SDL_Init(SDL_INIT_VIDEO) failed. rc=" +
                std::to_string(rc) + " msg='" + err + "'"
            );
            return false;
        }

        if (rc > 0) {
            LogInfo("SDL_Init(SDL_INIT_VIDEO) rc=" + std::to_string(rc) +
                " (video subsystem already initialized).");
        }

        HBE::Platform::Input::Initialize();

        Uint32 windowFlags = SDL_WINDOW_RESIZABLE;
        if (config.useOpenGL) {
            windowFlags |= SDL_WINDOW_OPENGL;
        }

        SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

        m_window = SDL_CreateWindow(
            config.title.c_str(),
            config.width,
            config.height,
            windowFlags
        );

        if (!m_window) {
            LogError(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
            HBE::Platform::Input::Shutdown();
            SDL_Quit();
            return false;
        }

        m_width = config.width;
        m_height = config.height;
        m_mode = config.mode;
        m_vsync = config.vsync;

        if (config.useOpenGL) {
            if (!createGLContext(config)) {
                LogFatal("Failed to create OpenGL context.");
                //Explicit cleanup since m_initialized is still false
                if (m_window) {
                    SDL_DestroyWindow(m_window);
                    m_window = nullptr;
                }
                HBE::Platform::Input::Shutdown();
                SDL_Quit();
                return false;
            }
        }

        // Apply initial graphics settings (mode, vsync, etc.)
        if (!applyGraphicsSettings(config)) {
            LogWarn("SDLPlatform: failed to fully apply initial graphics settings.");
        }

        m_initialized = true;
        LogInfo("SDLPlatform initialized successfully.");
        return true;
    }

    bool SDLPlatform::createGLContext(const WindowConfig& config) {
        // Request OpenGL 3.3 core
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

        m_glContext = SDL_GL_CreateContext(m_window);
        if (!m_glContext) {
            LogError(std::string("SDL_GL_CreateContext failed: ") + SDL_GetError());
            return false;
        }

        if (!SDL_GL_MakeCurrent(m_window, m_glContext)) {
            LogError(std::string("SDL_GL_MakeCurrent failed: ") + SDL_GetError());
            SDL_GL_DestroyContext(m_glContext);
            m_glContext = nullptr;
            return false;
        }

        if (SDL_GL_GetCurrentContext() == nullptr) {
            LogError("SDL_GL_GetCurrentContext returned null after MakeCurrent!");
            SDL_GL_DestroyContext(m_glContext);
            m_glContext = nullptr;
            return false;
        }

        // Initial vsync from config
        applyVSync(config.vsync);

        LogInfo("OpenGL context created successfully.");
        return true;
    }

    // Apply full graphics config (mode + size + vsync).
    bool SDLPlatform::applyGraphicsSettings(const WindowConfig& config) {
        if (!m_window) return false;

        bool ok = true;

        // Change fullscreen / windowed only if different
        if (config.mode != m_mode) {
            ok = setWindowMode(config.mode) && ok;
        }

        // Window size only matters in windowed mode and only if it changed
        if (config.mode == WindowMode::Windowed &&
            (config.width != m_width || config.height != m_height)) {
            ok = setWindowSize(config.width, config.height) && ok;
        }

        // Only touch vsync if it actually changed AND we have a GL context
        if (m_glContext && config.vsync != m_vsync) {
            ok = applyVSync(config.vsync) && ok;
        }

        // IMPORTANT: we do NOT overwrite m_width / m_height / m_mode / m_vsync here.
        // Those members are updated INSIDE setWindowMode / setWindowSize / applyVSync
        // *only when those operations succeed*, keeping our state in sync with SDL.

        return ok;
    }



    bool SDLPlatform::setWindowSize(int width, int height) {
        if (!m_window) return false;

        if (!SDL_SetWindowSize(m_window, width, height)) {
            LogWarn(std::string("SDL_SetWindowSize failed: ") + SDL_GetError());
            return false;
        }

        m_width = width;
        m_height = height;              
        return true;
    }

    bool SDLPlatform::setWindowMode(WindowMode mode) {
        if (!m_window) return false;

        bool result = true;

        switch (mode) {
        case WindowMode::Windowed:
            // Exit fullscreen state
            result = SDL_SetWindowFullscreen(m_window, false);
            if (!result) {
                LogError(std::string("SDL_SetWindowFullscreen(false) failed: ") + SDL_GetError());
            }
            break;

        case WindowMode::FullscreenDesktop:
            // Borderless fullscreen at desktop resolution (SDL3 default)
            result = SDL_SetWindowFullscreen(m_window, true);  // true = fullscreen
            if (!result) {
                LogError(std::string("SDL_SetWindowFullscreen(true) failed: ") + SDL_GetError());
            }
            break;
        }

        if (result) {
            m_mode = mode;
        }

        return result;
    }

    bool SDLPlatform::applyVSync(bool enabled) {
        if (!m_glContext) return false;

        int rc = SDL_GL_SetSwapInterval(enabled ? 1 : 0);
        if (rc == 0) {
            m_vsync = enabled;
            return true;
        }

        // Only complain once so we don't spam logs every toggle
        static bool s_warnedOnce = false;
        if (!s_warnedOnce) {
            LogWarn(std::string("SDL_GL_SetSwapInterval failed: ") + SDL_GetError());
            s_warnedOnce = true;
        }
        return false;
    }

    bool SDLPlatform::pumpEvents(const std::function<void(const SDL_Event&)>& onEvent) {
        if (!m_initialized) return true;

        SDL_Event e;
        bool quit = false;

        while (SDL_PollEvent(&e)) {
            if (onEvent) onEvent(e);

            if (e.type == SDL_EVENT_QUIT) {
                quit = true;
            }
        }
        return quit;
    }

    void SDLPlatform::shutdown() {
        if (!m_initialized) {
            return;
        }

        if (m_glContext) {
            SDL_GL_DestroyContext(m_glContext);
            m_glContext = nullptr;
        }

        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        HBE::Platform::Input::Shutdown();

        SDL_Quit();
        m_initialized = false;

        LogInfo("SDLPlatform shutdown complete.");
    }

    bool SDLPlatform::pollQuitRequested() {
        if (!m_initialized) {
            return true;
        }

        SDL_Event e;
        bool quit = false;

        while (SDL_PollEvent(&e)) {
            HBE::Platform::Input::HandleEvent(e);

            switch (e.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                // Escape to quit is handy during early engine work
                if (e.key.key == SDL_SCANCODE_ESCAPE) {
                    LogInfo("SDLPlatform::pollQuitRequested: EscapeKeyPressed!");
                    quit = true;
                }
                break;
            default:
                break;
            }
        }
        return quit;
    }

    void SDLPlatform::delayMillis(std::uint32_t ms) {
        SDL_Delay(ms);
    }

    void SDLPlatform::swapBuffers() {
        if (m_window && m_glContext) {
            SDL_GL_SwapWindow(m_window);
        }
    }

} // namespace HBE::Platform
