#include "qt/qt_settings_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

NeoQtSettingsDialog::NeoQtSettingsDialog(QWidget *parent)
    : QDialog(parent), m_intervalSpin(nullptr), m_cpuThresholdSpin(nullptr),
      m_ramThresholdMbSpin(nullptr), m_ramThresholdPercentSpin(nullptr),
      m_userEdit(nullptr), m_stateEdit(nullptr), m_pidEdit(nullptr),
      m_ppidEdit(nullptr), m_includeEdit(nullptr), m_excludeEdit(nullptr),
      m_sortCombo(nullptr), m_limitSpin(nullptr), m_longArgsCheck(nullptr),
      m_vszCheck(nullptr), m_swapCheck(nullptr), m_ioCheck(nullptr),
      m_threadsCheck(nullptr), m_startTimeCheck(nullptr),
      m_elapsedCheck(nullptr), m_reverseCheck(nullptr), m_buttons(nullptr) {
  setWindowTitle(QStringLiteral("NEO Monitoring Settings"));

  setModal(true);
  resize(620, 720);

  setupUi();
  setupConnections();
}

NeoQtSettingsDialog::~NeoQtSettingsDialog() = default;

void NeoQtSettingsDialog::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);

  /*
   * Monitoring
   */
  auto *monitoringGroup = new QGroupBox(QStringLiteral("Monitoring"), this);

  auto *monitoringLayout = new QFormLayout(monitoringGroup);

  m_intervalSpin = new QDoubleSpinBox(monitoringGroup);

  m_intervalSpin->setRange(MIN_INTERVAL, MAX_INTERVAL);

  m_intervalSpin->setDecimals(1);
  m_intervalSpin->setSingleStep(0.1);
  m_intervalSpin->setSuffix(QStringLiteral(" s"));

  m_cpuThresholdSpin = new QDoubleSpinBox(monitoringGroup);

  m_cpuThresholdSpin->setRange(0.0, 100000.0);

  m_cpuThresholdSpin->setDecimals(1);
  m_cpuThresholdSpin->setSuffix(QStringLiteral(" %"));

  m_ramThresholdMbSpin = new QDoubleSpinBox(monitoringGroup);

  m_ramThresholdMbSpin->setRange(0.0, 1024.0 * 1024.0);

  m_ramThresholdMbSpin->setDecimals(1);
  m_ramThresholdMbSpin->setSuffix(QStringLiteral(" MB"));

  m_ramThresholdPercentSpin = new QDoubleSpinBox(monitoringGroup);

  m_ramThresholdPercentSpin->setRange(0.0, 100.0);

  m_ramThresholdPercentSpin->setDecimals(1);
  m_ramThresholdPercentSpin->setSuffix(QStringLiteral(" %"));

  monitoringLayout->addRow(QStringLiteral("Refresh interval:"), m_intervalSpin);

  monitoringLayout->addRow(QStringLiteral("CPU threshold:"),
                           m_cpuThresholdSpin);

  monitoringLayout->addRow(QStringLiteral("RAM threshold:"),
                           m_ramThresholdMbSpin);

  monitoringLayout->addRow(QStringLiteral("RAM percentage:"),
                           m_ramThresholdPercentSpin);

  mainLayout->addWidget(monitoringGroup);

  /*
   * Filters
   */
  auto *filtersGroup = new QGroupBox(QStringLiteral("Filters"), this);

  auto *filtersLayout = new QFormLayout(filtersGroup);

  m_includeEdit = new QLineEdit(filtersGroup);

  m_includeEdit->setPlaceholderText(
      QStringLiteral("process name or command substring"));

  m_excludeEdit = new QLineEdit(filtersGroup);

  m_excludeEdit->setPlaceholderText(
      QStringLiteral("process name or command substring"));

  m_userEdit = new QLineEdit(filtersGroup);

  m_stateEdit = new QLineEdit(filtersGroup);

  m_stateEdit->setPlaceholderText(QStringLiteral("R,S,D,T,Z,I"));

  m_pidEdit = new QLineEdit(filtersGroup);

  m_pidEdit->setPlaceholderText(QStringLiteral("123,456,789"));

  m_ppidEdit = new QLineEdit(filtersGroup);

  m_ppidEdit->setPlaceholderText(QStringLiteral("1,100,200"));

  filtersLayout->addRow(QStringLiteral("Include pattern:"), m_includeEdit);

  filtersLayout->addRow(QStringLiteral("Exclude pattern:"), m_excludeEdit);

  filtersLayout->addRow(QStringLiteral("User:"), m_userEdit);

  filtersLayout->addRow(QStringLiteral("States:"), m_stateEdit);

  filtersLayout->addRow(QStringLiteral("PIDs:"), m_pidEdit);

  filtersLayout->addRow(QStringLiteral("PPIDs:"), m_ppidEdit);

  mainLayout->addWidget(filtersGroup);

  /*
   * Sorting
   */
  auto *sortingGroup = new QGroupBox(QStringLiteral("Sorting"), this);

  auto *sortingLayout = new QFormLayout(sortingGroup);

  m_sortCombo = new QComboBox(sortingGroup);

  m_sortCombo->addItem(QStringLiteral("CPU"), static_cast<int>(SORT_CPU));

  m_sortCombo->addItem(QStringLiteral("Memory"), static_cast<int>(SORT_MEM));

  m_sortCombo->addItem(QStringLiteral("PID"), static_cast<int>(SORT_PID));

  m_sortCombo->addItem(QStringLiteral("RSS"), static_cast<int>(SORT_RSS));

  m_sortCombo->addItem(QStringLiteral("I/O Read"),
                       static_cast<int>(SORT_IO_READ));

  m_sortCombo->addItem(QStringLiteral("I/O Write"),
                       static_cast<int>(SORT_IO_WRITE));

  m_limitSpin = new QSpinBox(sortingGroup);

  m_limitSpin->setRange(0, 1000000);

  m_limitSpin->setSpecialValueText(QStringLiteral("Unlimited"));

  m_reverseCheck =
      new QCheckBox(QStringLiteral("Reverse sort order"), sortingGroup);

  sortingLayout->addRow(QStringLiteral("Sort by:"), m_sortCombo);

  sortingLayout->addRow(QStringLiteral("Process limit:"), m_limitSpin);

  sortingLayout->addRow(m_reverseCheck);

  mainLayout->addWidget(sortingGroup);

  /*
   * Display
   */
  auto *displayGroup = new QGroupBox(QStringLiteral("Display"), this);

  auto *displayLayout = new QVBoxLayout(displayGroup);

  m_longArgsCheck =
      new QCheckBox(QStringLiteral("Show full command line"), displayGroup);

  m_vszCheck = new QCheckBox(QStringLiteral("Show VSZ"), displayGroup);

  m_swapCheck = new QCheckBox(QStringLiteral("Show swap"), displayGroup);

  m_ioCheck = new QCheckBox(QStringLiteral("Show I/O rates"), displayGroup);

  m_threadsCheck =
      new QCheckBox(QStringLiteral("Show thread count"), displayGroup);

  m_startTimeCheck =
      new QCheckBox(QStringLiteral("Show start time"), displayGroup);

  m_elapsedCheck =
      new QCheckBox(QStringLiteral("Show elapsed time"), displayGroup);

  displayLayout->addWidget(m_longArgsCheck);

  displayLayout->addWidget(m_vszCheck);

  displayLayout->addWidget(m_swapCheck);

  displayLayout->addWidget(m_ioCheck);

  displayLayout->addWidget(m_threadsCheck);

  displayLayout->addWidget(m_startTimeCheck);

  displayLayout->addWidget(m_elapsedCheck);

  mainLayout->addWidget(displayGroup);

  /*
   * Buttons
   */
  m_buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

  mainLayout->addWidget(m_buttons);
}

