#include <cmath>
#include <cassert>
#include <limits>
#include <list>
#include <set>
#include <vector>
#include <ostream>
#include <functional>
#include <QPainter>
#include <QPaintDevice>
#include <QBrush>
#include "fragments/f_main/f_menu_bar/f_settings_dialog/f_raycast/renderer.hpp"
#include "fragments/f_main/f_menu_bar/f_settings_dialog/f_raycast/spatial_system.hpp"
#include "fragments/f_main/f_menu_bar/f_settings_dialog/f_raycast/render_system.hpp"
#include "fragments/f_main/f_menu_bar/f_settings_dialog/f_raycast/entity_manager.hpp"
#include "exception.hpp"


using std::string;
using std::list;
using std::set;
using std::vector;
using std::array;
using std::unique_ptr;
using std::function;


static const double ATAN_MIN = -10.0;
static const double ATAN_MAX = 10.0;


struct ScreenSlice {
  int sliceBottom_px;
  int sliceTop_px;
  int viewportBottom_px;
  int viewportTop_px;
};

enum class XWrapperKind {
  JOIN,
  WALL,
  SPRITE
};

struct XWrapper {
  XWrapper(XWrapperKind kind)
    : kind(kind) {}

  XWrapperKind kind;

  virtual ~XWrapper() {}
};

typedef std::unique_ptr<XWrapper> pXWrapper_t;

struct CastResult {
  std::list<pXWrapper_t> intersections;
};

struct Slice {
  double sliceBottom_wd;
  double sliceTop_wd;
  double projSliceBottom_wd;
  double projSliceTop_wd;
  double viewportBottom_wd;
  double viewportTop_wd;
};

struct JoinX : public XWrapper {
  JoinX(pIntersection_t X)
    : XWrapper(XWrapperKind::JOIN),
      X(std::move(X)) {}

  pIntersection_t X;
  const CSoftEdge* softEdge = nullptr;
  const CJoin* join = nullptr;
  Slice slice0;
  Slice slice1;
  const CZone* nearZone;
  const CZone* farZone;

  virtual ~JoinX() override {}
};

struct WallX : public XWrapper {
  WallX(pIntersection_t X)
    : XWrapper(XWrapperKind::WALL),
      X(std::move(X)) {}

  pIntersection_t X;
  const CHardEdge* hardEdge = nullptr;
  const CWall* wall = nullptr;
  Slice slice;

  virtual ~WallX() override {}
};

struct SpriteX : public XWrapper {
  SpriteX(pIntersection_t X)
    : XWrapper(XWrapperKind::SPRITE),
      X(std::move(X)) {}

  pIntersection_t X;
  const CVRect* vRect = nullptr;
  const CSprite* sprite = nullptr;
  Slice slice;

  virtual ~SpriteX() override {}
};

static inline CZone& getZone(const SpatialSystem& spatialSystem, const CRegion& r) {
  return dynamic_cast<CZone&>(spatialSystem.getComponent(r.entityId()));
}

static inline CEdge& getEdge(const SpatialSystem& spatialSystem, const CBoundary& b) {
  return dynamic_cast<CEdge&>(spatialSystem.getComponent(b.entityId()));
}

static inline CVRect& getVRect(const SpatialSystem& spatialSystem, const CSprite& s) {
  return dynamic_cast<CVRect&>(spatialSystem.getComponent(s.entityId()));
}

static inline CVRect& getVRect(const SpatialSystem& spatialSystem, const CWallDecal& d) {
  return dynamic_cast<CVRect&>(spatialSystem.getComponent(d.entityId()));
}

static inline CHRect& getHRect(const SpatialSystem& spatialSystem, const CFloorDecal& d) {
  return dynamic_cast<CHRect&>(spatialSystem.getComponent(d.entityId()));
}

//===========================================
// forEachConstCRegion
//===========================================
void forEachConstCRegion(const CRegion& region, function<void(const CRegion&)> fn) {
  fn(region);
  std::for_each(region.children.begin(), region.children.end(), [&](const unique_ptr<CRegion>& r) {
    forEachConstCRegion(*r, fn);
  });
}

//===========================================
// forEachCRegion
//===========================================
void forEachCRegion(CRegion& region, function<void(CRegion&)> fn) {
  fn(region);
  std::for_each(region.children.begin(), region.children.end(), [&](unique_ptr<CRegion>& r) {
    forEachCRegion(*r, fn);
  });
}

//===========================================
// fastTan_rp
//
// Retrieves the reciprocal of tan(a) from the lookup table
//===========================================
static double fastTan_rp(const tanMap_t& tanMap_rp, double a) {
  static const double x = static_cast<double>(tanMap_rp.size()) / (2.0 * PI);
  return tanMap_rp[static_cast<int>(normaliseAngle(a) * x)];
}

//===========================================
// fastATan
//
// Retrieves atan(x) from the lookup table
//===========================================
static double fastATan(const atanMap_t& atanMap, double x) {
  if (x < ATAN_MIN) {
    x = ATAN_MIN;
  }
  if (x > ATAN_MAX) {
    x = ATAN_MAX;
  }
  double dx = (ATAN_MAX - ATAN_MIN) / static_cast<double>(atanMap.size());
  return atanMap[static_cast<int>((x - ATAN_MIN) / dx)];
}

