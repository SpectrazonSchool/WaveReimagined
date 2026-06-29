#include <Geode/Geode.hpp>
#include <Geode/modify/HardStreak.hpp>

#include "getSetting.hpp"
#include "levelState.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

using namespace geode::prelude;

#ifndef GL_MAX
    #ifdef GL_MAX_EXT
        #define GL_MAX GL_MAX_EXT
    #endif
#endif

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float kWeldTol = 0.1f;

struct EdgeInfo {
    int count = 0;
    int k1 = 0, k2 = 0;
    CCPoint p1, p2, outward;
};

struct VAcc {
    CCPoint pos;
    CCPoint n[2];
    CCPoint neighbor[2];
    int cnt = 0;
};

struct VJoin {
    bool valid = false;
    bool convex = false;
    CCPoint miter;
};

ccV2F_C4B_T2F makeVertex(CCPoint const& pos, ccColor4B color) {
    ccV2F_C4B_T2F v;
    v.vertices = {pos.x, pos.y};
    v.colors = color;
    v.texCoords = {0.f, 0.f};
    return v;
}

void uploadBuffer(CCDrawNode* node, std::vector<ccV2F_C4B_T2F> const& verts) {
    auto needed = static_cast<unsigned int>(verts.size());
    if (node->m_uBufferCapacity < needed) {
        node->m_uBufferCapacity = needed + 64;
        node->m_pBuffer = static_cast<ccV2F_C4B_T2F*>(
            std::realloc(node->m_pBuffer, node->m_uBufferCapacity * sizeof(ccV2F_C4B_T2F)));
    }
    if (needed > 0) {
        std::memcpy(node->m_pBuffer, verts.data(), needed * sizeof(ccV2F_C4B_T2F));
    }
    node->m_nBufferCount = needed;
    node->m_bDirty = true;
}

class GlowNode : public CCDrawNode {
public:
    bool m_useMax = false;
    CCDrawNode* m_streak = nullptr;

    static GlowNode* create() {
        auto ret = new GlowNode();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void draw() override {
        if (m_streak && m_streak->m_nBufferCount < 3) return;
        if (m_useMax) {
            glBlendEquation(GL_MAX);
            CCDrawNode::draw();
            glBlendEquation(GL_FUNC_ADD);
        } else {
            CCDrawNode::draw();
        }
    }
};

} // namespace

class $modify(LightsaberStreak, HardStreak) {
    struct Fields {
        GlowNode* glow = nullptr;
        float musicAmp = 0.f;
        std::vector<CCPoint> uniq;
        std::unordered_map<int64_t, std::vector<int>> weldGrid;
        std::unordered_map<int64_t, EdgeInfo> edges;
        std::vector<VAcc> vmap;
        std::vector<VJoin> joins;
        std::vector<ccV2F_C4B_T2F> verts;
    };

    void updateStroke(float dt) {
        auto fields = m_fields.self();

        static bool meteringEnabled = false;
        if (!meteringEnabled) {
            if (auto* fae = FMODAudioEngine::sharedEngine()) {
                fae->enableMetering();
                meteringEnabled = true;
            }
        }

        float meter = 0.f;
        if (auto* fae = FMODAudioEngine::sharedEngine()) {
            meter = fae->getMeteringValue();
        }
        float level = std::clamp((meter - 0.2f) / 0.6f, 0.f, 1.f);
        fields->musicAmp = level;

        float baseMult = getSetting<float, "base-size">();
        float pulseMult = getSetting<float, "pulse-multiplier">();
        float base = m_waveSize * baseMult;
        m_pulseSize = base * (1.f + pulseMult * level);

        HardStreak::updateStroke(dt);

        if (!getSetting<bool, "enabled">() || wr::currentLevelDisabled()) {
            this->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
            if (fields->glow) {
                fields->glow->m_nBufferCount = 0;
                fields->glow->m_bDirty = true;
            }
            return;
        }

        this->applyLightsaber();
    }

    float waveScale() {
        if (auto* gl = GJBaseGameLayer::get()) {
            PlayerObject* player = gl->m_player1;
            if (gl->m_player2 && gl->m_player2->m_waveTrail == this) {
                player = gl->m_player2;
            }
            if (player && player->m_vehicleSize > 0.f) {
                return player->m_vehicleSize;
            }
        }
        return 1.f;
    }

    void ensureGlow() {
        auto fields = m_fields.self();

        CCNode* parent = this->getParent();
        if (!parent) return;

        int z = this->getZOrder();
        if (auto* gl = GJBaseGameLayer::get()) {
            PlayerObject* player = gl->m_player1;
            if (gl->m_player2 && gl->m_player2->m_waveTrail == this) {
                player = gl->m_player2;
            }
            if (player) {
                z = player->getZOrder() - 1;
                this->setZOrder(z);
            }
        }

        if (!fields->glow) {
            auto glow = GlowNode::create();
            glow->m_streak = this;
            parent->addChild(glow, z - 1);
            fields->glow = glow;
        }
        auto* glow = fields->glow;

        if (glow->getParent() != parent) {
            glow->removeFromParentAndCleanup(false);
            parent->addChild(glow, z - 1);
        } else {
            glow->setZOrder(z - 1);
        }
        glow->setPosition(this->getPosition());
        glow->setScale(this->getScale());
        glow->setRotation(this->getRotation());
    }

