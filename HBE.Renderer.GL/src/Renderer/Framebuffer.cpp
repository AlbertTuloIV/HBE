#include "HBE/Renderer/Framebuffer.h"
#include "HBE/Core/Log.h"

#include <glad/glad.h>
#include <string>

namespace HBE::Renderer {
	using HBE::Core::LogError;

	Framebuffer::~Framebuffer() {
		destroy();
	}

	bool Framebuffer::resize(int width, int height) {
		if (width <= 0 || height <= 0) return false;
		if (m_width == width && m_height == height && m_fbo != 0) return true;

		destroy();

		glGenTextures(1, &m_colorTex);
		glBindTexture(GL_TEXTURE_2D, m_colorTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenFramebuffers(1, &m_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			LogError("Framebuffer::resize: FBO incomplete, status=" + std::to_string(status));
			destroy();
			return false;
		}

		m_width = width;
		m_height = height;
		return true;
	}

	void Framebuffer::bind() const {
		glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	}

	void Framebuffer::bindDefault() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Framebuffer::destroy() {
		if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
		if(m_colorTex){ glDeleteTextures(1, &m_colorTex); m_colorTex = 0; }
		m_width = 0;
		m_height = 0;
	}
}