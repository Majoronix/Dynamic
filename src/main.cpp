#include "Dynamic.hpp"

#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

// ---------------------------------------------------------------------------
// Dynamic - asistente de precision de clics.
// Aqui estan: la intercepcion de inputs, la cola de retencion, el indicador
// en pantalla y el atajo de teclado.
// ---------------------------------------------------------------------------

// Atajo de teclado para activar/desactivar (se configura en los ajustes).
$execute {
    geode::listenForKeybindSettingPresses("toggle-key", [](Keybind const&, bool down, bool repeat, double) {
        if (down && !repeat) {
            auto mod = Mod::get();
            mod->setSettingValue<bool>("enabled", !mod->getSettingValue<bool>("enabled"));
        }
        return false;  // no "consumimos" la tecla
    });
}

namespace {
    // Ejecuta los inputs retenidos cuyo momento ya llego, en orden.
    void flushDue(GJBaseGameLayer* layer) {
        auto& g = DynamicState::get();
        g.flushing = true;
        while (!g.queue.empty() && g.queue.front().targetStep <= g.tick) {
            auto e = g.queue.front();
            g.queue.pop_front();
            layer->handleButton(e.down, 1, e.isPlayer1);  // pasa por nuestro hook, que lo deja pasar
        }
        g.flushing = false;
    }
}

class $modify(DynamicBGL, GJBaseGameLayer) {
    // Llamada una vez por cada paso de fisica: aqui avanzamos nuestro contador
    // de ticks y soltamos los inputs retenidos que ya toquen.
    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto& g = DynamicState::get();
        if (!isHalfTick) ++g.tick;

        if (!g.queue.empty()) {
            if (dyn::activeLayer(this) && !g.simulating) flushDue(this);
            else if (!dyn::activeLayer(this)) g.queue.clear();
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }

    // Aqui llega cada pulsar/soltar del jugador.
    void handleButton(bool down, int button, bool isPlayer1) {
        auto& g = DynamicState::get();
        auto pl = dyn::activeLayer(this);

        // Casos en los que NO intervenimos: editor/menus, nuestra propia
        // simulacion o liberacion, o botones que no son el de salto/clic (1).
        if (!pl || g.simulating || g.flushing || button != 1) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        PlayerObject* player = isPlayer1 ? m_player1 : m_player2;
        if (!player) return GJBaseGameLayer::handleButton(down, button, isPlayer1);

        bool assist = dyn::isEnabled() && dyn::margin() > 0 && !m_levelEndAnimationStarted && !player->m_isDead;
        if (assist && dyn::waveOnly() && !player->m_isDart) assist = false;

        bool queued = !g.queue.empty();
        if (!queued && !assist) return GJBaseGameLayer::handleButton(down, button, isPlayer1);

        // Nunca antes que un input ya retenido: asi se conserva el orden.
        int minDelay = queued ? std::max(0, g.queue.back().targetStep - g.tick) : 0;
        int delay = minDelay;
        if (assist) delay = dyn::chooseDelay(pl, player, down, minDelay);

        if (delay <= 0 && !queued) {
            // Sin retraso: se ejecuta ahora mismo, como siempre.
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        g.queue.push_back({down, isPlayer1, g.tick + delay});

        if (assist && down && delay > 0) {
            ++g.corrected;
            g.flash = 1.f;
        }
    }
};

class $modify(DynamicPlay, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        dyn::resetRuntime(true);
        dyn::createFake(this);

        auto& g = DynamicState::get();
        auto label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setID("dynamic-indicator"_spr);
        label->setAnchorPoint({0.f, 1.f});
        label->setScale(0.35f);
        label->setOpacity(90);
        auto size = CCDirector::get()->getWinSize();
        label->setPosition({8.f, size.height - 8.f});
        m_uiLayer->addChild(label, 100);
        g.label = label;
        g.lastText.clear();
    }

    void resetLevel() {
        dyn::resetRuntime(false);  // un clic retenido no debe pasar al nuevo intento
        PlayLayer::resetLevel();
    }

    void onQuit() {
        dyn::resetRuntime(true);
        PlayLayer::onQuit();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto& g = DynamicState::get();
        if (!g.label || g.owner != this) return;

        bool show = dyn::isEnabled() && Mod::get()->getSettingValue<bool>("show-indicator");
        g.label->setVisible(show);
        if (!show) return;

        std::string text = fmt::format("Dynamic ON ({})", dyn::margin());
        if (Mod::get()->getSettingValue<bool>("show-counter")) {
            text += fmt::format("  x{}", g.corrected);
        }
        if (text != g.lastText) {
            g.label->setString(text.c_str());
            g.lastText = text;
        }

        g.flash = std::max(0.f, g.flash - dt * 4.f);  // parpadeo sutil
        g.label->setOpacity(static_cast<unsigned char>(90.f + 165.f * g.flash));
        g.label->setColor(g.flash > 0.f ? ccc3(120, 255, 160) : ccc3(255, 255, 255));
    }
};
