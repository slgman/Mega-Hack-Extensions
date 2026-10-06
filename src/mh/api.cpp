// Low-level wrapper around Mega Hack exports. Windows-only.
#include "api.hpp"

#include <atomic>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <Geode/Geode.hpp>

namespace mh {
    namespace detail {
        std::atomic<bool> g_ready{false};
        HMODULE g_module = nullptr;
        std::vector<std::string> g_missing;

        using Fn_getBool    = bool(*)(std::string const&);
        using Fn_getInt     = int64_t(*)(std::string const&);
        using Fn_getDouble  = double(*)(std::string const&);
        using Fn_getString  = std::string(*)(std::string const&);
        using Fn_getBytes   = std::vector<uint32_t>(*)(std::string const&);

        using Fn_setBool    = void(*)(std::string const&, bool const&);
        using Fn_setInt     = void(*)(std::string const&, int64_t const&);
        using Fn_setDouble  = void(*)(std::string const&, double const&);
        using Fn_setString  = void(*)(std::string const&, std::string const&);
        using Fn_setBytes   = void(*)(std::string const&, std::vector<uint32_t> const&);

        using Fn_isHack     = bool(*)(std::string const&);
        using Fn_setHack    = void(*)(std::string const&, bool);

        using Fn_regBoolL   = void(*)(std::string const&, std::function<void(Tag, bool const&)>);
        using Fn_regIntL    = void(*)(std::string const&, std::function<void(Tag, int64_t const&)>);
        using Fn_regDoubleL = void(*)(std::string const&, std::function<void(Tag, double const&)>);
        using Fn_regStringL = void(*)(std::string const&, std::function<void(Tag, std::string const&)>);
        using Fn_regBytesL  = void(*)(std::string const&, std::function<void(Tag, std::vector<uint32_t> const&)>);

        using Fn_regHackL   = void(*)(std::string const&, std::function<void(Tag, bool const&)>);
        using Fn_regActionL = void(*)(std::string const&, std::function<void(Tag)>);

        using Fn_registerTab = bool(*)(std::istream&, Delegate*);
        using Fn_addDef      = void(*)(std::string const&, std::string const&);

        using Fn_showError   = void(*)(std::string const&);
        using Fn_showMessage = void(*)(MessageType, std::string const&, std::function<void()>);

        using Fn_tagFor      = std::optional<Tag>(*)(std::string const&);
        using Fn_keyFor      = std::string(*)(Tag);
        using Fn_define      = std::string const&(*)(std::string const&);

        using Fn_areCheats   = bool(*)();
        using Fn_hasCheated  = bool(*)();
        using Fn_disableAll  = void(*)();


        Fn_getBool      p_getBool = nullptr;
        Fn_getInt       p_getInt = nullptr;
        Fn_getDouble    p_getDouble = nullptr;
        Fn_getString    p_getString = nullptr;
        Fn_getBytes     p_getBytes = nullptr;
        Fn_setBool      p_setBool = nullptr;
        Fn_setInt       p_setInt = nullptr;
        Fn_setDouble    p_setDouble = nullptr;
        Fn_setString    p_setString = nullptr;
        Fn_setBytes     p_setBytes = nullptr;
        Fn_isHack       p_isHack = nullptr;
        Fn_setHack      p_setHack = nullptr;
        Fn_regBoolL     p_regBoolL = nullptr;
        Fn_regIntL      p_regIntL = nullptr;
        Fn_regDoubleL   p_regDoubleL = nullptr;
        Fn_regStringL   p_regStringL = nullptr;
        Fn_regBytesL    p_regBytesL = nullptr;
        Fn_regHackL     p_regHackL = nullptr;
        Fn_regActionL   p_regActionL = nullptr;
        Fn_registerTab  p_registerTab = nullptr;
        Fn_addDef       p_addDef = nullptr;
        Fn_showError    p_showError = nullptr;
        Fn_showMessage  p_showMessage = nullptr;
        Fn_tagFor       p_tagFor = nullptr;
        Fn_keyFor       p_keyFor = nullptr;
        Fn_define       p_define = nullptr;
        Fn_areCheats    p_areCheats = nullptr;
        Fn_hasCheated   p_hasCheated = nullptr;
        Fn_disableAll   p_disableAll = nullptr;

        template <typename T>
        T resolve(char const* mangled) {
            return reinterpret_cast<T>(GetProcAddress(g_module, mangled));
        }

        // Required symbol: if it is not found, init() returns false and reports which one is missing.
        #define REQ(var, label, sym) do { var = resolve<decltype(var)>(sym); if (!var) g_missing.emplace_back(label); } while (0)
        // Optional symbol: the wrapper function simply becomes a no-op.
        #define OPT(var, label, sym) do { var = resolve<decltype(var)>(sym); } while (0)

