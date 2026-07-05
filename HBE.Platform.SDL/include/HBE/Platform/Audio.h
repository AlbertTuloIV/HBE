#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h> // needed for SDLCALL

struct MIX_Mixer;
struct MIX_Track;
struct MIX_Audio;
struct MIX_StereoGains;

namespace HBE::Platform {

    class Audio {
    public:
        using VoiceHandle = std::uint64_t;

        enum class Bus {
            Master = 0,
            Music,
            SFX,
            UI,
            Ambient
        };

        struct PlayParams {
            int loops = 0;                 // 0 = once, -1 = forever
            float gain = 1.0f;            // per-voice gain
            Bus bus = Bus::SFX;

            // positional-ish 2D audio
            bool positional = false;
            float worldX = 0.0f;
            float worldY = 0.0f;
            float minDistance = 48.0f;    // full volume within this range
            float maxDistance = 800.0f;   // silent at/beyond this range
            float panRange = 350.0f;      // horizontal range used for stereo pan
            bool stopWhenTooFar = false;  // optional voice culling
        };

        struct AudioStats {
            int activeVoices = 0;
            bool musicPlaying = false;
            float masterGain = 1.0f;
            float musicGain = 1.0f;
            float sfxGain = 1.0f;
            float uiGain = 1.0f;
            float ambientGain = 1.0f;
        };

    public:
        Audio() = default;
        ~Audio();

        Audio(const Audio&) = delete;
        Audio& operator=(const Audio&) = delete;

        bool initialize();
        void shutdown();
        void update(float dt);

        bool isInitialized() const { return m_initialized; }

        // asset loading
        bool loadSound(const std::string& name, const std::string& path, bool predecode = true);
        bool loadMusic(const std::string& name, const std::string& path);

        bool unloadSound(const std::string& name);
        bool unloadMusic(const std::string& name);

        // backwards-compatible simple API
        bool playSound(const std::string& name, int loops = 0);

        // richer API
        VoiceHandle playSoundEx(const std::string& name, const PlayParams& params);
        VoiceHandle playMusic(const std::string& name, int loops = -1, float gain = 1.0f);
        void stopVoice(VoiceHandle handle);
        void stopMusic();
        void stopAll();

        void pauseAll();
        void resumeAll();

        void pauseBus(Bus bus);
        void resumeBus(Bus bus);
        void stopBus(Bus bus);

        void setMasterGain(float gain);
        float masterGain() const { return m_masterGain; }

        void setBusGain(Bus bus, float gain);
        float busGain(Bus bus) const;

        void setListenerPosition(float x, float y);
        void getListenerPosition(float& x, float& y) const {
            x = m_listenerX;
            y = m_listenerY;
        }

        AudioStats stats() const;

    private:
        struct AssetEntry {
            MIX_Audio* audio = nullptr;
            std::string path;
            bool predecoded = true;
        };

        struct VoiceEntry {
            VoiceHandle handle = 0;
            MIX_Track* track = nullptr;
            std::string assetName;
            Bus bus = Bus::SFX;

            float baseGain = 1.0f;

            bool positional = false;
            float worldX = 0.0f;
            float worldY = 0.0f;
            float minDistance = 48.0f;
            float maxDistance = 800.0f;
            float panRange = 350.0f;
            bool stopWhenTooFar = false;

            bool isMusic = false;
        };

    private:
        bool m_initialized = false;
        MIX_Mixer* m_mixer = nullptr;

        std::unordered_map<std::string, AssetEntry> m_sounds;
        std::unordered_map<std::string, AssetEntry> m_music;

        std::unordered_map<VoiceHandle, VoiceEntry> m_activeVoices;
        std::vector<VoiceHandle> m_finishedVoices;

        VoiceHandle m_nextHandle = 1;
        VoiceHandle m_currentMusic = 0;

        float m_masterGain = 1.0f;
        float m_musicGain = 1.0f;
        float m_sfxGain = 1.0f;
        float m_uiGain = 1.0f;
        float m_ambientGain = 1.0f;

        float m_listenerX = 0.0f;
        float m_listenerY = 0.0f;

        mutable std::mutex m_mutex;

    private:
        static void SDLCALL onTrackStopped(void* userdata, MIX_Track* track);

        static const char* busTag(Bus bus);
        void cleanupFinishedVoicesLocked();
        void destroyVoiceLocked(VoiceHandle handle);
        void applyBusGainsLocked();
        bool applyPositionalLocked(VoiceEntry& voice);

        static float clamp01(float v);
        static float clampNonNegative(float v);
    };

} // namespace HBE::Platform