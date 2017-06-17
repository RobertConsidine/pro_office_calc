#include <cmath>
#include <cassert>
#include <limits>
#include <list>
#include <QPainter>
#include <QPaintDevice>
#include <QBrush>
#include "fragments/f_main/f_menu_bar/f_settings_dialog/f_raycast/renderer.hpp"
#include "fragments/f_main/f_menu_bar/f_settings_dialog/f_raycast/scene.hpp"


using std::string;
using std::list;


struct WallCollision {
  const Wall* wall;
  Point collisionPoint;
  double distanceFromCamera;
  double distanceAlongWall;
};

struct SpriteCollision {
  const Sprite* sprite;
  double distanceFromCamera;
  double distanceAlongSprite;
  double screenY;
};

struct CastResult {
  bool hitWall = false;
  WallCollision wallCollision;

  list<SpriteCollision> spriteCollisions;
};

struct WallSlice {
  int wallBottom_px;
  int sliceH_px;
};


//===========================================
// computeF
//===========================================
static double computeF(double w, double fov) {
  return w / (2.0 * tan(0.5 * fov));
}

//===========================================
// computeSliceHeight
//===========================================
static double computeSliceHeight(double F, double d, double h) {
  return h * F / d;
}

//===========================================
// worldCoordToFloorTexel
//===========================================
static Point worldPointToFloorTexel(const Point& p, const Size& texSz_wd, const Size& texSz_px) {
  double nx = p.x / texSz_wd.x;
  double ny = p.y / texSz_wd.y;
  return Point((nx - floor(nx)) * texSz_px.x, (ny - floor(ny)) * texSz_px.y);
}

//===========================================
// castRay
//===========================================
static void castRay(Vec2f r, const Scene& scene, double F, CastResult& result) {
  const Camera& cam = *scene.camera;
  LineSegment ray(Point(0, 0), Point(r.x * 999.9, r.y * 999.9));
  double distanceFromCamera = std::numeric_limits<double>::infinity();
  Matrix invCamMatrix = cam.matrix().inverse();

  for (auto it = scene.walls.begin(); it != scene.walls.end(); ++it) {
    const Wall& wall = **it;
    LineSegment lseg = transform(wall.lseg, invCamMatrix);

    Point pt;
    if (lineSegmentIntersect(ray, lseg, pt)) {
      // In camera space the ray will always point in positive x direction
      assert(pt.x > 0.0);

      if (pt.x < distanceFromCamera) {
        distanceFromCamera = pt.x;

        result.hitWall = true;
        result.wallCollision.wall = &wall;
        result.wallCollision.collisionPoint = cam.matrix() * pt;
        result.wallCollision.distanceFromCamera = pt.x;
        result.wallCollision.distanceAlongWall = distance(lseg.A, pt);
      }
    }
  }

  for (auto it = scene.sprites.begin(); it != scene.sprites.end(); ++it) {
    const Sprite& sprite = **it;
    Point pos = invCamMatrix * sprite.pos;
    double w = sprite.size.x;
    LineSegment lseg(Point(pos.x, pos.y - 0.5 * w), Point(pos.x * 1.00001, pos.y + 0.5 * w));

    Point pt;
    if (lineSegmentIntersect(ray, lseg, pt)) {
      SpriteCollision collision;
      collision.sprite = &sprite;
      collision.distanceFromCamera = pt.x;

      if (!result.hitWall ||
        result.wallCollision.distanceFromCamera > collision.distanceFromCamera) {

        collision.distanceAlongSprite = distance(lseg.A, pt);

        double camY_wd = scene.wallHeight / 2;
        LineSegment vRay(Point(0, 0), Point(pos.x, -camY_wd));
        LineSegment screen(Point(F + 0.0001, scene.viewport.y / 2), Point(F, -scene.viewport.y / 2));

        Matrix m(-cam.vAngle, Vec2f());
        LineSegment rotScreen = transform(screen, m);

        Point p;
        lineSegmentIntersect(vRay, rotScreen, p);

        collision.screenY = distance(rotScreen.A, p);

        result.spriteCollisions.push_back(collision);
      }
    }
  }

  result.spriteCollisions.sort([](const SpriteCollision& a, const SpriteCollision& b) {
    return a.distanceFromCamera > b.distanceFromCamera;
  });
}

