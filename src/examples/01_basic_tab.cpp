// Minimal tab: button, checkbox and a spinner inside a layout.
#include <Geode/Geode.hpp>
#include "../mh/mh.hpp"

#include "examples.hpp"

using namespace geode::prelude;

namespace examples {
    void basicTab() {
        mh::Tab tab("MY_CUSTOM_TAB", "My Tab");

        tab.button("MY_TEST_BUTTON", "Test Button", [] {
                log::info("MY_TEST_BUTTON clicked");
            })
            .checkbox("MY_TEST_CHECKBOX", "Test Checkbox", false)
            .layout("MY_SPINNER_LAYOUT", "Spinner Setting", [](mh::Layout& row) {
                row.spinner("MY_TEST_SPINNER", "Test Spinner", 10.0, {.min = 0, .max = 100, .step = 1});
            })
            // by value: [&tab] would dangle once basicTab() returns
            .onCommit([tab] {
                log::info("Mega Hack closed. Checkbox: {}, Spinner: {}",
                    tab.get<bool>("MY_TEST_CHECKBOX"), tab.get<double>("MY_TEST_SPINNER"));
            });

        if (!tab.registerTab()) log::error("basicTab: {}", tab.lastError());
    }
}