//===========================================
// worldPointToFloorTexel
//===========================================
static Point worldPointToFloorTexel(const Point& p, const Size& texSz_wd_rp, const Size& texSz_px) {
  double nx = p.x * texSz_wd_rp.x;
  double ny = p.y * texSz_wd_rp.y;

  if (nx < 0 || std::isinf(nx) || std::isnan(nx)) {
    nx = 0;
  }
  if (ny < 0 || std::isinf(ny) || std::isnan(ny)) {
    ny = 0;
  }

  return Point((nx - floor(nx)) * texSz_px.x, (ny - floor(ny)) * texSz_px.y);
}

//===========================================
// constructXWrapper
//===========================================
static XWrapper* constructXWrapper(const SpatialSystem& spatialSystem,
  const RenderSystem& renderSystem, pIntersection_t X) {

  Intersection* pX = X.get();

  switch (pX->kind) {
    case CSpatialKind::HARD_EDGE: {
      WallX* wrapper = new WallX(std::move(X));
      wrapper->hardEdge = dynamic_cast<const CHardEdge*>(&spatialSystem.getComponent(pX->entityId));
      wrapper->wall = dynamic_cast<const CWall*>(&renderSystem.getComponent(pX->entityId));
      return wrapper;
    }
    case CSpatialKind::SOFT_EDGE: {
      JoinX* wrapper = new JoinX(std::move(X));
      wrapper->softEdge = dynamic_cast<const CSoftEdge*>(&spatialSystem.getComponent(pX->entityId));
      wrapper->join = dynamic_cast<const CJoin*>(&renderSystem.getComponent(pX->entityId));
      return wrapper;
    }
    case CSpatialKind::V_RECT: {
      SpriteX* wrapper = new SpriteX(std::move(X));
      wrapper->vRect = dynamic_cast<const CVRect*>(&spatialSystem.getComponent(pX->entityId));
      wrapper->sprite = dynamic_cast<const CSprite*>(&renderSystem.getComponent(pX->entityId));
      return wrapper;
    }
    default:
      EXCEPTION("Error constructing XWrapper for Intersection of unknown type");
  }

  return nullptr;
}

