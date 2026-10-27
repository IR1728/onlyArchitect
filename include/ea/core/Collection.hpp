#pragma once

#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <functional>

namespace ea {

template <typename T>
class Collection {
public:
    using ItemPtr = std::shared_ptr<T>;
    using FactoryFunc = std::function<ItemPtr(const std::string& name, const std::string& type)>;
    using DeleterFunc = std::function<void(ItemPtr item)>;

    Collection() = default;

    explicit Collection(FactoryFunc factory, DeleterFunc deleter = nullptr)
        : m_factory(std::move(factory)), m_deleter(std::move(deleter)) {}

    // EA IDualCollection API methods
    short Count() const noexcept {
        return static_cast<short>(m_items.size());
    }

    ItemPtr GetAt(short index) const {
        if (index < 0 || static_cast<size_t>(index) >= m_items.size()) {
            return nullptr;
        }
        return m_items[static_cast<size_t>(index)];
    }

    ItemPtr GetByName(const std::string& name) const {
        for (const auto& item : m_items) {
            if constexpr (requires(T t) { { t.GetName() } -> std::convertible_to<std::string>; }) {
                if (item && item->GetName() == name) {
                    return item;
                }
            }
        }
        return nullptr;
    }

    ItemPtr AddNew(const std::string& name = "", const std::string& type = "") {
        ItemPtr item;
        if (m_factory) {
            item = m_factory(name, type);
        } else {
            item = std::make_shared<T>();
            if constexpr (requires(T t) { t.SetName(name); }) {
                item->SetName(name);
            }
        }
        m_items.push_back(item);
        return item;
    }

    void Delete(short index) {
        if (index >= 0 && static_cast<size_t>(index) < m_items.size()) {
            auto it = m_items.begin() + index;
            if (m_deleter && *it) {
                m_deleter(*it);
            }
            m_items.erase(it);
        }
    }

    void DeleteAt(short index, const std::string& /*destination*/) {
        Delete(index);
    }

    void Refresh() {
        // Can be customized if hooked to a database reload callback
    }

    // Direct collection management
    void AddExisting(ItemPtr item) {
        if (item) {
            m_items.push_back(std::move(item));
        }
    }

    void Clear() noexcept {
        m_items.clear();
    }

    // Modern C++ standard iterator support
    auto begin() noexcept { return m_items.begin(); }
    auto end() noexcept { return m_items.end(); }
    auto begin() const noexcept { return m_items.begin(); }
    auto end() const noexcept { return m_items.end(); }
    auto cbegin() const noexcept { return m_items.cbegin(); }
    auto cend() const noexcept { return m_items.cend(); }

    size_t size() const noexcept { return m_items.size(); }
    bool empty() const noexcept { return m_items.empty(); }

    ItemPtr operator[](size_t index) const {
        return (index < m_items.size()) ? m_items[index] : nullptr;
    }

private:
    std::vector<ItemPtr> m_items;
    FactoryFunc m_factory;
    DeleterFunc m_deleter;
};

} // namespace ea
