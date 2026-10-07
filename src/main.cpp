// Demo mod. For your own mod you only need src/mh/.
#include <Geode/Geode.hpp>
#include "mh/mh.hpp"

#include "examples/examples.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    mh::onReady([] {
        examples::basicTab();
        examples::allWidgets();
    });
}
