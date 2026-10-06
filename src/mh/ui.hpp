#pragma once

// High-level API: describe a tab in code and get JSON, definitions (names),
// default values, persistence between launches, and listeners. No manual paths or strings.
//
//   mh::Tab tab("MY_TAB", "My Tab");
//   tab.button("RUN", "Run", [] { ... })
//      .checkbox("ENABLED", "Enabled", true, [](bool on) { ... })
//      .layout("SPEED_ROW", "Speed", [](mh::Layout& row) {
//          row.spinner("SPEED", "Speed", 1.0, {.min = 0, .max = 10, .step = 0.1});
//      });
//   tab.registerTab();

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

    // Spinner bounds. Field order matters for designated initialization: {.min=0, .max=10, .step=0.1}
    struct Range {
        double min = 0.0;
        double max = 100.0;
        double step = 1.0;
    };

    // A widget value in any form. Converted to the required type through Tab::get<T>().
    using Value = std::variant<bool, int64_t, double, std::string>;

    // ---- value storage between launches -----------------------------------
    // By default (if glue.cpp is included), values are stored in your Geode mod's saved values.
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
    void setStorage(std::shared_ptr<Storage> storage); // nullptr — disable persistence
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
            bool decimal = true; // Spinner: true => double, false => int64

            std::function<void()> onClick;
            std::function<void(bool)> onBool;
            std::function<void(double)> onNumber;
            std::function<void(std::string const&)> onString;

            std::vector<Node> children;

            std::string rawJson;                                          // Kind::Raw
            std::vector<std::pair<std::string, std::string>> rawDefs;     // Kind::Raw: path -> name
        };

        struct TabImpl;
    }

    // Common widget-adding methods for Tab and Layout. Each returns *this for chaining.
    template <class Self>
    class Container {
    public:
        // Button. onClick is called when pressed.
        Self& button(std::string id, std::string label, std::function<void()> onClick = {}) {
            detail::Node n; n.kind = detail::Kind::Button; n.id = std::move(id); n.label = std::move(label);
            n.onClick = std::move(onClick);
            return add(std::move(n));
        }

        // Checkbox (bool).
        Self& checkbox(std::string id, std::string label, bool def = false, std::function<void(bool)> onChange = {}) {
            detail::Node n; n.kind = detail::Kind::Checkbox; n.id = std::move(id); n.label = std::move(label);
            n.defBool = def; n.onBool = std::move(onChange);
            return add(std::move(n));
        }

        // Spinner with floating-point values (double).
        Self& spinner(std::string id, std::string label, double def, Range range = {}, std::function<void(double)> onChange = {}) {
            detail::Node n; n.kind = detail::Kind::Spinner; n.id = std::move(id); n.label = std::move(label);
            n.defNumber = def; n.range = range; n.decimal = true; n.onNumber = std::move(onChange);
            return add(std::move(n));
        }

        // Spinner with integer values (int64).
        Self& integer(std::string id, std::string label, int64_t def, Range range = {}, std::function<void(double)> onChange = {}) {
            detail::Node n; n.kind = detail::Kind::Spinner; n.id = std::move(id); n.label = std::move(label);
            n.defNumber = static_cast<double>(def); n.range = range; n.decimal = false; n.onNumber = std::move(onChange);
            return add(std::move(n));
        }

        // Text field (string).
        Self& textbox(std::string id, std::string label, std::string def = {}, std::function<void(std::string const&)> onChange = {}) {
            detail::Node n; n.kind = detail::Kind::Textbox; n.id = std::move(id); n.label = std::move(label);
            n.defString = std::move(def); n.onString = std::move(onChange);
            return add(std::move(n));
        }

        // Group of widgets in a single row/block. Inside the lambda, the same methods are available.
        Self& layout(std::string id, std::string label, std::function<void(Layout&)> build);

        // Fallback for widgets that are not available in the builder (combobox, colour, shortcut, picker, option...).
        // json — one complete JSON widget object; defs — pairs of {full path, name}.
        Self& raw(std::string json, std::vector<std::pair<std::string, std::string>> defs = {}) {
            detail::Node n; n.kind = detail::Kind::Raw; n.rawJson = std::move(json); n.rawDefs = std::move(defs);
            return add(std::move(n));
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
        detail::Node n; n.kind = detail::Kind::Layout; n.id = std::move(id); n.label = std::move(label);
        Layout inner;
        if (build) build(inner);
        n.children = std::move(inner.nodes());
        return add(std::move(n));
    }

    // Tab. This is a lightweight handle: copies point to the same tab,
    // and after registerTab(), its internal state lives until the end of the game (MH stores pointers to it).
    class Tab : public Container<Tab> {
    public:
        // id: only A-Z a-z 0-9 _ (the path separator '/' is forbidden). title — the title shown in the MH tab.
        Tab(std::string id, std::string title);

        Tab& onOpen(std::function<void()> cb);    // MH menu opened
        Tab& onCommit(std::function<void()> cb);  // menu closed, values committed
        Tab& onUnload(std::function<void()> cb);

        // Persist values between launches (enabled by default).
        Tab& persist(bool enabled);

        // Validates the description, writes definitions and default values, registers the tab in MH
        // and attaches listeners. false => check lastError() and the log.
        bool registerTab();
        bool isRegistered() const;
        std::string const& lastError() const;

        // Tab JSON (for debugging and tests).
        std::string toJson() const;

        std::string const& id() const;
        // Full widget key. Accepts a short id ("SPEED", if it is unique within the tab)
        // or a relative path ("SPEED_ROW/SPEED"). An empty string means the widget was not found or is ambiguous.
        std::string path(std::string const& id) const;

        // Read/write a widget value. Type T can be any convenient type; reading uses the method
        // required by the widget itself (checkbox -> bool, spinner -> double, integer -> int64, textbox -> string).
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

    // ---- when tabs can be registered -----------------------------------
    // Mega Hack does not load instantly. onReady waits for the main menu (and, if needed, delayFrames frames),
    // checks that MH is installed, and only then calls fn. If MH is not present, fn is simply not called.
    // Implementation is in glue.cpp (requires Geode).
    void onReady(std::function<void()> fn, int delayFrames = 0);
}
