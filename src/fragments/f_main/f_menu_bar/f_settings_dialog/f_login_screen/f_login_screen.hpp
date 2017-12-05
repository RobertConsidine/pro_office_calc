#ifndef __PROCALC_FRAGMENTS_F_LOGIN_SCREEN_HPP__
#define __PROCALC_FRAGMENTS_F_LOGIN_SCREEN_HPP__


#include <memory>
#include <random>
#include <QLabel>
#include <QPixmap>
#include <QMargins>
#include <QTimer>
#include <QImage>
#include "fragment.hpp"


struct FLoginScreenData : public FragmentData {
  std::unique_ptr<QPixmap> background;
};

class FLoginScreen : public QLabel, public Fragment {
  Q_OBJECT

  public:
    FLoginScreen(Fragment& parent, FragmentData& parentData);

    virtual void rebuild(const FragmentSpec& spec) override;
    virtual void cleanUp() override;

  public slots:
    void tick();

  private:
    FLoginScreenData m_data;

    struct {
      int spacing;
      QMargins margins;
    } m_origParentState;

    std::unique_ptr<QLabel> m_glitchOverlay;
    std::unique_ptr<QImage> m_glitchBuffer;
    std::unique_ptr<QTimer> m_glitchTimer;
    std::mt19937 m_randEngine;
};


#endif
