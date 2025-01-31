#ifndef __PROCALC_RAYCAST_SPATIAL_COMPONENTS_HPP__
#define __PROCALC_RAYCAST_SPATIAL_COMPONENTS_HPP__


#include <string>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <QImage>
#include "raycast/geometry.hpp"
#include "raycast/component.hpp"


enum class CSpatialKind {
  ZONE,
  HARD_EDGE,
  SOFT_EDGE,
  V_RECT,
  H_RECT,
  PATH
};

class CSpatial : public Component {
  public:
    CSpatial(CSpatialKind kind, entityId_t entityId, entityId_t parentId)
      : Component(entityId, ComponentKind::C_SPATIAL),
        kind(kind),
        parentId(parentId) {}

    CSpatialKind kind;
    entityId_t parentId;
};

typedef std::unique_ptr<CSpatial> pCSpatial_t;

class CPath : public CSpatial {
  public:
    CPath(entityId_t entityId, entityId_t parentId)
      : CSpatial(CSpatialKind::PATH, entityId, parentId) {}

    std::vector<Point> points;
};

typedef std::unique_ptr<CPath> pCPath_t;

class CZone;

class CVRect : public CSpatial {
  public:
    CVRect(entityId_t entityId, entityId_t parentId, const Size& size)
      : CSpatial(CSpatialKind::V_RECT, entityId, parentId),
        size(size) {}

    void setTransform(const Matrix& m) {
      pos.x = m.tx();
      pos.y = m.ty();
      angle = m.a();
    }

    CZone* zone = nullptr;
    Vec2f pos;
    double angle;
    Size size;
    // Height above floor
    double y = 0;
};

typedef std::unique_ptr<CVRect> pCVRect_t;

class CEdge : public CSpatial {
  public:
    CEdge(CSpatialKind kind, entityId_t entityId, entityId_t parentId)
      : CSpatial(kind, entityId, parentId) {}

    LineSegment lseg;
    std::list<pCVRect_t> vRects;
};

typedef std::unique_ptr<CEdge> pCEdge_t;

class CHRect : public CSpatial {
  public:
    CHRect(entityId_t entityId, entityId_t parentId)
      : CSpatial(CSpatialKind::H_RECT, entityId, parentId) {}

    Size size;
    Matrix transform;
};

typedef std::unique_ptr<CHRect> pCHRect_t;

typedef std::unique_ptr<CZone> pCZone_t;

class CZone : public CSpatial {
  public:
    CZone(entityId_t entityId, entityId_t parentId)
      : CSpatial(CSpatialKind::ZONE, entityId, parentId) {}

    double floorHeight = 0;
    double ceilingHeight = 100;
    bool hasCeiling = true;
    std::vector<pCZone_t> children;
    std::vector<pCEdge_t> edges;
    std::list<pCVRect_t> vRects;
    std::vector<pCHRect_t> hRects;
    std::vector<pCPath_t> paths;
    CZone* parent = nullptr;
};

class CHardEdge : public CEdge {
  public:
    CHardEdge(entityId_t entityId, entityId_t parentId)
      : CEdge(CSpatialKind::HARD_EDGE, entityId, parentId) {}

    CZone* zone = nullptr;

    double height() const {
      return zone->ceilingHeight - zone->floorHeight;
    }
};

class CSoftEdge : public CEdge {
  public:
    CSoftEdge(entityId_t entityId, entityId_t parentId, entityId_t joinId)
      : CEdge(CSpatialKind::SOFT_EDGE, entityId, parentId),
        joinId(joinId) {}

    entityId_t joinId = -1;
    entityId_t twinId = -1;

    bool isPortal = false;
    Matrix toTwin;

    CZone* zoneA = nullptr;
    CZone* zoneB = nullptr;
};


#endif
