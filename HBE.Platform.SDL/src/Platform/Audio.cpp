#include "HBE/Platform/Audio.h"
#include "HBE/Core/Log.h"

#include <algorithm>
#include <cmath>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

namespace HBE::Platform {

    using HBE::Core::LogError;
    using HBE::Core::LogInfo;
    using HBE::Core::LogWarn;

    namespace {
        constexpr float kEpsilon = 0.0001f;
    }

    Audio::~Audio() {
        shutdown();
    }

    float Audio::clamp01(float v) {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }

    float Audio::clampNonNegative(float v) {
        return (v < 0.0f) ? 0.0f : v;
    }

    const char* Audio::busTag(Bus bus) {
        switch (bus) {
        case Bus::Music:   return "bus.music";
        case Bus::SFX:     return "bus.sfx";
        case Bus::UI:      return "bus.ui";
        case Bus::Ambient: return "bus.ambient";
        case Bus::Master:
        default:           return "bus.master";
        }
    }

    bool Audio::initialize() {
        if (m_initialized) return true;

        if (!MIX_Init()) {
            LogError(std::string("Audio: MIX_Init failed: ") + SDL_GetError());
            return false;
        }

        m_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (!m_mixer) {
            LogError(std::string("Audio: MIX_CreateMixerDevice failed: ") + SDL_GetError());
            MIX_Quit();
            return false;
        }

        {
            std::scoped_lock lock(m_mutex);
            m_initialized = true;
            applyBusGainsLocked();
        }

        LogInfo("Audio initialized (SDL3_mixer MIX_* API).");
        return true;
    }

    void Audio::shutdown() {
        std::scoped_lock lock(m_mutex);

        if (!m_initialized) return;

        for (auto& [handle, voice] : m_activeVoices) {
            if (voice.track) {
                MIX_StopTrack(voice.track, 0);
                MIX_DestroyTrack(voice.track);
                voice.track = nullptr;
            }
        }
        m_activeVoices.clear();
        m_finishedVoices.clear();
        m_currentMusic = 0;

        for (auto& [name, asset] : m_sounds) {
            if (asset.audio) {
                MIX_DestroyAudio(asset.audio);
                asset.audio = nullptr;
            }
        }
        m_sounds.clear();

        for (auto& [name, asset] : m_music) {
            if (asset.audio) {
                MIX_DestroyAudio(asset.audio);
                asset.audio = nullptr;
            }
        }
        m_music.clear();

        if (m_mixer) {
            MIX_DestroyMixer(m_mixer);
            m_mixer = nullptr;
        }

        MIX_Quit();
        m_initialized = false;

        LogInfo("Audio shut down.");
    }

    bool Audio::loadSound(const std::string& name, const std::string& path, bool predecode) {
        std::scoped_lock lock(m_mutex);

        if (!m_initialized) {
            LogError("Audio::loadSound called before initialize().");
            return false;
        }

        if (m_sounds.find(name) != m_sounds.end()) {
            LogWarn("Audio: sound '" + name + "' already loaded.");
            return true;
        }

        MIX_Audio* audio = MIX_LoadAudio(m_mixer, path.c_str(), predecode);
        if (!audio) {
            LogError("Audio: failed to load sound '" + path + "': " + SDL_GetError());
            return false;
        }

        AssetEntry entry;
        entry.audio = audio;
        entry.path = path;
        entry.predecoded = predecode;
        m_sounds[name] = entry;

        LogInfo("Audio: loaded sound '" + name + "' from " + path);
        return true;
    }

    bool Audio::loadMusic(const std::string& name, const std::string& path) {
        std::scoped_lock lock(m_mutex);

        if (!m_initialized) {
            LogError("Audio::loadMusic called before initialize().");
            return false;
        }

        if (m_music.find(name) != m_music.end()) {
            LogWarn("Audio: music '" + name + "' already loaded.");
            return true;
        }

        // streamed/deferred decode is more music-friendly
        MIX_Audio* audio = MIX_LoadAudio(m_mixer, path.c_str(), false);
        if (!audio) {
            LogError("Audio: failed to load music '" + path + "': " + SDL_GetError());
            return false;
        }

        AssetEntry entry;
        entry.audio = audio;
        entry.path = path;
        entry.predecoded = false;
        m_music[name] = entry;

        LogInfo("Audio: loaded music '" + name + "' from " + path);
        return true;
    }

    bool Audio::unloadSound(const std::string& name) {
        std::scoped_lock lock(m_mutex);

        auto it = m_sounds.find(name);
        if (it == m_sounds.end()) return false;

        if (it->second.audio) {
            MIX_DestroyAudio(it->second.audio);
        }

        m_sounds.erase(it);
        return true;
    }

