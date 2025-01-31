#ifndef __PROCALC_FRAGMENTS_F_LOGIN_SCREEN_HPP__
#define __PROCALC_FRAGMENTS_F_LOGIN_SCREEN_HPP__


#include <QLabel>
#include <QTimer>
#include <QImage>
#include <QLineEdit>
#include <QMargins>
#include "fragment.hpp"
#include "event_system.hpp"
#include "qt_obj_ptr.hpp"


struct FLoginScreenData : public FragmentData {
  QtObjPtr<QLineEdit> wgtUser;
  QtObjPtr<QLineEdit> wgtPassword;
  std::string password;
};

struct PasswordGeneratedEvent : public Event {
  PasswordGeneratedEvent(const std::string& password)
    : Event("passwordGeneratedEvent"),
      password(password) {}

  std::string password;
};

class FLoginScreen : public QLabel, public Fragment {
  Q_OBJECT

  public:
    FLoginScreen(Fragment& parent, FragmentData& parentData, const CommonFragData& commonData);

    virtual void reload(const FragmentSpec& spec) override;
    virtual void cleanUp() override;

    virtual ~FLoginScreen() override;

  private slots:
    void onLoginAttempt();

  private:
    FLoginScreenData m_data;

    struct {
      int spacing;
      QMargins margins;
    } m_origParentState;

    EventHandle m_hPwdGen;
};


#endif
