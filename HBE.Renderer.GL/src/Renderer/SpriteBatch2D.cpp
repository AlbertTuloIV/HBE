#include "HBE/Renderer/SpriteBatch2D.h"

#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/GLShader.h"
#include "HBE/Renderer/Texture2D.h"

#include <glad/glad.h>
#include <algorithm>
#include <cmath>

namespace HBE::Renderer {
	
	SpriteBatch2D::~SpriteBatch2D() {
		destroyGL();
	}

	void SpriteBatch2D::begin() {
		m_drawCalls = 0;
		m_quadsSubmitted = 0;
		m_stateChanges = 0;
		m_orderCounter = 0;
		m_quads.clear();
		resetStateCache();
	}

	void SpriteBatch2D::resetStateCache() {
		m_lastShader = nullptr;
		m_lastTexture = nullptr;
		m_lastMaterial = nullptr;
		m_lastBlend = BlendMode::Invalid;
	}

	std::uint64_t SpriteBatch2D::buildSortKey(const RenderItem& item, const Material& mat) {
		const std::uint64_t pass = static_cast<std::uint64_t>(item.pass);
		const std::uint64_t layer = static_cast<std::uint64_t>(std::clamp(item.layer, 0, 255));
		const std::uint64_t blend = static_cast<std::uint64_t>(mat.blend);
		const std::uint64_t shader = mat.shaderId();
		const std::uint64_t texture = mat.textureId() & 0xFFFu; 

		int depthI = static_cast<int>(item.sortKey);
		depthI = std::clamp(depthI, 0, 4095);
		const std::uint64_t depth = static_cast<std::uint64_t>(depthI);

		return (pass << 56)
			| (layer << 48)
			| (blend << 40)
			| (shader << 24)
			| (texture << 12)
			| depth;
	}

	void SpriteBatch2D::submit(const RenderItem& item) {
		if (!m_quadMesh || item.mesh != m_quadMesh) return;
		if (!item.material || !item.material->shader) return;

		Quad q;
		q.material = item.material;
		q.sortKey = buildSortKey(item, *item.material);
		q.order = m_orderCounter++;
		emitQuadVertices(item, q.v);

		m_quads.push_back(q);
		++m_quadsSubmitted;
	}

	void SpriteBatch2D::flush(const float* viewProj) {
		if (m_quads.empty()) return;

		initGL();
		std::sort(m_quads.begin(), m_quads.end(), quadLess);

		// Force filled polygons for all batched geometry, then restore the caller's
		// mode when we're done. Without saving/restoring, a wireframe debug rect that
		// flushed the batch (via drawDirect) would find polygon mode left at GL_FILL
		// afterwards and render solid instead of outlined.
		GLint prevPolyMode[2] = { GL_FILL, GL_FILL };
		glGetIntegerv(GL_POLYGON_MODE, prevPolyMode);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		m_vertexStaging.clear();
		m_vertexStaging.reserve(static_cast<size_t>(m_maxQuadsPerFlush) * 6ull * 9ull);

		const Material* currentMat = nullptr;
		int currentVertexCount = 0;

		auto flushCurrent = [&]() {
			if (!currentMat || currentVertexCount <= 0) return;

			// Apply only the state that actually changed.
			applyStateDiff(currentMat, viewProj);

			glBindVertexArray(m_vao);
			glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

			const size_t floatCount = static_cast<size_t>(currentVertexCount) * 9ull;
			glBufferSubData(GL_ARRAY_BUFFER, 0, floatCount * sizeof(float), m_vertexStaging.data());

			glDrawArrays(GL_TRIANGLES, 0, currentVertexCount);
			++m_drawCalls;

			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);

			m_vertexStaging.clear();
			currentVertexCount = 0;
			};

