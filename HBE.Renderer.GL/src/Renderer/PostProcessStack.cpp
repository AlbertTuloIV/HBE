#include "HBE/Renderer/PostProcessStack.h"
#include "HBE/Renderer/GLShader.h"
#include "HBE/Core/Log.h"

#include <glad/glad.h>
#include <algorithm>

namespace HBE::Renderer {
	using HBE::Core::LogError;

	static constexpr float k_quadVerts[] = {
		//x		y		u		v
		-1.0f,	-1.0f,	0.0f,	0.0f,
		1.0f,	-1.0f,	1.0f,	0.0f,
		1.0f,	1.0f,	1.0f,	1.0f,

		1.0f,	1.0f,	1.0f,	1.0f,
		-1.0f,	1.0f,	0.0f,	1.0f,
		-1.0f,	-1.0f,	0.0f,	0.0f,
	};

	PostProcessStack::~PostProcessStack() {
		destroyQuad();
	}

	bool PostProcessStack::initialize(int logicalWidth, int logicalHeight) {
		if (!m_scene.resize(logicalWidth, logicalHeight)) return false;
		if (!m_ping.resize(logicalWidth, logicalHeight)) return false;
		if (!m_pong.resize(logicalWidth, logicalHeight)) return false;
		initQuad();
		m_initialized = true;
		return true;
	}

	void PostProcessStack::resize(int logicalWidth, int logicalHeight) {
		m_scene.resize(logicalWidth, logicalHeight);
		m_ping.resize(logicalWidth, logicalHeight);
		m_pong.resize(logicalWidth, logicalHeight);
	}

	void PostProcessStack::addEffect(PostProcessEffect effect) {
		m_effects.push_back(std::move(effect));
	}

	PostProcessEffect* PostProcessStack::getEffect(const std::string& name) {
		for (auto& fx : m_effects) {
			if (fx.name == name) return &fx;
		}
		return nullptr;
	}

	void PostProcessStack::setEffectEnabled(const std::string& name, bool enabled) {
		if (auto* fx = getEffect(name)) fx->enabled = enabled;
	}

	void PostProcessStack::clearEffects() {
		m_effects.clear();
	}

	void PostProcessStack::bindSceneFBO() {
		if (!m_initialized) return;
		m_scene.bind();
	}

	void PostProcessStack::present(int vpX, int vpY, int vpW, int vpH) {
		if (!m_initialized) return;
		int enabledCount = 0;
		for (const auto& fx : m_effects) {
			if (fx.enabled && fx.shader) ++enabledCount;
		}

		if (enabledCount == 0) {
			blitToScreen(m_scene, vpX, vpY, vpW, vpH);
			return;
		}

		const Framebuffer* src = &m_scene;
		const Framebuffer* dst = &m_ping;
		bool usedPing = false;

		for (const auto& fx : m_effects) {
			if (!fx.enabled || !fx.shader) continue;

			if (dst == &m_ping) {
				runEffect(fx, *src, m_ping);
				src = &m_ping;
				dst = &m_pong;
				usedPing = true;
			}
			else {
				runEffect(fx, *src, m_pong);
				src = &m_pong;
				dst = &m_ping;
			}
		}
		blitToScreen(*src, vpX, vpY, vpW, vpH);
	}

	void PostProcessStack::initQuad() {
		glGenVertexArrays(1, &m_quadVAO);
		glGenBuffers(1, &m_quadVBO);

		glBindVertexArray(m_quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(k_quadVerts), k_quadVerts, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}

	void PostProcessStack::destroyQuad() {
		if (m_quadVAO) { glDeleteVertexArrays(1, &m_quadVAO); m_quadVAO = 0; }
		if (m_quadVBO) { glDeleteBuffers(1, &m_quadVBO); m_quadVBO = 0; }
	}

	void PostProcessStack::runEffect(const PostProcessEffect& fx, const Framebuffer& src, const Framebuffer& dst) {
		dst.bind();
		glViewport(0, 0, dst.width(), dst.height());
		glDisable(GL_BLEND); // post-process quads must not blend against the cleared dst
		glClear(GL_COLOR_BUFFER_BIT);

		fx.shader->use();

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, src.colorTextureID());
		int sceneLoc = fx.shader->getUniformLocation("uScene");
		if (sceneLoc >= 0) glUniform1i(sceneLoc, 0);

		int resLoc = fx.shader->getUniformLocation("uResolution");
		if (resLoc >= 0) glUniform2f(resLoc, (float)src.width(), (float)src.height());

		// Upload named uniforms by convention from the params array.
		// Each shader only exposes the ones it needs; -1 locations are silently skipped.
		const auto& p = fx.params;
		auto u1f = [&](const char* name, float v) {
			int loc = fx.shader->getUniformLocation(name);
			if (loc >= 0) glUniform1f(loc, v);
		};

		// Bloom
		u1f("uThreshold",        p[0]);
		u1f("uIntensity",        p[1]);
		// Color grade
		u1f("uBrightness",       p[0]);
		u1f("uContrast",         p[1]);
		u1f("uSaturation",       p[2]);
		// Vignette
		u1f("uRadius",           p[0]);
		u1f("uSoftness",         p[1]);
		// CRT
		u1f("uScanlineStrength", p[0]);
		u1f("uCurvature",        p[1]);
		// Pixelate
		u1f("uPixelSize",        p[0]);

		glBindVertexArray(m_quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Restore alpha blending for any subsequent world/UI rendering.
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	void PostProcessStack::blitToScreen(const Framebuffer& src, int vpX, int vpY, int vpW, int vpH) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, src.fboID());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(
			0, 0, src.width(), src.height(),
			vpX, vpY, vpX + vpW, vpY + vpH,
			GL_COLOR_BUFFER_BIT, GL_LINEAR
		);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}