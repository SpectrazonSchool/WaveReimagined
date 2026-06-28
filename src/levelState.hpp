#pragma once

#include <Geode/Geode.hpp>

namespace wr {

inline std::string disableKey(GJGameLevel* level) {
    if (!level) return "";
    return fmt::format("disable-{}-{}-{}",
        static_cast<int>(level->m_levelType),
        level->m_levelID.value(),
        level->m_levelName);
}

inline bool isDisabledForLevel(GJGameLevel* level) {
    auto key = disableKey(level);
    if (key.empty()) return false;
    return geode::Mod::get()->getSavedValue<bool>(key, false);
}

inline void setDisabledForLevel(GJGameLevel* level, bool value) {
    auto key = disableKey(level);
    if (key.empty()) return;
    geode::Mod::get()->setSavedValue<bool>(key, value);
}

inline bool& currentLevelDisabled() {
    static bool value = false;
    return value;
}

}