    bool Audio::unloadMusic(const std::string& name) {
        std::scoped_lock lock(m_mutex);

        auto it = m_music.find(name);
        if (it == m_music.end()) return false;

        if (it->second.audio) {
            MIX_DestroyAudio(it->second.audio);
        }

        m_music.erase(it);
        return true;
    }

    bool Audio::playSound(const std::string& name, int loops) {
        PlayParams p;
        p.loops = loops;
        p.bus = Bus::SFX;
        return playSoundEx(name, p) != 0;
    }

    Audio::VoiceHandle Audio::playSoundEx(const std::string& name, const PlayParams& params) {
        std::scoped_lock lock(m_mutex);

        if (!m_initialized || !m_mixer) {
            LogError("Audio::playSoundEx called before initialize().");
            return 0;
        }

        auto it = m_sounds.find(name);
        if (it == m_sounds.end()) {
            LogError("Audio: sound '" + name + "' not loaded.");
            return 0;
        }

        MIX_Track* track = MIX_CreateTrack(m_mixer);
        if (!track) {
            LogError(std::string("Audio: MIX_CreateTrack failed for sound '") + name + "': " + SDL_GetError());
            return 0;
        }

        if (!MIX_SetTrackAudio(track, it->second.audio)) {
            LogError(std::string("Audio: MIX_SetTrackAudio failed for sound '") + name + "': " + SDL_GetError());
            MIX_DestroyTrack(track);
            return 0;
        }

        if (!MIX_SetTrackLoops(track, params.loops)) {
            LogWarn(std::string("Audio: MIX_SetTrackLoops failed for sound '") + name + "': " + SDL_GetError());
        }

        if (!MIX_SetTrackStoppedCallback(track, &Audio::onTrackStopped, this)) {
            LogWarn(std::string("Audio: failed to set stop callback for sound '") + name + "': " + SDL_GetError());
        }

        MIX_TagTrack(track, busTag(params.bus));
        MIX_TagTrack(track, "bus.master");

        VoiceEntry voice;
        voice.handle = m_nextHandle++;
        voice.track = track;
        voice.assetName = name;
        voice.bus = params.bus;
        voice.baseGain = clampNonNegative(params.gain);
        voice.positional = params.positional;
        voice.worldX = params.worldX;
        voice.worldY = params.worldY;
        voice.minDistance = std::max(1.0f, params.minDistance);
        voice.maxDistance = std::max(voice.minDistance + 1.0f, params.maxDistance);
        voice.panRange = std::max(1.0f, params.panRange);
        voice.stopWhenTooFar = params.stopWhenTooFar;
        voice.isMusic = false;

        applyPositionalLocked(voice);

        if (!MIX_PlayTrack(track, 0)) {
            LogError(std::string("Audio: MIX_PlayTrack failed for sound '") + name + "': " + SDL_GetError());
            MIX_DestroyTrack(track);
            return 0;
        }

        m_activeVoices[voice.handle] = voice;
        return voice.handle;
    }

    Audio::VoiceHandle Audio::playMusic(const std::string& name, int loops, float gain) {
        std::scoped_lock lock(m_mutex);

        if (!m_initialized || !m_mixer) {
            LogError("Audio::playMusic called before initialize().");
            return 0;
        }

        auto it = m_music.find(name);
        if (it == m_music.end()) {
            LogError("Audio: music '" + name + "' not loaded.");
            return 0;
        }

        if (m_currentMusic != 0) {
            destroyVoiceLocked(m_currentMusic);
            m_currentMusic = 0;
        }

        MIX_Track* track = MIX_CreateTrack(m_mixer);
        if (!track) {
            LogError(std::string("Audio: MIX_CreateTrack failed for music '") + name + "': " + SDL_GetError());
            return 0;
        }

        if (!MIX_SetTrackAudio(track, it->second.audio)) {
            LogError(std::string("Audio: MIX_SetTrackAudio failed for music '") + name + "': " + SDL_GetError());
            MIX_DestroyTrack(track);
            return 0;
        }

        if (!MIX_SetTrackLoops(track, loops)) {
            LogWarn(std::string("Audio: MIX_SetTrackLoops failed for music '") + name + "': " + SDL_GetError());
        }

        if (!MIX_SetTrackStoppedCallback(track, &Audio::onTrackStopped, this)) {
            LogWarn(std::string("Audio: failed to set stop callback for music '") + name + "': " + SDL_GetError());
        }

        MIX_TagTrack(track, busTag(Bus::Music));
        MIX_TagTrack(track, "bus.master");

        VoiceEntry voice;
        voice.handle = m_nextHandle++;
        voice.track = track;
        voice.assetName = name;
        voice.bus = Bus::Music;
        voice.baseGain = clampNonNegative(gain);
        voice.positional = false;
        voice.isMusic = true;

        if (!MIX_SetTrackGain(track, voice.baseGain)) {
            LogWarn(std::string("Audio: MIX_SetTrackGain failed for music '") + name + "': " + SDL_GetError());
        }

        if (!MIX_PlayTrack(track, 0)) {
            LogError(std::string("Audio: MIX_PlayTrack failed for music '") + name + "': " + SDL_GetError());
            MIX_DestroyTrack(track);
            return 0;
        }

        m_currentMusic = voice.handle;
        m_activeVoices[voice.handle] = voice;
        return voice.handle;
    }

