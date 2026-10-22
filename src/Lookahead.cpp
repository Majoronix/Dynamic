#include "Dynamic.hpp"

// ---------------------------------------------------------------------------
// Lookahead: simula el futuro cercano con un jugador "fantasma".
//
// Idea: copiamos el estado del jugador real al fantasma (usando el mismo
// sistema de guardado que usa el Practice Mode del juego), y lo hacemos avanzar
// tick a tick con la misma física del juego. Si el fantasma muere, ese plan
// de clic es "inseguro". Probamos retrasos de 0, 1, 2... hasta el margen y
// elegimos el primero que no mata.
// ---------------------------------------------------------------------------

namespace {

struct SimEvent {
    int tick;   // tick relativo (0 = ahora)
    bool down;  // pulsar / soltar
};

// Cuántos ticks seguimos simulando DESPUÉS del último input. Es un compromiso:
// muy corto = no ve peligros; muy largo = cree que mueres por inputs futuros
// del jugador que todavía no existen.
constexpr int kTicksAfterInput = 6;

bool simulate(PlayLayer* pl, PlayerObject* real, std::vector<SimEvent> const& events, int totalTicks) {
    auto& g = DynamicState::get();
    auto fake = g.fake;

    g.simulating = true;
    g.simDead = false;

    fake->loadFromCheckpoint(g.snapshot);
    fake->m_yVelocity = real->m_yVelocity;
    fake->m_holdingButtons[1] = real->m_holdingButtons[1];
    fake->m_isDead = false;

    float const dt = g.lastDt;

    for (int i = 0; i < totalTicks && !g.simDead; ++i) {
        fake->m_collisionLogTop->removeAllObjects();
        fake->m_collisionLogBottom->removeAllObjects();
        fake->m_collisionLogLeft->removeAllObjects();
        fake->m_collisionLogRight->removeAllObjects();

        pl->checkCollisions(fake, dt, false);
        if (g.simDead) break;

        for (auto const& e : events) {
            if (e.tick != i) continue;
            if (e.down) fake->pushButton(PlayerButton::Jump);
            else fake->releaseButton(PlayerButton::Jump);
        }

        fake->update(dt);
        fake->updateRotation(dt);
        fake->updatePlayerScale();
    }

    g.simulating = false;
    return !g.simDead;
}

} // namespace

void dyn::resetRuntime(bool forgetFake) {
    auto& g = DynamicState::get();
    g.queue.clear();
    g.simulating = false;
    g.simDead = false;
    g.flushing = false;
    if (forgetFake) {
        g.fake = nullptr;  // el nivel dueño del fantasma ya no existe
        g.label = nullptr;
        g.owner = nullptr;
    }
}

void dyn::createFake(PlayLayer* pl) {
    auto& g = DynamicState::get();
    g.fake = nullptr;
    g.owner = pl;

    auto fake = PlayerObject::create(1, 1, pl, pl, true);
    if (!fake) {
        log::warn("Dynamic: no se pudo crear el jugador fantasma; el asistente no corregira nada.");
        return;
    }
    fake->setPosition({0.f, 105.f});
    fake->setVisible(false);
    pl->m_objectLayer->addChild(fake);  // el nivel es el dueño; se libera con él
    g.fake = fake;

    if (!g.snapshot) {
        g.snapshot = PlayerCheckpoint::create();
        if (g.snapshot) g.snapshot->retain();  // lo conservamos mientras el juego esté abierto
    }
}

int dyn::chooseDelay(PlayLayer* pl, PlayerObject* real, bool isDown, int minDelay) {
    auto& g = DynamicState::get();
    int const maxDelay = margin();

    // Plan de seguridad: si algo falta, NO tocamos el input (retraso mínimo).
    if (!g.fake || !g.snapshot || !real || g.owner != pl || maxDelay <= minDelay) return minDelay;

    real->saveToCheckpoint(g.snapshot);

    bool const isP1 = (real == pl->m_player1);

    // Inputs ya retenidos del mismo jugador: forman parte del "futuro conocido".
    std::vector<SimEvent> known;
    for (auto const& p : g.queue) {
        if (p.isPlayer1 != isP1) continue;
        known.push_back({p.targetStep - g.tick, p.down});
    }

    for (int d = minDelay; d <= maxDelay; ++d) {
        auto events = known;
        events.push_back({d, isDown});
        if (simulate(pl, real, events, d + kTicksAfterInput)) return d;
    }

    // Ningún tick dentro del margen es seguro: no bloqueamos el clic,
    // se ejecuta en su momento original.
    return minDelay;
}