    void applyLightsaber() {
        auto* buffer = this->m_pBuffer;
        int count = this->m_nBufferCount;

        this->ensureGlow();
        auto* glow = m_fields->glow;
        if (!glow) return;

        if (count < 3) {
            glow->m_nBufferCount = 0;
            glow->m_bDirty = true;
            return;
        }

        ccColor4B trailColor = buffer[0].colors;

        this->buildGlow(buffer, count, trailColor);

        for (int i = 0; i < count; ++i) {
            buffer[i].colors = {255, 255, 255, 255};
        }
        this->setBlendFunc({GL_ONE, GL_ZERO});
        this->m_bDirty = true;
    }

    void buildGlow(ccV2F_C4B_T2F* buffer, int count, ccColor4B trailColor) {
        auto fields = m_fields.self();
        auto* glow = fields->glow;

        float glowSize = getSetting<float, "glow-size">();
        float strength = getSetting<float, "glow-strength">();
        bool additive = getSetting<bool, "glow-additive">();

        glowSize *= this->waveScale();

        float musicGlow = getSetting<float, "music-glow">();
        if (musicGlow > 0.f) {
            float m = fields->musicAmp;
            glowSize *= 1.f + musicGlow * m;
            strength = std::clamp(strength + musicGlow * m, 0.f, 1.f);
        }

        if (glowSize <= 0.f || strength <= 0.f) {
            glow->m_nBufferCount = 0;
            glow->m_bDirty = true;
            return;
        }

        float s = std::clamp(strength, 0.f, 1.f);
        ccColor4B innerColor, outerColor;
        if (additive) {
            innerColor = {static_cast<GLubyte>(trailColor.r * s),
                          static_cast<GLubyte>(trailColor.g * s),
                          static_cast<GLubyte>(trailColor.b * s),
                          static_cast<GLubyte>(255.f * s)};
            outerColor = {0, 0, 0, 0};
        } else {
            auto innerAlpha = static_cast<GLubyte>(s * 255.f);
            innerColor = {trailColor.r, trailColor.g, trailColor.b, innerAlpha};
            outerColor = {trailColor.r, trailColor.g, trailColor.b, 0};
        }

        auto& uniq = fields->uniq;
        auto& weldGrid = fields->weldGrid;
        auto& edges = fields->edges;
        auto& vmap = fields->vmap;
        auto& joins = fields->joins;
        auto& verts = fields->verts;

        uniq.clear();
        uniq.reserve(static_cast<size_t>(count));
        weldGrid.clear();
        edges.clear();
        edges.reserve(static_cast<size_t>(count));
        verts.clear();

        auto cellKey = [](int cx, int cy) {
            return (static_cast<int64_t>(static_cast<uint32_t>(cx)) << 32)
                | static_cast<uint32_t>(cy);
        };
        auto weld = [&](CCPoint p) -> int {
            int cx = static_cast<int>(std::floor(p.x / kWeldTol));
            int cy = static_cast<int>(std::floor(p.y / kWeldTol));
            float tol2 = kWeldTol * kWeldTol;
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    auto it = weldGrid.find(cellKey(cx + dx, cy + dy));
                    if (it == weldGrid.end()) continue;
                    for (int idx : it->second) {
                        if (uniq[idx].getDistanceSq(p) <= tol2) return idx;
                    }
                }
            }
            int id = static_cast<int>(uniq.size());
            uniq.push_back(p);
            weldGrid[cellKey(cx, cy)].push_back(id);
            return id;
        };

        auto edgeKey = [](int a, int b) -> int64_t {
            int lo = a < b ? a : b;
            int hi = a < b ? b : a;
            return (static_cast<int64_t>(static_cast<uint32_t>(lo)) << 32)
                | static_cast<uint32_t>(hi);
        };
        auto addEdge = [&](int k1, int k2, CCPoint opp) {
            if (k1 == k2) return;
            CCPoint p1 = uniq[k1];
            CCPoint p2 = uniq[k2];
            auto& e = edges[edgeKey(k1, k2)];
            e.count++;
            e.k1 = k1;
            e.k2 = k2;
            e.p1 = p1;
            e.p2 = p2;
            CCPoint dir = p2 - p1;
            float len = dir.getLength();
            if (len >= 0.0001f) {
                CCPoint normal = ccp(-dir.y, dir.x) * (1.f / len);
                CCPoint mid = (p1 + p2) * 0.5f;
                if (normal.dot(mid - opp) < 0.f) normal = normal * -1.f;
                e.outward = normal;
            }
        };

        for (int i = 0; i + 2 < count; i += 3) {
            int ia = weld({buffer[i].vertices.x, buffer[i].vertices.y});
            int ib = weld({buffer[i + 1].vertices.x, buffer[i + 1].vertices.y});
            int ic = weld({buffer[i + 2].vertices.x, buffer[i + 2].vertices.y});
            addEdge(ia, ib, uniq[ic]);
            addEdge(ib, ic, uniq[ia]);
            addEdge(ic, ia, uniq[ib]);
        }

        vmap.assign(uniq.size(), VAcc{});
        auto addV = [&](int k, CCPoint pos, CCPoint n, CCPoint neighbor) {
            auto& v = vmap[k];
            v.pos = pos;
            if (v.cnt < 2) {
                v.n[v.cnt] = n;
                v.neighbor[v.cnt] = neighbor;
            }
            v.cnt++;
        };
        for (auto const& [key, e] : edges) {
            if (e.count != 1) continue;
            addV(e.k1, e.p1, e.outward, e.p2);
            addV(e.k2, e.p2, e.outward, e.p1);
        }

        joins.assign(uniq.size(), VJoin{});
        for (size_t k = 0; k < uniq.size(); ++k) {
            VAcc const& v = vmap[k];
            if (v.cnt != 2) continue;
            VJoin j;
            CCPoint n1 = v.n[0], n2 = v.n[1];
            CCPoint u1 = v.neighbor[0] - v.pos;
            CCPoint u2 = v.neighbor[1] - v.pos;
            float l1 = u1.getLength(), l2 = u2.getLength();
            if (l1 >= 0.0001f) u1 = u1 * (1.f / l1);
            if (l2 >= 0.0001f) u2 = u2 * (1.f / l2);
            j.convex = (u1 + u2).dot(n1 + n2) < 0.f;
            CCPoint md = n1 + n2;
            float ml = md.getLength();
            if (ml >= 0.0001f) {
                md = md * (1.f / ml);
                float d = std::max(md.dot(n1), 0.25f);
                j.miter = v.pos + md * (glowSize / d);
            } else {
                j.miter = v.pos;
            }
            j.valid = true;
            joins[k] = j;
        }

        verts.reserve(edges.size() * 9);

        auto outerAt = [&](int k, CCPoint pos, CCPoint edgeN) -> CCPoint {
            VJoin const& jn = joins[k];
            if (jn.valid && !jn.convex) return jn.miter;
            return pos + edgeN * glowSize;
        };

        for (auto const& [key, e] : edges) {
            if (e.count != 1) continue;
            CCPoint ao = outerAt(e.k1, e.p1, e.outward);
            CCPoint bo = outerAt(e.k2, e.p2, e.outward);
            verts.push_back(makeVertex(e.p1, innerColor));
            verts.push_back(makeVertex(e.p2, innerColor));
            verts.push_back(makeVertex(bo, outerColor));
            verts.push_back(makeVertex(e.p1, innerColor));
            verts.push_back(makeVertex(bo, outerColor));
            verts.push_back(makeVertex(ao, outerColor));
        }

        float maxStep = static_cast<float>(15.0 * kPi / 180.0);
        for (size_t k = 0; k < uniq.size(); ++k) {
            VJoin const& jn = joins[k];
            if (!jn.valid || !jn.convex) continue;
            VAcc const& v = vmap[k];
            CCPoint n1 = v.n[0], n2 = v.n[1];
            float a1 = std::atan2(n1.y, n1.x);
            float a2 = std::atan2(n2.y, n2.x);
            float d = a2 - a1;
            while (d > static_cast<float>(kPi)) d -= static_cast<float>(2.0 * kPi);
            while (d < -static_cast<float>(kPi)) d += static_cast<float>(2.0 * kPi);
            int steps = std::max(2, static_cast<int>(std::ceil(std::fabs(d) / maxStep)));
            CCPoint prev = v.pos + ccp(std::cos(a1), std::sin(a1)) * glowSize;
            for (int i = 1; i <= steps; ++i) {
                float a = a1 + d * static_cast<float>(i) / static_cast<float>(steps);
                CCPoint cur = v.pos + ccp(std::cos(a), std::sin(a)) * glowSize;
                verts.push_back(makeVertex(v.pos, innerColor));
                verts.push_back(makeVertex(prev, outerColor));
                verts.push_back(makeVertex(cur, outerColor));
                prev = cur;
            }
        }

        glow->m_useMax = additive;
        glow->setBlendFunc(additive
            ? ccBlendFunc{GL_ONE, GL_ONE}
            : ccBlendFunc{GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
        uploadBuffer(glow, verts);
    }
};