//===========================================
// sampleTexture
//===========================================
static QRect sampleTexture(const QRect& rect, double distanceAlongWall, double wallHeight) {
  double worldUnit = static_cast<double>(rect.height()) / wallHeight;
  double texW = static_cast<double>(rect.width()) / worldUnit;

  double n = distanceAlongWall / texW;
  double x = (n - floor(n)) * texW;

  int i = x * worldUnit;
  return QRect(i, 0, 1, rect.height());
}

//===========================================
// drawCeilingSlice
//===========================================
static void drawCeilingSlice(QImage& target, const Scene& scene, const Point& collisionPoint,
  int wallTop_px, int screenX_px, int screenH_px, double screenX_wd, double vWorldUnitsInPx,
  double F) {

  const Camera& cam = *scene.camera;
  const QImage& ceilingTex = scene.textures.at("ceiling");

  double hAngle = atan(screenX_wd / F);
  LineSegment ray(cam.pos, collisionPoint);

  Size texSz_px(ceilingTex.rect().width(), ceilingTex.rect().height());
  double texelInWorldUnits = scene.wallHeight / texSz_px.y;
  Size texSz_wd(texSz_px.x * texelInWorldUnits, texSz_px.y * texelInWorldUnits);

  for (int j = wallTop_px; j >= 0; --j) {
    double screenY_wd = (screenH_px / 2 - j) / vWorldUnitsInPx;
    double vAngle = atan(screenY_wd / F) - cam.vAngle;
    double d_ = scene.wallHeight / (2.0 * tan(vAngle));
    double d = d_ / cos(hAngle);
    double s = d / ray.length();
    Point p(ray.A.x + (ray.B.x - ray.A.x) * s, ray.A.y + (ray.B.y - ray.A.y) * s);

    Point texel = worldPointToFloorTexel(p, texSz_wd, texSz_px);

    QRgb* pixels = reinterpret_cast<QRgb*>(target.scanLine(j));
    pixels[screenX_px] = ceilingTex.pixel(texel.x, texel.y);
  }
}

//===========================================
// drawFloorSlice
//===========================================
static void drawFloorSlice(QImage& target, const Scene& scene, const Point& collisionPoint,
  int wallBottom_px, int screenX_px, int screenH_px, double screenX_wd, double vWorldUnitsInPx,
  double F) {

  const Camera& cam = *scene.camera;
  const QImage& floorTex = scene.textures.at("floor");

  double hAngle = atan(screenX_wd / F);
  LineSegment ray(cam.pos, collisionPoint);

  Size texSz_px(floorTex.rect().width(), floorTex.rect().height());
  double texelInWorldUnits = scene.wallHeight / texSz_px.y;
  Size texSz_wd(texSz_px.x * texelInWorldUnits, texSz_px.y * texelInWorldUnits);

  for (int j = wallBottom_px; j < screenH_px; ++j) {
    double screenY_wd = (j - screenH_px / 2) / vWorldUnitsInPx;
    double vAngle = atan(screenY_wd / F) + cam.vAngle;
    double d_ = scene.wallHeight / (2.0 * tan(vAngle));
    double d = d_ / cos(hAngle);

    double s = d / ray.length();
    Point p(ray.A.x + (ray.B.x - ray.A.x) * s, ray.A.y + (ray.B.y - ray.A.y) * s);

    Point texel = worldPointToFloorTexel(p, texSz_wd, texSz_px);

    QRgb* pixels = reinterpret_cast<QRgb*>(target.scanLine(j));
    pixels[screenX_px] = floorTex.pixel(texel.x, texel.y);
  }
}

//===========================================
// computeHorizon
//===========================================
static double computeHorizon(double vAngle, double F, double screenH_wd) {
  LineSegment screen(Point(F + 0.0001, -screenH_wd / 2), Point(F, screenH_wd / 2));

  Matrix m(vAngle, Vec2f());
  LineSegment rotScreen = transform(screen, m);

  LineSegment ray(Point(0, 0), Point(F * 2.0, 0));

  Point p;
  bool intersect = lineSegmentIntersect(ray, rotScreen, p);

  assert(intersect);

  return distance(rotScreen.A, p);
}

