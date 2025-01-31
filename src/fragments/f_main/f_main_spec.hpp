#ifndef __PROCALC_FRAGMENTS_F_MAIN_SPEC_HPP__
#define __PROCALC_FRAGMENTS_F_MAIN_SPEC_HPP__


#include <QColor>
#include <QString>
#include "fragment_spec.hpp"
#include "fragments/relocatable/f_glitch/f_glitch_spec.hpp"
#include "fragments/relocatable/f_tetrominos/f_tetrominos_spec.hpp"
#include "fragments/relocatable/f_calculator/f_calculator_spec.hpp"
#include "fragments/f_main/f_shuffled_calc/f_shuffled_calc_spec.hpp"
#include "fragments/f_main/f_countdown_to_start/f_countdown_to_start_spec.hpp"
#include "fragments/f_main/f_settings_dialog/f_settings_dialog_spec.hpp"
#include "fragments/f_main/f_login_screen/f_login_screen_spec.hpp"
#include "fragments/f_main/f_desktop/f_desktop_spec.hpp"
#include "fragments/f_main/f_troubleshooter_dialog/f_troubleshooter_dialog_spec.hpp"
#include "fragments/f_main/f_app_dialog/f_app_dialog_spec.hpp"
#include "fragments/f_main/f_maze_3d/f_maze_3d_spec.hpp"


struct FMainSpec : public FragmentSpec {
  FMainSpec()
    : FragmentSpec("FMain", {
        &glitchSpec,
        &calculatorSpec,
        &shuffledCalcSpec,
        &loginScreenSpec,
        &desktopSpec,
        &countdownToStartSpec,
        &settingsDialogSpec,
        &troubleshooterDialogSpec,
        &appDialogSpec0,
        &appDialogSpec1,
        &appDialogSpec2,
        &appDialogSpec3,
        &appDialogSpec4,
        &tetrominosSpec,
        &maze3dSpec
      }),
      appDialogSpec0(0),
      appDialogSpec1(1),
      appDialogSpec2(2),
      appDialogSpec3(3),
      appDialogSpec4(4) {}

  FGlitchSpec glitchSpec;
  FCalculatorSpec calculatorSpec;
  FShuffledCalcSpec shuffledCalcSpec;
  FLoginScreenSpec loginScreenSpec;
  FDesktopSpec desktopSpec;
  FCountdownToStartSpec countdownToStartSpec;
  FSettingsDialogSpec settingsDialogSpec;
  FAppDialogSpec appDialogSpec0;
  FAppDialogSpec appDialogSpec1;
  FAppDialogSpec appDialogSpec2;
  FAppDialogSpec appDialogSpec3;
  FAppDialogSpec appDialogSpec4;
  FTroubleshooterDialogSpec troubleshooterDialogSpec;
  FTetrominosSpec tetrominosSpec;
  FMaze3dSpec maze3dSpec;

  QString windowTitle = "Pro Office Calculator";
  int width = 400;
  int height = 300;
  QColor bgColour = QColor(240, 240, 240);
  QString backgroundImage;
  QString fileLabel = "File";
  QString quitLabel = "Quit";
  QString helpLabel = "Help";
  QString aboutLabel = "About";
  QString aboutDialogTitle = "About";
  QString aboutDialogText;
};


#endif
