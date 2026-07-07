#include "HBE/Renderer/SpriteBatch2D.h"

#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/GLShader.h"

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
		m_orderCounter = 0;
		m_quads.clear();
	}

	void SpriteBatch2D::submit(const RenderItem& item) {
		// Only batch if it looks like a sprite-quad draw.
		if (!m_quadMesh || item.mesh != m_quadMesh) {
			return;
		}
		if (!item.material || !item.material->shader) {
			return;
		}

		Quad q{};
		q.material = item.material;
		q.layer = item.layer;
		q.sortKey = item.sortKey;
		q.order = m_orderCounter++;
		emitQuadVertices(item, q.v);

		m_quads.push_back(q);
		m_quadsSubmitted++;
	}

	bool SpriteBatch2D::materialLess(const Quad& a, const Quad& b) {
		// simple stable key: pointer compare.
		// if later want more sharing: compare shader ptr, texture ptr, uniforms, etc.
		return a.material < b.material;
	}

	void SpriteBatch2D::flush(const float* viewProj) {
		if (m_quads.empty()) return;

		initGL();

		// sort by layer -> material -> sortKey -> order FIRST
		std::sort(m_quads.begin(), m_quads.end(), quadLess);

		// one big stsaging buffer for up to m_maxQWuadsPerFlush quads
		m_vertexStaging.clear();
		m_vertexStaging.reserve((size_t)m_maxQuadsPerFlush * 6ull * 5ull);

		const Material* currentMat = nullptr;
		int currentVertexCount = 0;

		auto flushCurrent = [&]() {
			if (!currentMat || currentVertexCount <= 0) return;
			drawRange(currentMat, viewProj, m_vertexStaging.data(), currentVertexCount);
			m_vertexStaging.clear();
			currentVertexCount = 0;
		};

		for (const Quad& q : m_quads) {
			if (!q.material) continue;

			// material change or capcity reached => flush
			if (currentMat && q.material != currentMat) {
				flushCurrent();
			}
			if (!currentMat) currentMat = q.material;

			// if exceed flush capacity, flush and keep same material
			const int vertsToAdd = 6;
			const int maxVerts = m_maxQuadsPerFlush * 6;
			if (currentVertexCount + vertsToAdd > maxVerts) {
				flushCurrent();
			}

			// Append this quad's 30 floats
			m_vertexStaging.insert(m_vertexStaging.end(), q.v, q.v + 30);
			currentVertexCount += 6;

			currentMat = q.material;
		}

		flushCurrent();
	}

	void SpriteBatch2D::initGL() {
		if (m_glInited) return;

		glGenVertexArrays(1, &m_vao);
		glGenBuffers(1, &m_vbo);

		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

		// Allocate a dynamic buffer big enough for one flush
		const size_t maxFloats = (size_t)m_maxQuadsPerFlush * 6ull * 5ull;
		glBufferData(GL_ARRAY_BUFFER, maxFloats * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

		// aPos (locatin = 0) : vec3
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		// aUV (location = 1) : vec2
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

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

	void SpriteBatch2D::drawRange(const Material* mat, const float* viewProj, const float* verts, int vertexCount) {
		if (!mat || !mat->shader || vertexCount <= 0) return;

		// Apply material with VP matrix (we pre-baked model transforms into vertices)
		mat->apply(viewProj);

		// Because your sprite shader multiplies by uUVRect, we force identity.
		// We already baked per-sprite UVRect into vertex UVs.
		int uvLoc = mat->shader->getUniformLocation("uUVRect");
		if (uvLoc >= 0) {
			glUniform4f(uvLoc, 0.0f, 0.0f, 1.0f, 1.0f);
		}

		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

		const size_t floatCount = (size_t)vertexCount * 5ull;
		glBufferSubData(GL_ARRAY_BUFFER, 0, floatCount * sizeof(float), verts);

		glDrawArrays(GL_TRIANGLES, 0, vertexCount);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		m_drawCalls++;
	}

	void SpriteBatch2D::emitQuadVertices(const RenderItem& item, float out30[30]) const {
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

			out30[o++] = wp.x;
			out30[o++] = wp.y;
			out30[o++] = 0.0f; // z
			out30[o++] = fuv.u;
			out30[o++] = fuv.v;
		}
	}

	bool SpriteBatch2D::quadLess(const Quad& a, const Quad& b) {
		if (a.layer != b.layer) return a.layer < b.layer;
		if (a.material != b.material) return a.material < b.material;
		if (a.sortKey != b.sortKey) return a.sortKey < b.sortKey;
		return a.order < b.order; // keep deterministic order within same key
	}
}