    void Audio::stopVoice(VoiceHandle handle) {
        std::scoped_lock lock(m_mutex);
        destroyVoiceLocked(handle);
    }

    void Audio::stopMusic() {
        std::scoped_lock lock(m_mutex);
        if (m_currentMusic != 0) {
            destroyVoiceLocked(m_currentMusic);
            m_currentMusic = 0;
        }
    }

    void Audio::stopAll() {
        std::scoped_lock lock(m_mutex);

        std::vector<VoiceHandle> handles;
        handles.reserve(m_activeVoices.size());
        for (const auto& [handle, voice] : m_activeVoices) {
            handles.push_back(handle);
        }

        for (VoiceHandle h : handles) {
            destroyVoiceLocked(h);
        }

        m_currentMusic = 0;
    }

    void Audio::pauseAll() {
        std::scoped_lock lock(m_mutex);

        for (auto& [handle, voice] : m_activeVoices) {
            if (voice.track) {
                MIX_PauseTrack(voice.track);
            }
        }
    }

    void Audio::resumeAll() {
        std::scoped_lock lock(m_mutex);

        for (auto& [handle, voice] : m_activeVoices) {
            if (voice.track) {
                MIX_ResumeTrack(voice.track);
            }
        }
    }

    void Audio::pauseBus(Bus bus) {
        std::scoped_lock lock(m_mutex);
        if (m_mixer) {
            MIX_PauseTag(m_mixer, busTag(bus));
        }
    }

    void Audio::resumeBus(Bus bus) {
        std::scoped_lock lock(m_mutex);
        if (m_mixer) {
            MIX_ResumeTag(m_mixer, busTag(bus));
        }
    }

    void Audio::stopBus(Bus bus) {
        std::scoped_lock lock(m_mutex);
        if (m_mixer) {
            MIX_StopTag(m_mixer, busTag(bus), 0);
        }
    }

    void Audio::setMasterGain(float gain) {
        std::scoped_lock lock(m_mutex);
        m_masterGain = clampNonNegative(gain);
        applyBusGainsLocked();
    }

    void Audio::setBusGain(Bus bus, float gain) {
        std::scoped_lock lock(m_mutex);

        gain = clampNonNegative(gain);

        switch (bus) {
        case Bus::Music:   m_musicGain = gain; break;
        case Bus::SFX:     m_sfxGain = gain; break;
        case Bus::UI:      m_uiGain = gain; break;
        case Bus::Ambient: m_ambientGain = gain; break;
        case Bus::Master:
        default:
            m_masterGain = gain;
            break;
        }

        applyBusGainsLocked();
    }

    float Audio::busGain(Bus bus) const {
        switch (bus) {
        case Bus::Music:   return m_musicGain;
        case Bus::SFX:     return m_sfxGain;
        case Bus::UI:      return m_uiGain;
        case Bus::Ambient: return m_ambientGain;
        case Bus::Master:
        default:           return m_masterGain;
        }
    }

    void Audio::setListenerPosition(float x, float y) {
        std::scoped_lock lock(m_mutex);
        m_listenerX = x;
        m_listenerY = y;
    }

    void Audio::update(float /*dt*/) {
        std::scoped_lock lock(m_mutex);

        if (!m_initialized) return;

        cleanupFinishedVoicesLocked();

        for (auto& [handle, voice] : m_activeVoices) {
            if (!voice.track) continue;
            if (voice.positional) {
                applyPositionalLocked(voice);
            }
        }
    }

