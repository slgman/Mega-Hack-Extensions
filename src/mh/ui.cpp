#include "ui.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <set>

namespace mh {

    namespace {
        std::shared_ptr<Storage> g_storage;

        // MH keeps raw pointers to the delegate and listener state, so tabs must never die
        std::vector<std::shared_ptr<detail::TabImpl>>& alive() {
            static std::vector<std::shared_ptr<detail::TabImpl>> v;
            return v;
        }

        void logErr(std::string const& m) { detail::log(detail::LogLevel::Error, "mh::Tab: " + m); }
        void logWarn(std::string const& m) { detail::log(detail::LogLevel::Warn, "mh::Tab: " + m); }

        // exceptions must not leak into Mega Hack
        template <class F>
        void guarded(char const* what, F&& f) {
            try { f(); }
            catch (std::exception const& e) { logErr(std::string(what) + ": " + e.what()); }
            catch (...) { logErr(std::string(what) + ": unknown exception"); }
        }

        bool validId(std::string const& s) {
            return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) {
                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
            });
        }

        std::string esc(std::string const& s) {
            std::string o;
            o.reserve(s.size() + 2);
            for (unsigned char c : s) {
                switch (c) {
                    case '"': o += "\\\""; break;
                    case '\\': o += "\\\\"; break;
                    case '\n': o += "\\n"; break;
                    case '\r': o += "\\r"; break;
                    case '\t': o += "\\t"; break;
                    default:
                        if (c < 0x20) {
                            char b[8];
                            std::snprintf(b, sizeof b, "\\u%04x", c);
                            o += b;
                        } else {
                            o += static_cast<char>(c);
                        }
                }
            }
            return o;
        }

        // to_chars instead of printf: no "0,1" on a Russian locale
        std::string num(double v) {
            if (!std::isfinite(v)) return "0";
            char buf[64];
            auto r = std::to_chars(buf, buf + sizeof buf, v);
            return std::string(buf, r.ptr);
        }
    }

    void setStorage(std::shared_ptr<Storage> s) { g_storage = std::move(s); }
    std::shared_ptr<Storage> storage() { return g_storage; }

    namespace detail {
        struct TabImpl {
            std::string id, title;
            std::vector<Node> nodes;
            bool persist = true;
            bool registered = false;
            std::string lastError;
            std::function<void()> cbOpen, cbCommit, cbUnload;
            std::unique_ptr<Delegate> delegate;
        };
    }

    namespace {
        using detail::Kind;
        using detail::Node;
        using detail::TabImpl;

        class TabDelegate final : public Delegate {
        public:
            explicit TabDelegate(TabImpl* impl) : m_impl(impl) {}
            void onOpen() override { call(m_impl->cbOpen, "onOpen"); }
            void onCommit() override { call(m_impl->cbCommit, "onCommit"); }
            void onUnload() override { call(m_impl->cbUnload, "onUnload"); }

        private:
            static void call(std::function<void()> const& f, char const* what) { if (f) guarded(what, f); }
            TabImpl* m_impl;
        };

        // fn gets each node with its full path ("TAB/LAYOUT/WIDGET"); raw nodes have an empty path
        template <class F>
        void walk(std::vector<Node> const& nodes, std::string const& prefix, F&& fn) {
            for (auto const& n : nodes) {
                if (n.kind == Kind::Raw) {
                    fn(n, std::string{});
                    continue;
                }
                auto p = prefix + "/" + n.id;
                fn(n, p);
                if (n.kind == Kind::Layout) walk(n.children, p, fn);
            }
        }

        // returns the first problem found, or an empty string
        std::string validateNodes(std::vector<Node> const& nodes, std::string const& where) {
            std::set<std::string> seen;
            for (auto const& n : nodes) {
                if (n.kind == Kind::Raw) {
                    auto const& j = n.rawJson;
                    auto a = j.find_first_not_of(" \t\r\n");
                    auto b = j.find_last_not_of(" \t\r\n");
                    if (a == std::string::npos || j[a] != '{' || j[b] != '}')
                        return where + ": raw() expects a single JSON object like { ... }";
                    continue;
                }
                if (!validId(n.id))
                    return where + ": invalid id \"" + n.id + "\" (allowed: A-Z a-z 0-9 _)";
                if (!seen.insert(n.id).second)
                    return where + ": duplicate id \"" + n.id + "\" in the same container";

                auto here = where + "/" + n.id;
                if (n.kind == Kind::Spinner) {
                    if (n.range.min > n.range.max) return here + ": min > max";
                    if (!(n.range.step > 0)) return here + ": step must be > 0";
                }
                if (n.kind == Kind::Layout) {
                    if (n.children.empty()) return here + ": empty layout";
                    auto e = validateNodes(n.children, here);
                    if (!e.empty()) return e;
                }
            }
            return {};
        }

        void writeNodes(std::string& out, std::vector<Node> const& nodes, int ind);

        void writeNode(std::string& out, Node const& n, int ind) {
            std::string pad(ind * 2, ' ');
            if (n.kind == Kind::Raw) {
                out += pad + n.rawJson;
                return;
            }

            auto head = [&](char const* type) {
                out += pad + "{\n" + pad + "  \"type\": \"" + type + "\",\n" + pad + "  \"id\": \"" + esc(n.id) + "\"";
            };
            switch (n.kind) {
                case Kind::Button:   head("button"); break;
                case Kind::Checkbox: head("checkbox"); break;
                case Kind::Textbox:  head("textbox"); break;
                case Kind::Spinner:
                    head("spinner");
                    out += ",\n" + pad + "  \"decimal\": " + (n.decimal ? "true" : "false");
                    out += ",\n" + pad + "  \"min\": " + num(n.range.min);
                    out += ",\n" + pad + "  \"max\": " + num(n.range.max);
                    out += ",\n" + pad + "  \"step\": " + num(n.range.step);
                    break;
                case Kind::Layout:
                    head("layout");
                    out += ",\n" + pad + "  \"elements\": [\n";
                    writeNodes(out, n.children, ind + 2);
                    out += "\n" + pad + "  ]";
                    break;
                default: break;
            }
            out += "\n" + pad + "}";
        }

        void writeNodes(std::string& out, std::vector<Node> const& nodes, int ind) {
            for (size_t i = 0; i < nodes.size(); ++i) {
                if (i) out += ",\n";
                writeNode(out, nodes[i], ind);
            }
        }

        // short id ("SPEED") or relative path ("ROW/SPEED"); warns and returns null on a miss or a clash
        Node const* find(TabImpl const& I, std::string const& id, std::string& outPath) {
            std::vector<std::pair<Node const*, std::string>> hits;
            bool relative = id.find('/') != std::string::npos;
            walk(I.nodes, I.id, [&](Node const& n, std::string const& p) {
                if (n.kind == Kind::Raw || n.kind == Kind::Layout) return;
                if (relative ? p == I.id + "/" + id : n.id == id) hits.emplace_back(&n, p);
            });
            if (hits.empty()) {
                logWarn("widget \"" + id + "\" was not found in tab " + I.id);
                return nullptr;
            }
            if (hits.size() > 1) {
                logWarn("id \"" + id + "\" is ambiguous in " + I.id + ", use a path like LAYOUT/" + id);
                return nullptr;
            }
            outPath = hits[0].second;
            return hits[0].first;
        }

        double toDouble(Value const& v) {
            return std::visit([](auto const& x) -> double {
                if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::string>) return std::strtod(x.c_str(), nullptr);
                else return static_cast<double>(x);
            }, v);
        }

        bool toBool(Value const& v) {
            return std::visit([](auto const& x) -> bool {
                using X = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<X, std::string>) return !x.empty() && x != "0" && x != "false";
                else return x != X{};
            }, v);
        }

        std::string toStr(Value const& v) {
            return std::visit([](auto const& x) -> std::string {
                using X = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<X, std::string>) return x;
                else if constexpr (std::is_same_v<X, bool>) return x ? "true" : "false";
                else return std::to_string(x);
            }, v);
        }

        double clampTo(Range const& r, double v) { return std::fmin(std::fmax(v, r.min), r.max); }

        // writes the default (or the value saved last launch) into the MH config
        void applyInitial(TabImpl const& I, Node const& n, std::string const& path) {
            auto st = I.persist ? g_storage : nullptr;
            std::string key = "mhext/" + path;
            switch (n.kind) {
                case Kind::Checkbox: {
                    bool v = n.defBool;
                    if (st) if (auto s = st->loadBool(key)) v = *s;
                    extensions::setBool(path, v);
                    break;
                }
                case Kind::Spinner: {
                    double v = n.defNumber;
                    if (st) if (auto s = st->loadNumber(key)) v = *s;
                    v = clampTo(n.range, v);
                    if (n.decimal) extensions::setDouble(path, v);
                    else extensions::setInt(path, static_cast<int64_t>(std::llround(v)));
                    break;
                }
                case Kind::Textbox: {
                    std::string v = n.defString;
                    if (st) if (auto s = st->loadString(key)) v = *s;
                    extensions::setString(path, v);
                    break;
                }
                default: break;
            }
        }

        // has to run after registerTab: MH only creates the elements and their tags there
        void attach(std::shared_ptr<TabImpl> const& keep, Node const& n, std::string const& path) {
            std::string key = "mhext/" + path;
            bool wantSave = keep->persist;
            switch (n.kind) {
                case Kind::Button:
                    if (n.onClick)
                        extensions::registerActionListener(path, [cb = n.onClick](Tag) { guarded("button", cb); });
                    break;
                case Kind::Checkbox:
                    if (wantSave || n.onBool)
                        extensions::registerBoolListener(path, [keep, key, cb = n.onBool](Tag, bool const& v) {
                            if (keep->persist && g_storage) g_storage->saveBool(key, v);
                            if (cb) guarded("checkbox", [&] { cb(v); });
                        });
                    break;
                case Kind::Spinner:
                    if (!wantSave && !n.onNumber) break;
                    if (n.decimal)
                        extensions::registerDoubleListener(path, [keep, key, cb = n.onNumber](Tag, double const& v) {
                            if (keep->persist && g_storage) g_storage->saveNumber(key, v);
                            if (cb) guarded("spinner", [&] { cb(v); });
                        });
                    else
                        extensions::registerIntListener(path, [keep, key, cb = n.onNumber](Tag, int64_t const& v) {
                            if (keep->persist && g_storage) g_storage->saveNumber(key, static_cast<double>(v));
                            if (cb) guarded("integer", [&] { cb(static_cast<double>(v)); });
                        });
                    break;
                case Kind::Textbox:
                    if (wantSave || n.onString)
                        extensions::registerStringListener(path, [keep, key, cb = n.onString](Tag, std::string const& v) {
                            if (keep->persist && g_storage) g_storage->saveString(key, v);
                            if (cb) guarded("textbox", [&] { cb(v); });
                        });
                    break;
                default: break;
            }
        }
    }

    Tab::Tab(std::string id, std::string title) : m_impl(std::make_shared<detail::TabImpl>()) {
        m_impl->id = std::move(id);
        m_impl->title = std::move(title);
    }

    std::vector<detail::Node>& Tab::nodes() {
        if (m_impl->registered) logWarn("tab " + m_impl->id + " is already registered, new widgets won't show up");
        return m_impl->nodes;
    }

    Tab& Tab::onOpen(std::function<void()> cb) { m_impl->cbOpen = std::move(cb); return *this; }
    Tab& Tab::onCommit(std::function<void()> cb) { m_impl->cbCommit = std::move(cb); return *this; }
    Tab& Tab::onUnload(std::function<void()> cb) { m_impl->cbUnload = std::move(cb); return *this; }
    Tab& Tab::persist(bool enabled) { m_impl->persist = enabled; return *this; }

    bool Tab::isRegistered() const { return m_impl->registered; }
    std::string const& Tab::lastError() const { return m_impl->lastError; }
    std::string const& Tab::id() const { return m_impl->id; }

    std::string Tab::toJson() const {
        auto const& I = *m_impl;
        std::string out = "{\n  \"type\": \"tab\",\n  \"id\": \"" + esc(I.id) + "\",\n  \"elements\": [\n";
        writeNodes(out, I.nodes, 2);
        out += "\n  ]\n}";
        return out;
    }

    bool Tab::registerTab() {
        auto& I = *m_impl;
        if (I.registered) return true;

        auto fail = [&](std::string msg) {
            I.lastError = std::move(msg);
            logErr(I.id + ": " + I.lastError);
            return false;
        };

        if (!validId(I.id)) return fail("invalid tab id (allowed: A-Z a-z 0-9 _)");
        if (I.nodes.empty()) return fail("the tab has no widgets");
        if (auto e = validateNodes(I.nodes, I.id); !e.empty()) return fail(e);
        if (!mh::init()) return fail("Mega Hack is unavailable (not installed or unsupported version)");

        extensions::addDefinition(I.id, I.title);
        walk(I.nodes, I.id, [&](Node const& n, std::string const& p) {
            if (n.kind == Kind::Raw) {
                for (auto& [k, v] : n.rawDefs) extensions::addDefinition(k, v);
            } else {
                extensions::addDefinition(p, n.label);
            }
        });

        // values go in before the tab itself, same order as in the original example
        extensions::setBool(I.id, true);
        walk(I.nodes, I.id, [&](Node const& n, std::string const& p) {
            if (n.kind != Kind::Raw) applyInitial(I, n, p);
        });

        I.delegate = std::make_unique<TabDelegate>(&I);
        if (!extensions::registerTab(toJson(), I.delegate.get())) {
            I.delegate.reset();
            return fail("Mega Hack rejected the tab (see tab.toJson())");
        }

        I.registered = true;
        I.lastError.clear();
        alive().push_back(m_impl);
        walk(I.nodes, I.id, [&](Node const& n, std::string const& p) {
            if (n.kind != Kind::Raw) attach(m_impl, n, p);
        });

        detail::log(detail::LogLevel::Info, "mh::Tab: tab " + I.id + " registered");
        return true;
    }

    std::string Tab::path(std::string const& id) const {
        std::string p;
        return find(*m_impl, id, p) ? p : std::string{};
    }

    std::optional<Value> Tab::value(std::string const& id) const {
        std::string p;
        auto const* n = find(*m_impl, id, p);
        if (!n) return std::nullopt;
        switch (n->kind) {
            case Kind::Checkbox: return Value{extensions::getBool(p)};
            case Kind::Spinner:  return n->decimal ? Value{extensions::getDouble(p)} : Value{extensions::getInt(p)};
            case Kind::Textbox:  return Value{extensions::getString(p)};
            default: return std::nullopt;
        }
    }

    bool Tab::setValue(std::string const& id, Value const& v) {
        std::string p;
        auto const* n = find(*m_impl, id, p);
        if (!n) return false;

        // MH listeners may not fire on a write from code, so persist here too
        auto st = m_impl->persist ? g_storage : nullptr;
        std::string key = "mhext/" + p;
        switch (n->kind) {
            case Kind::Checkbox: {
                bool b = toBool(v);
                extensions::setBool(p, b);
                if (st) st->saveBool(key, b);
                return true;
            }
            case Kind::Spinner: {
                double d = clampTo(n->range, toDouble(v));
                if (n->decimal) {
                    extensions::setDouble(p, d);
                } else {
                    d = std::llround(d);
                    extensions::setInt(p, static_cast<int64_t>(d));
                }
                if (st) st->saveNumber(key, d);
                return true;
            }
            case Kind::Textbox: {
                auto str = toStr(v);
                extensions::setString(p, str);
                if (st) st->saveString(key, str);
                return true;
            }
            default: return false;
        }
    }
}
