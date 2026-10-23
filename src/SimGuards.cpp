#include "Dynamic.hpp"

#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/RingObject.hpp>

// ---------------------------------------------------------------------------
// "Protecciones": mientras corre la simulación, el jugador fantasma NO debe
// tener ningún efecto sobre el nivel real (triggers, monedas, sonidos,
// partículas, efectos visuales...). Estos hooks bloquean esos efectos SOLO
// durante la simulación. Fuera de la simulación llaman a la función original.
//
// Orbes y pads SI se dejan actuar sobre el fantasma (si el ajuste esta activo);
// sus marcas de "ya activado" se guardan antes y se restauran despues.
// Los portales NUNCA se activan de verdad: se aplican a mano solo al fantasma.
// ---------------------------------------------------------------------------

namespace {

enum class Kind { Hard, Orb, Portal, Other };

bool isSpeedPortalId(int id) {
    return id == 200 || id == 201 || id == 202 || id == 203 || id == 1334;
}

Kind classify(GameObject* obj) {
    auto& c = dyn::cfg();

    if (isSpeedPortalId(obj->m_objectID)) return c.simPortals ? Kind::Portal : Kind::Other;

    switch (obj->m_objectType) {
        // Lo que puede matar o bloquear.
        case GameObjectType::Solid:
        case GameObjectType::Hazard:
        case GameObjectType::AnimatedHazard:
        case GameObjectType::Slope:
            return Kind::Hard;

        // Orbes y pads.
        case GameObjectType::YellowJumpPad:
        case GameObjectType::PinkJumpPad:
        case GameObjectType::GravityPad:
        case GameObjectType::RedJumpPad:
        case GameObjectType::SpiderPad:
        case GameObjectType::YellowJumpRing:
        case GameObjectType::PinkJumpRing:
        case GameObjectType::RedJumpRing:
        case GameObjectType::GravityRing:
        case GameObjectType::GreenRing:
        case GameObjectType::DropRing:
        case GameObjectType::CustomRing:
        case GameObjectType::DashRing:
        case GameObjectType::GravityDashRing:
        case GameObjectType::SpiderOrb:
            return c.simOrbs ? Kind::Orb : Kind::Other;

        // Portales de tamaño.
        case GameObjectType::MiniSizePortal:
        case GameObjectType::RegularSizePortal:
            return c.simPortals ? Kind::Portal : Kind::Other;

        // Portales de gravedad y de vehiculo (experimental).
        case GameObjectType::InverseGravityPortal:
        case GameObjectType::NormalGravityPortal:
        case GameObjectType::CubePortal:
        case GameObjectType::ShipPortal:
        case GameObjectType::BallPortal:
        case GameObjectType::UfoPortal:
        case GameObjectType::WavePortal:
        case GameObjectType::RobotPortal:
        case GameObjectType::SpiderPortal:
        case GameObjectType::SwingPortal:
            return c.simPortalsAdv ? Kind::Portal : Kind::Other;

        default:
            return Kind::Other;
    }
}

// Apaga todos los vehiculos activos del fantasma (antes de encender otro).
void clearModes(PlayerObject* p) {
    if (p->m_isShip) p->toggleFlyMode(false, true);
    if (p->m_isBall) p->toggleRollMode(false, true);
    if (p->m_isBird) p->toggleBirdMode(false, true);
    if (p->m_isDart) p->toggleDartMode(false, true);
    if (p->m_isRobot) p->toggleRobotMode(false, true);
    if (p->m_isSpider) p->toggleSpiderMode(false, true);
    if (p->m_isSwing) p->toggleSwingMode(false, true);
}

// Aplica un portal SOLO al jugador fantasma (nunca al juego real).
void emulatePortal(PlayerObject* p, GameObject* obj) {
    auto& g = DynamicState::get();
    auto& c = dyn::cfg();

    // Cada portal se aplica una sola vez por simulacion.
    for (auto o : g.emulated) {
        if (o == obj) return;
    }
    g.emulated.push_back(obj);

    if (c.simPortals) {
        switch (obj->m_objectID) {
            case 200: p->m_playerSpeed = 0.7f; return;
            case 201: p->m_playerSpeed = 0.9f; return;
            case 202: p->m_playerSpeed = 1.1f; return;
            case 203: p->m_playerSpeed = 1.3f; return;
            case 1334: p->m_playerSpeed = 1.6f; return;
            default: break;
        }
    }

    switch (obj->m_objectType) {
        case GameObjectType::MiniSizePortal:
            if (c.simPortals) { p->togglePlayerScale(true, true); p->updatePlayerScale(); }
            break;
        case GameObjectType::RegularSizePortal:
            if (c.simPortals) { p->togglePlayerScale(false, true); p->updatePlayerScale(); }
            break;
        default:
            break;
    }

    if (!c.simPortalsAdv) return;

    switch (obj->m_objectType) {
        case GameObjectType::InverseGravityPortal:
            if (!p->m_isUpsideDown) p->flipGravity(true, true);
            break;
        case GameObjectType::NormalGravityPortal:
            if (p->m_isUpsideDown) p->flipGravity(false, true);
            break;
        case GameObjectType::CubePortal:
            clearModes(p);
            break;
        case GameObjectType::ShipPortal:
            if (!p->m_isShip) { clearModes(p); p->toggleFlyMode(true, true); }
            break;
        case GameObjectType::BallPortal:
            if (!p->m_isBall) { clearModes(p); p->toggleRollMode(true, true); }
            break;
        case GameObjectType::UfoPortal:
            if (!p->m_isBird) { clearModes(p); p->toggleBirdMode(true, true); }
            break;
        case GameObjectType::WavePortal:
            if (!p->m_isDart) { clearModes(p); p->toggleDartMode(true, true); }
            break;
        case GameObjectType::RobotPortal:
            if (!p->m_isRobot) { clearModes(p); p->toggleRobotMode(true, true); }
            break;
        case GameObjectType::SpiderPortal:
            if (!p->m_isSpider) { clearModes(p); p->toggleSpiderMode(true, true); }
            break;
        case GameObjectType::SwingPortal:
            if (!p->m_isSwing) { clearModes(p); p->toggleSwingMode(true, true); }
            break;
        default:
            break;
    }
}

} // namespace