		for (const Quad& q : m_quads) {
			if (!q.material) continue;

			const bool materialChanged = (currentMat != q.material);
			const bool wouldOverflow = (currentVertexCount + 6 > m_maxQuadsPerFlush * 6);

			// We can keep growing the current batch only if the *bindable state* matches.
			// Uniform-only differences also force a flush because glUniform is per-draw-call.
			bool canExtend = !materialChanged;
			if (materialChanged && currentMat) {
				canExtend = currentMat->uniformsEqual(*q.material)
					&& currentMat->shader == q.material->shader
					&& currentMat->texture == q.material->texture
					&& currentMat->blend == q.material->blend;
			}

			if ((materialChanged && !canExtend) || wouldOverflow) {
				flushCurrent();
			}

			if (!currentMat) currentMat = q.material;

			m_vertexStaging.insert(m_vertexStaging.end(), q.v, q.v + 54);
			currentVertexCount += 6;
			currentMat = q.material;
		}

		flushCurrent();

		// Restore polygon mode so callers that flushed the batch mid-draw
		// (e.g. DebugDraw2D::rect via drawDirect while in GL_LINE mode)
		// see the same state they set.
		glPolygonMode(GL_FRONT_AND_BACK, prevPolyMode[0]);
	}

	void SpriteBatch2D::initGL() {
		if (m_glInited) return;

		glGenVertexArrays(1, &m_vao);
		glGenBuffers(1, &m_vbo);

		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

		// Allocate a dynamic buffer big enough for one flush.
		// Layout per vertex: pos(3) + uv(2) + color(4) = 9 floats
		const size_t stride = 9ull * sizeof(float);
		const size_t maxFloats = (size_t)m_maxQuadsPerFlush * 6ull * 9ull;
		glBufferData(GL_ARRAY_BUFFER, maxFloats * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

		// aPos (location = 0) : vec3
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)0);
		glEnableVertexAttribArray(0);

		// aUV (location = 1) : vec2
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		// aColor (location = 2) : vec4
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(5 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		m_glInited = true;
	}

	void SpriteBatch2D::destroyGL() {
		if (m_vbo) {
			glDeleteBuffers(1, &m_vbo);
			m_vbo = 0;
		}
		if (m_vao) {
			glDeleteVertexArrays(1, &m_vao);
			m_vao = 0;
		}
		m_glInited = false;
	}

	//void SpriteBatch2D::drawRange(const Material* mat, const float* viewProj, const float* verts, int vertexCount) {
	//	if (!mat || !mat->shader || vertexCount <= 0) return;

	//	// Apply material with VP matrix (we pre-baked model transforms into vertices)
	//	mat->apply(viewProj);

	//	// Because your sprite shader multiplies by uUVRect, we force identity.
	//	// We already baked per-sprite UVRect into vertex UVs.
	//	int uvLoc = mat->shader->getUniformLocation("uUVRect");
	//	if (uvLoc >= 0) {
	//		glUniform4f(uvLoc, 0.0f, 0.0f, 1.0f, 1.0f);
	//	}

	//	glBindVertexArray(m_vao);
	//	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

	//	const size_t floatCount = (size_t)vertexCount * 5ull;
	//	glBufferSubData(GL_ARRAY_BUFFER, 0, floatCount * sizeof(float), verts);

	//	glDrawArrays(GL_TRIANGLES, 0, vertexCount);

	//	glBindBuffer(GL_ARRAY_BUFFER, 0);
	//	glBindVertexArray(0);

	//	m_drawCalls++;
	//}

	void SpriteBatch2D::emitQuadVertices(const RenderItem& item, float out54[54]) const {
		// base unit quad corners (match quad mesh: -0.5..0.5)
		// build 2 traingles:
		// A(-.5, -.5) B(.5, -.5) C(.5,.5)
		// A(-.5, -.5) C(.5, .5) D(-.5, .5)
		struct P2 { float x, y; };
		const P2 local[4] = {
			{-0.5f, -0.5f}, // A 0
			{+0.5f, -0.5f}, // B 1
			{+0.5f, +0.5f}, // C 2
			{-0.5f, +0.5f} // D 3
		};

		// Base UVs for those corners (0..1)
		struct UV2 { float u, v; };
		const UV2 uv[4] = {
			{0.0f, 0.0f}, // A
			{1.0f, 0.0f}, // B
			{1.0f, 1.0f}, // C
			{0.0f, 1.0f} // D
		};

		// Unpack transform
		const float sx = item.transform.scaleX;
		const float sy = item.transform.scaleY;
		const float tx = item.transform.posX;
		const float ty = item.transform.posY;

		const float r = item.transform.rotation;
		const float c = std::cos(r);
		const float s = std::sin(r);

		// UVRect is {u0, v0, uScale, vScale}
		const float u0 = item.uvRect[0];
		const float v0 = item.uvRect[1];
		const float us = item.uvRect[2];
		const float vs = item.uvRect[3];

		// Per-vertex tint (baked in so many differently-colored items can share a draw call)
		const float tr = item.tint.r;
		const float tg = item.tint.g;
		const float tb = item.tint.b;
		const float ta = item.tint.a;

		auto xform = [&](const P2& p) -> P2 {
			// scale
			float x = p.x * sx;
			float y = p.y * sy;

			// rotate
			float xr = x * c - y * s;
			float yr = x * s + y * c;

			// translate
			return { xr + tx, yr + ty };
			};

		auto bakeUV = [&](const UV2& in) -> UV2 {
			// bake: finalUV = baseUV * scale + offset
			return { in.u * us + u0, in.v * vs + v0 };
			};

		const int tri[6] = { 0,1,2, 0,2,3 };

		int o = 0;
		for (int i = 0; i < 6; ++i) {
			const int idx = tri[i];
			const P2 wp = xform(local[idx]);
			const UV2 fuv = bakeUV(uv[idx]);

			out54[o++] = wp.x;
			out54[o++] = wp.y;
			out54[o++] = 0.0f; // z
			out54[o++] = fuv.u;
			out54[o++] = fuv.v;
			out54[o++] = tr;
			out54[o++] = tg;
			out54[o++] = tb;
			out54[o++] = ta;
		}
	}

	bool SpriteBatch2D::quadLess(const Quad& a, const Quad& b) {
		if (a.sortKey != b.sortKey) return a.sortKey < b.sortKey;
		return a.order < b.order;
	}

	bool SpriteBatch2D::applyStateDiff(const Material* mat, const float* viewProj) {
		if (!mat || !mat->shader) return false;
		bool changed = false;

		if (mat->shader != m_lastShader) {
			mat->shader->use();
			m_lastShader = mat->shader;
			changed = true;
		}

		mat->shader->setMat4("uMVP", viewProj);

		int uvLoc = mat->shader->getUniformLocation("uUVRect");
		if (uvLoc >= 0) {
			glUniform4f(uvLoc, 0.0f, 0.0f, 1.0f, 1.0f);
		}

		if (mat->blend != m_lastBlend) {
			switch (mat->blend) {
			case BlendMode::Alpha:
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				break;
			case BlendMode::Additive:
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				break;
			case BlendMode::Opaque:
				glDisable(GL_BLEND);
				break;
			default: break;
			}
			m_lastBlend = mat->blend;
			changed = true;
		}

		if (mat->texture != m_lastTexture) {
			if (mat->texture) {
				glActiveTexture(GL_TEXTURE0);
				mat->texture->bind();
				int texLoc = mat->shader->getUniformLocation("uTex");
				if (texLoc >= 0) glUniform1i(texLoc, 0);
			}
			m_lastTexture = mat->texture;
			changed = true;
		}

		if (!m_lastMaterial || !m_lastMaterial->uniformsEqual(*mat)) {
			int colorLoc = mat->shader->getUniformLocation("uColor");
			if (colorLoc >= 0) {
				glUniform4f(colorLoc, mat->color.r, mat->color.g, mat->color.b, mat->color.a);
			}
			int isSdfLoc = mat->shader->getUniformLocation("uIsSDF");
			if (isSdfLoc >= 0) glUniform1i(isSdfLoc, mat->useSDF ? 1 : 0);
			int sdfSoftLoc = mat->shader->getUniformLocation("uSDFSoftness");
			if (sdfSoftLoc >= 0) glUniform1f(sdfSoftLoc, mat->sdfSoftness);
			changed = true;
		}

		m_lastMaterial = mat;
		if (changed) ++m_stateChanges;
		return changed;
	}
}