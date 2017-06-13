#ifndef __PROCALC_FRAGMENTS_F_RAYCAST_HPP__
#define __PROCALC_FRAGMENTS_F_RAYCAST_HPP__


#include <memory>
#include <map>
#include <QOpenGLWidget>
#include <QTimer>
#include "fragment.hpp"
#include "fragments/f_main/f_menu_bar/f_settings_dialog/f_raycast/scene.hpp"
#ifdef DEBUG
#  include <chrono>
#endif


class QPaintEvent;


struct FRaycastData : public FragmentData {};

class FRaycast : public QOpenGLWidget, public Fragment {
  Q_OBJECT

  public:
    FRaycast(Fragment& parent, FragmentData& parentData);

    virtual void rebuild(const FragmentSpec& spec) override;
    virtual void cleanUp() override;

  public slots:
    void tick();

  protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

  private:
    FRaycastData m_data;

    std::unique_ptr<QTimer> m_timer;
    std::unique_ptr<Scene> m_scene;
    std::map<int, bool> m_keyStates;
    Point m_cursor;
    bool m_cursorCaptured;
    Qt::CursorShape m_defaultCursor;

#ifdef DEBUG
    std::chrono::high_resolution_clock::time_point m_t = std::chrono::high_resolution_clock::now();
    double m_frameRate;
#endif
};


#endif
