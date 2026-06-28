#include <Geode/Geode.hpp>
#include <Geode/modify/GameLevelOptionsLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "levelState.hpp"

using namespace geode::prelude;

namespace {
constexpr int kDisableTag = 0x77115E;
}

class $modify(WRGameLevelOptionsLayer, GameLevelOptionsLayer) {
    void setupOptions() {
        GameLevelOptionsLayer::setupOptions();
        this->addToggle(
            "Disable Wave Reimagined",
            kDisableTag,
            wr::isDisabledForLevel(m_level),
            "Falls back to the vanilla wave trail for <cy>this level only</c>, leaving Wave Reimagined enabled everywhere else.");
    }

    void didToggle(int tag) {
        if (tag == kDisableTag) {
            bool value = !wr::isDisabledForLevel(m_level);
            wr::setDisabledForLevel(m_level, value);
            wr::currentLevelDisabled() = value;
            return;
        }
        GameLevelOptionsLayer::didToggle(tag);
    }
};

class $modify(WRDisablePlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        wr::currentLevelDisabled() = wr::isDisabledForLevel(level);
        return true;
    }
};