class $modify(DynGuardBGL, GJBaseGameLayer) {
    void collisionCheckObjects(PlayerObject* p0, gd::vector<GameObject*>* objects, int count, float dt) {
        auto& g = DynamicState::get();
        if (!g.simulating) return GJBaseGameLayer::collisionCheckObjects(p0, objects, count, dt);

        // Desactivamos un instante todo lo que no nos interese simular.
        static std::vector<GameObject*> disabled;  // reutilizado: sin pedir memoria cada vez
        disabled.clear();

        size_t n = std::min<size_t>(static_cast<size_t>(std::max(count, 0)), objects->size());
        for (size_t i = 0; i < n; ++i) {
            auto obj = (*objects)[i];
            if (!obj) continue;

            switch (classify(obj)) {
                case Kind::Hard:
                case Kind::Portal:
                    break;
                case Kind::Orb:
                    dyn::backupActivation(obj);  // para devolverlo a como estaba
                    break;
                case Kind::Other:
                    if (obj->m_isDisabled || obj->m_isDisabled2) break;
                    disabled.push_back(obj);
                    obj->m_isDisabled = true;
                    obj->m_isDisabled2 = true;
                    break;
            }
        }

        GJBaseGameLayer::collisionCheckObjects(p0, objects, count, dt);

        for (auto obj : disabled) {
            obj->m_isDisabled = false;
            obj->m_isDisabled2 = false;
        }
    }

