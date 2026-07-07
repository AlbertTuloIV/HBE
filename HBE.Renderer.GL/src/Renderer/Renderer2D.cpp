#include "HBE/Renderer/Renderer2D.h"
#include "HBE/Renderer/GLRenderer.h"
#include "HBE/Renderer/Camera2D.h"
#include "HBE/Renderer/SpriteBatch2D.h"
#include "HBE/Renderer/Mesh.h"

#include <glad/glad.h>

namespace HBE::Renderer {

	Renderer2D::Renderer2D(GLRenderer& backend) : m_backend(backend) {}
	Renderer2D::~Renderer2D() = default;

	void Renderer2D::ensureBatch() {
		if (!m_batch) {
			m_batch = std::make_unique<SpriteBatch2D>();
			m_batch->setQuadMesh(m_spriteQuadMesh);
		}
	}

	void Renderer2D::setSpriteQuadMesh(const Mesh* quadMesh) {
		m_spriteQuadMesh = quadMesh;
		if (m_batch) {
			m_batch->setQuadMesh(m_spriteQuadMesh);
		}
	}

	void Renderer2D::beginScene(const Camera2D& camera) {
		beginScene(camera, RenderPass::World);
	}

	void Renderer2D::beginScene(const Camera2D& camera, RenderPass pass) {
		if (m_activeCamera) endScene();

		m_activeCamera = &camera;
		m_currentPass = pass;
		m_backend.setCamera(camera);

		// Safety net: for any draw that uses the sprite shader but bypasses the batch
		// (i.e. a Mesh whose VAO doesn't feed aColor at location 2), fall back to a
		// generic vertex attribute default of (1,1,1,1). Without this, undefined color
		// values multiply the texture down to black.
		glVertexAttrib4f(2, 1.0f, 1.0f, 1.0f, 1.0f);

		ensureBatch();
		m_batch->begin();
	}

	void Renderer2D::endScene() {
		if (!m_activeCamera) return;

		if (m_batch) {
			float vp[16];
			m_backend.getViewProjection(vp);
			m_batch->flush(vp);

			m_frameDrawCalls += m_batch->drawCalls();
			m_frameQuads += m_batch->quadCount();
			m_frameStateChanges += m_batch->stateChanges();
		}

		++m_framePasses;
		m_activeCamera = nullptr;
	}

	void Renderer2D::draw(const RenderItem& item) {
		if (m_batch && m_spriteQuadMesh && item.mesh == m_spriteQuadMesh) {
			RenderItem stamped = item;
			if (stamped.pass == RenderPass::World && m_currentPass != RenderPass::World) {
				stamped.pass = m_currentPass;
			}
			m_batch->submit(stamped);
			return;
		}
		// Fallback for everything else (debug draw meshes, etc.)
		m_backend.draw(item);
	}

	void Renderer2D::drawDirect(const RenderItem& item) {
		if (m_batch && m_activeCamera) {
			float vp[16];
			m_backend.getViewProjection(vp);
			m_batch->flush(vp);
			m_batch->begin();
		}
		m_backend.draw(item);
	}

	Renderer2D::Renderer2DStats Renderer2D::getStats() const {
		Renderer2DStats s{};
		s.drawCalls = m_frameDrawCalls;
		s.quads = m_frameQuads;
		s.stateChanges = m_frameStateChanges;
		s.passes = m_framePasses;
		return s;
	}
}