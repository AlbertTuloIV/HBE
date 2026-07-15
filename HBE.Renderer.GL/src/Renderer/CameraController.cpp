#include "HBE/Renderer/CameraController.h"

#include <algorithm>
#include <cmath>

namespace HBE::Renderer {

    // Exponential smoothing that is frame-rate independent.
    // response = higher -> snappier. dt in seconds.
    static float ExpSmooth(float from, float to, float response, float dt) {
        if (response <= 0.0f || dt <= 0.0f) return from;
        const float alpha = 1.0f - std::exp(-response * dt);
        return from + (to - from) * alpha;
    }

    // A trivially cheap 1D "noise". Not real perlin -- just a deterministic
    // pseudo-random value in [-1, 1] for a given (seed, channel, frame).
    static float ShakeNoise(uint32_t seed, uint32_t channel, uint32_t frame) {
        uint32_t h = seed;
        h ^= channel * 0x9E3779B1u;
        h ^= frame   * 0x85EBCA77u;
        h ^= h >> 16;
        h *= 0xC2B2AE3Du;
        h ^= h >> 13;
        return (static_cast<float>(h & 0xFFFFu) / 32767.5f) - 1.0f;
    }

    void CameraController::focusOn(float worldX, float worldY, float seconds) {
        if (seconds <= 0.0f) {
            snapTo(worldX, worldY);
            return;
        }
        // Only capture followEnabled if we're not already in a scripted move,
        // otherwise focusOn-during-focusOn would overwrite the real saved value.
        if (m_scriptDuration <= 0.0f) {
            m_savedFollowEnabled = followEnabled;
        }
        followEnabled = false;

        m_scriptStartX = m_posX;
        m_scriptStartY = m_posY;
        m_scriptTargetX = worldX;
        m_scriptTargetY = worldY;
        m_scriptDuration = seconds;
        m_scriptElapsed  = 0.0f;
    }

    void CameraController::update(float dt) {
        // ---- Scripted focus takes priority over follow ----
        if (m_scriptDuration > 0.0f) {
            m_scriptElapsed += dt;
            float t = m_scriptElapsed / m_scriptDuration;
            if (t >= 1.0f) t = 1.0f;
            // Smoothstep for pleasant ease-in/out.
            t = t * t * (3.0f - 2.0f * t);
            m_posX = m_scriptStartX + (m_scriptTargetX - m_scriptStartX) * t;
            m_posY = m_scriptStartY + (m_scriptTargetY - m_scriptStartY) * t;

            if (m_scriptElapsed >= m_scriptDuration) {
                m_scriptDuration = 0.0f;
                m_scriptElapsed  = 0.0f;
                followEnabled    = m_savedFollowEnabled;
            }
        }
        else if (followEnabled && m_hasTarget) {
            // ---- Multi-target weighted midpoint ----
            float aimTX = m_targetX;
            float aimTY = m_targetY;

            if (!m_extraTargets.empty()) {
                float wxSum = m_targetX;
                float wySum = m_targetY;
                float wSum  = 1.0f;
                for (const auto& s : m_extraTargets) {
                    wxSum += s.x * s.weight;
                    wySum += s.y * s.weight;
                    wSum  += s.weight;
                }
                aimTX = wxSum / wSum;
                aimTY = wySum / wSum;
            }

            // --- Look-ahead ---
            float desiredLookX = 0.0f;
            float desiredLookY = 0.0f;

            if (lookAheadX > 0.0f) {
                float vx = m_hasTargetVel ? m_targetVelX : 0.0f;
                if (std::fabs(vx) < 5.0f && facingHintX != 0) {
                    desiredLookX = lookAheadX * static_cast<float>(facingHintX);
                }
                else {
                    const float sat = std::max(1.0f, lookAheadVelSaturateX);
                    float t = vx / sat;
                    if (t > 1.0f) t = 1.0f; else if (t < -1.0f) t = -1.0f;
                    desiredLookX = lookAheadX * t;
                }
            }

            if (lookAheadY > 0.0f) {
                float vy = m_hasTargetVel ? m_targetVelY : 0.0f;
                const float sat = std::max(1.0f, lookAheadVelSaturateY);
                float t = vy / sat;
                if (t > 1.0f) t = 1.0f; else if (t < -1.0f) t = -1.0f;
                desiredLookY = lookAheadY * t;
            }

            m_lookX = ExpSmooth(m_lookX, desiredLookX, lookAheadResponse, dt);
            m_lookY = ExpSmooth(m_lookY, desiredLookY, lookAheadResponse, dt);

            // --- Dead zone on the (aim + look-ahead) point ---
            const float aimX = aimTX + m_lookX;
            const float aimY = aimTY + m_lookY;

            float desiredX = m_posX;
            float desiredY = m_posY;

            const float dx = aimX - m_posX;
            const float dy = aimY - m_posY;

            if (std::fabs(dx) > deadzoneHalfW) {
                const float overshoot = std::fabs(dx) - deadzoneHalfW;
                desiredX = m_posX + (dx < 0.0f ? -overshoot : +overshoot);
            }
            if (std::fabs(dy) > deadzoneHalfH) {
                const float overshoot = std::fabs(dy) - deadzoneHalfH;
                desiredY = m_posY + (dy < 0.0f ? -overshoot : +overshoot);
            }

            m_posX = ExpSmooth(m_posX, desiredX, followResponse, dt);
            m_posY = ExpSmooth(m_posY, desiredY, followResponse, dt);
        }

        // Zoom ease (runs regardless of follow, so scripts can drive it).
        m_cam.zoom = ExpSmooth(m_cam.zoom, zoomTarget, zoomResponse, dt);
        if (m_cam.zoom < 1e-3f) m_cam.zoom = 1e-3f;

        // Shake decay (advances the seed so noise samples change per frame).
        if (m_trauma > 0.0f) {
            m_trauma -= shakeDecay * dt;
            if (m_trauma < 0.0f) m_trauma = 0.0f;
            ++m_shakeSeed;
        }

        // Secondary targets are per-frame; sandbox re-adds them next tick.
        m_extraTargets.clear();

        writeToCamera();
    }

