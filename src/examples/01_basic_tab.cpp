// Example 1. Minimal tab: button + checkbox + spinner in a layout.
// This is equivalent to what the original main.cpp did (~70 lines of JSON, definitions, and listeners).
#include <Geode/Geode.hpp>
#include "../mh/mh.hpp"

#include "examples.hpp"

using namespace geode::prelude;

namespace examples {
    void basicTab() {
        // The object can be stored anywhere: after registerTab(), its state lives until the end of the game.
        mh::Tab tab("MY_CUSTOM_TAB", "My Tab");

        tab.button("MY_TEST_BUTTON", "Test Button", [] {
                log::info(">>> MY_TEST_BUTTON WAS CLICKED! <<<");
            })
            .checkbox("MY_TEST_CHECKBOX", "Test Checkbox", false)
            .layout("MY_SPINNER_LAYOUT", "Spinner Setting", [](mh::Layout& row) {
                row.spinner("MY_TEST_SPINNER", "Test Spinner", 10.0, {.min = 0, .max = 100, .step = 1});
            })
            // Capture tab BY VALUE: it is a lightweight handle. A reference [&tab] would dangle after basicTab() returns.
            .onCommit([tab] {
                // Values are read by the short id — the full path is built automatically.
                log::info("Mega Hack closed. Checkbox: {}, Spinner: {}",
                    tab.get<bool>("MY_TEST_CHECKBOX"), tab.get<double>("MY_TEST_SPINNER"));
            });

        if (!tab.registerTab()) log::error("basicTab: {}", tab.lastError());
    }
}
