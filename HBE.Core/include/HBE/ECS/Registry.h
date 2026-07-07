#pragma once

#include "HBE/ECS/Entity.h"

#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <cassert>

namespace HBE::ECS {

    // ----------------------------
    // Storage (sparse-set)
    // ----------------------------
    struct IStorage {
        virtual ~IStorage() = default;
        virtual void onEntityDestroyed(Entity e) = 0;
        virtual bool has(Entity e) const = 0;
        virtual std::size_t size() const = 0;
    };

    template<typename T>
    class Storage final : public IStorage {
    public:
        bool has(Entity e) const override {
            if (e == Null) return false;
            const std::uint32_t id = e;
            if (id >= m_sparse.size()) return false;
            const int denseIndex = m_sparse[id];
            if (denseIndex < 0) return false;
            return static_cast<std::size_t>(denseIndex) < m_dense.size() && m_dense[denseIndex] == e;
        }

        std::size_t size() const override { return m_dense.size(); }

        T& get(Entity e) {
            assert(has(e) && "Storage<T>::get called but entity doesn't have component");
            return m_data[m_sparse[e]];
        }

        const T& get(Entity e) const {
            assert(has(e) && "Storage<T>::get const called but entity doesn't have component");
            return m_data[m_sparse[e]];
        }

        template<typename... Args>
        T& emplace(Entity e, Args&&... args) {
            if (has(e)) {
                // overwrite existing
                T& ref = get(e);
                ref = T(std::forward<Args>(args)...);
                return ref;
            }

            const std::uint32_t id = e;
            if (id >= m_sparse.size()) m_sparse.resize(id + 1, -1);

            const int index = static_cast<int>(m_dense.size());
            m_sparse[id] = index;
            m_dense.push_back(e);
            m_data.emplace_back(std::forward<Args>(args)...);
            return m_data.back();
        }

        void remove(Entity e) {
            if (!has(e)) return;

            const int idx = m_sparse[e];
            const int last = static_cast<int>(m_dense.size() - 1);

            if (idx != last) {
                // swap-remove
                m_dense[idx] = m_dense[last];
                m_data[idx] = std::move(m_data[last]);
                m_sparse[m_dense[idx]] = idx;
            }

            m_dense.pop_back();
            m_data.pop_back();
            m_sparse[e] = -1;
        }

        const std::vector<Entity>& denseEntities() const { return m_dense; }

        void onEntityDestroyed(Entity e) override {
            remove(e);
        }

    private:
        std::vector<Entity> m_dense;
        std::vector<T>      m_data;

        // sparse[entity] -> dense index (or -1)
        std::vector<int>    m_sparse;
    };

    // ----------------------------
    // View (iterate entities with Components...)
    // ----------------------------
    template<typename... Components>
    class View {
    public:
        View(class Registry& reg) : m_reg(reg) {}

        struct Iterator {
            View* view = nullptr;
            std::size_t index = 0;

            Entity operator*() const;
            Iterator& operator++();
            bool operator!=(const Iterator& rhs) const { return index != rhs.index; }

        private:
            void advanceToValid();
            friend class View;
        };

        Iterator begin();
        Iterator end();

    private:
        Registry& m_reg;

        // pick a "driver" storage (smallest) to iterate
        IStorage* m_driver = nullptr;
        const std::vector<Entity>* m_driverDense = nullptr;

        void initDriver();
        bool matchesAll(Entity e) const;
    };

    // ----------------------------
    // Registry
    // ----------------------------
    class Registry {
    public:
        Entity create() {
            if (!m_free.empty()) {
                Entity e = m_free.back();
                m_free.pop_back();
                m_alive[e] = true;
                return e;
            }

            Entity e = static_cast<Entity>(m_alive.size());
            m_alive.push_back(true);
            return e;
        }

        void destroy(Entity e) {
            if (!valid(e)) return;

            // Collect storage pinters first to avoid iterator invalidation
            std::vector<IStorage*> storagePtrs;
            storagePtrs.reserve(m_storages.size());
            for (auto& kv : m_storages) {
                storagePtrs.push_back(kv.second.get());
            }

            // safe to call onEntityDestroyed
            for (IStorage* storage : storagePtrs) {
                storage->onEntityDestroyed(e);
            }

            m_alive[e] = false;
            m_free.push_back(e);
        }

        bool valid(Entity e) const {
            return e != Null && e < m_alive.size() && m_alive[e];
        }

