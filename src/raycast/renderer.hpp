#ifndef __PROCALC_RAYCAST_RENDERER_HPP__
#define __PROCALC_RAYCAST_RENDERER_HPP__


#include <array>
#include <list>
#include <set>
#include <QPainter>
#include "raycast/spatial_system.hpp"
#include "raycast/render_graph.hpp"
#include "app_config.hpp"


class QImage;
class EntityManager;
class Camera;
class RenderSystem;

class Renderer {
  public:
    Renderer(const AppConfig& appConfig, EntityManager& entityManager, QImage& target);

    void renderScene(const RenderGraph& rg, const Camera& cam);

  private:
    struct Slice {
      double sliceBottom_wd;
      double sliceTop_wd;
      double projSliceBottom_wd;
      double projSliceTop_wd;
      double viewportBottom_wd;
      double viewportTop_wd;
      bool visible = true;
    };

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
      XWrapper(XWrapperKind kind, pIntersection_t X)
        : kind(kind),
          X(std::move(X)) {}

      XWrapperKind kind;
      pIntersection_t X;

      virtual ~XWrapper() {}
    };

    typedef std::unique_ptr<XWrapper> pXWrapper_t;

    struct CastResult {
      std::list<pXWrapper_t> intersections;
    };

    struct JoinX : public XWrapper {
      JoinX(pIntersection_t X)
        : XWrapper(XWrapperKind::JOIN, std::move(X)) {}

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
        : XWrapper(XWrapperKind::WALL, std::move(X)) {}

      const CHardEdge* hardEdge = nullptr;
      const CWall* wall = nullptr;
      Slice slice;
      const CZone* nearZone;

      virtual ~WallX() override {}
    };

    struct SpriteX : public XWrapper {
      SpriteX(pIntersection_t X)
        : XWrapper(XWrapperKind::SPRITE, std::move(X)) {}

      const CVRect* vRect = nullptr;
      const CSprite* sprite = nullptr;
      Slice slice;

      virtual ~SpriteX() override {}
    };

    AppConfig m_appConfig;
    EntityManager& m_entityManager;
    QImage& m_target;

    const double ATAN_MIN = -10.0;
    const double ATAN_MAX = 10.0;

    typedef std::array<double, 10000> tanMap_t;
    typedef std::array<double, 10000> atanMap_t;

    tanMap_t m_tanMap_rp;
    atanMap_t m_atanMap;

    void drawImage(const QRect& trgRect, const QImage& tex, const QRect& srcRect,
      double distance = 0) const;

    void drawSprite(const RenderGraph& rg, const Camera& camera, const SpriteX& X,
      double screenX_px) const;

    ScreenSlice drawSlice(const RenderGraph& rg, const Camera& cam, const Intersection& X,
      const Slice& slice, const std::string& texture, const QRectF& texRect, double screenX_px,
      double targetH_wd = 0) const;

    void drawFloorSlice(const RenderGraph& rg, const Camera& cam,
      const SpatialSystem& spatialSystem, const Intersection& X, const CRegion& region,
      double floorHeight, const ScreenSlice& slice, int screenX_px, double projX_wd) const;

    void drawCeilingSlice(const RenderGraph& rg, const Camera& cam, const Intersection& X,
      const CRegion& region, double ceilingHeight, const ScreenSlice& slice, int screenX_px,
      double projX_wd) const;

    void drawSkySlice(const RenderGraph& rg, const Camera& cam, const ScreenSlice& slice,
      int screenX_px) const;

    void drawWallDecal(const RenderGraph& rg, const Camera& cam, const SpatialSystem& spatialSystem,
      const CWallDecal& decal, const Intersection& X, const Slice& slice, const CZone& zone,
      int screenX_px) const;

    void drawColourOverlay(const RenderGraph& rg, QPainter& painter,
      const CColourOverlay& overlay) const;

    void drawImageOverlay(const RenderGraph& rg, QPainter& painter,
      const CImageOverlay& overlay) const;

    void drawTextOverlay(const RenderGraph& rg, QPainter& painter,
      const CTextOverlay& overlay) const;

    void sampleWallTexture(const RenderGraph& rg, const Camera& cam, double screenX_px,
      double texAnchor_wd, double distanceAlongTarget, const Slice& slice, const Size& texSz,
      const QRectF& frameRect, const Size& tileSz_wd, std::vector<QRect>& trgRects,
      std::vector<QRect>& srcRects) const;

    QRect sampleSpriteTexture(const Camera& cam, const QRect& rect, const SpriteX& X,
      const Size& size_wd, double y_wd) const;

    XWrapper* constructXWrapper(const SpatialSystem& spatialSystem,
      const RenderSystem& renderSystem, pIntersection_t X) const;

    Slice computeSlice(const LineSegment& rotProjPlane, const LineSegment& wall, double subview0,
      double subview1, const LineSegment& projRay0, const LineSegment& projRay1, Point& projX0,
      Point& projX1) const;

    void castRay(const RenderGraph& rg, const Camera& cam, const SpatialSystem& spatialSystem,
      const RenderSystem& renderSystem, const Vec2f& dir, CastResult& result) const;

    inline double projToScreenY(const RenderGraph& rg, double y) const;
    inline double fastTan_rp(double a) const;
    inline double fastATan(double x) const;
};

//===========================================
// Renderer::projToScreenY
//===========================================
inline double Renderer::projToScreenY(const RenderGraph& rg, double y) const {
  return rg.viewport_px.y - (y * rg.vWorldUnit_px);
}

//===========================================
// Renderer::fastTan_rp
//
// Retrieves the reciprocal of tan(a) from the lookup table
//===========================================
inline double Renderer::fastTan_rp(double a) const {
  static const double x = static_cast<double>(m_tanMap_rp.size()) / (2.0 * PI);
  return m_tanMap_rp[static_cast<int>(normaliseAngle(a) * x)];
}

//===========================================
// Renderer::fastATan
//
// Retrieves atan(x) from the lookup table
//===========================================
inline double Renderer::fastATan(double x) const {
  if (x < ATAN_MIN) {
    x = ATAN_MIN;
  }
  if (x > ATAN_MAX) {
    x = ATAN_MAX;
  }
  static double dx = (ATAN_MAX - ATAN_MIN) / static_cast<double>(m_atanMap.size());
  return m_atanMap[static_cast<int>((x - ATAN_MIN) / dx)];
}


#endif
