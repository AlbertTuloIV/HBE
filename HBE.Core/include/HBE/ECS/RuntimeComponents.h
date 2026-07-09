#pragma once

#include <string>

namespace HBE::ECS {

    // completely optional in gameplay code. bookkeeping components for systems.
    struct ScriptRuntimeState {
        bool created = false;
    };

    // Stable ID for serialization
    struct IDComponent {
        std::string uuid; // hex string
    };

    // Friendly name / tag
    struct TagComponent {
        std::string tag;
    };

    struct AnimatorPresetComponent {
        std::string preset;
    };
}