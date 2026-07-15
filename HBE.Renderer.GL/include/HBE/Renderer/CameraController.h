#pragma once

#include "HBE/Renderer/Camera2D.h"

#include <cstdint>
#include <vector>

namespace HBE::Renderer {

    // Controller that owns a Camera2D and updates it every frame.
    // GameLayer keeps a CameraController instead of a raw Camera2D and
    // hands `controller.camera()` to GLRenderer::setCamera().
    class CameraController {
    public:
        // ---- Configuration ---------------------------------------------

        // Response rate for follow. Higher = snappier.
        // 9.0 ~= the old `LerpF(*, *, 0.15)` at 60fps.
        // 0.0 = camera never moves (useful for scripted holds).
        float followResponse = 9.0f;

        // If true, camera x/y are rounded to integer pixels before being
        // stamped into the underlying Camera2D. Matches the pixel-perfect
        // look the sandbox already uses.
        bool pixelSnap = true;

        // ---- Runtime state ---------------------------------------------

        // The current follow target. Set by setFollowTarget().
        // If followEnabled is false the controller keeps its current
        // position and does not read the target.
        bool  followEnabled = true;

        // ---- Dead zone -------------------------------------------------
        // Rectangle around the camera center that the target can move
        // inside without pulling the camera. Half-extents in world units.
        // Set to 0 to disable the dead zone (target always pulls).
        float deadzoneHalfW = 0.0f;
        float deadzoneHalfH = 0.0f;

        // ---- Look-ahead ------------------------------------------------

        // Max look-ahead distance (world units) on each axis.
        // Set to 0 to disable an axis. Typical: (100, 40).
        float lookAheadX = 0.0f;
        float lookAheadY = 0.0f;

        // Target velocity (world units / sec) at which look-ahead reaches
        // its max. Slower than this ramps linearly. Typical: (240, 240).
        float lookAheadVelSaturateX = 240.0f;
        float lookAheadVelSaturateY = 240.0f;

        // Smoothing on the look-ahead offset itself. Higher = snappier
        // when the player changes direction. 4.0 feels natural.
        float lookAheadResponse = 4.0f;

        // Optional facing hint (-1, 0, +1) used when velocity is ~0.
        // Set every frame from the sandbox via setFacingHint().
        int   facingHintX = 0;

        // ---- Level bounds ---------------------------------------------

        // If enabled, the camera position is clamped so the visible rect
        // stays inside [boundsMinX..boundsMaxX] x [boundsMinY..boundsMaxY].
        // If the map is smaller than the viewport on an axis, the camera
        // is centered on that axis instead of clamped hard.
        bool  boundsEnabled = false;
        float boundsMinX = 0.0f;
        float boundsMinY = 0.0f;
        float boundsMaxX = 0.0f;
        float boundsMaxY = 0.0f;

        // ---- Zoom ------------------------------------------------------

        // Target zoom the camera is easing toward. Reads from camera().zoom
        // as the starting value.
        float zoomTarget = 1.0f;
        float zoomResponse = 6.0f;

        // Clamp any zoom the sandbox tries to set.
        float minZoom = 0.25f;
        float maxZoom = 4.0f;

        // ---- Screen shake ---------------------------------------------

        // Peak positional shake at trauma == 1 (world units).
        float shakeMaxTranslation = 12.0f;

        // Peak rotational shake in radians at trauma == 1.
        // Left as data even though our renderer doesn't rotate the camera
        // -- item 25 / room transitions may wire this up.
        float shakeMaxRotation = 0.0f;

        // How fast trauma decays (per second). 1.0 = fully faded in 1s.
        float shakeDecay = 1.2f;

        // ---- API --------------------------------------------------------

        // Read-only view of the underlying Camera2D (for GLRenderer, tile
        // culling, screen-to-world math). Never null.
        const Camera2D& camera() const { return m_cam; }
        Camera2D&       camera()       { return m_cam; }

        // Set the target position we're following (typically the player's
        // world center). Safe to call every frame.
        void setFollowTarget(float worldX, float worldY) {
            m_hasTarget = true;
            m_targetX = worldX;
            m_targetY = worldY;
        }

        // Clear any target and freeze the camera in place.
        void clearFollowTarget() { m_hasTarget = false; }

        // Snap position and desired target immediately, skipping smoothing.
        // Call this on scene load, reset, room transition, etc.
        void snapTo(float worldX, float worldY) {
            m_scriptDuration = 0.0f;
            m_scriptElapsed  = 0.0f;
            followEnabled    = m_savedFollowEnabled;

            m_posX = worldX;
            m_posY = worldY;
            m_targetX = worldX;
            m_targetY = worldY;
            m_hasTarget = true;

            m_lookX = 0.0f;
            m_lookY = 0.0f;
            m_targetVelX = 0.0f;
            m_targetVelY = 0.0f;

            writeToCamera();
        }

        // Set the logical viewport size (matches window / render target).
        // Call this once from onAttach and any time the design resolution
        // changes.
        void setViewport(float w, float h) {
            m_cam.viewportWidth = w;
            m_cam.viewportHeight = h;
        }

