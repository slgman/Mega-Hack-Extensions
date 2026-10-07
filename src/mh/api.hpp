#pragma once

// Thin wrapper over Mega Hack exports (hackpro::extensions). No Geode dependency.

#include <cstdint>
#include <functional>
#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace mh {

    enum class MessageType : int {
        Error = 0,   // OK button
        Info = 1,    // OK button
        Confirm = 2, // Cancel / OK
        Notice = 3,  // no buttons
    };

    // First argument of every listener.
    struct Tag {
        uint64_t value = 0;

        Tag() = default;
        explicit Tag(uint64_t v) : value(v) {}

        bool operator==(Tag const&) const = default;
        explicit operator bool() const { return value != 0; }
    };

    // The order of virtual methods is part of the ABI with Mega Hack:
    // slot 0 - destructor, 1 - onOpen, 2 - onCommit, 3 - onUnload. Don't reorder or add above them.
    struct Delegate {
        virtual ~Delegate() = default;
        virtual void onOpen() {}
        virtual void onCommit() {} // menu closed, values committed
        virtual void onUnload() {}
    };

    // false => Mega Hack is missing or too old, see missingSymbols()
    bool init();
    bool isReady();
    std::vector<std::string> const& missingSymbols();

    namespace extensions {
        // Config keys look like "TAB/LAYOUT/WIDGET"
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

        // built-in hacks, e.g. "HACK_NOCLIP"
        bool isHackEnabled(std::string const& id);
        void setHackEnabled(std::string const& id, bool enabled);

        void registerBoolListener(std::string const& key, std::function<void(Tag, bool const&)> cb);
        void registerIntListener(std::string const& key, std::function<void(Tag, int64_t const&)> cb);
        void registerDoubleListener(std::string const& key, std::function<void(Tag, double const&)> cb);
        void registerStringListener(std::string const& key, std::function<void(Tag, std::string const&)> cb);
        void registerBytesListener(std::string const& key, std::function<void(Tag, std::vector<uint32_t> const&)> cb);

        void registerHackListener(std::string const& id, std::function<void(Tag, bool const&)> cb);
        void registerActionListener(std::string const& id, std::function<void(Tag)> cb);

        // MH wants the stream as a non-const lvalue reference
        bool registerTab(std::istream& stream, Delegate* delegate = nullptr);
        bool registerTab(std::string const& json, Delegate* delegate = nullptr);
        void addDefinition(std::string const& key, std::string const& value);

        void showError(std::string const& message);
        void showMessage(MessageType type, std::string const& message, std::function<void()> onClose = {});

        std::optional<Tag> tagFor(std::string const& key);
        std::string keyFor(Tag tag);
        std::string define(std::string const& key);
    }

    namespace safe_mode {
        bool areCheatsEnabled();
        bool hasCheatedInAttempt();
        void disableAllCheats();
    }

    namespace detail {
        enum class LogLevel { Info, Warn, Error };
        void log(LogLevel level, std::string const& message); // defined in api.cpp via geode::log
    }
}