        template<typename T>
        bool has(Entity e) const {
            const auto* s = tryStorage<T>();
            return s ? s->has(e) : false;
        }

        template<typename T>
        T* tryGet(Entity e){
            auto* s = tryStorage<T>();
            if (!s || !s->has(e)) return nullptr;
            return &s->get(e);
        }

        template<typename T>
        T& get(Entity e) {
            auto* s = getOrCreateStorage<T>();
            return s->get(e);
        }

        template<typename T>
        const T& get(Entity e) const {
            const auto* s = tryStorage<T>();
            assert(s && "Registry::get const but storage doesn't exist");
            return s->get(e);
        }

        template<typename T, typename... Args>
        T& emplace(Entity e, Args&&... args) {
            auto* s = getOrCreateStorage<T>();
            return s->emplace(e, std::forward<Args>(args)...);
        }

        template<typename T>
        void remove(Entity e) {
            auto* s = tryStorage<T>();
            if (s) s->remove(e);
        }

        template<typename... Components>
        View<Components...> view() {
            return View<Components...>(*this);
        }

        // internal helpers (used by View)
        template<typename T>
        Storage<T>* tryStorage() {
            auto it = m_storages.find(std::type_index(typeid(T)));
            return (it == m_storages.end()) ? nullptr : static_cast<Storage<T>*>(it->second.get());
        }

        template<typename T>
        const Storage<T>* tryStorage() const {
            auto it = m_storages.find(std::type_index(typeid(T)));
            return (it == m_storages.end()) ? nullptr : static_cast<const Storage<T>*>(it->second.get());
        }

        template<typename T>
        Storage<T>* getOrCreateStorage() {
            auto key = std::type_index(typeid(T));
            auto it = m_storages.find(key);
            if (it != m_storages.end()) return static_cast<Storage<T>*>(it->second.get());

            auto ptr = std::make_unique<Storage<T>>();
            Storage<T>* raw = ptr.get();
            m_storages.emplace(key, std::move(ptr));
            return raw;
        }

    private:
        std::vector<bool> m_alive{ false }; // index 0 reserved for Null
        std::vector<Entity> m_free;

        std::unordered_map<std::type_index, std::unique_ptr<IStorage>> m_storages;

        template<typename... Components>
        friend class View;
    };

    // ----------------------------
    // View impl
    // ----------------------------
    template<typename... Components>
    void View<Components...>::initDriver() {
        if (m_driver) return;

        // choose smallest storage among Components... that exists
        std::size_t bestSize = (std::numeric_limits<std::size_t>::max)();

        auto consider = [&](IStorage* s, const std::vector<Entity>* dense) {
            if (!s || !dense) return;
            const std::size_t sz = s->size();
            if (sz < bestSize) {
                bestSize = sz;
                m_driver = s;
                m_driverDense = dense;
            }
            };

        // expand over template pack
        (void)std::initializer_list<int>{
            ([&] {
                auto* st = m_reg.template tryStorage<Components>();
                consider(st, st ? &st->denseEntities() : nullptr);
                }(), 0)...
        };

        // if none exist, set empty
        if (!m_driverDense) {
            static const std::vector<Entity> empty;
            m_driverDense = &empty;
        }
    }

    template<typename... Components>
    bool View<Components...>::matchesAll(Entity e) const {
        // must have all components
        bool ok = true;
        (void)std::initializer_list<int>{
            (ok = ok && m_reg.template has<Components>(e), 0)...
        };
        return ok;
    }

    template<typename... Components>
    typename View<Components...>::Iterator View<Components...>::begin() {
        initDriver();
        Iterator it;
        it.view = this;
        it.index = 0;
        it.advanceToValid();
        return it;
    }

    template<typename... Components>
    typename View<Components...>::Iterator View<Components...>::end() {
        initDriver();
        Iterator it;
        it.view = this;
        it.index = m_driverDense->size();
        return it;
    }

    template<typename... Components>
    void View<Components...>::Iterator::advanceToValid() {
        const auto& dense = *view->m_driverDense;
        while (index < dense.size()) {
            Entity e = dense[index];
            if (view->m_reg.valid(e) && view->matchesAll(e)) break;
            ++index;
        }
    }

    template<typename... Components>
    Entity View<Components...>::Iterator::operator*() const {
        return (*view->m_driverDense)[index];
    }

    template<typename... Components>
    typename View<Components...>::Iterator& View<Components...>::Iterator::operator++() {
        ++index;
        advanceToValid();
        return *this;
    }

}