//===========================================
// castRay
//===========================================
static void castRay(const SpatialSystem& spatialSystem, const RenderSystem& renderSystem,
  double camSpaceAngle, const RenderGraph& rg, const Player& player, CastResult& result) {

  const Camera& cam = player.camera();

  list<pIntersection_t> intersections = spatialSystem.entitiesAlongRay(camSpaceAngle);

  intersections.sort([](const pIntersection_t& a, const pIntersection_t& b) {
    return a->distanceFromCamera < b->distanceFromCamera;
  });

  LineSegment projPlane(Point(cam.F, -rg.viewport.y / 2), Point(cam.F, rg.viewport.y * 0.5));

  Matrix m(cam.vAngle, Vec2f(0, 0));
  LineSegment rotProjPlane = transform(projPlane, m);

  LineSegment projRay0(Point(0, 0), rotProjPlane.A * 999.9);
  LineSegment projRay1(Point(0, 0), rotProjPlane.B * 999.9);
  double subview0 = 0;
  double subview1 = rg.viewport.y;

  const CZone* zone = &dynamic_cast<const CZone&>(spatialSystem.getComponent(player.currentRegion));
  int last = -1;
  for (auto it = intersections.begin(); it != intersections.end(); ++it) {
    ++last;
    XWrapper* X = constructXWrapper(spatialSystem, renderSystem, std::move(*it));
    result.intersections.push_back(pXWrapper_t(X));

    if (X->kind == XWrapperKind::WALL) {
      WallX& wallX = dynamic_cast<WallX&>(*X);

      CZone& z = *wallX.hardEdge->zone;

      double floorHeight = z.floorHeight;
      double targetHeight = z.ceilingHeight - z.floorHeight;
      const Point& pt = wallX.X->point_cam;

      LineSegment wall(Point(pt.x, floorHeight - cam.height),
        Point(pt.x, floorHeight - cam.height + targetHeight));

      LineSegment wallRay0(Point(0, 0), Point(pt.x, wall.A.y));
      LineSegment wallRay1(Point(0, 0), Point(pt.x, wall.B.y));

      Point p = lineIntersect(wallRay0.line(), rotProjPlane.line());
      double proj_w0 = rotProjPlane.signedDistance(p.x);
      p = lineIntersect(wallRay1.line(), rotProjPlane.line());
      double proj_w1 = rotProjPlane.signedDistance(p.x);

      double wall_s0 = lineIntersect(projRay0.line(), wall.line()).y;
      double wall_s1 = lineIntersect(projRay1.line(), wall.line()).y;

      auto wallAClip = clipNumber(wall.A.y, Size(wall_s0, wall_s1), wallX.slice.sliceBottom_wd);
      auto wallBClip = clipNumber(wall.B.y, Size(wall_s0, wall_s1), wallX.slice.sliceTop_wd);

      if (wallAClip == CLIPPED_TO_BOTTOM) {
        proj_w0 = subview0;
      }
      if (wallAClip == CLIPPED_TO_TOP) {
        proj_w0 = subview1;
      }
      if (wallBClip == CLIPPED_TO_BOTTOM) {
        proj_w1 = subview0;
      }
      if (wallBClip == CLIPPED_TO_TOP) {
        proj_w1 = subview1;
      }

      wallX.slice.projSliceBottom_wd = proj_w0;
      wallX.slice.projSliceTop_wd = proj_w1;

      wallX.slice.viewportBottom_wd = subview0;
      wallX.slice.viewportTop_wd = subview1;

      break;
    }
    else if (X->kind == XWrapperKind::JOIN) {
      JoinX& joinX = dynamic_cast<JoinX&>(*X);

      // TODO: Fix
      //assert(zone == joinX.softEdge->zoneA || zone == joinX.softEdge->zoneB);

      CZone* nextZone = zone == joinX.softEdge->zoneA ? joinX.softEdge->zoneB
        : joinX.softEdge->zoneA;

      joinX.nearZone = zone;
      joinX.farZone = nextZone;

      const Point& pt = joinX.X->point_cam;

      double floorDiff = nextZone->floorHeight - zone->floorHeight;
      double ceilingDiff = zone->ceilingHeight - nextZone->ceilingHeight;
      double nextZoneSpan = nextZone->ceilingHeight - nextZone->floorHeight;

      double bottomWallA = zone->floorHeight - cam.height;
      double bottomWallB = bottomWallA + floorDiff;
      double topWallA = bottomWallB + nextZoneSpan;
      double topWallB = topWallA + ceilingDiff;

      if (floorDiff < 0) {
        bottomWallB = bottomWallA;
      }
      if (ceilingDiff < 0) {
        topWallA = topWallB;
      }

      LineSegment bottomWall(Point(pt.x, bottomWallA), Point(pt.x, bottomWallB));
      LineSegment topWall(Point(pt.x, topWallA), Point(pt.x, topWallB));

      LineSegment bottomWallRay0(Point(0, 0), bottomWall.A);
      LineSegment bottomWallRay1(Point(0, 0), bottomWall.B);

      LineSegment topWallRay0(Point(0, 0), topWall.A);
      LineSegment topWallRay1(Point(0, 0), topWall.B);

      LineSegment wall(Point(pt.x, bottomWallA), Point(pt.x, topWallB));
      double wall_s0 = lineIntersect(projRay0.line(), wall.line()).y;
      double wall_s1 = lineIntersect(projRay1.line(), wall.line()).y;

      auto bWallAClip = clipNumber(bottomWall.A.y, Size(wall_s0, wall_s1),
        joinX.slice0.sliceBottom_wd);
      auto bWallBClip = clipNumber(bottomWall.B.y, Size(wall_s0, wall_s1),
        joinX.slice0.sliceTop_wd);

      auto tWallAClip = clipNumber(topWall.A.y, Size(wall_s0, wall_s1),
        joinX.slice1.sliceBottom_wd);
      auto tWallBClip = clipNumber(topWall.B.y, Size(wall_s0, wall_s1), joinX.slice1.sliceTop_wd);

      Point p = lineIntersect(bottomWallRay0.line(), rotProjPlane.line());
      double proj_bw0 = rotProjPlane.signedDistance(p.x);
      p = lineIntersect(bottomWallRay1.line(), rotProjPlane.line());
      Point vw0 = p = clipToLineSegment(p, rotProjPlane);
      double proj_bw1 = rotProjPlane.signedDistance(p.x);
      p = lineIntersect(topWallRay0.line(), rotProjPlane.line());
      Point vw1 = p = clipToLineSegment(p, rotProjPlane);
      double proj_tw0 = rotProjPlane.signedDistance(p.x);
      p = lineIntersect(topWallRay1.line(), rotProjPlane.line());
      double proj_tw1 = rotProjPlane.signedDistance(p.x);

      if (bWallAClip == CLIPPED_TO_BOTTOM) {
        proj_bw0 = subview0;
      }
      if (bWallAClip == CLIPPED_TO_TOP) {
        proj_bw0 = subview1;
      }
      if (bWallBClip == CLIPPED_TO_BOTTOM) {
        proj_bw1 = subview0;
        vw0 = rotProjPlane.A;
      }
      if (bWallBClip == CLIPPED_TO_TOP) {
        proj_bw1 = subview1;
        vw0 = rotProjPlane.B;
      }
      if (tWallAClip == CLIPPED_TO_BOTTOM) {
        proj_tw0 = subview0;
        vw1 = rotProjPlane.A;
      }
      if (tWallAClip == CLIPPED_TO_TOP) {
        proj_tw0 = subview1;
        vw1 = rotProjPlane.B;
      }
      if (tWallBClip == CLIPPED_TO_BOTTOM) {
        proj_tw1 = subview0;
      }
      if (tWallBClip == CLIPPED_TO_TOP) {
        proj_tw1 = subview1;
      }

      joinX.slice0.projSliceBottom_wd = proj_bw0;
      joinX.slice0.projSliceTop_wd = proj_bw1;
      joinX.slice1.projSliceBottom_wd = proj_tw0;
      joinX.slice1.projSliceTop_wd = proj_tw1;

      joinX.slice0.viewportBottom_wd = subview0;
      joinX.slice0.viewportTop_wd = subview1;
      joinX.slice1.viewportBottom_wd = subview0;
      joinX.slice1.viewportTop_wd = subview1;

      if (proj_bw1 > subview0) {
        subview0 = proj_bw1;
        projRay0 = LineSegment(Point(0, 0), vw0 * 999.9);
      }

      if (proj_tw0 < subview1) {
        subview1 = proj_tw0;
        projRay1 = LineSegment(Point(0, 0), vw1 * 999.9);
      }

      zone = nextZone;
    }
    else if (X->kind == XWrapperKind::SPRITE) {
      SpriteX& spriteX = dynamic_cast<SpriteX&>(*X);

      double floorHeight = zone->floorHeight;
      const Point& pt = spriteX.X->point_cam;

      LineSegment wall(Point(pt.x, floorHeight - cam.height),
        Point(pt.x, floorHeight - cam.height + spriteX.vRect->size.y));

      LineSegment wallRay0(Point(0, 0), Point(pt.x, wall.A.y));
      LineSegment wallRay1(Point(0, 0), Point(pt.x, wall.B.y));

      Point p = lineIntersect(wallRay0.line(), rotProjPlane.line());
      double proj_w0 = rotProjPlane.signedDistance(p.x);
      p = lineIntersect(wallRay1.line(), rotProjPlane.line());
      double proj_w1 = rotProjPlane.signedDistance(p.x);

      spriteX.slice.viewportBottom_wd = subview0;
      spriteX.slice.viewportTop_wd = subview1;

      spriteX.slice.projSliceBottom_wd = clipNumber(proj_w0, Size(subview0, subview1));
      spriteX.slice.projSliceTop_wd = clipNumber(proj_w1, Size(subview0, subview1));

      double wall_s0 = lineIntersect(projRay0.line(), wall.line()).y;
      double wall_s1 = lineIntersect(projRay1.line(), wall.line()).y;

      spriteX.slice.sliceBottom_wd = clipNumber(wall.A.y, Size(wall_s0, wall_s1));
      spriteX.slice.sliceTop_wd = clipNumber(wall.B.y, Size(wall_s0, wall_s1));
    }

    if (subview1 <= subview0) {
      break;
    }
  }

  if (intersections.size() > 0) {
    intersections.resize(last + 1);
  }
}

