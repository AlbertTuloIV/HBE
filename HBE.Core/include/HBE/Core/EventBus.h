#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace HBE::Core {

	struct EventSubscription {
		std::type_index type = std::type_index(typeid(void));
		std::uint64_t id = 0;

		bool valid() const { return id != 0; }
	};

	class EventBus {
	public:
		EventBus() = default;
		~EventBus() = default;

		EventBus(const EventBus&) = delete;
		EventBus& operator=(const EventBus&) = delete;

		template <class TEvent> EventSubscription subscribe(std::function<void(const TEvent&)> handler) {
			if (!handler) return {};

			const std::type_index key = std::type_index(typeid(TEvent));
			auto& bucket = m_buckets[key];

			Entry e{};
			e.id = ++m_nextId;
			e.handler = [fn = std::move(handler)](const void* payload) {
				fn(*static_cast<const TEvent*>(payload));
			};
			bucket.push_back(std::move(e));

			EventSubscription sub{ key, m_nextId };
			return sub;
		}
		void unsubscribe(const EventSubscription& sub) {
			if (!sub.valid()) return;
			auto it = m_buckets.find(sub.type);
			if (it == m_buckets.end()) return;

			auto& bucket = it->second;
			for (std::size_t i = 0; i < bucket.size(); ++i) {
				if (bucket[i].id == sub.id) {
					bucket[i].handler = nullptr;
					bucket[i].id = 0;
					m_dirty = true;
					return;
				}
			}
		}

		template <class TEvent> void publish(const TEvent& evt) {
			const std::type_index key = std::type_index(typeid(TEvent));
			auto it = m_buckets.find(key);
			if (it == m_buckets.end()) return;

			auto& bucket = it->second;
			const std::size_t snapshot = bucket.size();
			for (std::size_t i = 0; i < snapshot; ++i) {
				if (i >= bucket.size()) break;
				if (!bucket[i].handler) continue;
				bucket[i].handler(&evt);
			}

			if (m_dirty) compact(key);
		}

		template <class TEvent> void enqueue(const TEvent& evt) {
			const std::type_index key = std::type_index(typeid(TEvent));
			auto& q = m_pending;
			Pending p{};
			p.dispatch = [key, evt, this]() {
				this->publish<TEvent>(evt);
			};
			q.push_back(std::move(p));
		}

		void drain() {
			std::vector<Pending> local;
			local.swap(m_pending);
			for (auto& p : local) {
				if (p.dispatch) p.dispatch();
			}
		}

		std::size_t subscriberCount(std::type_index t) const {
			auto it = m_buckets.find(t);
			if (it == m_buckets.end()) return 0;
			std::size_t n = 0;
			for (const auto& e : it->second) if (e.handler) ++n;
			return n;
		}

		void clear() {
			m_buckets.clear();
			m_pending.clear();
			m_dirty = false;
			// NOTE: m_nextId is intentionally NOT reset. Subscription IDs must
			// stay monotonic for the lifetime of the bus: ScopedSubscription
			// handles created before a clear() still hold their old IDs, and
			// reusing IDs would let their reset()/unsubscribe() tear down a
			// freshly re-subscribed handler that happened to reuse the same ID.
		}
	private:
		struct Entry {
			std::uint64_t id = 0;
			std::function<void(const void*)> handler;
		};
		struct Pending {
			std::function<void()> dispatch;
		};

		void compact(std::type_index key) {
			auto it = m_buckets.find(key);
			if (it == m_buckets.end()) return;
			auto& v = it->second;
			v.erase(std::remove_if(v.begin(), v.end(), [](const Entry& e) {return !e.handler; }),
				v.end());
			m_dirty = false;
		}

		std::unordered_map<std::type_index, std::vector<Entry>> m_buckets;
		std::vector<Pending> m_pending;
		std::uint64_t m_nextId = 0;
		bool m_dirty = false;
	};

	class ScopedSubscription {
	public:
		ScopedSubscription() = default;
		ScopedSubscription(EventBus& bus, EventSubscription sub) : m_bus(&bus), m_sub(sub) {}

		~ScopedSubscription() { reset(); }

		ScopedSubscription(const ScopedSubscription&) = delete;
		ScopedSubscription& operator = (const ScopedSubscription&) = delete;

		ScopedSubscription(ScopedSubscription&& other) noexcept : m_bus(other.m_bus), m_sub(other.m_sub) {
			other.m_bus = nullptr;
			other.m_sub = {};
		}
		ScopedSubscription& operator = (ScopedSubscription&& other) noexcept {
			if (this != &other) {
				reset();
				m_bus = other.m_bus;
				m_sub = other.m_sub;
				other.m_bus = nullptr;
				other.m_sub = {};
			}
			return *this;
		}

		void reset() {
			if (m_bus && m_sub.valid()) {
				m_bus->unsubscribe(m_sub);
			}
			m_bus = nullptr;
			m_sub = {};
		}

		bool valid() const { return m_bus && m_sub.valid(); }

	private:
		EventBus* m_bus = nullptr;
		EventSubscription m_sub{};
	};
}