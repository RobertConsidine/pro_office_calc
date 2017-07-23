#ifndef __PROCALC_FRAGMENTS_F_RAYCAST_HPP__
#define __PROCALC_FRAGMENTS_F_RAYCAST_HPP__


#include <memory>
#include <map>
#include <QWidget>
#include <QTimer>
#include <QImage>
#include "fragment.hpp"
#include "fragments/f_main/f_menu_bar/f_settings_dialog/f_raycast/entity_manager.hpp"
#include "fragments/f_main/f_menu_bar/f_settings_dialog/f_raycast/geometry.hpp"
#ifdef DEBUG
#  include <chrono>
#endif


class QPaintEvent;
class EventSystem;

struct FRaycastData : public FragmentData {};

class FRaycast : public QWidget, public Fragment {
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

    EntityManager m_entityManager;
    EventSystem* m_eventSystem;
    std::unique_ptr<QTimer> m_timer;
    QImage m_buffer;
    std::map<int, bool> m_keyStates;
    Point m_cursor;
    bool m_cursorCaptured;
    Qt::CursorShape m_defaultCursor;

#ifdef DEBUG
    std::chrono::high_resolution_clock::time_point m_t = std::chrono::high_resolution_clock::now();
    double m_frameRate = 0;
    long m_frame = 0;
#endif
};


#endif