//===========================================
// drawSkySlice
//===========================================
static void drawSkySlice(QImage& target, const RenderGraph& rg, const Player& player,
  const ScreenSlice& slice, int screenX_px) {

  const Camera& cam = player.camera();

  const Texture& skyTex = rg.textures.at("sky");
  Size texSz_px(skyTex.image.rect().width(), skyTex.image.rect().height());

  int W_px = target.rect().width();
  int H_px = target.rect().height();

  double hPxAngle = cam.hFov / W_px;
  double vPxAngle = cam.vFov / H_px;

  double hAngle = normaliseAngle(cam.angle - 0.5 * cam.hFov + hPxAngle * screenX_px);
  double s = hAngle / (2.0 * PI);
  assert(isBetween(s, 0.0, 1.0));

  int x = texSz_px.x * s;

  double maxPitch = DEG_TO_RAD(20.0); // TODO
  double minVAngle = -0.5 * cam.vFov - maxPitch;
  double maxVAngle = 0.5 * cam.vFov + maxPitch;
  double vAngleRange = maxVAngle - minVAngle;
  double viewportBottomAngle = cam.vAngle - 0.5 * cam.vFov;

  for (int j = slice.viewportTop_px; j <= slice.sliceTop_px; ++j) {
    double vAngle = viewportBottomAngle + (H_px - j) * vPxAngle;
    double s = 1.0 - normaliseAngle(vAngle - minVAngle) / vAngleRange;
    assert(isBetween(s, 0.0, 1.0));

    QRgb* pixels = reinterpret_cast<QRgb*>(target.scanLine(j));
    pixels[screenX_px] = skyTex.image.pixel(x, s * texSz_px.y);
  }
}

//===========================================
// drawCeilingSlice
//===========================================
static void drawCeilingSlice(QImage& target, const RenderGraph& rg, const Player& player,
  const CRegion* region, double ceilingHeight, const Point& collisionPoint,
  const ScreenSlice& slice, int screenX_px, double projX_wd, double vWorldUnit_px,
  const tanMap_t& tanMap_rp, const atanMap_t& atanMap) {

  double screenH_px = rg.viewport.y * vWorldUnit_px;
  const Camera& cam = player.camera();
  const Texture& ceilingTex = rg.textures.at(region->ceilingTexture);

  double hAngle = atan(projX_wd / cam.F);
  LineSegment ray(cam.pos, collisionPoint);

  Size texSz_px(ceilingTex.image.rect().width(), ceilingTex.image.rect().height());
  Size texSz_wd_rp(1.0 / ceilingTex.size_wd.x, 1.0 / ceilingTex.size_wd.y);

  double vWorldUnit_px_rp = 1.0 / vWorldUnit_px;
  double F_rp = 1.0 / cam.F;
  double rayLen_rp = 1.0 / ray.length();
  double cosHAngle_rp = 1.0 / cos(hAngle);

  for (int j = slice.sliceTop_px; j >= slice.viewportTop_px; --j) {
    double projY_wd = (screenH_px * 0.5 - j) * vWorldUnit_px_rp;
    double vAngle = fastATan(atanMap, projY_wd * F_rp) + cam.vAngle;
    double d_ = (ceilingHeight - cam.height) * fastTan_rp(tanMap_rp, vAngle);
    double d = d_ * cosHAngle_rp;
    double s = d * rayLen_rp;
    Point p(ray.A.x + (ray.B.x - ray.A.x) * s, ray.A.y + (ray.B.y - ray.A.y) * s);

    Point texel = worldPointToFloorTexel(p, texSz_wd_rp, texSz_px);

    QRgb* pixels = reinterpret_cast<QRgb*>(target.scanLine(j));
    pixels[screenX_px] = ceilingTex.image.pixel(texel.x, texel.y);
  }
}

