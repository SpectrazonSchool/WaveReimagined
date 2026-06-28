#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <ninxout.options_api/include/API.hpp>

#include "levelState.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    OptionsAPI::addPreLevelSetting<bool>(
        "Disable Wave Reimagined",
        "disable-wave-reimagined"_spr,
        [](GJGameLevel* level) {
            bool value = !wr::isDisabledForLevel(level);
            wr::setDisabledForLevel(level, value);
            wr::currentLevelDisabled() = value;
        },
        [](GJGameLevel* level) { return wr::isDisabledForLevel(level); },
        "Falls back to the vanilla wave trail for <cy>this level only</c>, leaving Wave Reimagined enabled everywhere else.");
}

class $modify(WRDisablePlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        wr::currentLevelDisabled() = wr::isDisabledForLevel(level);
        return true;
    }
};
