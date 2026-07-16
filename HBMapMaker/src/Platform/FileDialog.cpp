#include "Platform/FileDialog.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>

#pragma comment(lib, "Comdlg32.lib")

#include <vector>

namespace {

    std::wstring Widen(const char* s) {
        if (!s) return std::wstring{};
        const int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
        if (len <= 0) return std::wstring{};
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), len);
        if (!w.empty() && w.back() == L'\0') w.pop_back();
        return w;
    }

    std::string Narrow(const wchar_t* s) {
        if (!s) return std::string{};
        const int len = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return std::string{};
        std::string a(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s, -1, a.data(), len, nullptr, nullptr);
        if (!a.empty() && a.back() == '\0') a.pop_back();
        return a;
    }

    // Convert a "a\0b\0" ascii double-null filter into a wide double-null block.
    std::vector<wchar_t> WidenFilter(const char* filter) {
        std::vector<wchar_t> out;
        if (!filter) { out.push_back(L'\0'); out.push_back(L'\0'); return out; }
        const char* p = filter;
        // Walk until a double null.
        for (;;) {
            if (*p == '\0') {
                out.push_back(L'\0');
                if (*(p + 1) == '\0') { out.push_back(L'\0'); break; }
                ++p;
                continue;
            }
            out.push_back((wchar_t)(unsigned char)*p);
            ++p;
        }
        return out;
    }

} // namespace

namespace hbmm::FileDialog {

    std::string OpenFile(const char* title, const char* filter) {
        wchar_t path[MAX_PATH] = { 0 };
        std::wstring wtitle = Widen(title);
        std::vector<wchar_t> wfilter = WidenFilter(filter);

        OPENFILENAMEW ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = wfilter.data();
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = wtitle.empty() ? nullptr : wtitle.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameW(&ofn)) return Narrow(path);
        return std::string{};
    }

    std::string SaveFile(const char* title, const char* filter, const char* defExt) {
        wchar_t path[MAX_PATH] = { 0 };
        std::wstring wtitle = Widen(title);
        std::wstring wext = Widen(defExt);
        std::vector<wchar_t> wfilter = WidenFilter(filter);

        OPENFILENAMEW ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = wfilter.data();
        ofn.nFilterIndex = 1;
        ofn.lpstrTitle = wtitle.empty() ? nullptr : wtitle.c_str();
        ofn.lpstrDefExt = wext.empty() ? nullptr : wext.c_str();
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        if (GetSaveFileNameW(&ofn)) return Narrow(path);
        return std::string{};
    }

}

#else // !_WIN32

namespace hbmm::FileDialog {
    std::string OpenFile(const char*, const char*) { return std::string{}; }
    std::string SaveFile(const char*, const char*, const char*) { return std::string{}; }
}

#endif