//===========================================
// getFloorDecal
//
// x is set to the point inside decal space
//===========================================
static const CFloorDecal* getFloorDecal(const SpatialSystem& spatialSystem, const CRegion* region,
  const Point& pt, Point& x) {

  for (auto it = region->floorDecals.begin(); it != region->floorDecals.end(); ++it) {
    const CFloorDecal& decal = **it;

    const CHRect& hRect = getHRect(spatialSystem, decal);
    x = hRect.transform.inverse() * pt;

    if (isBetween(x.x, 0, hRect.size.x) && isBetween(x.y, 0, hRect.size.y)) {
      return &decal;
    }
  }

  return nullptr;
}

//===========================================
// drawFloorSlice
//===========================================
static void drawFloorSlice(QImage& target, const SpatialSystem& spatialSystem,
  const RenderGraph& rg, const Player& player, const CRegion* region, double floorHeight,
  const Point& collisionPoint, const ScreenSlice& slice, int screenX_px, double projX_wd,
  double vWorldUnit_px, const tanMap_t& tanMap_rp, const atanMap_t& atanMap) {

  const Camera& cam = player.camera();

  double screenH_px = rg.viewport.y * vWorldUnit_px;
  const Texture& floorTex = rg.textures.at(region->floorTexture);

  double hAngle = atan(projX_wd / cam.F);
  LineSegment ray(cam.pos, collisionPoint);

  Size texSz_px(floorTex.image.rect().width(), floorTex.image.rect().height());
  Size texSz_wd_rp(1.0 / floorTex.size_wd.x, 1.0 / floorTex.size_wd.y);

  double vWorldUnit_px_rp = 1.0 / vWorldUnit_px;
  double F_rp = 1.0 / cam.F;
  double rayLen_rp = 1.0 / ray.length();
  double cosHAngle_rp = 1.0 / cos(hAngle);

  for (int j = slice.sliceBottom_px; j < slice.viewportBottom_px; ++j) {
    double projY_wd = (j - screenH_px * 0.5) * vWorldUnit_px_rp;
    double vAngle = fastATan(atanMap, projY_wd * F_rp) - cam.vAngle;
    double d_ = (cam.height - floorHeight) * fastTan_rp(tanMap_rp, vAngle);
    double d = d_ * cosHAngle_rp;
    double s = d * rayLen_rp;
    Point p(ray.A.x + (ray.B.x - ray.A.x) * s, ray.A.y + (ray.B.y - ray.A.y) * s);

    Point decalPt;
    const CFloorDecal* decal = getFloorDecal(spatialSystem, region, p, decalPt);

    if (decal != nullptr) {
      const CHRect& hRect = getHRect(spatialSystem, *decal);

      const Texture& decalTex = rg.textures.at(decal->texture);
      Size texSz_px(decalTex.image.rect().width(), decalTex.image.rect().height());

      Point texel(texSz_px.x * decalPt.x / hRect.size.x, texSz_px.y * decalPt.y / hRect.size.y);
      QRgb* pixels = reinterpret_cast<QRgb*>(target.scanLine(j));
      pixels[screenX_px] = decalTex.image.pixel(texel.x, texel.y);
    }
    else {
      Point texel = worldPointToFloorTexel(p, texSz_wd_rp, texSz_px);
      QRgb* pixels = reinterpret_cast<QRgb*>(target.scanLine(j));
      pixels[screenX_px] = floorTex.image.pixel(texel.x, texel.y);
    }
  }
}

