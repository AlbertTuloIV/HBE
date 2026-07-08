#pragma once

#include "HBE/Renderer/Framebuffer.h"

#include <vector> 
#include <string>
#include <array>

namespace HBE::Renderer {
	class GLShader;

	struct PostProcessEffect {
		std::string name;
		GLShader* shader = nullptr;
		bool enabled = true;

		std::array<float, 8> params{};
	};

	class PostProcessStack {
	public:
		PostProcessStack() = default;
		~PostProcessStack();

		PostProcessStack(const PostProcessStack&) = delete;
		PostProcessStack& operator = (const PostProcessStack&) = delete;

		bool initialize(int logicalWidth, int logicalHeight);

		void resize(int logicalWidth, int logicalHeight);
		void addEffect(PostProcessEffect effect);
		PostProcessEffect* getEffect(const std::string& name);
		void setEffectEnabled(const std::string& name, bool enabled);
		void clearEffects();

		int effectCount() const { return static_cast<int>(m_effects.size()); }

		void bindSceneFBO();
		void present(int vpX, int vpY, int vpW, int vpH);
		
		bool isInitialized() const { return m_initialized; }

		const Framebuffer& sceneFBO() const { return m_scene; }

	private:
		bool m_initialized = false;

		Framebuffer m_scene;
		Framebuffer m_ping;
		Framebuffer m_pong;

		unsigned int m_quadVAO = 0;
		unsigned int m_quadVBO = 0;

		std::vector<PostProcessEffect> m_effects;

		void initQuad();
		void destroyQuad();

		void runEffect(const PostProcessEffect& fx, const Framebuffer& src, const Framebuffer& dst);
		void blitToScreen(const Framebuffer& src, int vpX, int vpY, int vpW, int vpH);
	};
}
