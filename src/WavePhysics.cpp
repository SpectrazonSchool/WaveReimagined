#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include "getSetting.hpp"

#include <cmath>

using namespace geode::prelude;

namespace {
constexpr double kPi = 3.14159265358979323846;
}

class $modify(WaveAnglePlayer, PlayerObject) {
    struct Fields {
        double prevX = 0.0;
        double prevY = 0.0;
        bool havePrev = false;
        int frame = 0;
    };

    void update(float dt) {
        PlayerObject::update(dt);

        auto fields = m_fields.self();

        if (!this->m_isDart) {
            fields->havePrev = false;
            return;
        }

        bool fixOn = getSetting<bool, "fix-wave-angle">();

        double px = this->m_position.x;
        double py = this->m_position.y;
        double dx = px - fields->prevX;
        double dy = py - fields->prevY;

        if (fixOn && fields->havePrev && std::fabs(dx) > 0.001 && std::fabs(dy) > 0.001) {
            double target = std::fabs(this->m_yVelocity) * (std::fabs(dx) / std::fabs(dy));
            this->m_yVelocity = std::copysign(target, this->m_yVelocity);
        }

        if (fields->havePrev && std::fabs(dx) > 0.5 && ++fields->frame % 30 == 0) {
            double angle = std::atan2(std::fabs(dy), std::fabs(dx)) * 180.0 / kPi;
            log::info("angle={:.4f} dx={:.5f} dy={:.5f} fix={}", angle, dx, dy, fixOn ? "ON" : "off");
        }

        fields->prevX = px;
        fields->prevY = py;
        fields->havePrev = true;
    }
};
