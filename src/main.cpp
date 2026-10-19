#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

struct BufferedClick {
    bool active = false;
    bool releaseQueued = false;
    bool releaseNext = false;
    int ticksLeft = 0;
    int button = 1;
};

static BufferedClick g_buf[2];
static bool g_replaying = false;

static bool canActNow(PlayerObject* p) {
    if (!p) return true;
    if (p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing) return true;
    if (p->m_isOnGround) return true;
    if (p->m_touchingRings && p->m_touchingRings->count() > 0) return true;
    return false;
}

static void clearBuffers() {
    g_buf[0] = BufferedClick();
    g_buf[1] = BufferedClick();
}

class $modify(BufferGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        auto pl = PlayLayer::get();
        auto mod = Mod::get();
        int ticks = mod->getSettingValue<int64_t>("buffer-ticks");

        if (g_replaying || !pl || static_cast<GJBaseGameLayer*>(pl) != this ||
            !mod->getSettingValue<bool>("enabled") || ticks <= 0 || button != 1) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto player = isPlayer1 ? m_player1 : m_player2;
        auto& b = g_buf[isPlayer1 ? 0 : 1];

        if (down) {
            if (canActNow(player)) {
                b = BufferedClick();
                GJBaseGameLayer::handleButton(down, button, isPlayer1);
            } else {
                b.active = true;
                b.releaseQueued = false;
                b.releaseNext = false;
                b.ticksLeft = ticks;
                b.button = button;
            }
        } else {
            if (b.active) {
                b.releaseQueued = true;
            } else {
                GJBaseGameLayer::handleButton(down, button, isPlayer1);
            }
        }
    }

    void processCommands(float dt) {
        auto pl = PlayLayer::get();
        if (pl && static_cast<GJBaseGameLayer*>(pl) == this &&
            Mod::get()->getSettingValue<bool>("enabled")) {
            for (int i = 0; i < 2; i++) {
                auto& b = g_buf[i];
                bool isP1 = (i == 0);
                auto player = isP1 ? m_player1 : m_player2;

                if (b.releaseNext) {
                    g_replaying = true;
                    GJBaseGameLayer::handleButton(false, b.button, isP1);
                    g_replaying = false;
                    b = BufferedClick();
                    continue;
                }
                if (!b.active) continue;

                if (canActNow(player)) {
                    g_replaying = true;
                    GJBaseGameLayer::handleButton(true, b.button, isP1);
                    g_replaying = false;
                    if (b.releaseQueued) {
                        b.active = false;
                        b.releaseNext = true;
                    } else {
                        b = BufferedClick();
                    }
                } else if (--b.ticksLeft <= 0) {
                    b = BufferedClick();
                }
            }
        }
        GJBaseGameLayer::processCommands(dt);
    }
};

class $modify(PlayLayer) {
    void resetLevel() {
        clearBuffers();
        PlayLayer::resetLevel();
    }
};