    void CameraController::writeToCamera() {
        float outX = m_posX;
        float outY = m_posY;

        if (boundsEnabled) {
            const float halfW = 0.5f * m_cam.viewportWidth  / std::max(m_cam.zoom, 1e-3f);
            const float halfH = 0.5f * m_cam.viewportHeight / std::max(m_cam.zoom, 1e-3f);

            const float boundsW = boundsMaxX - boundsMinX;
            const float boundsH = boundsMaxY - boundsMinY;

            if (boundsW < halfW * 2.0f) {
                // Map narrower than viewport on X -- center it.
                outX = (boundsMinX + boundsMaxX) * 0.5f;
            }
            else {
                const float minCX = boundsMinX + halfW;
                const float maxCX = boundsMaxX - halfW;
                if (outX < minCX) outX = minCX;
                if (outX > maxCX) outX = maxCX;
            }

            if (boundsH < halfH * 2.0f) {
                outY = (boundsMinY + boundsMaxY) * 0.5f;
            }
            else {
                const float minCY = boundsMinY + halfH;
                const float maxCY = boundsMaxY - halfH;
                if (outY < minCY) outY = minCY;
                if (outY > maxCY) outY = maxCY;
            }
        }

        // Shake offset -- applied after bounds so it can visually push past
        // the map edge. trauma^2 makes weak shakes imperceptible and big
        // shakes hit hard (Squirrel Eiserloh's model).
        if (m_trauma > 0.0f) {
            const float t2 = m_trauma * m_trauma;
            const float ox = ShakeNoise(m_shakeSeed, 0, 1) * shakeMaxTranslation * t2;
            const float oy = ShakeNoise(m_shakeSeed, 1, 1) * shakeMaxTranslation * t2;
            outX += ox;
            outY += oy;
        }

        if (pixelSnap) {
            m_cam.x = std::round(outX);
            m_cam.y = std::round(outY);
        }
        else {
            m_cam.x = outX;
            m_cam.y = outY;
        }
    }
}