        void resolveAll() {
            g_missing.clear();
            REQ(p_getBool, "getBool", R"(??$getConfigVar@_N@extensions@hackpro@@YA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z)");
            REQ(p_getInt, "getInt", R"(??$getConfigVar@_J@extensions@hackpro@@YA_JAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z)");
            REQ(p_getDouble, "getDouble", R"(??$getConfigVar@N@extensions@hackpro@@YANAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z)");
            REQ(p_getString, "getString", R"(??$getConfigVar@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@extensions@hackpro@@YA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z)");
            REQ(p_getBytes, "getBytes", R"(??$getConfigVar@V?$vector@IV?$allocator@I@std@@@std@@@extensions@hackpro@@YA?AV?$vector@IV?$allocator@I@std@@@std@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@3@@Z)");
            REQ(p_setBool, "setBool", R"(??$setConfigVar@_N@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEB_N@Z)");
            REQ(p_setInt, "setInt", R"(??$setConfigVar@_J@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEB_J@Z)");
            REQ(p_setDouble, "setDouble", R"(??$setConfigVar@N@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBN@Z)");
            REQ(p_setString, "setString", R"(??$setConfigVar@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z)");
            REQ(p_setBytes, "setBytes", R"(??$setConfigVar@V?$vector@IV?$allocator@I@std@@@std@@@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV?$vector@IV?$allocator@I@std@@@3@@Z)");
            REQ(p_isHack, "isHack", R"(?isHackEnabled@extensions@hackpro@@YA_NAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z)");
            REQ(p_setHack, "setHack", R"(?setHackEnabled@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@_N@Z)");
            REQ(p_regBoolL, "regBoolL", R"(??$registerListener@_N@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$function@$$A6AXVTag@hackpro@@AEB_N@Z@3@@Z)");
            REQ(p_regIntL, "regIntL", R"(??$registerListener@_J@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$function@$$A6AXVTag@hackpro@@AEB_J@Z@3@@Z)");
            REQ(p_regDoubleL, "regDoubleL", R"(??$registerListener@N@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$function@$$A6AXVTag@hackpro@@AEBN@Z@3@@Z)");
            REQ(p_regStringL, "regStringL", R"(??$registerListener@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$function@$$A6AXVTag@hackpro@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z@3@@Z)");
            REQ(p_regBytesL, "regBytesL", R"(??$registerListener@V?$vector@IV?$allocator@I@std@@@std@@@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$function@$$A6AXVTag@hackpro@@AEBV?$vector@IV?$allocator@I@std@@@std@@@Z@3@@Z)");
            REQ(p_regHackL, "regHackL", R"(?registerHackListener@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$function@$$A6AXVTag@hackpro@@AEB_N@Z@4@@Z)");
            REQ(p_regActionL, "regActionL", R"(?registerActionListener@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$function@$$A6AXVTag@hackpro@@@Z@4@@Z)");
            REQ(p_registerTab, "registerTab", R"(?registerTab@extensions@hackpro@@YA_NAEAV?$basic_istream@DU?$char_traits@D@std@@@std@@PEAVDelegate@12@@Z)");
            REQ(p_addDef, "addDef", R"(?addDefinition@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@0@Z)");
            REQ(p_showError, "showError", R"(?showError@extensions@hackpro@@YAXAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z)");
            REQ(p_showMessage, "showMessage", R"(?showMessage@extensions@hackpro@@YAXW4MessageType@2@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$function@$$A6AXXZ@5@@Z)");
            REQ(p_tagFor, "tagFor", R"(?tagFor@extensions@hackpro@@YA?AV?$optional@VTag@hackpro@@@std@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@4@@Z)");
            REQ(p_keyFor, "keyFor", R"(?keyFor@extensions@hackpro@@YA?AV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@VTag@2@@Z)");
            REQ(p_define, "define", R"(?define@hackpro@@YAAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEBV23@@Z)");
            OPT(p_areCheats, "areCheats", R"(?areCheatsEnabled@safe_mode@utils@mega@@YA_NXZ)");
            OPT(p_hasCheated, "hasCheated", R"(?hasCheatedInAttempt@safe_mode@utils@mega@@YA_NXZ)");
            OPT(p_disableAll, "disableAll", R"(?disableAllCheats@safe_mode@utils@mega@@YAXXZ)");
        }
        #undef REQ
        #undef OPT

        void log(LogLevel level, std::string const& message) {
            switch (level) {
                case LogLevel::Info:  geode::log::info("{}", message); break;
                case LogLevel::Warn:  geode::log::warn("{}", message); break;
                case LogLevel::Error: geode::log::error("{}", message); break;
            }
        }
    } // namespace detail

    bool init() {
        if (detail::g_ready) return true;

        auto* mod = geode::Loader::get()->getLoadedMod("absolllute.megahack");
        if (!mod) return false; // Mega Hack is not installed / not loaded yet — this is not an error

        // First use the filename from the Geode mod itself, and only then the hardcoded name.
        detail::g_module = GetModuleHandleW(mod->getBinaryPath().filename().wstring().c_str());
        if (!detail::g_module) detail::g_module = GetModuleHandleW(L"absolllute.megahack.dll");
        if (!detail::g_module) {
            detail::log(detail::LogLevel::Error, "mh: failed to get the Mega Hack HMODULE");
            return false;
        }

        detail::resolveAll();
        if (!detail::g_missing.empty()) {
            std::string list;
            for (auto& s : detail::g_missing) list += (list.empty() ? "" : ", ") + s;
            detail::log(detail::LogLevel::Error,
                "mh: this version of Mega Hack is missing required exports: " + list);
            return false;
        }

        detail::g_ready = true;
        detail::log(detail::LogLevel::Info, "mh: Mega Hack extensions API is ready");
        return true;
    }

