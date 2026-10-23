#include "Dynamic.hpp"

#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

// ---------------------------------------------------------------------------
// Dynamic - asistente de precision de clics.
// Aqui estan: los ajustes en memoria, la intercepcion de inputs, la cola de
// retencion, el indicador en pantalla y los atajos de teclado.
// ---------------------------------------------------------------------------

// Lee todos los ajustes una sola vez (y cada vez que cambian).
void Cfg::refresh() {
    auto mod = Mod::get();
    enabled = mod->getSettingValue<bool>("enabled");
    waveOnly = mod->getSettingValue<bool>("wave-only");
    practiceOnly = mod->getSettingValue<bool>("practice-only");
    assistRelease = mod->getSettingValue<bool>("assist-release");
    showIndicator = mod->getSettingValue<bool>("show-indicator");
    showCounter = mod->getSettingValue<bool>("show-counter");
    debug = mod->getSettingValue<bool>("debug-log");
    simOrbs = mod->getSettingValue<bool>("sim-orbs");
    simPortals = mod->getSettingValue<bool>("sim-portals");
    simPortalsAdv = mod->getSettingValue<bool>("sim-portals-adv");

    char const* modeKeys[8] = {
        "mode-cube", "mode-ship", "mode-ball", "mode-ufo",
        "mode-wave", "mode-robot", "mode-spider", "mode-swing"
    };
    for (int i = 0; i < 8; ++i) modes[i] = mod->getSettingValue<bool>(modeKeys[i]);

    margin = static_cast<int>(std::clamp<int64_t>(mod->getSettingValue<int64_t>("margin"), 0, 64));
    lookahead = static_cast<int>(std::clamp<int64_t>(mod->getSettingValue<int64_t>("lookahead-ticks"), 2, 32));
    opacity = static_cast<int>(std::clamp<int64_t>(mod->getSettingValue<int64_t>("indicator-opacity"), 10, 100));
    budgetMs = static_cast<float>(std::max(0.0, mod->getSettingValue<double>("sim-budget-ms")));

    auto cornerName = mod->getSettingValue<std::string>("indicator-corner");
    if (cornerName == "Top-right") corner = 1;
    else if (cornerName == "Bottom-left") corner = 2;
    else if (cornerName == "Bottom-right") corner = 3;
    else corner = 0;

    DynamicState::get().hudDirty = true;
}

$execute {
    dyn::cfg().refresh();

    // Cada vez que cambias un ajuste en el menu, actualizamos la copia en memoria.
    geode::listenForAllSettingChanges([](std::string_view, std::shared_ptr<SettingV3>) {
        dyn::cfg().refresh();
    });

    // Atajo: activar/desactivar.
    geode::listenForKeybindSettingPresses("toggle-key", [](Keybind const&, bool down, bool repeat, double) {
        if (down && !repeat) {
            auto mod = Mod::get();
            mod->setSettingValue<bool>("enabled", !mod->getSettingValue<bool>("enabled"));
        }
        return false;  // no "consumimos" la tecla
    });

    // Atajo: reiniciar el contador de clics corregidos.
    geode::listenForKeybindSettingPresses("reset-key", [](Keybind const&, bool down, bool repeat, double) {
        if (down && !repeat) {
            auto& g = DynamicState::get();
            g.corrected = 0;
            g.maxSimMs = 0.0;
            g.timeouts = 0;
            g.hudDirty = true;
        }
        return false;
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

    // Coloca el indicador en la esquina elegida.
    void placeLabel(CCLabelBMFont* label, int corner) {
        auto size = CCDirector::get()->getWinSize();
        bool right = (corner == 1 || corner == 3);
        bool bottom = (corner >= 2);
        label->setAnchorPoint({right ? 1.f : 0.f, bottom ? 0.f : 1.f});
        label->setPosition({right ? size.width - 8.f : 8.f, bottom ? 8.f : size.height - 8.f});
    }

    unsigned char baseOpacity() {
        return static_cast<unsigned char>(dyn::cfg().opacity * 255 / 100);
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

        auto& c = dyn::cfg();
        bool assist = c.enabled && c.margin > 0 && !m_levelEndAnimationStarted && !player->m_isDead;
        if (assist && !dyn::modeAllowed(player)) assist = false;  // modo no elegido (o 'solo wave')
        if (assist && c.practiceOnly && !m_isPracticeMode) assist = false;
        if (assist && !down && !c.assistRelease) assist = false;

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
            g.hudDirty = true;
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
        label->setScale(0.35f);
        m_uiLayer->addChild(label, 100);
        g.label = label;
        g.hudDirty = true;
        g.flashActive = false;
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
        auto& c = dyn::cfg();
        if (!g.label || g.owner != this) return;

        // El texto, la posicion y la visibilidad solo se rehacen si algo cambio.
        if (g.hudDirty) {
            g.hudDirty = false;
            bool show = c.enabled && c.showIndicator;
            g.label->setVisible(show);
            if (show) {
                std::string text = fmt::format("Dynamic ON ({})", c.margin);
                if (c.showCounter) text += fmt::format("  x{}", g.corrected);
                if (c.debug) {
                    text += fmt::format("  {:.2f}ms (max {:.2f}) t/o {} [{}]", g.lastSimMs, g.maxSimMs, g.timeouts, g.lastMode);
                }
                g.label->setString(text.c_str());
                placeLabel(g.label, c.corner);
                g.label->setOpacity(baseOpacity());
                g.label->setColor(ccc3(255, 255, 255));
            }
        }

        // Parpadeo sutil: solo trabaja mientras dura.
        if (g.flash > 0.f && g.label->isVisible()) {
            g.flash = std::max(0.f, g.flash - dt * 4.f);
            unsigned char base = baseOpacity();
            g.label->setOpacity(static_cast<unsigned char>(base + (255 - base) * g.flash));
            g.label->setColor(ccc3(120, 255, 160));
            g.flashActive = true;
        } else if (g.flashActive) {
            g.flashActive = false;
            g.flash = 0.f;
            g.label->setOpacity(baseOpacity());
            g.label->setColor(ccc3(255, 255, 255));
        }
    }
};
