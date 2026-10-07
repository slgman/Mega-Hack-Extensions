// Geode glue: saved values as storage, and onReady(). Not part of the Geode-free tests.
#include "ui.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

using namespace geode::prelude;

namespace mh {
    namespace {
        class GeodeStorage final : public Storage {
            template <class T>
            static std::optional<T> load(std::string const& key) {
                auto* mod = Mod::get();
                if (!mod->hasSavedValue(key)) return std::nullopt;
                return mod->getSavedValue<T>(key);
            }

        public:
            std::optional<bool> loadBool(std::string const& key) override { return load<bool>(key); }
            std::optional<double> loadNumber(std::string const& key) override { return load<double>(key); }
            std::optional<std::string> loadString(std::string const& key) override { return load<std::string>(key); }

            void saveBool(std::string const& key, bool v) override { Mod::get()->setSavedValue(key, v); }
            void saveNumber(std::string const& key, double v) override { Mod::get()->setSavedValue(key, v); }
            void saveString(std::string const& key, std::string const& v) override { Mod::get()->setSavedValue(key, v); }
        };

        struct Pending {
            std::function<void()> fn;
            int delayFrames;
        };

        std::vector<Pending>& pending() {
            static std::vector<Pending> v;
            return v;
        }
        bool g_menuShown = false;

        // queueInMainThread runs on the next frame, so every nested call is one frame of delay
        void runLater(std::function<void()> fn, int frames) {
            queueInMainThread([fn = std::move(fn), frames]() mutable {
                if (frames > 0) {
                    runLater(std::move(fn), frames - 1);
                } else if (mh::init()) {
                    fn();
                } else {
                    log::info("mh: Mega Hack not found, extensions skipped");
                }
            });
        }
    }

    void onReady(std::function<void()> fn, int delayFrames) {
        if (g_menuShown) runLater(std::move(fn), delayFrames);
        else pending().push_back({std::move(fn), delayFrames});
    }
}

$on_mod(Loaded) {
    if (!mh::storage()) mh::setStorage(std::make_shared<mh::GeodeStorage>());
}

class $modify(MhReadyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        if (!mh::g_menuShown) {
            mh::g_menuShown = true;
            auto list = std::move(mh::pending());
            mh::pending().clear();
            for (auto& p : list) mh::runLater(std::move(p.fn), p.delayFrames);
        }
        return true;
    }
};
