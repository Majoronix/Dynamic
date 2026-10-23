#pragma once

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <algorithm>
#include <chrono>
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

// Copia en memoria de los ajustes (leerlos del disco en cada tick sería lento).
// Se actualiza sola cuando cambias un ajuste (ver main.cpp).
struct Cfg {
    bool enabled = true;
    bool waveOnly = false;
    bool practiceOnly = false;
    bool assistRelease = true;
    bool showIndicator = true;
    bool showCounter = true;
    bool debug = false;
    bool simOrbs = true;          // la simulación incluye orbes y pads
    bool simPortals = true;       // ... portales de tamaño y velocidad
    bool simPortalsAdv = false;   // ... portales de gravedad y de modo (experimental)
    // Modos asistidos: cubo, nave, bola, ufo, wave, robot, araña, swing
    bool modes[8] = {true, true, true, true, true, true, true, true};
    int margin = 4;
    int lookahead = 6;
    int opacity = 35;
    int corner = 0;        // 0 arriba-izq, 1 arriba-der, 2 abajo-izq, 3 abajo-der
    float budgetMs = 2.f;

    void refresh();
};

// Copia de seguridad de las marcas de "ya activado" de un orbe/pad, para
// dejarlos como estaban cuando termina una simulación.
struct ActivationBackup {
    EnhancedGameObject* obj;
    bool activated;
    bool byPlayer1;
    bool byPlayer2;
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
    bool flashActive = false;
    bool hudDirty = true;        // el texto/posición del indicador hay que rehacerlos

    std::vector<ActivationBackup> backups;   // orbes/pads tocados durante la simulación
    std::vector<GameObject*> emulated;       // portales ya aplicados en esta simulación
    char const* lastMode = "-";              // diagnóstico: modo del jugador

    double lastSimMs = 0.0;      // diagnóstico: última simulación
    double maxSimMs = 0.0;       // diagnóstico: la más lenta
    int timeouts = 0;            // diagnóstico: veces que se pasó del límite de tiempo
};

namespace dyn {
    inline Cfg& cfg() {
        static Cfg c;
        return c;
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

    // Guarda / restaura las marcas de activación de orbes y pads.
    void backupActivation(GameObject* obj);
    void restoreActivation();

    // Nombre del modo actual del jugador, y si el usuario lo quiere asistido.
    char const* modeName(PlayerObject* player);
    bool modeAllowed(PlayerObject* player);

    // Elige cuántos ticks retrasar un input (entre minDelay y el margen).
    int chooseDelay(PlayLayer* pl, PlayerObject* real, bool isDown, int minDelay);
}