    bool isReady() { return detail::g_ready; }
    std::vector<std::string> const& missingSymbols() { return detail::g_missing; }

    namespace extensions {
        bool getBool(std::string const& key) { return detail::p_getBool ? detail::p_getBool(key) : false; }
        int64_t getInt(std::string const& key) { return detail::p_getInt ? detail::p_getInt(key) : 0; }
        double getDouble(std::string const& key) { return detail::p_getDouble ? detail::p_getDouble(key) : 0.0; }
        std::string getString(std::string const& key) { return detail::p_getString ? detail::p_getString(key) : std::string{}; }
        std::vector<uint32_t> getBytes(std::string const& key) { return detail::p_getBytes ? detail::p_getBytes(key) : std::vector<uint32_t>{}; }

        void setBool(std::string const& key, bool value) { if (detail::p_setBool) detail::p_setBool(key, value); }
        void setInt(std::string const& key, int64_t value) { if (detail::p_setInt) detail::p_setInt(key, value); }
        void setDouble(std::string const& key, double value) { if (detail::p_setDouble) detail::p_setDouble(key, value); }
        void setString(std::string const& key, std::string const& value) { if (detail::p_setString) detail::p_setString(key, value); }
        void setBytes(std::string const& key, std::vector<uint32_t> const& value) { if (detail::p_setBytes) detail::p_setBytes(key, value); }

        bool isHackEnabled(std::string const& id) { return detail::p_isHack ? detail::p_isHack(id) : false; }
        void setHackEnabled(std::string const& id, bool enabled) { if (detail::p_setHack) detail::p_setHack(id, enabled); }

        void registerBoolListener(std::string const& key, std::function<void(Tag, bool const&)> cb) { if (detail::p_regBoolL) detail::p_regBoolL(key, std::move(cb)); }
        void registerIntListener(std::string const& key, std::function<void(Tag, int64_t const&)> cb) { if (detail::p_regIntL) detail::p_regIntL(key, std::move(cb)); }
        void registerDoubleListener(std::string const& key, std::function<void(Tag, double const&)> cb) { if (detail::p_regDoubleL) detail::p_regDoubleL(key, std::move(cb)); }
        void registerStringListener(std::string const& key, std::function<void(Tag, std::string const&)> cb) { if (detail::p_regStringL) detail::p_regStringL(key, std::move(cb)); }
        void registerBytesListener(std::string const& key, std::function<void(Tag, std::vector<uint32_t> const&)> cb) { if (detail::p_regBytesL) detail::p_regBytesL(key, std::move(cb)); }

        void registerHackListener(std::string const& id, std::function<void(Tag, bool const&)> cb) { if (detail::p_regHackL) detail::p_regHackL(id, std::move(cb)); }
        void registerActionListener(std::string const& id, std::function<void(Tag)> cb) { if (detail::p_regActionL) detail::p_regActionL(id, std::move(cb)); }

        bool registerTab(std::istream& stream, Delegate* delegate) {
            return detail::p_registerTab ? detail::p_registerTab(stream, delegate) : false;
        }
        bool registerTab(std::string const& json, Delegate* delegate) {
            std::istringstream ss(json);
            return registerTab(ss, delegate);
        }
        void addDefinition(std::string const& key, std::string const& value) { if (detail::p_addDef) detail::p_addDef(key, value); }

        void showError(std::string const& message) { if (detail::p_showError) detail::p_showError(message); }
        void showMessage(MessageType type, std::string const& message, std::function<void()> onClose) {
            if (!detail::p_showMessage) return;
            // An empty std::function passed to MH would throw bad_function_call when invoked inside external code.
            if (!onClose) onClose = [] {};
            detail::p_showMessage(type, message, std::move(onClose));
        }

        std::optional<Tag> tagFor(std::string const& key) { return detail::p_tagFor ? detail::p_tagFor(key) : std::nullopt; }
        std::string keyFor(Tag tag) { return detail::p_keyFor ? detail::p_keyFor(tag) : std::string{}; }
        std::string define(std::string const& key) { return detail::p_define ? detail::p_define(key) : std::string{}; }
    }

    namespace safe_mode {
        bool areCheatsEnabled() { return detail::p_areCheats ? detail::p_areCheats() : false; }
        bool hasCheatedInAttempt() { return detail::p_hasCheated ? detail::p_hasCheated() : false; }
        void disableAllCheats() { if (detail::p_disableAll) detail::p_disableAll(); }
    }
}