//===========================================
// sampleWallTexture
//===========================================
static void sampleWallTexture(const QRect& texRect, double camHeight_wd, const Size& viewport_px,
  double screenX_px, double hWorldUnit_px, double vWorldUnit_px, double targetH_wd,
  double distanceAlongTarget, const Slice& slice, const Size& texSz_wd, vector<QRectF>& trgRects,
  vector<QRect>& srcRects) {

  double H_tx = texRect.height();
  double W_tx = texRect.width();

  double projSliceH_wd = slice.projSliceTop_wd - slice.projSliceBottom_wd;
  double sliceH_wd = slice.sliceTop_wd - slice.sliceBottom_wd;
  double sliceToProjScale = projSliceH_wd / sliceH_wd;
  double projTexH_wd = texSz_wd.y * sliceToProjScale;

  // World space
  double sliceBottom_wd = slice.sliceBottom_wd + camHeight_wd - targetH_wd;
  double sliceTop_wd = slice.sliceTop_wd + camHeight_wd;

  // World space (not camera space)
  auto fnSliceToProjY = [&](double y) {
    return slice.projSliceBottom_wd + (y - sliceBottom_wd) * sliceToProjScale;
  };

  double hWorldUnit_tx = W_tx / texSz_wd.x;
  double vWorldUnit_tx = H_tx / texSz_wd.y;

  auto fnProjToScreenY = [&](double y) {
    return viewport_px.y - (y * vWorldUnit_px);
  };

  double nx = distanceAlongTarget / texSz_wd.x;
  double x = (nx - floor(nx)) * texSz_wd.x;

  int j0 = floor(sliceBottom_wd / texSz_wd.y);
  int j1 = ceil(sliceTop_wd / texSz_wd.y);

  double bottomOffset_wd = sliceBottom_wd - j0 * texSz_wd.y;
  double topOffset_wd = j1 * texSz_wd.y - sliceTop_wd;

  for (int j = j0; j < j1; ++j) {
    QRect srcRect;
    srcRect.setX(texRect.x() + x * hWorldUnit_tx);
    srcRect.setY(texRect.y());
    srcRect.setWidth(1);
    srcRect.setHeight(texRect.height());

    double y = j * texSz_wd.y;

    QRectF trgRect;
    trgRect.setX(screenX_px);
    trgRect.setY(fnProjToScreenY(fnSliceToProjY(y) + projTexH_wd));
    trgRect.setWidth(1);
    trgRect.setHeight(projTexH_wd * vWorldUnit_px);

    // Note that screen y-axis is inverted
    if (j == j0) {
      srcRect.setHeight(srcRect.height() - bottomOffset_wd * vWorldUnit_tx);
      trgRect.setHeight(trgRect.height() - bottomOffset_wd * sliceToProjScale * vWorldUnit_px);
    }
    if (j + 1 == j1) {
      // QRect::setY() also changes the height!
      srcRect.setY(srcRect.y() + topOffset_wd * vWorldUnit_tx);
      trgRect.setY(trgRect.y() + topOffset_wd * sliceToProjScale * vWorldUnit_px);
    }

    srcRects.push_back(srcRect);
    trgRects.push_back(trgRect);
  }
}

//===========================================
// sampleSpriteTexture
//===========================================
static QRect sampleSpriteTexture(const QRect& rect, const SpriteX& X, double camHeight,
  double width_wd, double height_wd, double y_wd) {

  double H_px = rect.height();
  double W_px = rect.width();

  double hWorldUnit_px = W_px / width_wd;
  double texW_wd = W_px / hWorldUnit_px;

  double n = X.X->distanceAlongTarget / texW_wd;
  double x = (n - floor(n)) * texW_wd;

  double texBottom_px = H_px - ((camHeight + X.slice.sliceBottom_wd - y_wd) / height_wd) * H_px;
  double texTop_px = H_px - ((camHeight + X.slice.sliceTop_wd - y_wd) / height_wd) * H_px;

  int i = x * hWorldUnit_px;

  return QRect(rect.x() + i, rect.y() + texTop_px, 1, texBottom_px - texTop_px);
}

//===========================================
// drawSlice
//===========================================
static ScreenSlice drawSlice(QPainter& painter, const RenderGraph& rg, const Player& player,
  double F, double distanceAlongTarget, const Slice& slice, const string& texture,
  double screenX_px, const Size& viewport_px, double targetH_wd = 0) {

  double hWorldUnit_px = viewport_px.x / rg.viewport.x;
  double vWorldUnit_px = viewport_px.y / rg.viewport.y;

  const Texture& wallTex = rg.textures.at(texture);

  int screenSliceBottom_px = viewport_px.y - slice.projSliceBottom_wd * vWorldUnit_px;
  int screenSliceTop_px = viewport_px.y - slice.projSliceTop_wd * vWorldUnit_px;

  if (screenSliceBottom_px - screenSliceTop_px > 0) {
    vector<QRect> srcRects;
    vector<QRectF> trgRects;
    sampleWallTexture(wallTex.image.rect(), player.camera().height, viewport_px, screenX_px,
      hWorldUnit_px, vWorldUnit_px, targetH_wd, distanceAlongTarget, slice, wallTex.size_wd,
      trgRects, srcRects);

    assert(srcRects.size() == trgRects.size());

    for (unsigned int i = 0; i < srcRects.size(); ++i) {
      painter.drawImage(trgRects[i], wallTex.image, srcRects[i]);
    }
  }

  int viewportBottom_px = (rg.viewport.y - slice.viewportBottom_wd) * vWorldUnit_px;
  int viewportTop_px = (rg.viewport.y - slice.viewportTop_wd) * vWorldUnit_px;

  return ScreenSlice{screenSliceBottom_px, screenSliceTop_px, viewportBottom_px, viewportTop_px};
}

