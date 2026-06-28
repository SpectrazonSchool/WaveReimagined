#pragma once

#include <Geode/Geode.hpp>

template <class S, geode::utils::string::ConstexprString key>
S getSetting() {
    static auto setting = geode::Mod::get()->getSettingValue<S>(key.data());
    static auto listener = geode::listenForSettingChanges<S>(key.data(), [](S value) {
        setting = std::move(value);
    });
    return setting;
}