        // Convenience: set both dead-zone half-extents at once.
        void setDeadzone(float halfW, float halfH) {
            deadzoneHalfW = halfW;
            deadzoneHalfH = halfH;
        }

        // Convenience: set look-ahead in one call.
        void setLookAhead(float maxX, float maxY,
                          float saturateVelX = 240.0f,
                          float saturateVelY = 240.0f)
        {
            lookAheadX = maxX;
            lookAheadY = maxY;
            lookAheadVelSaturateX = saturateVelX;
            lookAheadVelSaturateY = saturateVelY;
        }

        // Velocity of the follow target in world units / second.
        // Call every frame after setFollowTarget().
        void setFollowVelocity(float vx, float vy) {
            m_targetVelX = vx;
            m_targetVelY = vy;
            m_hasTargetVel = true;
        }

        // Provide a facing hint (usually the sign of sprite scaleX).
        // Used only when |velocity| is essentially zero.
        void setFacingHintX(int sign) {
            facingHintX = (sign < 0) ? -1 : (sign > 0 ? +1 : 0);
        }

        // ---- Level bounds ----
        void setBounds(float minX, float minY, float maxX, float maxY) {
            boundsEnabled = true;
            boundsMinX = minX;
            boundsMinY = minY;
            boundsMaxX = maxX;
            boundsMaxY = maxY;
        }

        void clearBounds() { boundsEnabled = false; }

        // ---- Zoom ----
        // Ease zoom to `z`. Clamped to [minZoom, maxZoom].
        void setZoomTarget(float z) {
            if (z < minZoom) z = minZoom;
            if (z > maxZoom) z = maxZoom;
            zoomTarget = z;
        }

        // Snap zoom immediately.
        void snapZoom(float z) {
            if (z < minZoom) z = minZoom;
            if (z > maxZoom) z = maxZoom;
            zoomTarget = z;
            m_cam.zoom = z;
        }

        // ---- Screen shake ----
        // Add trauma. Clamped to [0, 1]. Additive -- repeated hits stack up.
        void addShake(float amount) {
            m_trauma += amount;
            if (m_trauma > 1.0f) m_trauma = 1.0f;
            if (m_trauma < 0.0f) m_trauma = 0.0f;
        }

        // Force trauma to a specific value.
        void setShake(float amount) {
            if (amount < 0.0f) amount = 0.0f;
            if (amount > 1.0f) amount = 1.0f;
            m_trauma = amount;
        }

        float trauma() const { return m_trauma; }

        // ---- Follow control ----
        void pauseFollow()  { followEnabled = false; }
        void resumeFollow() { followEnabled = true;  }

        // ---- Multi-target (per frame; cleared each update) ----
        struct SecondaryTarget { float x, y, weight; };

        void addSecondaryTarget(float x, float y, float weight = 1.0f) {
            if (weight <= 0.0f) return;
            m_extraTargets.push_back({ x, y, weight });
        }
        void clearSecondaryTargets() { m_extraTargets.clear(); }

        // ---- Scripted move ----
        // Ease from current pos to (x, y) over `seconds` with smoothstep.
        // Follow is paused for the duration and restored on completion.
        // If seconds <= 0, behaves like snapTo.
        void focusOn(float worldX, float worldY, float seconds);

        // True while a scripted focusOn is in flight.
        bool isScripted() const { return m_scriptDuration > 0.0f; }

        // Rectangle covering the current dead zone in world space.
        struct DeadzoneRect { float minX, minY, maxX, maxY; };
        DeadzoneRect deadzoneRect() const {
            return {
                m_cam.x - deadzoneHalfW, m_cam.y - deadzoneHalfH,
                m_cam.x + deadzoneHalfW, m_cam.y + deadzoneHalfH
            };
        }

        // Advance the camera one frame.
        void update(float dt);

    private:
        Camera2D m_cam{};

        float m_posX = 0.0f;
        float m_posY = 0.0f;

        float m_targetX = 0.0f;
        float m_targetY = 0.0f;
        bool  m_hasTarget = false;

        float m_targetVelX = 0.0f;
        float m_targetVelY = 0.0f;
        bool  m_hasTargetVel = false;

        // Current smoothed look-ahead offset added on top of the target.
        float m_lookX = 0.0f;
        float m_lookY = 0.0f;

        // Trauma in [0, 1]. Decays every frame at shakeDecay per second.
        float m_trauma = 0.0f;

        // Frame counter used to seed shake noise. Simple + deterministic.
        uint32_t m_shakeSeed = 1;

        // Multi-target secondary framing (weighted midpoint with primary).
        // Cleared each update(); sandbox re-supplies each frame.
        std::vector<SecondaryTarget> m_extraTargets;

        // Scripted focus state. m_scriptDuration > 0 means active.
        float m_scriptStartX = 0.0f;
        float m_scriptStartY = 0.0f;
        float m_scriptTargetX = 0.0f;
        float m_scriptTargetY = 0.0f;
        float m_scriptDuration = 0.0f;
        float m_scriptElapsed  = 0.0f;
        bool  m_savedFollowEnabled = true;

        void writeToCamera();
    };
}
