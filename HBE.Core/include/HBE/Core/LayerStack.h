#pragma once

#include <vector>
#include <memory>

#include "HBE/Core/Layer.h"

namespace HBE::Core {

	class Application;

	class Event;

	class LayerStack {
	public:
		LayerStack() = default;
		~LayerStack() = default;

		std::vector<std::unique_ptr<Layer>> m_layers;

		void pushLayer(std::unique_ptr<Layer> layer, Application& app);
		void pushOverlay(std::unique_ptr<Layer> overlay, Application& app);

		void popLayer(Layer* layer);
		void popOverlay(Layer* overlay);

		void clear();

		void dispatchEvent(Event& e);

		// iteration
		auto begin() { return m_layers.begin(); }
		auto end() { return m_layers.end(); }

		auto rbegin() { return m_layers.rbegin(); }
		auto rend() { return m_layers.rend(); }

	private:
		std::size_t m_insertIndex = 0; // Layers go before overlays
	};
}