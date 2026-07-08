#pragma once
#include <cstdint>


namespace HBE::Renderer {
	class Framebuffer {
	public:
		Framebuffer() = default;
		~Framebuffer();

		Framebuffer(const Framebuffer&) = delete;
		Framebuffer& operator = (const Framebuffer&) = delete;

		bool resize(int width, int height);
		
		void bind() const;

		static void bindDefault();

		unsigned int colorTextureID() const { return m_colorTex; };
		unsigned int fboID() const { return m_fbo; }
		int width() const { return m_width; }
		int height() const { return m_height; }
		bool vlaid() const { return m_fbo != 0; }

	private:
		unsigned int m_fbo = 0;
		unsigned int m_colorTex = 0;
		int m_width = 0;
		int m_height = 0;

		void destroy();
	};
}