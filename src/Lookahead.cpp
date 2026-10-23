#include "Dynamic.hpp"

// ---------------------------------------------------------------------------
// Lookahead: simula el futuro cercano con un jugador "fantasma".
//
// Idea: copiamos el estado del jugador real al fantasma (usando el mismo
// sistema de guardado que usa el Practice Mode del juego), y lo hacemos avanzar
// tick a tick con la misma física del juego. Si el fantasma muere, ese plan
// de clic es "inseguro". Probamos retrasos de 0, 1, 2... hasta el margen y
// elegimos el primero que no mata.
//
// Rendimiento:
//  - solo se simula cuando llega un input;
//  - si el fantasma muere ANTES de llegar al tick del clic (esperar ya no
//    sirve), dejamos de probar retrasos mayores;
//  - hay un limite de tiempo (ajuste "Limite de simulacion"): si se pasa, el
//    clic se deja tal cual.
// ---------------------------------------------------------------------------

namespace {

using Clock = std::chrono::steady_clock;

struct SimEvent {
    int tick;   // tick relativo (0 = ahora)
    bool down;  // pulsar / soltar
};

enum class SimResult { Safe, Dead, DeadEarly, Timeout };

SimResult simulate(
    PlayLayer* pl, PlayerObject* real, std::vector<SimEvent> const& events,
    int totalTicks, int candidateTick, bool limited, Clock::time_point deadline
) {
    auto& g = DynamicState::get();
    auto& c = dyn::cfg();
    auto fake = g.fake;

    g.simulating = true;
    g.simDead = false;
    g.backups.clear();
    g.emulated.clear();

    fake->loadFromCheckpoint(g.snapshot);
    fake->m_yVelocity = real->m_yVelocity;
    fake->m_holdingButtons[1] = real->m_holdingButtons[1];
    fake->m_isDead = false;

    // Orbes: el fantasma debe "estar tocando" los mismos orbes que el jugador real.
    if (c.simOrbs) {
        if (fake->m_touchingRings) {
            fake->m_touchingRings->removeAllObjects();
            if (real->m_touchingRings) {
                for (auto obj : CCArrayExt<CCObject*>(real->m_touchingRings)) {
                    fake->m_touchingRings->addObject(obj);
                }
            }
        }
        fake->m_touchedRings.clear();
        for (auto id : real->m_touchedRings) fake->m_touchedRings.insert(id);
        fake->m_jumpPadRelated.clear();
        for (auto& [key, value] : real->m_jumpPadRelated) fake->m_jumpPadRelated[key] = value;
    }

    float const dt = g.lastDt;
    bool timeout = false;
    int deadAt = -1;
    bool deadInCollision = false;

    for (int i = 0; i < totalTicks; ++i) {
        if (limited && Clock::now() > deadline) {
            timeout = true;
            break;
        }

        fake->m_collisionLogTop->removeAllObjects();
        fake->m_collisionLogBottom->removeAllObjects();
        fake->m_collisionLogLeft->removeAllObjects();
        fake->m_collisionLogRight->removeAllObjects();

        pl->checkCollisions(fake, dt, false);
        if (g.simDead) {
            deadAt = i;
            deadInCollision = true;
            break;
        }

        for (auto const& e : events) {
            if (e.tick != i) continue;
            if (e.down) fake->pushButton(PlayerButton::Jump);
            else fake->releaseButton(PlayerButton::Jump);
        }

        fake->update(dt);
        fake->updateRotation(dt);
        fake->updatePlayerScale();
        if (g.simDead) {
            deadAt = i;
            break;
        }
    }

    dyn::restoreActivation();  // orbes y pads vuelven a como estaban
    g.simulating = false;

    if (timeout) return SimResult::Timeout;
    if (deadAt >= 0) {
        // Murio antes de que el clic se aplicara: esperar mas tampoco ayuda.
        bool early = deadAt < candidateTick || (deadAt == candidateTick && deadInCollision);
        return early ? SimResult::DeadEarly : SimResult::Dead;
    }
    return SimResult::Safe;
}

} // namespace