//===========================================
// drawSprite
//===========================================
static void drawSprite(QPainter& painter, const CSprite& sprite, const SpatialSystem& spatialSystem,
  const RenderGraph& rg, const Player& player, const Size& viewport_px, const SpriteX& X,
  double screenX_px) {

  const Camera& cam = player.camera();

  double vWorldUnit_px = viewport_px.y / rg.viewport.y;

  const CVRect& vRect = *X.vRect;
  const Slice& slice = X.slice;

  const Texture& tex = rg.textures.at(sprite.texture);
  const QRectF& uv = sprite.getView(vRect, cam.pos);
  QRect r = tex.image.rect();
  QRect frame(r.width() * uv.x(), r.height() * uv.y(), r.width() * uv.width(),
    r.height() * uv.height());

  int screenSliceBottom_px = viewport_px.y - slice.projSliceBottom_wd * vWorldUnit_px;
  int screenSliceTop_px = viewport_px.y - slice.projSliceTop_wd * vWorldUnit_px;

  const CZone& zone = getZone(spatialSystem, *sprite.region);

  QRect srcRect = sampleSpriteTexture(frame, X, cam.height, vRect.size.x, vRect.size.y,
    zone.floorHeight);
  QRect trgRect(screenX_px, screenSliceTop_px, 1, screenSliceBottom_px - screenSliceTop_px);

  painter.drawImage(trgRect, tex.image, srcRect);
}

//===========================================
// drawWallDecal
//===========================================
static void drawWallDecal(QPainter& painter, const SpatialSystem& spatialSystem,
  const RenderGraph& rg, const CWallDecal& decal, const WallX& X, int screenX_px,
  const Size& viewport_px, double camHeight, double vWorldUnit_px) {

  const CVRect& vRect = getVRect(spatialSystem, decal);

  const Texture& decalTex = rg.textures.at(decal.texture);

  double projSliceH_wd = X.slice.projSliceTop_wd - X.slice.projSliceBottom_wd;
  double sliceH_wd = X.slice.sliceTop_wd - X.slice.sliceBottom_wd;
  double sliceToProjScale = projSliceH_wd / sliceH_wd;

  // World space
  double sliceBottom_wd = X.slice.sliceBottom_wd + camHeight;

  // World space (not camera space)
  auto fnSliceToProjY = [&](double y) {
    return X.slice.projSliceBottom_wd + (y - sliceBottom_wd) * sliceToProjScale;
  };

  auto fnProjToScreenY = [&](double y) {
    return viewport_px.y - (y * vWorldUnit_px);
  };

  const CZone& zone = *X.hardEdge->zone;
  double floorH = zone.floorHeight;

  double x = X.X->distanceAlongTarget - vRect.pos.x;
  int i = decalTex.image.width() * x / vRect.size.x;

  double y0 = floorH + vRect.pos.y;
  double y1 = floorH + vRect.pos.y + vRect.size.y;

  int y0_px = fnProjToScreenY(fnSliceToProjY(y0));
  int y1_px = fnProjToScreenY(fnSliceToProjY(y1));

  QRect srcRect(i, 0, 1, decalTex.image.height());
  QRect trgRect(screenX_px, y1_px, 1, y0_px - y1_px);

  painter.drawImage(trgRect, decalTex.image, srcRect);
}

//===========================================
// drawImageOverlay
//===========================================
static void drawImageOverlay(QPainter& painter, const CImageOverlay& overlay, const RenderGraph& rg,
  const Size& viewport_px) {

  double x = (overlay.pos.x / rg.viewport.x) * viewport_px.x;
  double y = (1.0 - overlay.pos.y / rg.viewport.y) * viewport_px.y;
  double w = (overlay.size.x / rg.viewport.x) * viewport_px.x;
  double h = (overlay.size.y / rg.viewport.y) * viewport_px.y;

  const Texture& tex = rg.textures.at(overlay.texture);

  double sx = overlay.texRect.x() * tex.image.rect().width();
  double sy = overlay.texRect.y() * tex.image.rect().height();
  double sw = overlay.texRect.width() * tex.image.rect().width();
  double sh = overlay.texRect.height() * tex.image.rect().height();

  QRect srcRect(sx, sy, sw, sh);
  QRect trgRect(x, y - h, w, h);
  painter.drawImage(trgRect, tex.image, srcRect);
}

//===========================================
// drawTextOverlay
//===========================================
static void drawTextOverlay(QPainter& painter, const CTextOverlay& overlay, const RenderGraph& rg,
  const Size& viewport_px) {

  double x = (overlay.pos.x / rg.viewport.x) * viewport_px.x;
  double y = (1.0 - overlay.pos.y / rg.viewport.y) * viewport_px.y;
  double h = (overlay.height / rg.viewport.y) * viewport_px.y;

  QFont font;
  font.setPixelSize(h);

  painter.setFont(font);
  painter.setPen(overlay.colour);
  painter.drawText(x, y, overlay.text.c_str());
}

//===========================================
// Renderer::Renderer
//===========================================
Renderer::Renderer(EntityManager& entityManager)
  : m_entityManager(entityManager) {

  for (unsigned int i = 0; i < m_tanMap_rp.size(); ++i) {
    m_tanMap_rp[i] = 1.0 / tan(2.0 * PI * static_cast<double>(i)
      / static_cast<double>(m_tanMap_rp.size()));
  }

  double dx = (ATAN_MAX - ATAN_MIN) / static_cast<double>(m_atanMap.size());
  for (unsigned int i = 0; i < m_atanMap.size(); ++i) {
    m_atanMap[i] = atan(ATAN_MIN + dx * static_cast<double>(i));
  }
}

