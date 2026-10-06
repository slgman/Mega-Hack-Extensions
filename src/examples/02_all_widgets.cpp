// Example 2. All builder widgets: checkbox, spinners (floating-point and integer), text, buttons, nested layouts.
#include <Geode/Geode.hpp>
#include "../mh/mh.hpp"

#include "examples.hpp"

using namespace geode::prelude;

namespace examples {
    // The handle can be stored globally to read values from anywhere in the mod.
    static mh::Tab g_tab("EXAMPLE_WIDGETS", "Widgets");

    void allWidgets() {
        g_tab
            .checkbox("ENABLED", "Enabled", true, [](bool on) {
                log::info("ENABLED -> {}", on);
            })
            .layout("SPEED_ROW", "Speed", [](mh::Layout& row) {
                row.spinner("SPEED", "Speed", 1.0, {.min = 0.1, .max = 10.0, .step = 0.1},
                            [](double v) { log::info("SPEED -> {}", v); })
                   .checkbox("LOCK", "Lock", false);
            })
            .integer("COUNT", "Count", 3, {.min = 0, .max = 20, .step = 1})
            .textbox("NAME", "Name", "player", [](std::string const& s) {
                log::info("NAME -> {}", s);
            })
            .button("PRINT", "Print values", [] {
                // get<T>() automatically selects the correct way to read the widget type.
                log::info("enabled={} speed={} lock={} count={} name={}",
                    g_tab.get<bool>("ENABLED"), g_tab.get<double>("SPEED"), g_tab.get<bool>("LOCK"),
                    g_tab.get<int>("COUNT"), g_tab.get<std::string>("NAME"));
            })
            .button("RESET", "Reset speed", [] {
                g_tab.set("SPEED", 1.0);   // writing from code (also persists between launches)
            });

        if (!g_tab.registerTab()) log::error("allWidgets: {}", g_tab.lastError());
    }
}
