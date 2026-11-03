#pragma once

#include "StarPane.hpp"

namespace Star {

STAR_CLASS(SliderBarWidget);
STAR_CLASS(ButtonWidget);
STAR_CLASS(LabelWidget);
STAR_CLASS(ExplorationProfilingMenu);

// Editor for diagnostic-only exploration profiling. The settings are saved for
// the next process start; live profiles are deliberately not recreated here.
class ExplorationProfilingMenu : public Pane {
public:
  ExplorationProfilingMenu();

  void show() override;

private:
  void loadConfiguration();
  void syncGui();
  void apply();
  void updateLabels();

  JsonObject m_profile;
  List<unsigned> m_intervalValues;
  List<unsigned> m_slowOperationValues;
  List<unsigned> m_reportFileSizes;

  ButtonWidgetPtr m_enabledButton;
  ButtonWidgetPtr m_npcDetailsButton;
  ButtonWidgetPtr m_regionalDetailsButton;
  ButtonWidgetPtr m_writeReportsButton;
  SliderBarWidgetPtr m_intervalSlider;
  SliderBarWidgetPtr m_slowOperationSlider;
  SliderBarWidgetPtr m_entityLimitSlider;
  SliderBarWidgetPtr m_fileSizeSlider;
  SliderBarWidgetPtr m_backupsSlider;
  LabelWidgetPtr m_intervalValueLabel;
  LabelWidgetPtr m_slowOperationValueLabel;
  LabelWidgetPtr m_entityLimitValueLabel;
  LabelWidgetPtr m_fileSizeValueLabel;
  LabelWidgetPtr m_backupsValueLabel;
};

}
