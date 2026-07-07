#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "HBE/Core/Event.h"
#include "HBE/Renderer/UI/UIContext.h"
#include "HBE/Renderer/UI/UIRect.h"

struct SDL_Window; // forward decl (SDL3)

class DevConsole {
public:
    using CommandFn = std::function<void(const std::vector<std::string>& args)>;

    // NEW: supply window for SDL_StartTextInput(SDL_Window*)
    void setWindow(SDL_Window* win) { m_window = win; }

    void setOpen(bool open);
    void toggle();
    bool isOpen() const { return m_open; }

    void clear();
    void print(const std::string& line);

    void registerCommand(const std::string& name, const std::string& help, CommandFn fn);

    void onEvent(HBE::Core::Event& e);
    void draw(HBE::Renderer::UI::UIContext& ui, const HBE::Renderer::UI::UIRect& rect, float dt);

private:
    SDL_Window* m_window = nullptr;   // NEW

    bool m_open = false;

    std::string m_input;
    std::vector<std::string> m_log;

    std::vector<std::string> m_history;
    int m_historyIndex = -1;

    struct Cmd {
        std::string help;
        CommandFn fn;
    };
    std::unordered_map<std::string, Cmd> m_cmds;

    float m_cursorTimer = 0.0f;
    bool  m_cursorOn = true;

    void submitLine(const std::string& line);
    void execute(const std::string& line);

    static std::vector<std::string> splitWS(const std::string& s);
};