//===========================================
// drawWallSlice
//===========================================
static WallSlice drawWallSlice(QPainter& painter, const Scene& scene,
  const WallCollision& collision, double F, int screenX_px, int screenH_px,
  double vWorldUnitsInPx) {

  double screenH_wd = screenH_px / vWorldUnitsInPx;
  double horizon_px = computeHorizon(scene.camera->vAngle, F, screenH_wd) * vWorldUnitsInPx;

  int sliceH_px = computeSliceHeight(F, collision.distanceFromCamera, scene.wallHeight)
    * vWorldUnitsInPx;

  const QImage& wallTex = scene.textures.at(collision.wall->texture);

  int wallBottom_px = horizon_px - 0.5 * sliceH_px;
  QRect trgRect(screenX_px, wallBottom_px, 1, sliceH_px);
  QRect srcRect = sampleTexture(wallTex.rect(), collision.distanceAlongWall, scene.wallHeight);

  painter.drawImage(trgRect, wallTex, srcRect);

  return WallSlice{wallBottom_px, sliceH_px};
}

//===========================================
// drawSprites
//===========================================
static void drawSprites(QPainter& painter, const Scene& scene, const CastResult& result,
  const Camera& cam, double F, double screenX_px, double screenH_px, double vWorldUnitsInPx) {

  for (auto it = result.spriteCollisions.begin(); it != result.spriteCollisions.end(); ++it) {
    const SpriteCollision& collision = *it;
    const Sprite& sprite = *collision.sprite;

    int spriteH_px = computeSliceHeight(F, collision.distanceFromCamera, sprite.size.y)
      * vWorldUnitsInPx;
    int spriteY_px = collision.screenY * vWorldUnitsInPx;

    const QImage& tex = scene.textures.at(sprite.texture);
    const QRectF& uv = sprite.textureRegion(cam.pos);
    QRect r = tex.rect();
    QRect frame(r.width() * uv.x(), r.height() * uv.y(), r.width() * uv.width(),
      r.height() * uv.height());

    double worldUnit_px = frame.width() / sprite.size.x;
    int texX_px = collision.distanceAlongSprite * worldUnit_px;

    QRect srcRect(frame.x() + texX_px, frame.y(), 1, frame.height());
    QRect trgRect(screenX_px, spriteY_px - spriteH_px, 1, spriteH_px);

    painter.drawImage(trgRect, tex, srcRect);
  }
}

//===========================================
// Renderer::renderScene
//===========================================
void Renderer::renderScene(QImage& target, const Scene& scene) {
  QPainter painter;
  painter.begin(&target);

  const Camera& cam = *scene.camera;

  int screenW_px = target.width();
  int screenH_px = target.height();

  double hWorldUnitsInPx = screenW_px / scene.viewport.x;
  double vWorldUnitsInPx = screenH_px / scene.viewport.y;

  QRect rect(QPoint(), QSize(screenW_px, screenH_px));
  painter.fillRect(rect, QBrush(QColor(0, 0, 0)));

  double F = computeF(scene.viewport.x, cam.hFov);

  for (int screenX_px = 0; screenX_px < screenW_px; ++screenX_px) {
    double screenX_wd = static_cast<double>(screenX_px - screenW_px / 2) / hWorldUnitsInPx;

    CastResult result;
    castRay(Vec2f(F, screenX_wd), scene, F, result);

    if (result.hitWall) {
      WallCollision& collision = result.wallCollision;

      WallSlice slice = drawWallSlice(painter, scene, collision, F, screenX_px, screenH_px,
        vWorldUnitsInPx);
      drawFloorSlice(target, scene, collision.collisionPoint,
        slice.wallBottom_px + slice.sliceH_px, screenX_px, screenH_px, screenX_wd, vWorldUnitsInPx,
        F);
      drawCeilingSlice(target, scene, collision.collisionPoint, slice.wallBottom_px, screenX_px,
        screenH_px, screenX_wd, vWorldUnitsInPx, F);
    }

    drawSprites(painter, scene, result, cam, F, screenX_px, screenH_px, vWorldUnitsInPx);
  }

  painter.end();
}
