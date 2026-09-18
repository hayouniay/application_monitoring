#ifndef QT_SETTINGS_DIALOG_H
#define QT_SETTINGS_DIALOG_H

#include <QDialog>

extern "C" {
#include "monitoring_services.h"
}

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;

class NeoQtSettingsDialog : public QDialog {
  Q_OBJECT

public:
  explicit NeoQtSettingsDialog(QWidget *parent = nullptr);
  ~NeoQtSettingsDialog() override;

  NeoQtSettingsDialog(const NeoQtSettingsDialog &) = delete;

  NeoQtSettingsDialog &operator=(const NeoQtSettingsDialog &) = delete;

  void setConfig(const NeoConfig &config);

  double interval() const;
  double cpuThreshold() const;
  double ramThresholdMb() const;
  double ramThresholdPercent() const;

  QString userFilter() const;
  QString stateFilter() const;
  QString pidFilter() const;
  QString ppidFilter() const;

  QString includePattern() const;
  QString excludePattern() const;

  bool showLongArgs() const;
  bool showVsz() const;
  bool showSwap() const;
  bool showIo() const;
  bool showThreads() const;
  bool showStartTime() const;
  bool showElapsed() const;

  bool reverseSort() const;

  int processLimit() const;
  int sortMode() const;

private:
  void setupUi();
  void setupConnections();

  QDoubleSpinBox *m_intervalSpin;
  QDoubleSpinBox *m_cpuThresholdSpin;
  QDoubleSpinBox *m_ramThresholdMbSpin;
  QDoubleSpinBox *m_ramThresholdPercentSpin;

  QLineEdit *m_userEdit;
  QLineEdit *m_stateEdit;
  QLineEdit *m_pidEdit;
  QLineEdit *m_ppidEdit;

  QLineEdit *m_includeEdit;
  QLineEdit *m_excludeEdit;

  QComboBox *m_sortCombo;
  QSpinBox *m_limitSpin;

  QCheckBox *m_longArgsCheck;
  QCheckBox *m_vszCheck;
  QCheckBox *m_swapCheck;
  QCheckBox *m_ioCheck;
  QCheckBox *m_threadsCheck;
  QCheckBox *m_startTimeCheck;
  QCheckBox *m_elapsedCheck;
  QCheckBox *m_reverseCheck;

  QDialogButtonBox *m_buttons;
};

#endif /* QT_SETTINGS_DIALOG_H */