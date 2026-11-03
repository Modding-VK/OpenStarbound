#include "StarExplorationProfilingMenu.hpp"

#include "StarAssets.hpp"
#include "StarButtonWidget.hpp"
#include "StarGuiReader.hpp"
#include "StarLabelWidget.hpp"
#include "StarRoot.hpp"
#include "StarSliderBar.hpp"

namespace Star {

namespace {
  size_t nearestIndex(List<unsigned> const& values, unsigned value) {
    size_t result = 0;
    for (size_t i = 1; i < values.size(); ++i) {
      auto currentDifference = values[i] > value ? values[i] - value : value - values[i];
      auto selectedDifference = values[result] > value ? values[result] - value : value - values[result];
      if (currentDifference < selectedDifference)
        result = i;
    }
    return result;
  }
}

ExplorationProfilingMenu::ExplorationProfilingMenu()
  : m_intervalValues{1, 2, 5, 10, 15, 30, 60},
    m_slowOperationValues{1, 5, 10, 25, 50, 100, 250, 500, 1000, 5000, 10000, 60000},
    m_reportFileSizes{64 * 1024, 1024 * 1024, 8 * 1024 * 1024, 16 * 1024 * 1024, 32 * 1024 * 1024, 64 * 1024 * 1024} {
  GuiReader reader;
  reader.registerCallback("accept", [=](Widget*) { apply(); dismiss(); });
  reader.registerCallback("cancel", [=](Widget*) { dismiss(); });
  reader.registerCallback("enabled", [=](Widget*) { updateLabels(); });
  reader.registerCallback("npcDetails", [=](Widget*) { updateLabels(); });
  reader.registerCallback("regionalDetails", [=](Widget*) { updateLabels(); });
  reader.registerCallback("writeReports", [=](Widget*) { updateLabels(); });
  reader.registerCallback("interval", [=](Widget*) { updateLabels(); });
  reader.registerCallback("slowOperation", [=](Widget*) { updateLabels(); });
  reader.registerCallback("entityLimit", [=](Widget*) { updateLabels(); });
  reader.registerCallback("fileSize", [=](Widget*) { updateLabels(); });
  reader.registerCallback("backups", [=](Widget*) { updateLabels(); });

  auto config = Root::singleton().assets()->json("/interface/opensb/diagnostics/explorationprofiling.config");
  reader.construct(config.get("paneLayout"), this);

  m_enabledButton = fetchChild<ButtonWidget>("enabledCheckbox");
  m_npcDetailsButton = fetchChild<ButtonWidget>("npcDetailsCheckbox");
  m_regionalDetailsButton = fetchChild<ButtonWidget>("regionalDetailsCheckbox");
  m_writeReportsButton = fetchChild<ButtonWidget>("writeReportsCheckbox");
  m_intervalSlider = fetchChild<SliderBarWidget>("intervalSlider");
  m_slowOperationSlider = fetchChild<SliderBarWidget>("slowOperationSlider");
  m_entityLimitSlider = fetchChild<SliderBarWidget>("entityLimitSlider");
  m_fileSizeSlider = fetchChild<SliderBarWidget>("fileSizeSlider");
  m_backupsSlider = fetchChild<SliderBarWidget>("backupsSlider");
  m_intervalValueLabel = fetchChild<LabelWidget>("intervalValueLabel");
  m_slowOperationValueLabel = fetchChild<LabelWidget>("slowOperationValueLabel");
  m_entityLimitValueLabel = fetchChild<LabelWidget>("entityLimitValueLabel");
  m_fileSizeValueLabel = fetchChild<LabelWidget>("fileSizeValueLabel");
  m_backupsValueLabel = fetchChild<LabelWidget>("backupsValueLabel");

  m_intervalSlider->setRange(0, m_intervalValues.size() - 1, 1);
  m_slowOperationSlider->setRange(0, m_slowOperationValues.size() - 1, 1);
  m_entityLimitSlider->setRange(1, 50, 1);
  m_fileSizeSlider->setRange(0, m_reportFileSizes.size() - 1, 1);
  m_backupsSlider->setRange(0, 20, 1);
}

void ExplorationProfilingMenu::show() {
  Pane::show();
  loadConfiguration();
  syncGui();
}

void ExplorationProfilingMenu::loadConfiguration() {
  m_profile = Root::singleton().configuration()->get("explorationProfiling", JsonObject()).toObject();
  m_profile.set("enabled", m_profile.value("enabled", false).toBool());
  m_profile.set("intervalSeconds", m_profile.value("intervalSeconds", 5).toUInt());
  m_profile.set("slowOperationMilliseconds", m_profile.value("slowOperationMilliseconds", 50).toUInt());
  m_profile.set("entityDetailLimit", m_profile.value("entityDetailLimit", 12).toUInt());
  m_profile.set("npcDetails", m_profile.value("npcDetails", false).toBool());
  m_profile.set("regionalDetails", m_profile.value("regionalDetails", false).toBool());
  m_profile.set("writeReportsToFiles", m_profile.value("writeReportsToFiles", true).toBool());
  m_profile.set("reportFileMaxBytes", m_profile.value("reportFileMaxBytes", 8 * 1024 * 1024).toUInt());
  m_profile.set("reportFileBackups", m_profile.value("reportFileBackups", 3).toUInt());
}

void ExplorationProfilingMenu::syncGui() {
  m_enabledButton->setChecked(m_profile.get("enabled").toBool());
  m_npcDetailsButton->setChecked(m_profile.get("npcDetails").toBool());
  m_regionalDetailsButton->setChecked(m_profile.get("regionalDetails").toBool());
  m_writeReportsButton->setChecked(m_profile.get("writeReportsToFiles").toBool());
  m_intervalSlider->setVal(nearestIndex(m_intervalValues, m_profile.get("intervalSeconds").toUInt()), false);
  m_slowOperationSlider->setVal(nearestIndex(m_slowOperationValues, m_profile.get("slowOperationMilliseconds").toUInt()), false);
  m_entityLimitSlider->setVal(clamp<unsigned>(m_profile.get("entityDetailLimit").toUInt(), 1, 50), false);
  m_fileSizeSlider->setVal(nearestIndex(m_reportFileSizes, m_profile.get("reportFileMaxBytes").toUInt()), false);
  m_backupsSlider->setVal(clamp<unsigned>(m_profile.get("reportFileBackups").toUInt(), 0, 20), false);
  updateLabels();
}

void ExplorationProfilingMenu::updateLabels() {
  m_intervalValueLabel->setText(strf("{} s", m_intervalValues[m_intervalSlider->val()]));
  m_slowOperationValueLabel->setText(strf("{} ms", m_slowOperationValues[m_slowOperationSlider->val()]));
  m_entityLimitValueLabel->setText(toString(m_entityLimitSlider->val()));
  m_fileSizeValueLabel->setText(strf("{} MiB", m_reportFileSizes[m_fileSizeSlider->val()] / (1024 * 1024)));
  m_backupsValueLabel->setText(toString(m_backupsSlider->val()));
}

void ExplorationProfilingMenu::apply() {
  m_profile.set("enabled", m_enabledButton->isChecked());
  m_profile.set("npcDetails", m_npcDetailsButton->isChecked());
  m_profile.set("regionalDetails", m_regionalDetailsButton->isChecked());
  m_profile.set("writeReportsToFiles", m_writeReportsButton->isChecked());
  m_profile.set("intervalSeconds", m_intervalValues[m_intervalSlider->val()]);
  m_profile.set("slowOperationMilliseconds", m_slowOperationValues[m_slowOperationSlider->val()]);
  m_profile.set("entityDetailLimit", m_entityLimitSlider->val());
  m_profile.set("reportFileMaxBytes", m_reportFileSizes[m_fileSizeSlider->val()]);
  m_profile.set("reportFileBackups", m_backupsSlider->val());
  Root::singleton().configuration()->set("explorationProfiling", m_profile);
}

}
