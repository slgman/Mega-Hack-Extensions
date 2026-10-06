#pragma once

// Low-level wrapper around Mega Hack exports (namespace hackpro::extensions).
// There is NO dependency on Geode here — only STL, so the header can be tested separately.

#include <cstdint>
#include <functional>
#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace mh {

    enum class MessageType : int {
        Error = 0, // Error window (with an OK button)
        Info = 1, // Standard informational message (OK)
        Confirm = 2, // Action selection window (Cancel / OK)
        Notice = 3, // Static window/panel without close buttons
    };

    // UI element tag. Passed as the first argument to all listeners.
    struct Tag {
        uint64_t value = 0;

        Tag() = default;
        explicit Tag(uint64_t v) : value(v) {}

        bool operator==(Tag const& o) const { return value == o.value; }
        bool operator!=(Tag const& o) const { return value != o.value; }
        explicit operator bool() const { return value != 0; }
    };

    // IMPORTANT: the order of virtual methods is part of the ABI with Mega Hack.
    //   slot 0 — destructor, slot 1 — onOpen, slot 2 — onCommit, slot 3 — onUnload.
    // Do not change the order or add virtual methods above these.
    struct Delegate {
        virtual ~Delegate() = default;
        virtual void onOpen() {}    // MH menu opened
        virtual void onCommit() {}  // menu closed, values committed
        virtual void onUnload() {}
    };

    // ---- initialization -----------------------------------------------------

    // Finds Mega Hack and resolves its exports. Idempotent. false => MH is missing / version is incompatible.
    bool init();
    bool isReady();
    // Which symbols could not be found (empty if init() succeeded).
    std::vector<std::string> const& missingSymbols();

    namespace extensions {
        // ---- values (config vars). Key — path in the form "TAB/LAYOUT/WIDGET" ---
        bool getBool(std::string const& key);
        int64_t getInt(std::string const& key);
        double getDouble(std::string const& key);
        std::string getString(std::string const& key);
        std::vector<uint32_t> getBytes(std::string const& key);

        void setBool(std::string const& key, bool value);
        void setInt(std::string const& key, int64_t value);
        void setDouble(std::string const& key, double value);
        void setString(std::string const& key, std::string const& value);
        void setBytes(std::string const& key, std::vector<uint32_t> const& value);

        // ---- built-in MH hacks ("HACK_NOCLIP", etc.) -----------------------
        bool isHackEnabled(std::string const& id);
        void setHackEnabled(std::string const& id, bool enabled);

        // ---- listeners --------------------------------------------------------
        void registerBoolListener(std::string const& key, std::function<void(Tag, bool const&)> cb);
        void registerIntListener(std::string const& key, std::function<void(Tag, int64_t const&)> cb);
        void registerDoubleListener(std::string const& key, std::function<void(Tag, double const&)> cb);
        void registerStringListener(std::string const& key, std::function<void(Tag, std::string const&)> cb);
        void registerBytesListener(std::string const& key, std::function<void(Tag, std::vector<uint32_t> const&)> cb);

        void registerHackListener(std::string const& id, std::function<void(Tag, bool const&)> cb);
        void registerActionListener(std::string const& id, std::function<void(Tag)> cb);

        // ---- tabs -----------------------------------------------------------
        // The stream is passed by lvalue reference (as required by MH). There is an overload for strings below.
        bool registerTab(std::istream& stream, Delegate* delegate = nullptr);
        bool registerTab(std::string const& json, Delegate* delegate = nullptr);
        void addDefinition(std::string const& key, std::string const& value);

        // ---- messages ----------------------------------------------------------
        void showError(std::string const& message);
        // onClose can be omitted — MH receives a safe no-op instead of an empty std::function.
        void showMessage(MessageType type, std::string const& message, std::function<void()> onClose = {});

        // ---- tags / localization -------------------------------------------------
        std::optional<Tag> tagFor(std::string const& key);
        std::string keyFor(Tag tag);
        std::string define(std::string const& key);
    }

    namespace safe_mode {
        bool areCheatsEnabled();
        bool hasCheatedInAttempt();
        void disableAllCheats();
    }

    // ---- logging (implemented in glue.cpp via geode::log) ---------------
    namespace detail {
        enum class LogLevel { Info, Warn, Error };
        void log(LogLevel level, std::string const& message);
    }
}