//===========================================
// Renderer::getWallDecal
//===========================================
static CWallDecal* getWallDecal(const SpatialSystem& spatialSystem, const CWall& wall, double x) {
  for (auto it = wall.decals.begin(); it != wall.decals.end(); ++it) {
    CWallDecal* decal = it->get();
    const CVRect& vRect = getVRect(spatialSystem, *decal);

    double x0 = vRect.pos.x;
    double x1 = vRect.pos.x + vRect.size.x;

    if (isBetween(x, x0, x1)) {
      return decal;
    }
  }

  return nullptr;
}

//===========================================
// Renderer::renderScene
//===========================================
void Renderer::renderScene(QImage& target, const RenderGraph& rg, const Player& player) {
  const SpatialSystem& spatialSystem = m_entityManager
    .system<SpatialSystem>(ComponentKind::C_SPATIAL);
  const RenderSystem& renderSystem = m_entityManager.system<RenderSystem>(ComponentKind::C_RENDER);

  QPainter painter;
  painter.begin(&target);

  Size viewport_px(target.width(), target.height());
  const Camera& cam = player.camera();

  double hWorldUnit_px = viewport_px.x / rg.viewport.x;
  double vWorldUnit_px = viewport_px.y / rg.viewport.y;

  QRect rect(QPoint(), QSize(viewport_px.x, viewport_px.y));
  painter.fillRect(rect, QBrush(QColor(0, 0, 0)));

  double da = cam.hFov / viewport_px.x;

  for (int screenX_px = 0; screenX_px < viewport_px.x; ++screenX_px) {
    double projX_wd = static_cast<double>(screenX_px - viewport_px.x / 2) / hWorldUnit_px;

    CastResult result;
    castRay(spatialSystem, renderSystem, screenX_px * da - 0.5 * cam.hFov, rg, player, result);

    for (auto it = result.intersections.rbegin(); it != result.intersections.rend(); ++it) {
      XWrapper& X = **it;

      if (X.kind == XWrapperKind::WALL) {
        const WallX& wallX = dynamic_cast<const WallX&>(X);

        ScreenSlice slice = drawSlice(painter, rg, player, cam.F, wallX.X->distanceAlongTarget,
          wallX.slice, wallX.wall->texture, screenX_px, viewport_px);

        CWallDecal* decal = getWallDecal(spatialSystem, *wallX.wall, wallX.X->distanceAlongTarget);
        if (decal != nullptr) {
          drawWallDecal(painter, spatialSystem, rg, *decal, wallX, screenX_px, viewport_px,
            cam.height, vWorldUnit_px);
        }

        CZone& zone = *wallX.hardEdge->zone;
        CRegion& region = *wallX.wall->region;

        drawFloorSlice(target, spatialSystem, rg, player, &region, zone.floorHeight,
          wallX.X->point_world, slice, screenX_px, projX_wd, vWorldUnit_px, m_tanMap_rp, m_atanMap);

        if (region.hasCeiling) {
          drawCeilingSlice(target, rg, player, &region, zone.ceilingHeight, wallX.X->point_world,
            slice, screenX_px, projX_wd, vWorldUnit_px, m_tanMap_rp, m_atanMap);
        }
        else {
          drawSkySlice(target, rg, player, slice, screenX_px);
        }
      }
      else if (X.kind == XWrapperKind::JOIN) {
        const JoinX& joinX = dynamic_cast<const JoinX&>(X);

        ScreenSlice slice0 = drawSlice(painter, rg, player, cam.F, joinX.X->distanceAlongTarget,
          joinX.slice0, joinX.join->bottomTexture, screenX_px, viewport_px);

        const CRegion& nearRegion = dynamic_cast<const CRegion&>(renderSystem
          .getComponent(joinX.nearZone->entityId()));

        drawFloorSlice(target, spatialSystem, rg, player, &nearRegion, joinX.nearZone->floorHeight,
          joinX.X->point_world, slice0, screenX_px, projX_wd, vWorldUnit_px, m_tanMap_rp,
          m_atanMap);

        ScreenSlice slice1 = drawSlice(painter, rg, player, cam.F, joinX.X->distanceAlongTarget,
          joinX.slice1, joinX.join->topTexture, screenX_px, viewport_px,
          joinX.farZone->ceilingHeight);

        if (nearRegion.hasCeiling) {
          drawCeilingSlice(target, rg, player, &nearRegion, joinX.nearZone->ceilingHeight,
            joinX.X->point_world, slice1, screenX_px, projX_wd, vWorldUnit_px, m_tanMap_rp,
            m_atanMap);
        }
        else {
          drawSkySlice(target, rg, player, slice1, screenX_px);
        }
      }
      else if (X.kind == XWrapperKind::SPRITE) {
        const SpriteX& spriteX = dynamic_cast<const SpriteX&>(X);
        drawSprite(painter, *spriteX.sprite, spatialSystem, rg, player, viewport_px, spriteX,
          screenX_px);
      }
    }
  }

  for (auto it = rg.overlays.begin(); it != rg.overlays.end(); ++it) {
    const COverlay& overlay = **it;
    if (overlay.kind == COverlayKind::IMAGE) {
      drawImageOverlay(painter, dynamic_cast<const CImageOverlay&>(overlay), rg, viewport_px);
    }
    else if (overlay.kind == COverlayKind::TEXT) {
      drawTextOverlay(painter, dynamic_cast<const CTextOverlay&>(overlay), rg, viewport_px);
    }
  }

  painter.end();
}
