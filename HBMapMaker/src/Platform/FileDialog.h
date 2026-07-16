#pragma once

#include <string>

// Native, synchronous file dialogs (Windows / comdlg32).
namespace hbmm::FileDialog {

    // Show an "Open" dialog. `filter` is a Win32 double-null filter spec, e.g.
    //   "PNG Image\0*.png\0All Files\0*.*\0"
    // Returns the chosen absolute path, or an empty string if cancelled.
    std::string OpenFile(const char* title, const char* filter);

    // Show a "Save As" dialog. `defExt` (without dot) is appended if the user
    // types no extension. Returns the chosen path or empty if cancelled.
    std::string SaveFile(const char* title, const char* filter, const char* defExt);

}
