#pragma once

// Tab builder on top of api.hpp: describe a tab in code, get JSON, names,
// default values, persistence and listeners. Usage example is in README.

#include "api.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace mh {

    // Field order matters for designated init: {.min = 0, .max = 10, .step = 0.1}
    struct Range {
        double min = 0.0;
        double max = 100.0;
        double step = 1.0;
    };

    using Value = std::variant<bool, int64_t, double, std::string>;

    // Where widget values live between launches. glue.cpp plugs in Geode saved values.
    class Storage {
    public:
        virtual ~Storage() = default;
        virtual std::optional<bool> loadBool(std::string const& key) = 0;
        virtual std::optional<double> loadNumber(std::string const& key) = 0;
        virtual std::optional<std::string> loadString(std::string const& key) = 0;
        virtual void saveBool(std::string const& key, bool value) = 0;
        virtual void saveNumber(std::string const& key, double value) = 0;
        virtual void saveString(std::string const& key, std::string const& value) = 0;
    };
    void setStorage(std::shared_ptr<Storage> storage); // nullptr disables persistence
    std::shared_ptr<Storage> storage();

    class Layout;
    class Tab;

    namespace detail {
        enum class Kind { Button, Checkbox, Spinner, Textbox, Layout, Raw };

        struct Node {
            Kind kind = Kind::Button;
            std::string id;
            std::string label;

            bool defBool = false;
            double defNumber = 0.0;
            std::string defString;
            Range range;
            bool decimal = true; // spinner: double or int64

            std::function<void()> onClick;
            std::function<void(bool)> onBool;
            std::function<void(double)> onNumber;
            std::function<void(std::string const&)> onString;

            std::vector<Node> children;

            std::string rawJson;
            std::vector<std::pair<std::string, std::string>> rawDefs; // full path -> name
        };

        struct TabImpl;
    }

    // Widget methods shared by Tab and Layout, all chainable.
    template <class Self>
    class Container {
    public:
        Self& button(std::string id, std::string label, std::function<void()> onClick = {}) {
            return add({.kind = detail::Kind::Button, .id = std::move(id), .label = std::move(label),
                        .onClick = std::move(onClick)});
        }

        Self& checkbox(std::string id, std::string label, bool def = false, std::function<void(bool)> onChange = {}) {
            return add({.kind = detail::Kind::Checkbox, .id = std::move(id), .label = std::move(label),
                        .defBool = def, .onBool = std::move(onChange)});
        }

        Self& spinner(std::string id, std::string label, double def, Range range = {}, std::function<void(double)> onChange = {}) {
            return add({.kind = detail::Kind::Spinner, .id = std::move(id), .label = std::move(label),
                        .defNumber = def, .range = range, .decimal = true, .onNumber = std::move(onChange)});
        }

        Self& integer(std::string id, std::string label, int64_t def, Range range = {}, std::function<void(double)> onChange = {}) {
            return add({.kind = detail::Kind::Spinner, .id = std::move(id), .label = std::move(label),
                        .defNumber = static_cast<double>(def), .range = range, .decimal = false,
                        .onNumber = std::move(onChange)});
        }

        Self& textbox(std::string id, std::string label, std::string def = {}, std::function<void(std::string const&)> onChange = {}) {
            return add({.kind = detail::Kind::Textbox, .id = std::move(id), .label = std::move(label),
                        .defString = std::move(def), .onString = std::move(onChange)});
        }

        // Several widgets in one row. The lambda gets the same methods.
        Self& layout(std::string id, std::string label, std::function<void(Layout&)> build);

        // For widgets the builder doesn't know yet (combobox, colour, shortcut...).
        // json is one complete widget object, defs are {full path, name} pairs.
        Self& raw(std::string json, std::vector<std::pair<std::string, std::string>> defs = {}) {
            return add({.kind = detail::Kind::Raw, .rawJson = std::move(json), .rawDefs = std::move(defs)});
        }

    private:
        Self& add(detail::Node n) {
            self().nodes().push_back(std::move(n));
            return self();
        }
        Self& self() { return static_cast<Self&>(*this); }
    };

    class Layout : public Container<Layout> {
    public:
        std::vector<detail::Node>& nodes() { return m_nodes; }
    private:
        std::vector<detail::Node> m_nodes;
    };

    template <class Self>
    Self& Container<Self>::layout(std::string id, std::string label, std::function<void(Layout&)> build) {
        Layout inner;
        if (build) build(inner);
        return add({.kind = detail::Kind::Layout, .id = std::move(id), .label = std::move(label),
                    .children = std::move(inner.nodes())});
    }

    // Cheap handle: copies share the same tab. After registerTab() the state lives until
    // the game exits, because Mega Hack keeps raw pointers into it.
    class Tab : public Container<Tab> {
    public:
        // id: A-Z a-z 0-9 _ only ('/' is the path separator)
        Tab(std::string id, std::string title);

        Tab& onOpen(std::function<void()> cb);
        Tab& onCommit(std::function<void()> cb);
        Tab& onUnload(std::function<void()> cb);

        Tab& persist(bool enabled); // on by default

        // false => see lastError()
        bool registerTab();
        bool isRegistered() const;
        std::string const& lastError() const;

        std::string toJson() const;

        std::string const& id() const;
        // Full key by short id ("SPEED", if unique in the tab) or relative path ("SPEED_ROW/SPEED").
        // Empty if not found or ambiguous.
        std::string path(std::string const& id) const;

        std::optional<Value> value(std::string const& id) const;
        bool setValue(std::string const& id, Value const& v);

        template <class T>
        T get(std::string const& id) const {
            auto v = value(id);
            if (!v) return T{};
            return std::visit([](auto const& x) -> T { return convert<T>(x); }, *v);
        }

        template <class T>
        bool set(std::string const& id, T const& v) {
            if constexpr (std::is_same_v<T, bool>) return setValue(id, Value{v});
            else if constexpr (std::is_integral_v<T>) return setValue(id, Value{static_cast<int64_t>(v)});
            else if constexpr (std::is_floating_point_v<T>) return setValue(id, Value{static_cast<double>(v)});
            else return setValue(id, Value{std::string(v)});
        }

        std::vector<detail::Node>& nodes();

    private:
        template <class T, class X>
        static T convert(X const& x) {
            if constexpr (std::is_same_v<T, std::string>) {
                if constexpr (std::is_same_v<X, std::string>) return x;
                else if constexpr (std::is_same_v<X, bool>) return x ? "true" : "false";
                else return std::to_string(x);
            } else if constexpr (std::is_same_v<T, bool>) {
                if constexpr (std::is_same_v<X, std::string>) return !x.empty();
                else return x != X{};
            } else if constexpr (std::is_arithmetic_v<T>) {
                if constexpr (std::is_same_v<X, std::string>) return T{};
                else return static_cast<T>(x);
            } else {
                return T{};
            }
        }

        std::shared_ptr<detail::TabImpl> m_impl;
    };

    // Mega Hack isn't ready at startup. This waits for the main menu (plus delayFrames),
    // checks that MH is installed and only then calls fn; without MH it never does.
    // Implemented in glue.cpp, needs Geode.
    void onReady(std::function<void()> fn, int delayFrames = 0);
}
