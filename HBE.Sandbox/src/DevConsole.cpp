#include "DevConsole.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL.h>

using namespace HBE::Core;
using namespace HBE::Renderer::UI;

void DevConsole::clear() {
    m_log.clear();
}

void DevConsole::print(const std::string& line) {
    m_log.push_back(line);

    // cap log size
    const size_t MAX_LINES = 200;
    if (m_log.size() > MAX_LINES) {
        m_log.erase(m_log.begin(), m_log.begin() + (m_log.size() - MAX_LINES));
    }
}

void DevConsole::registerCommand(const std::string& name, const std::string& help, CommandFn fn) {
    m_cmds[name] = Cmd{ help, std::move(fn) };
}

std::vector<std::string> DevConsole::splitWS(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

void DevConsole::submitLine(const std::string& line) {
    if (line.empty()) return;

    m_history.push_back(line);
    const int MAX_HIST = 50;
    if ((int)m_history.size() > MAX_HIST) {
        m_history.erase(m_history.begin());
    }
    m_historyIndex = -1;

    print(std::string("> ") + line);
    execute(line);
}

void DevConsole::execute(const std::string& line) {
    auto tokens = splitWS(line);
    if (tokens.empty()) return;

    std::string cmd = tokens[0];
    tokens.erase(tokens.begin());

    auto it = m_cmds.find(cmd);
    if (it == m_cmds.end()) {
        print("Unknown command: " + cmd + " (try: help)");
        return;
    }

    it->second.fn(tokens);
}

void DevConsole::onEvent(HBE::Core::Event& e) {
    if (!m_open) return;

    // If console is open, it should “eat” typing keys so gameplay doesn’t react.
    if (e.type() == EventType::TextInput) {
        auto& te = static_cast<TextInputEvent&>(e);
        m_input += te.text;
        e.handled = true;
    }
    else if (e.type() == EventType::KeyPressed) {
        auto& ke = static_cast<KeyPressedEvent&>(e);
        if (ke.repeat) return;

        const int sc = ke.keyScancode;

        if (sc == SDL_SCANCODE_BACKSPACE) {
            if (!m_input.empty()) m_input.pop_back();
            e.handled = true;
        }
        else if (sc == SDL_SCANCODE_RETURN) {
            submitLine(m_input);
            m_input.clear();
            e.handled = true;
        }
        else if (sc == SDL_SCANCODE_ESCAPE) {
            // close console
            m_open = false;
            e.handled = true;
        }
        else if (sc == SDL_SCANCODE_UP) {
            if (!m_history.empty()) {
                if (m_historyIndex < 0) m_historyIndex = (int)m_history.size() - 1;
                else m_historyIndex = std::max(0, m_historyIndex - 1);

                m_input = m_history[m_historyIndex];
            }
            e.handled = true;
        }
        else if (sc == SDL_SCANCODE_DOWN) {
            if (!m_history.empty()) {
                if (m_historyIndex < 0) {
                    // nothing
                }
                else {
                    m_historyIndex++;
                    if (m_historyIndex >= (int)m_history.size()) {
                        m_historyIndex = -1;
                        m_input.clear();
                    }
                    else {
                        m_input = m_history[m_historyIndex];
                    }
                }
            }
            e.handled = true;
        }
    }
}

static void DrawWrappedLine(UIContext& ui, const std::string& s, float maxW, float textScale, bool small)
{
    // crude but effective: assume monospace-ish ~8px per char at scale 1.0
    float charW = 11.0f * textScale; // more realistic for BoldPixels + your UI scale
    int maxCols = (charW > 0.0f) ? (int)(maxW / charW) : 80;
    maxCols = std::max(20, maxCols);

    int start = 0;
    int n = (int)s.size();

    while (start < n) {
        int end = std::min(start + maxCols, n);

        // try break on space
        int breakPos = -1;
        for (int i = end - 1; i > start; --i) {
            if (s[i] == ' ') { breakPos = i; break; }
        }
        if (breakPos > start) end = breakPos;

        std::string line = s.substr(start, end - start);
        // trim leading spaces
        while (!line.empty() && line.front() == ' ') line.erase(line.begin());

        ui.label(line.c_str(), small);

        start = end;
        while (start < n && s[start] == ' ') start++;
    }
}

void DevConsole::draw(UIContext& ui, const UIRect& rect, float dt) {
    if (!m_open) return;

    auto saved = ui.style();
    auto s = saved;
    s.textScale = 0.85f;
    s.itemH = 22.0f;
    s.padding = 10.0f;
    s.spacing = 4.0f;
    ui.setStyle(s);

    ui.beginPanel("dev_console", rect, "Console");

    float pad = ui.style().padding;
    float innerW = rect.w - pad * 2.0f;

    // estimate visible lines based on available height (leave room for input/footer)
    int maxVisible = std::max(4, (int)((rect.h - 70.0f) / ui.style().itemH));

    // Draw from bottom up
    int startIdx = 0;
    if ((int)m_log.size() > maxVisible) startIdx = (int)m_log.size() - maxVisible;

    for (int i = startIdx; i < (int)m_log.size(); ++i) {
        DrawWrappedLine(ui, m_log[i], innerW, ui.style().textScale, true);
    }

    ui.spacing(6.0f);

    // cursor blink
    m_cursorTimer += dt;
    if (m_cursorTimer >= 0.5f) {
        m_cursorTimer = 0.0f;
        m_cursorOn = !m_cursorOn;
    }

    std::string line = "> " + m_input + (m_cursorOn ? "_" : " ");
    ui.label(line.c_str(), false);

    // footer smaller
    {
        auto s2 = ui.style();
        s2.textScale = 0.75f;
        s2.itemH = 18.0f;
        ui.setStyle(s2);
        ui.label("Enter=run   Esc=close   Up/Down=history   Backspace=delete", true);
        ui.setStyle(s);
    }

    ui.endPanel();

    ui.setStyle(saved);
}

void DevConsole::setOpen(bool open) {
    if (m_open == open) return;
    m_open = open;

    if (m_open) {
        if (m_window) SDL_StartTextInput(m_window);
        m_cursorTimer = 0.0f;
        m_cursorOn = true;
    }
    else {
        if (m_window) SDL_StopTextInput(m_window);
        m_historyIndex = -1;
    }
}

void DevConsole::toggle() {
    setOpen(!m_open);
}