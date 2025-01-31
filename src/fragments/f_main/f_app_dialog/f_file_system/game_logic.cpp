#include "fragments/f_main/f_app_dialog/f_file_system/game_logic.hpp"
#include "raycast/entity_manager.hpp"
#include "raycast/spatial_system.hpp"
#include "raycast/event_handler_system.hpp"
#include "raycast/damage_system.hpp"
#include "raycast/inventory_system.hpp"
#include "raycast/behaviour_system.hpp"
#include "raycast/focus_system.hpp"
#include "raycast/agent_system.hpp"
#include "raycast/c_switch_behaviour.hpp"
#include "raycast/audio_service.hpp"
#include "raycast/time_service.hpp"
#include "event_system.hpp"
#include "state_ids.hpp"
#include "utils.hpp"
#include "app_config.hpp"


using std::string;


namespace going_in_circles {


static const double TIME_LIMIT = 5.2;


//===========================================
// GameLogic::GameLogic
//===========================================
GameLogic::GameLogic(EventSystem& eventSystem, AudioService& audioService, TimeService& timeService,
  EntityManager& entityManager)
  : SystemAccessor(entityManager),
    m_eventSystem(eventSystem),
    m_audioService(audioService),
    m_timeService(timeService) {

  DBG_PRINT("GameLogic::GameLogic\n");

  m_entityId = Component::getNextId();

  Player& player = *spatialSys().sg.player;
  player.invincible = true;

  CEventHandler* events = new CEventHandler(m_entityId);
  events->broadcastedEventHandlers.push_back(EventHandler{"entity_destroyed",
    std::bind(&GameLogic::onEntityDestroyed, this, std::placeholders::_1)});
  events->broadcastedEventHandlers.push_back(EventHandler{"entity_changed_zone",
    std::bind(&GameLogic::onEntityChangeZone, this, std::placeholders::_1)});
  eventHandlerSys().addComponent(pComponent_t(events));
  events->broadcastedEventHandlers.push_back(EventHandler{"switch_activated",
    std::bind(&GameLogic::onSwitchActivated, this, std::placeholders::_1)});

  setupLarry();
  resetSwitches();
}

//===========================================
// GameLogic::setupLarry
//===========================================
void GameLogic::setupLarry() {
  entityId_t larryId = Component::getIdFromString("larry");
  CFocus& focus = dynamic_cast<CFocus&>(focusSys().getComponent(larryId));

  focus.captionText = "\"Life is pleasant here\"";

  auto& larryEvents = dynamic_cast<CEventHandler&>(eventHandlerSys().getComponent(larryId));
  larryEvents.targetedEventHandlers.push_back(EventHandler{"player_activate_entity",
    [=](const GameEvent&) {

    focusSys().showCaption(larryId);
  }});

  entityId_t switchId = Component::getIdFromString("exit_switch");
  CEventHandler* switchEvents = nullptr;

  if (eventHandlerSys().hasComponent(switchId)) {
    switchEvents = &dynamic_cast<CEventHandler&>(eventHandlerSys().getComponent(switchId));
  }
  else {
    switchEvents = new CEventHandler(switchId);
    eventHandlerSys().addComponent(pComponent_t(switchEvents));
  }

  switchEvents->targetedEventHandlers.push_back(EventHandler{"player_activate_entity",
    [=](const GameEvent&) {

    entityId_t navPointId = Component::getIdFromString("exit_nav_point");
    auto& vRect = dynamic_cast<CVRect&>(spatialSys().getComponent(navPointId));

    agentSys().navigateTo(larryId, vRect.pos);
  }});
}

//===========================================
// GameLogic::getSwitch
//===========================================
CSwitchBehaviour& GameLogic::getSwitch(entityId_t id) const {
  return dynamic_cast<CSwitchBehaviour&>(behaviourSys().getComponent(id));
}

//===========================================
// GameLogic::resetSwitches
//===========================================
void GameLogic::resetSwitches() {
  static const entityId_t switch0Id = Component::getIdFromString("switch0");
  static const entityId_t switch1Id = Component::getIdFromString("switch1");
  static const entityId_t switch2Id = Component::getIdFromString("switch2");
  static const entityId_t switch3Id = Component::getIdFromString("switch3");

  auto& switch0 = getSwitch(switch0Id);
  auto& switch1 = getSwitch(switch1Id);
  auto& switch2 = getSwitch(switch2Id);
  auto& switch3 = getSwitch(switch3Id);

  switch0.setState(SwitchState::OFF);
  switch1.setState(SwitchState::OFF);
  switch2.setState(SwitchState::OFF);
  switch3.setState(SwitchState::OFF);

  switch0.disabled = false;
  switch1.disabled = true;
  switch2.disabled = true;
  switch3.disabled = true;
}

//===========================================
// GameLogic::onSwitchActivated
//===========================================
void GameLogic::onSwitchActivated(const GameEvent& e_) {
  auto& e = dynamic_cast<const ESwitchActivate&>(e_);

  static const entityId_t switch0Id = Component::getIdFromString("switch0");
  static const entityId_t switch1Id = Component::getIdFromString("switch1");
  static const entityId_t switch2Id = Component::getIdFromString("switch2");
  static const entityId_t switch3Id = Component::getIdFromString("switch3");

  auto& switch1 = getSwitch(switch1Id);
  auto& switch2 = getSwitch(switch2Id);
  auto& switch3 = getSwitch(switch3Id);

  if (e.switchEntityId == switch0Id) {
    switch1.disabled = false;
    m_switchSoundId = m_audioService.playSound("tick");

    m_timeService.onTimeout([this]() {
      resetSwitches();
      m_audioService.stopSound("tick", m_switchSoundId);
    }, TIME_LIMIT);
  }
  else if (e.switchEntityId == switch1Id) {
    switch2.disabled = false;
  }
  else if (e.switchEntityId == switch2Id) {
    switch3.disabled = false;
  }
  else if (e.switchEntityId == switch3Id) {
    m_audioService.stopSound("tick", m_switchSoundId);
  }
}

//===========================================
// GameLogic::onEntityDestroyed
//===========================================
void GameLogic::onEntityDestroyed(const GameEvent& event) {
  auto& e = dynamic_cast<const EEntityDestroyed&>(event);

  Player& player = *spatialSys().sg.player;

  if (e.entityId == player.body) {
    damageSys().damageEntity(player.body, -10);

    int ammo = inventorySys().getBucketValue(player.body, "ammo");
    inventorySys().subtractFromBucket(player.body, "ammo", ammo);

    entityId_t spawnPointId = Component::getIdFromString("spawn_point");
    CVRect& spawnPoint = dynamic_cast<CVRect&>(spatialSys().getComponent(spawnPointId));

    spatialSys().relocateEntity(player.body, *spawnPoint.zone, spawnPoint.pos);
    player.setFeetHeight(0);

    m_audioService.playMusic("birds", true);
  }
}

//===========================================
// GameLogic::onEntityChangeZone
//===========================================
void GameLogic::onEntityChangeZone(const GameEvent& event) {
  const EChangedZone& e = dynamic_cast<const EChangedZone&>(event);

  Player& player = *spatialSys().sg.player;

  if (e.entityId == player.body) {
    if (e.newZone == Component::getIdFromString("level_exit")) {
      m_eventSystem.fire(pEvent_t(new RequestStateChangeEvent(ST_DOOMSWEEPER)));
    }
  }
}

//===========================================
// GameLogic::~GameLogic
//===========================================
GameLogic::~GameLogic() {

}


}
