#pragma once

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <algorithm>
#include <deque>
#include <string>
#include <vector>

using namespace geode::prelude;

// Un input (pulsar o soltar) que hemos retenido y que se ejecutará más tarde.
struct PendingInput {
    bool down;       // true = pulsar, false = soltar
    bool isPlayer1;  // a qué jugador pertenece
    int targetStep;  // tick (contador propio) en el que se ejecutará
};

// Estado global del mod (un único objeto compartido por todos los archivos).
struct DynamicState {
    static DynamicState& get() {
        static DynamicState s;
        return s;
    }

    std::deque<PendingInput> queue;          // inputs retenidos, en orden
    PlayerObject* fake = nullptr;            // jugador "fantasma" para simular
    PlayerCheckpoint* snapshot = nullptr;    // foto del estado del jugador real
    CCLabelBMFont* label = nullptr;          // indicador en pantalla
    PlayLayer* owner = nullptr;              // partida a la que pertenecen fake y label

    int tick = 0;                // contador propio de ticks (ver processCommands)
    bool simulating = false;     // true mientras corre la simulación
    bool simDead = false;        // la simulación detectó una muerte
    bool flushing = false;       // estamos soltando inputs retenidos
    float lastDt = 1.f / 240.f;  // duración real de un tick (la copiamos del juego)
    int corrected = 0;           // clics corregidos en la sesión
    float flash = 0.f;           // 0..1, parpadeo del indicador
    std::string lastText;        // último texto mostrado
};

namespace dyn {
    inline bool isEnabled() { return Mod::get()->getSettingValue<bool>("enabled"); }
    inline bool waveOnly() { return Mod::get()->getSettingValue<bool>("wave-only"); }
    inline int margin() {
        auto v = Mod::get()->getSettingValue<int64_t>("margin");
        return static_cast<int>(std::clamp<int64_t>(v, 0, 16));
    }

    // Devuelve el PlayLayer solo si "layer" es la partida real (no el editor).
    inline PlayLayer* activeLayer(GJBaseGameLayer* layer) {
        auto pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != layer) return nullptr;
        return pl;
    }

    // Vacía la cola y olvida los jugadores fantasma (nuevo nivel / salir).
    void resetRuntime(bool forgetFake);

    // Crea el jugador fantasma dentro de la partida.
    void createFake(PlayLayer* pl);

    // Elige cuántos ticks retrasar un input (entre minDelay y el margen).
    int chooseDelay(PlayLayer* pl, PlayerObject* real, bool isDown, int minDelay);
}
