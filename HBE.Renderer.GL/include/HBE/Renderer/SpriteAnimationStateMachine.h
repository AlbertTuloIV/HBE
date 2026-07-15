#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include "HBE/Renderer/SpriteRenderer2D.h"

namespace HBE::Renderer {
	struct RenderItem;

    // A slightly higher-level animation system:
// - Flipbook clips (row + startCol + frameCount)
// - State machine (Idle/Run/Attack style)
// - Per-clip events fired on specific frames ("footstep", "hitframe", etc.)
//
// Intended usage:
//   SpriteAnimationStateMachine sm;
//   sm.sheet = &sheet;
//   sm.addClip({ "Idle", 0, 0, 6, 0.10f, true });
//   sm.addClip({ "Run",  1, 0, 6, 0.08f, true });
//   sm.addClip({ "Attack", 2, 0, 6, 0.07f, false });
//   sm.addEvent("Run", 1, "footstep");
//   sm.addEvent("Attack", 3, "hitframe");
//
//   sm.addState("Idle", "Idle");
//   sm.addState("Run", "Run");
//   sm.addState("Attack", "Attack");
//   sm.addTransitionBool("Idle", "Run", "moving", true);
//   sm.addTransitionBool("Run", "Idle", "moving", false);
//   sm.addTransitionTrigger("*", "Attack", "attack");    // from any state
//   sm.addTransitionFinished("Attack", "Idle");
//
//   sm.setState("Idle");
//
// In update(dt):
//   sm.setBool("moving", ...);
//   if (pressed) sm.trigger("attack");
//   sm.update(dt, [&](const std::string& ev){ ... });
//   sm.apply(renderItem);
//

	class SpriteAnimationStateMachine {
	public:
		using EventCallback = std::function<void(const std::string& eventName)>;

		const SpriteRenderer2D::SpriteSheetHandle* sheet = nullptr;

		// playback controls
		float globalSpeed = 1.0f; // multiplies dt for all clips (1.0 = normal)

		// Clip + events

		struct Clip {
			std::string name;

			int row = 0;
			int startCol = 0;
			int frameCount = 1;

			float frameDuration = 0.1f; // seconds per frame before speed
			float loop = true;

			// optional clip-specific speed multiplier (1.0 = normal).
			// final dt scale: dt * globalSpeed * clip.speed.
			float speed = 1.0f;
		};

		struct ClipEvent {
			std::string name;
			int frame = 0; // 0-based frame inside the clip
		};

		void addClip(const Clip& clip);
		const Clip* getClip(const std::string& name) const;

		// Add an event on a frame within a named clip
		void addEvent(const std::string& clipName, int frame, const std::string& eventName);

		// state machine

		struct State {
			std::string name;
			std::string clipName;
			float speed = 1.0f; // additional multiplier on top of clip.speed
		};

		enum class TransitionType {
			Always,
			BoolEquals,
			Trigger,
			Finished,
		};

		struct Transition {
			std::string fromState; // use "*" for any
			std::string toState;

			TransitionType type = TransitionType::Always;

			// condition payload (one of these is used depending on type)
			std::string varOrTrigger;
			bool boolValue = false;
		};

		void addState(const std::string& stateName, const std::string& clipName, float speed = 1.0f);

		void addTransitionAlways(const std::string& fromState, const std::string& toState);
		void addTransitionBool(const std::string& fromState, const std::string& toState, const std::string& var, bool value);
		void addTransitionTrigger(const std::string& fromState, const std::string& toState, const std::string& triggerName);
		void addTransitionFinished(const std::string& fromState, const std::string& toState);

		// variables / triggers
		void setBool(const std::string& name, bool value);
		bool getBool(const std::string& name) const;

		// A trigger is consumed the first time a transition uses it (per update).
		void trigger(const std::string& name);

		// force-set the current state (optionally restart tis clip)
		void setState(const std::string& stateName, bool restart = true);
		const std::string& getState() const { return m_currentState; }

		// High-level update + apply
		void update(float dt, const EventCallback& onEvent = {});
		void apply(RenderItem& item) const;

		// useful for "attack" style states
		bool isClipFinished() const { return m_clipFinished; }
		int currentFrame() const { return m_frame; }

	private:
		// state machine data
		std::unordered_map<std::string, Clip> m_clips;
		std::unordered_map<std::string, State> m_states;
		std::vector<Transition> m_transitions;

		// per-clip events
		std::unordered_map<std::string, std::vector<ClipEvent>> m_events;

		// runtime vars/triggers
		std::unordered_map<std::string, bool> m_bools;
		std::unordered_set<std::string> m_triggers;

		// playback runtime
		std::string m_currentState;
		const State* m_statePtr = nullptr;
		const Clip* m_clipPtr = nullptr;

		int m_frame = 0;
		float m_timer = 0.0f;
		bool m_playing = true;
		bool m_clipFinished = false;

		void resolvePointersForState();
		void restartClip();
		void stepFrames(float scaleDt, const EventCallback& onEvent);
		void fireFrameEvents(const EventCallback& onEvent, int frame);

		bool transitionMatchesFrom(const Transition& t) const;
		bool transitionConditionMet(const Transition& t) const;
		void consumeTriggerIfUsed(const Transition& t);
		void runTransitions(); // may change state
	};
}