    bool canBeActivatedByPlayer(PlayerObject* player, EffectGameObject* object) {
        if (!DynamicState::get().simulating) return GJBaseGameLayer::canBeActivatedByPlayer(player, object);

        switch (classify(object)) {
            case Kind::Orb:
                return GJBaseGameLayer::canBeActivatedByPlayer(player, object);
            case Kind::Portal:
                emulatePortal(player, object);
                return false;  // el portal real no se activa
            default:
                return false;
        }
    }

    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        if (DynamicState::get().simulating && !dyn::cfg().simOrbs) return;
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }

    void playerTouchedTrigger(PlayerObject* player, EffectGameObject* trigger) {
        if (!DynamicState::get().simulating) return GJBaseGameLayer::playerTouchedTrigger(player, trigger);

        switch (classify(trigger)) {
            case Kind::Orb:
                GJBaseGameLayer::playerTouchedTrigger(player, trigger);
                break;
            case Kind::Portal:
                emulatePortal(player, trigger);
                break;
            default:
                break;
        }
    }

    void activateSFXTrigger(SFXTriggerGameObject* object) {
        if (DynamicState::get().simulating) return;
        GJBaseGameLayer::activateSFXTrigger(object);
    }

    void activateSongEditTrigger(SongTriggerGameObject* object) {
        if (DynamicState::get().simulating) return;
        GJBaseGameLayer::activateSongEditTrigger(object);
    }

    void gameEventTriggered(GJGameEvent event, int material, int playerID) {
        if (DynamicState::get().simulating) return;
        GJBaseGameLayer::gameEventTriggered(event, material, playerID);
    }
};

class $modify(DynGuardPlayer, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);
        auto& g = DynamicState::get();
        if (!g.simulating && dt > 0.f) g.lastDt = dt;  // duración real de un tick
    }

    void playSpiderDashEffect(CCPoint from, CCPoint to) {
        if (DynamicState::get().simulating) return;
        PlayerObject::playSpiderDashEffect(from, to);
    }

    void incrementJumps() {
        if (DynamicState::get().simulating) return;
        PlayerObject::incrementJumps();
    }

    void ringJump(RingObject* ring, bool skipCheck) {
        if (DynamicState::get().simulating && !dyn::cfg().simOrbs) return;
        PlayerObject::ringJump(ring, skipCheck);
    }

    // Efectos visuales que el fantasma NO debe mostrar.
    void spawnCircle() {
        if (DynamicState::get().simulating) return;
        PlayerObject::spawnCircle();
    }

    void spawnDualCircle() {
        if (DynamicState::get().simulating) return;
        PlayerObject::spawnDualCircle();
    }

    void spawnPortalCircle(ccColor3B color, float startRadius) {
        if (DynamicState::get().simulating) return;
        PlayerObject::spawnPortalCircle(color, startRadius);
    }

    void playBumpEffect(int objectType, GameObject* object) {
        if (DynamicState::get().simulating) return;
        PlayerObject::playBumpEffect(objectType, object);
    }
};

class $modify(DynGuardRing, RingObject) {
    void spawnCircle() {
        if (DynamicState::get().simulating) return;
        RingObject::spawnCircle();
    }

    void powerOnObject(int state) {
        if (DynamicState::get().simulating) return;
        RingObject::powerOnObject(state);
    }
};

class $modify(DynGuardStreak, HardStreak) {
    void addPoint(CCPoint point) {
        if (DynamicState::get().simulating) return;
        HardStreak::addPoint(point);
    }
};

class $modify(DynGuardObject, GameObject) {
    void playShineEffect() {
        if (DynamicState::get().simulating) return;
        GameObject::playShineEffect();
    }
};

class $modify(DynGuardEffect, EffectGameObject) {
    void triggerObject(GJBaseGameLayer* layer, int uniqueID, gd::vector<int> const* remapKeys) {
        if (DynamicState::get().simulating) return;
        EffectGameObject::triggerObject(layer, uniqueID, remapKeys);
    }
};

// Si el fantasma "muere", solo lo anotamos: la partida real no se entera.
class $modify(DynGuardPlay, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto& g = DynamicState::get();
        if (g.simulating) {
            g.simDead = true;
            return;
        }
        g.queue.clear();  // un input retenido no debe sobrevivir a una muerte real
        PlayLayer::destroyPlayer(player, object);
    }
};