void NeoQtSettingsDialog::setupConnections() {
  connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

  connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void NeoQtSettingsDialog::setConfig(const NeoConfig &config) {
  m_intervalSpin->setValue(config.interval);

  m_cpuThresholdSpin->setValue(config.cpu_threshold);

  m_ramThresholdMbSpin->setValue(config.ram_threshold_mb);

  m_ramThresholdPercentSpin->setValue(config.ram_threshold_percent);

  m_userEdit->setText(QString::fromLocal8Bit(config.filter_user));

  QString states;

  for (size_t i = 0; i < config.state_count; ++i) {
    states += QChar(config.states[i]);
  }

  m_stateEdit->setText(states);

  QString pids;

  for (size_t i = 0; i < config.pid_count; ++i) {
    if (!pids.isEmpty())
      pids += QLatin1Char(',');

    pids += QString::number(static_cast<qlonglong>(config.pids[i]));
  }

  m_pidEdit->setText(pids);

  QString ppids;

  for (size_t i = 0; i < config.ppid_count; ++i) {
    if (!ppids.isEmpty())
      ppids += QLatin1Char(',');

    ppids += QString::number(static_cast<qlonglong>(config.ppids[i]));
  }

  m_ppidEdit->setText(ppids);

  if (config.pattern_count > 0 && config.patterns != nullptr &&
      config.patterns[0] != nullptr) {
    m_includeEdit->setText(QString::fromLocal8Bit(config.patterns[0]));
  } else {
    m_includeEdit->clear();
  }

  if (config.exclude_count > 0 && config.exclude_patterns != nullptr &&
      config.exclude_patterns[0] != nullptr) {
    m_excludeEdit->setText(QString::fromLocal8Bit(config.exclude_patterns[0]));
  } else {
    m_excludeEdit->clear();
  }

  const int sortIndex =
      m_sortCombo->findData(static_cast<int>(config.sort_mode));

  if (sortIndex >= 0)
    m_sortCombo->setCurrentIndex(sortIndex);

  m_limitSpin->setValue(
      config.limit > 1000000 ? 1000000 : static_cast<int>(config.limit));

  m_reverseCheck->setChecked(config.reverse);

  m_longArgsCheck->setChecked(config.show_long_args);

  m_vszCheck->setChecked(config.show_vsz);

  m_swapCheck->setChecked(config.show_swap);

  m_ioCheck->setChecked(config.show_io);

  m_threadsCheck->setChecked(config.show_threads);

  m_startTimeCheck->setChecked(config.show_start_time);

  m_elapsedCheck->setChecked(config.show_elapsed);
}

double NeoQtSettingsDialog::interval() const { return m_intervalSpin->value(); }

double NeoQtSettingsDialog::cpuThreshold() const {
  return m_cpuThresholdSpin->value();
}

double NeoQtSettingsDialog::ramThresholdMb() const {
  return m_ramThresholdMbSpin->value();
}

double NeoQtSettingsDialog::ramThresholdPercent() const {
  return m_ramThresholdPercentSpin->value();
}

QString NeoQtSettingsDialog::userFilter() const {
  return m_userEdit->text().trimmed();
}

QString NeoQtSettingsDialog::stateFilter() const {
  return m_stateEdit->text().trimmed();
}

QString NeoQtSettingsDialog::pidFilter() const {
  return m_pidEdit->text().trimmed();
}

QString NeoQtSettingsDialog::ppidFilter() const {
  return m_ppidEdit->text().trimmed();
}

QString NeoQtSettingsDialog::includePattern() const {
  return m_includeEdit->text().trimmed();
}

QString NeoQtSettingsDialog::excludePattern() const {
  return m_excludeEdit->text().trimmed();
}

bool NeoQtSettingsDialog::showLongArgs() const {
  return m_longArgsCheck->isChecked();
}

bool NeoQtSettingsDialog::showVsz() const { return m_vszCheck->isChecked(); }

bool NeoQtSettingsDialog::showSwap() const { return m_swapCheck->isChecked(); }

bool NeoQtSettingsDialog::showIo() const { return m_ioCheck->isChecked(); }

bool NeoQtSettingsDialog::showThreads() const {
  return m_threadsCheck->isChecked();
}

bool NeoQtSettingsDialog::showStartTime() const {
  return m_startTimeCheck->isChecked();
}

bool NeoQtSettingsDialog::showElapsed() const {
  return m_elapsedCheck->isChecked();
}

bool NeoQtSettingsDialog::reverseSort() const {
  return m_reverseCheck->isChecked();
}

int NeoQtSettingsDialog::processLimit() const { return m_limitSpin->value(); }

int NeoQtSettingsDialog::sortMode() const {
  return m_sortCombo->currentData().toInt();
}
