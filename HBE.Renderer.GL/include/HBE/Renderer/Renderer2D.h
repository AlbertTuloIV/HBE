#pragma once

#include "HBE/Renderer/RenderItem.h"
#include "HBE/Renderer/RenderPass.h"
#include "HBE/Renderer/Camera2D.h"
#include <memory>

namespace HBE::Renderer {
	class GLRenderer;
	class Mesh;
	class SpriteBatch2D;

	// high-level 2D renderer that wraps a specific backend (currently GLRender)
	class Renderer2D {
	public:
		explicit Renderer2D(GLRenderer& backend);
		~Renderer2D();

		const Camera2D* activeCamera() const { return m_activeCamera; }

		struct Renderer2DStats {
			int drawCalls = 0;
			int quads = 0;
			int stateChanges = 0;
			int passes = 0;
		};

		Renderer2DStats getStats() const;

		// Tell the renderer which mesh is the standard sprite quad ( quad_pos_uv).
		// only draws using this mesh will be batched
		void setSpriteQuadMesh(const Mesh* quadMesh);
				
		// set up the camera for this scene
		void beginScene(const Camera2D& camera);
		void beginScene(const Camera2D& camera, RenderPass pass);

		// for now this is a no-op but later we can flush batches
		void endScene();
		
		// draw a single 2D item
		void draw(const RenderItem& item);

		// Draw immediately, bypassing the batch (use when each item needs a unique color,
		// e.g. per-particle rendering through DebugDraw2D).
		void drawDirect(const RenderItem& item);

	private:
		GLRenderer& m_backend;
		const Camera2D* m_activeCamera = nullptr;
		const Mesh* m_spriteQuadMesh = nullptr;

		// batching
		std::unique_ptr<SpriteBatch2D> m_batch;
		void ensureBatch();

		RenderPass m_currentPass = RenderPass::World;

		int m_frameDrawCalls = 0;
		int m_frameQuads = 0;
		int m_frameStateChanges = 0;
		int m_framePasses = 0;
	};
}