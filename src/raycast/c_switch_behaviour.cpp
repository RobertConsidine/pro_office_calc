#include <set>
#include "raycast/c_switch_behaviour.hpp"
#include "raycast/render_system.hpp"
#include "raycast/entity_manager.hpp"
#include "raycast/spatial_system.hpp"
#include "raycast/inventory_system.hpp"
#include "event.hpp"


using std::string;
using std::set;
using std::map;


//===========================================
// CSwitchBehaviour::CSwitchBehaviour
//===========================================
CSwitchBehaviour::CSwitchBehaviour(entityId_t entityId, EntityManager& entityManager,
  entityId_t target, const string& message, SwitchState initialState, bool toggleable,
  double toggleDelay)
  : CBehaviour(entityId),
    m_entityManager(entityManager),
    m_target(target),
    m_message(message),
    m_state(initialState),
    m_toggleable(toggleable),
    m_timer(toggleDelay) {

  setDecal();
}

//===========================================
// CSwitchBehaviour::update
//===========================================
void CSwitchBehaviour::update() {}

//===========================================
// CSwitchBehaviour::setDecal
//===========================================
void CSwitchBehaviour::setDecal() {
  QRectF texRect;

  switch (m_state) {
    case SwitchState::OFF:
      texRect = QRectF(0, 0, 0.5, 1);
      break;
    case SwitchState::ON:
      texRect = QRectF(0.5, 0, 0.5, 1);
      break;
  }

  CWallDecal* decal = getDecal();
  if (decal != nullptr) {
    decal->texRect = texRect;
  }
}

//===========================================
// CSwitchBehaviour::handleTargetedEvent
//===========================================
void CSwitchBehaviour::handleTargetedEvent(const GameEvent& e_) {
  if (e_.name == "player_activate_entity") {
    const EPlayerActivateEntity& e = dynamic_cast<const EPlayerActivateEntity&>(e_);

    if (m_toggleable || m_state == SwitchState::OFF) {
      if (m_timer.ready()) {
        const auto& inventorySystem = m_entityManager
          .system<InventorySystem>(ComponentKind::C_INVENTORY);

        if (requiredItemType != "") {
          const map<string, entityId_t>& items =
            inventorySystem.getBucketItems(e.player.body, requiredItemType);

          if (!contains(items, requiredItemName)) {
            return;
          }
        }

        switch (m_state) {
          case SwitchState::OFF:
            m_state = SwitchState::ON;
            break;
          case SwitchState::ON:
            m_state = SwitchState::OFF;
            break;
        }

        setDecal();

        ESwitchActivate eActivate(entityId(), m_state, m_message);
        m_entityManager.fireEvent(eActivate, {m_target});
        m_entityManager.broadcastEvent(eActivate);
      }
    }
  }
}

//===========================================
// CSwitchBehaviour::getDecal
//===========================================
CWallDecal* CSwitchBehaviour::getDecal() const {
  RenderSystem& renderSystem = m_entityManager.system<RenderSystem>(ComponentKind::C_RENDER);

  if (renderSystem.hasComponent(entityId())) {
    return dynamic_cast<CWallDecal*>(&renderSystem.getComponent(entityId()));
  }

  return nullptr;
}
