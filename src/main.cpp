// Demo mod: shows how to integrate Mega Hack extensions. For your own mod, you only need src/mh/.
#include <Geode/Geode.hpp>
#include "mh/mh.hpp"

#include "examples/examples.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    // onReady invokes the lambda when the main menu opens and Mega Hack is found.
    mh::onReady([] {
        examples::basicTab();
    });
}