void dyn::backupActivation(GameObject* obj) {
    auto eff = typeinfo_cast<EnhancedGameObject*>(obj);
    if (!eff) return;
    auto& g = DynamicState::get();
    for (auto const& b : g.backups) {
        if (b.obj == eff) return;  // ya guardado en esta simulacion
    }
    g.backups.push_back({eff, eff->m_activated, eff->m_activatedByPlayer1, eff->m_activatedByPlayer2});
}

void dyn::restoreActivation() {
    auto& g = DynamicState::get();
    for (auto const& b : g.backups) {
        b.obj->m_activated = b.activated;
        b.obj->m_activatedByPlayer1 = b.byPlayer1;
        b.obj->m_activatedByPlayer2 = b.byPlayer2;
    }
    g.backups.clear();
}

char const* dyn::modeName(PlayerObject* p) {
    if (p->m_isShip) return "ship";
    if (p->m_isBall) return "ball";
    if (p->m_isBird) return "ufo";
    if (p->m_isDart) return "wave";
    if (p->m_isRobot) return "robot";
    if (p->m_isSpider) return "spider";
    if (p->m_isSwing) return "swing";
    return "cube";
}

bool dyn::modeAllowed(PlayerObject* p) {
    auto& c = cfg();
    if (c.waveOnly) return p->m_isDart;
    if (p->m_isShip) return c.modes[1];
    if (p->m_isBall) return c.modes[2];
    if (p->m_isBird) return c.modes[3];
    if (p->m_isDart) return c.modes[4];
    if (p->m_isRobot) return c.modes[5];
    if (p->m_isSpider) return c.modes[6];
    if (p->m_isSwing) return c.modes[7];
    return c.modes[0];
}

void dyn::resetRuntime(bool forgetFake) {
    auto& g = DynamicState::get();
    g.queue.clear();
    g.simulating = false;
    g.simDead = false;
    g.flushing = false;
    g.backups.clear();
    g.emulated.clear();
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
    auto& c = cfg();
    int const maxDelay = c.margin;

    // Plan de seguridad: si algo falta, NO tocamos el input (retraso mínimo).
    if (!g.fake || !g.snapshot || !real || g.owner != pl || maxDelay <= minDelay) return minDelay;

    auto const start = Clock::now();
    bool const limited = c.budgetMs > 0.f;
    auto const deadline = start + std::chrono::microseconds(static_cast<int64_t>(c.budgetMs * 1000.f));

    real->saveToCheckpoint(g.snapshot);
    g.lastMode = modeName(real);

    bool const isP1 = (real == pl->m_player1);

    // Vectores reutilizados para no pedir memoria en cada clic.
    static std::vector<SimEvent> known;
    static std::vector<SimEvent> events;
    known.clear();

    // Inputs ya retenidos del mismo jugador: forman parte del "futuro conocido".
    for (auto const& p : g.queue) {
        if (p.isPlayer1 != isP1) continue;
        known.push_back({p.targetStep - g.tick, p.down});
    }

    // Si ningún tick dentro del margen es seguro (o se acaba el tiempo),
    // no bloqueamos el clic: se ejecuta en su momento original.
    int result = minDelay;
    bool timedOut = false;

    for (int d = minDelay; d <= maxDelay; ++d) {
        events = known;
        events.push_back({d, isDown});

        auto r = simulate(pl, real, events, d + c.lookahead, d, limited, deadline);
        if (r == SimResult::Safe) {
            result = d;
            break;
        }
        if (r == SimResult::Timeout) {
            timedOut = true;
            break;
        }
        if (r == SimResult::DeadEarly) break;  // esperar mas no puede ayudar
    }

    double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    g.lastSimMs = ms;
    g.maxSimMs = std::max(g.maxSimMs, ms);
    if (timedOut) ++g.timeouts;

    if (c.debug) {
        g.hudDirty = true;
        log::debug(
            "Dynamic: {} en modo {} -> retraso {} ticks (min {}, margen {}) en {:.3f} ms{}",
            isDown ? "pulsar" : "soltar", g.lastMode, result, minDelay, maxDelay, ms,
            timedOut ? " [LIMITE DE TIEMPO]" : ""
        );
    }

    return result;
}