    Audio::AudioStats Audio::stats() const {
        std::scoped_lock lock(m_mutex);

        AudioStats s;
        s.activeVoices = static_cast<int>(m_activeVoices.size());
        s.musicPlaying = (m_currentMusic != 0);
        s.masterGain = m_masterGain;
        s.musicGain = m_musicGain;
        s.sfxGain = m_sfxGain;
        s.uiGain = m_uiGain;
        s.ambientGain = m_ambientGain;
        return s;
    }

    void Audio::onTrackStopped(void* userdata, MIX_Track* track) {
        if (!userdata || !track) return;

        Audio* self = static_cast<Audio*>(userdata);
        std::scoped_lock lock(self->m_mutex);

        for (const auto& [handle, voice] : self->m_activeVoices) {
            if (voice.track == track) {
                self->m_finishedVoices.push_back(handle);
                break;
            }
        }
    }

    void Audio::cleanupFinishedVoicesLocked() {
        if (m_finishedVoices.empty()) return;

        for (VoiceHandle handle : m_finishedVoices) {
            auto it = m_activeVoices.find(handle);
            if (it == m_activeVoices.end()) continue;

            if (it->second.track) {
                MIX_DestroyTrack(it->second.track);
                it->second.track = nullptr;
            }

            if (m_currentMusic == handle) {
                m_currentMusic = 0;
            }

            m_activeVoices.erase(it);
        }

        m_finishedVoices.clear();
    }

    void Audio::destroyVoiceLocked(VoiceHandle handle) {
        auto it = m_activeVoices.find(handle);
        if (it == m_activeVoices.end()) return;

        if (it->second.track) {
            MIX_StopTrack(it->second.track, 0);
            MIX_DestroyTrack(it->second.track);
            it->second.track = nullptr;
        }

        if (m_currentMusic == handle) {
            m_currentMusic = 0;
        }

        m_activeVoices.erase(it);
    }

    void Audio::applyBusGainsLocked() {
        if (!m_mixer) return;

        if (!MIX_SetMixerGain(m_mixer, m_masterGain)) {
            LogWarn(std::string("Audio: MIX_SetMixerGain failed: ") + SDL_GetError());
        }

        if (!MIX_SetTagGain(m_mixer, busTag(Bus::Music), m_musicGain)) {
            LogWarn(std::string("Audio: MIX_SetTagGain(Music) failed: ") + SDL_GetError());
        }

        if (!MIX_SetTagGain(m_mixer, busTag(Bus::SFX), m_sfxGain)) {
            LogWarn(std::string("Audio: MIX_SetTagGain(SFX) failed: ") + SDL_GetError());
        }

        if (!MIX_SetTagGain(m_mixer, busTag(Bus::UI), m_uiGain)) {
            LogWarn(std::string("Audio: MIX_SetTagGain(UI) failed: ") + SDL_GetError());
        }

        if (!MIX_SetTagGain(m_mixer, busTag(Bus::Ambient), m_ambientGain)) {
            LogWarn(std::string("Audio: MIX_SetTagGain(Ambient) failed: ") + SDL_GetError());
        }
    }

    void Audio::applyPositionalLocked(VoiceEntry& voice) {
        if (!voice.track) return;

        const float dx = voice.worldX - m_listenerX;
        const float dy = voice.worldY - m_listenerY;
        const float distance = std::sqrt(dx * dx + dy * dy);

        if (voice.stopWhenTooFar && distance >= voice.maxDistance) {
            destroyVoiceLocked(voice.handle);
            return;
        }

        float attenuation = 1.0f;
        if (distance > voice.minDistance) {
            const float denom = std::max(kEpsilon, voice.maxDistance - voice.minDistance);
            const float t = clamp01((distance - voice.minDistance) / denom);
            attenuation = 1.0f - t;
        }

        const float finalGain = clampNonNegative(voice.baseGain * attenuation);
        if (!MIX_SetTrackGain(voice.track, finalGain)) {
            LogWarn(std::string("Audio: MIX_SetTrackGain(positional) failed: ") + SDL_GetError());
        }

        const float panNorm = std::clamp(dx / voice.panRange, -1.0f, 1.0f);

        MIX_StereoGains stereo{};
        if (panNorm >= 0.0f) {
            stereo.left = 1.0f - panNorm;
            stereo.right = 1.0f;
        }
        else {
            stereo.left = 1.0f;
            stereo.right = 1.0f + panNorm; // panNorm is negative here
        }

        if (!MIX_SetTrackStereo(voice.track, &stereo)) {
            LogWarn(std::string("Audio: MIX_SetTrackStereo failed: ") + SDL_GetError());
        }
    }

} // namespace HBE::Platform