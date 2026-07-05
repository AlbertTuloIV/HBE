#include "HBE/Core/Log.h"

#include "HBE/Renderer/Texture2D.h"

#include <glad/glad.h>
#include <vector>
#include <cstdint>
#include <string>

// stb_image implementation in exactly ONE .cpp:
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace HBE::Renderer {

    Texture2D::~Texture2D() {
        destroy();
    }

    void Texture2D::destroy() {
        if (m_id != 0) {
            glDeleteTextures(1, &m_id);
            m_id = 0;
        }
    }

    bool Texture2D::createChecker(int width, int height) {
        if (width <= 0 || height <= 0) {
            HBE::Core::LogError("Texture2D::createChecker: invalid dimensions " + std::to_string(width) + "x" + std::to_string(height));
            return false;
        }

        destroy();

        m_width = width;
        m_height = height;

        const int comp = 4; // RGBA
        std::vector<std::uint8_t> pixels(width * height * comp);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int tile = ((x / 8) + (y / 8)) & 1;
                std::uint8_t r = tile ? 255 : 40;
                std::uint8_t g = tile ? 255 : 40;
                std::uint8_t b = tile ? 255 : 160;
                std::uint8_t a = 255;

                int idx = (y * width + x) * comp;
                pixels[idx + 0] = r;
                pixels[idx + 1] = g;
                pixels[idx + 2] = b;
                pixels[idx + 3] = a;
            }
        }

        glGenTextures(1, &m_id);
        glBindTexture(GL_TEXTURE_2D, m_id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            width,
            height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            pixels.data()
        );

        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    bool Texture2D::createFromRGBA(int width, int height, const unsigned char* rgbaPixels) {
        if (!rgbaPixels) {
            HBE::Core::LogError("Texture2D::createFromRGBA: null pixel data");
            return false;
        }
        if (width <= 0 || height <= 0) {
            HBE::Core::LogError("Texture2D::createdFromRGBA: invalid dimensions " + std::to_string(width) + "x" + std::to_string(height));
            return false;
        }

        destroy();

        m_width = width;
        m_height = height;

        glGenTextures(1, &m_id);
        glBindTexture(GL_TEXTURE_2D, m_id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            width,
            height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            rgbaPixels
        );

        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }


    bool Texture2D::loadFromFile(const std::string& path) {
        destroy();

        int width = 0, height = 0, channels = 0;

        // Flip vertically so texture coords (0,0) = bottom-left
        stbi_set_flip_vertically_on_load(1);

        unsigned char* data =
            stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!data) {
            // could log here if you like
            return false;
        }

        if (width <= 0 || height <= 0) {
            HBE::Core::LogError("Texture2D::loadFromFile: invalid dimensions in " + path);
            stbi_image_free(data);
            return false;
        }

        // ?? Store size for later UV / frame calculations        
        m_width = width;
        m_height = height;

        glGenTextures(1, &m_id);
        glBindTexture(GL_TEXTURE_2D, m_id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            width,
            height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            data
        );

        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(data);

        return true;
    }


    void Texture2D::bind() const {
        glBindTexture(GL_TEXTURE_2D, m_id);
    }

    void Texture2D::setFiltering(bool linear) {
        if (m_id == 0) return;

        glBindTexture(GL_TEXTURE_2D, m_id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

} // namespace HBE::Renderer
