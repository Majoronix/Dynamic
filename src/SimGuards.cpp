#include "Dynamic.hpp"

#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

// ---------------------------------------------------------------------------
// "Protecciones": mientras corre la simulación, el jugador fantasma NO debe
// tener ningún efecto sobre el nivel real (triggers, monedas, orbes, sonidos,
// partículas...). Estos hooks bloquean esos efectos SOLO durante la simulación.
// Fuera de la simulación no hacen nada (llaman a la función original).
// ---------------------------------------------------------------------------

namespace {
    // Solo estos tipos pueden matar o bloquear al fantasma.
    bool isHard(GameObject* obj) {
        switch (obj->m_objectType) {
            case GameObjectType::Solid:
            case GameObjectType::Hazard:
            case GameObjectType::AnimatedHazard:
            case GameObjectType::Slope:
                return true;
            default:
                return false;
        }
    }
}

class $modify(DynGuardBGL, GJBaseGameLayer) {
    void collisionCheckObjects(PlayerObject* p0, gd::vector<GameObject*>* objects, int count, float dt) {
        auto& g = DynamicState::get();
        if (!g.simulating) return GJBaseGameLayer::collisionCheckObjects(p0, objects, count, dt);

        // Desactivamos un instante todo lo que no sea sólido/pincho/rampa.
        std::vector<GameObject*> disabled;
        size_t n = std::min<size_t>(static_cast<size_t>(std::max(count, 0)), objects->size());
        for (size_t i = 0; i < n; ++i) {
            auto obj = (*objects)[i];
            if (!obj || isHard(obj)) continue;
            if (obj->m_isDisabled || obj->m_isDisabled2) continue;
            disabled.push_back(obj);
            obj->m_isDisabled = true;
            obj->m_isDisabled2 = true;
        }

        GJBaseGameLayer::collisionCheckObjects(p0, objects, count, dt);

        for (auto obj : disabled) {
            obj->m_isDisabled = false;
            obj->m_isDisabled2 = false;
        }
    }

    bool canBeActivatedByPlayer(PlayerObject* player, EffectGameObject* object) {
        if (DynamicState::get().simulating) return false;
        return GJBaseGameLayer::canBeActivatedByPlayer(player, object);
    }

    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        if (DynamicState::get().simulating) return;
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }

    void playerTouchedTrigger(PlayerObject* player, EffectGameObject* trigger) {
        if (DynamicState::get().simulating) return;
        GJBaseGameLayer::playerTouchedTrigger(player, trigger);
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
        if (DynamicState::get().simulating) return;
        PlayerObject::ringJump(ring, skipCheck);
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
