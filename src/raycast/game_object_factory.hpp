#ifndef __PROCALC_RAYCAST_GAME_OBJECT_FACTORY_HPP__
#define __PROCALC_RAYCAST_GAME_OBJECT_FACTORY_HPP__


#include <string>
#include <memory>
#include <set>
#include "raycast/component.hpp"


class Matrix;
namespace parser { struct Object; }

class GameObjectFactory {
  public:
    virtual bool constructObject(const std::string& type, entityId_t entityId, parser::Object& obj,
      entityId_t parentId, const Matrix& parentTransform) = 0;

    virtual const std::set<std::string>& types() const = 0;

    virtual ~GameObjectFactory() {}
};

typedef std::unique_ptr<GameObjectFactory> pGameObjectFactory_t;


#endif
