#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

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

        if (!this->m_isDart) {
            m_fields->havePrev = false;
            return;
        }

        bool fixOn = Mod::get()->getSettingValue<bool>("fix-wave-angle");

        double px = this->m_position.x;
        double py = this->m_position.y;
        double dx = px - m_fields->prevX;
        double dy = py - m_fields->prevY;

        if (fixOn && m_fields->havePrev && std::fabs(dx) > 0.001 && std::fabs(dy) > 0.001) {
            double target = std::fabs(this->m_yVelocity) * (std::fabs(dx) / std::fabs(dy));
            this->m_yVelocity = std::copysign(target, this->m_yVelocity);
        }

        if (m_fields->havePrev && std::fabs(dx) > 0.5 && ++m_fields->frame % 30 == 0) {
            double angle = std::atan2(std::fabs(dy), std::fabs(dx)) * 180.0 / kPi;
            log::info("angle={:.4f} dx={:.5f} dy={:.5f} fix={}", angle, dx, dy, fixOn ? "ON" : "off");
        }

        m_fields->prevX = px;
        m_fields->prevY = py;
        m_fields->havePrev = true;
    }
};
