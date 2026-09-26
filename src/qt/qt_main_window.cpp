#include "qt/qt_main_window.h"
#include <cstdio>

#include "qt/qt_application.h"
#include "qt/qt_capture_window.h"
#include "qt/qt_graphs_window.h"
#include "qt/qt_log_window.h"
#include "qt/qt_monitor_controller.h"
#include "qt/qt_process_model.h"
#include "qt/qt_remote_dialog.h"
#include "qt/qt_settings_dialog.h"
#include "qt/qt_theme.h"

extern "C" {
#include "monitoring_capture.h"
}

#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QSystemTrayIcon>
#include <QTableView>
#include <QTime>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

extern "C" {
#include <signal.h>
}

NeoQtMainWindow::NeoQtMainWindow(QWidget *parent)
    : QMainWindow(parent), m_processModel(new NeoQtProcessModel(this)),
      m_monitorController(new NeoQtMonitorController(this)),
      m_refreshTimer(new QTimer(this)), m_centralWidget(nullptr),
      m_cpuLabel(nullptr), m_memoryLabel(nullptr), m_swapLabel(nullptr),
      m_processCountLabel(nullptr), m_loadLabel(nullptr), m_filterEdit(nullptr),
      m_sortCombo(nullptr), m_stateCombo(nullptr), m_treeCheck(nullptr),
      m_refreshInterval(nullptr), m_processView(nullptr), m_detailText(nullptr),
      m_refreshButton(nullptr), m_pauseButton(nullptr),
      m_settingsButton(nullptr), m_themeButton(nullptr),
      m_remoteButton(nullptr), m_backToLocalButton(nullptr),
      m_remoteDialog(nullptr), m_graphsWindow(nullptr),
      m_captureWindow(nullptr), m_logWindow(nullptr), m_trayIcon(nullptr),
      m_trayMenu(nullptr), m_quitting(false), m_monitoring(true),
      m_viewingRemote(false) {
  config_init(&m_alertConfig);
  alert_state_init(&m_alertState);

  setupUi();
  setupTrayIcon();

  connect(m_refreshTimer, &QTimer::timeout, this, &NeoQtMainWindow::refresh);

  connect(m_refreshButton, &QPushButton::clicked, this,
          &NeoQtMainWindow::refresh);

  connect(m_pauseButton, &QPushButton::clicked, this,
          &NeoQtMainWindow::toggleMonitoring);

  connect(m_settingsButton, &QPushButton::clicked, this,
          &NeoQtMainWindow::showSettings);

  connect(m_remoteButton, &QPushButton::clicked, this,
          &NeoQtMainWindow::showRemoteDialog);

  connect(m_backToLocalButton, &QPushButton::clicked, this,
          &NeoQtMainWindow::backToLocalMonitoring);

  connect(m_filterEdit, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            m_monitorController->setSearchPattern(text);
          });

  connect(m_stateCombo, &QComboBox::currentIndexChanged, this,
          [this](int index) {
            if (index < 0)
              return;

            m_monitorController->setStateFilter(
                m_stateCombo->itemData(index).toString());
          });

  connect(m_sortCombo, &QComboBox::currentIndexChanged, this,
          [this](int index) {
            if (index < 0)
              return;

            const NeoSortMode mode =
                static_cast<NeoSortMode>(m_sortCombo->itemData(index).toInt());

            m_monitorController->setSortMode(mode);
          });

  connect(m_processView->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          &NeoQtMainWindow::processSelectionChanged);

  connect(m_monitorController, &NeoQtMonitorController::processesUpdated, this,
          [this]() {
            m_processModel->setProcesses(m_monitorController->processes());

            updateStats();
            updateDetails();

            double cpuPercent = 0.0;
            double memPercent = 0.0;
            double swapPercent = 0.0;

            capture_read_system(&cpuPercent, &memPercent, &swapPercent);

            updateTrayTooltip(cpuPercent, memPercent);

            const int fired = alert_evaluate(&m_alertConfig, &m_alertState,
                                             cpuPercent, memPercent);

            if (fired != 0) {
              QString message;

              if (fired & NEO_ALERT_CPU) {
                message = QStringLiteral("CPU usage %1% has stayed above %2%")
                              .arg(cpuPercent, 0, 'f', 1)
                              .arg(m_alertConfig.alert_cpu_percent, 0, 'f', 1);
              }

              if (fired & NEO_ALERT_MEM) {
                if (!message.isEmpty()) {
                  message += QStringLiteral("\n");
                }

                message +=
                    QStringLiteral("Memory usage %1% has stayed above %2%")
                        .arg(memPercent, 0, 'f', 1)
                        .arg(m_alertConfig.alert_mem_percent, 0, 'f', 1);
              }

              handleAlertTriggered(fired, cpuPercent, memPercent, message);
            }

            if (m_graphsWindow != nullptr || m_captureWindow != nullptr) {

              double ioRead = 0.0;
              double ioWrite = 0.0;

              const NeoProcessList &list = m_monitorController->processes();

              for (size_t i = 0; i < list.count; ++i) {
                ioRead += list.items[i].io_read_mb_s;
                ioWrite += list.items[i].io_write_mb_s;
              }

              if (m_graphsWindow != nullptr) {
                m_graphsWindow->addSample(cpuPercent, memPercent, swapPercent,
                                          ioRead, ioWrite);
              }

              if (m_captureWindow != nullptr) {
                m_captureWindow->feedSample(cpuPercent, memPercent, swapPercent,
                                            ioRead, ioWrite, list.count);
              }
            }
          });

  connect(m_monitorController, &NeoQtMonitorController::systemInfoUpdated, this,
          &NeoQtMainWindow::updateStats);

  connect(
      m_monitorController, &NeoQtMonitorController::errorOccurred, this,
      [this](const QString &message) { statusBar()->showMessage(message); });

  m_refreshTimer->setInterval(m_refreshInterval->value());

  m_monitorController->start();

  m_processModel->setProcesses(m_monitorController->processes());

  updateStats();

  m_processView->resizeColumnsToContents();

  m_refreshTimer->start();
}

void NeoQtMainWindow::setupUi() {
  setWindowTitle(baseWindowTitle());

  resize(1400, 850);

  setupToolbar();
  setupMenuBar();

  m_centralWidget = new QWidget(this);

  setCentralWidget(m_centralWidget);

  auto *mainLayout = new QVBoxLayout(m_centralWidget);

  mainLayout->setContentsMargins(14, 14, 14, 14);

  mainLayout->setSpacing(10);

  setupStats();

  auto *statsGroup = new QGroupBox(QStringLiteral("System"), m_centralWidget);

  auto *statsLayout = new QHBoxLayout(statsGroup);

  statsLayout->addWidget(m_cpuLabel);
  statsLayout->addWidget(m_memoryLabel);
  statsLayout->addWidget(m_swapLabel);
  statsLayout->addWidget(m_processCountLabel);
  statsLayout->addWidget(m_loadLabel);
  statsLayout->addStretch();

  mainLayout->addWidget(statsGroup);

  QGroupBox *filterGroup = setupFilters();

  mainLayout->addWidget(filterGroup);

  setupProcessView();
  setupDetails();

  auto *splitter = new QSplitter(Qt::Vertical, m_centralWidget);

  splitter->addWidget(m_processView);
  splitter->addWidget(m_detailText);

  splitter->setStretchFactor(0, 4);
  splitter->setStretchFactor(1, 1);

  mainLayout->addWidget(splitter, 1);

  statusBar()->showMessage(QStringLiteral("Monitoring ready"));
}

void NeoQtMainWindow::setupToolbar() {
  auto *toolbar = addToolBar(QStringLiteral("Monitoring"));

  toolbar->setMovable(false);

  m_refreshButton = new QPushButton(QStringLiteral("Refresh"), this);

  m_pauseButton = new QPushButton(QStringLiteral("Pause"), this);

  m_settingsButton = new QPushButton(QStringLiteral("Settings"), this);

  m_remoteButton = new QPushButton(QStringLiteral("Connect to Remote"), this);

  m_backToLocalButton = new QPushButton(QStringLiteral("Back to Local"), this);

  m_backToLocalButton->setVisible(false);

  m_themeButton = new QPushButton(this);

  m_themeButton->setObjectName(QStringLiteral("themeToggleButton"));

  m_themeButton->setCursor(Qt::PointingHandCursor);

  toolbar->addWidget(m_refreshButton);
  toolbar->addWidget(m_pauseButton);
  toolbar->addWidget(m_settingsButton);
  toolbar->addWidget(m_remoteButton);

  /*
   * Deliberately NOT added to the toolbar: with enough buttons
   * already there, a narrower window pushes extra widgets into
   * Qt's toolbar overflow/extension area, where they become
   * effectively unclickable (a real, reproducible bug caught while
   * testing this). The status bar's permanent-widget area has no
   * such overflow behavior and is a natural home for this kind of
   * contextual "you're viewing X, click to go back" control.
   */
  statusBar()->addPermanentWidget(m_backToLocalButton);

  /*
   * Push the theme toggle to the far right of the toolbar.
   */
  auto *spacer = new QWidget(this);

  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  toolbar->addWidget(spacer);
  toolbar->addWidget(m_themeButton);

  updateThemeButtonLabel();

  connect(m_themeButton, &QPushButton::clicked, this, [this]() {
    if (auto *application = qobject_cast<NeoQtApplication *>(qApp)) {
      application->toggleTheme();
    }
  });

  if (auto *application = qobject_cast<NeoQtApplication *>(qApp)) {
    connect(application, &NeoQtApplication::themeChanged, this,
            [this](NeoTheme) { updateThemeButtonLabel(); });
  }
}

void NeoQtMainWindow::setupMenuBar() {
  auto *viewMenu = menuBar()->addMenu(QStringLiteral("&View"));

  QAction *liveGraphsAction =
      viewMenu->addAction(QStringLiteral("Live Graphs..."));

  connect(liveGraphsAction, &QAction::triggered, this,
          &NeoQtMainWindow::showGraphsWindow);

  QAction *captureAction = viewMenu->addAction(QStringLiteral("Capture..."));

  connect(captureAction, &QAction::triggered, this,
          &NeoQtMainWindow::showCaptureWindow);

  QAction *logsAction = viewMenu->addAction(QStringLiteral("Logs..."));

  connect(logsAction, &QAction::triggered, this,
          &NeoQtMainWindow::showLogWindow);

  viewMenu->addSeparator();

  QAction *alertsAction =
      viewMenu->addAction(QStringLiteral("Alert Thresholds..."));

  connect(alertsAction, &QAction::triggered, this,
          &NeoQtMainWindow::showAlertSettingsDialog);

  auto *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));

  QAction *aboutAction =
      helpMenu->addAction(QStringLiteral("About NEO Monitoring Services"));

  connect(aboutAction, &QAction::triggered, this,
          &NeoQtMainWindow::showAboutDialog);
}

void NeoQtMainWindow::setupStats() {
  m_cpuLabel = new QLabel(QStringLiteral("CPU: --"), this);

  m_memoryLabel = new QLabel(QStringLiteral("Memory: --"), this);

  m_swapLabel = new QLabel(QStringLiteral("Swap: --"), this);

  m_processCountLabel = new QLabel(QStringLiteral("Processes: 0"), this);

  m_loadLabel = new QLabel(QStringLiteral("Load: --"), this);
}

QGroupBox *NeoQtMainWindow::setupFilters() {
  auto *group =
      new QGroupBox(QStringLiteral("Filters & Sorting"), m_centralWidget);

  auto *layout = new QHBoxLayout(group);

  layout->setContentsMargins(8, 8, 8, 8);

  auto *filterLabel = new QLabel(QStringLiteral("Filter:"), group);

  m_filterEdit = new QLineEdit(group);

  m_filterEdit->setPlaceholderText(QStringLiteral("Search process..."));

  auto *stateLabel = new QLabel(QStringLiteral("State:"), group);

  m_stateCombo = new QComboBox(group);

  m_stateCombo->addItem(QStringLiteral("All"), QString());

  m_stateCombo->addItem(QStringLiteral("Running"), QStringLiteral("R"));

  m_stateCombo->addItem(QStringLiteral("Sleeping"), QStringLiteral("S"));

  m_stateCombo->addItem(QStringLiteral("Stopped"), QStringLiteral("T"));

  m_stateCombo->addItem(QStringLiteral("Zombie"), QStringLiteral("Z"));

  auto *sortLabel = new QLabel(QStringLiteral("Sort:"), group);

  m_sortCombo = new QComboBox(group);

  m_sortCombo->addItem(QStringLiteral("CPU"), static_cast<int>(SORT_CPU));

  m_sortCombo->addItem(QStringLiteral("Memory"), static_cast<int>(SORT_MEM));

  m_sortCombo->addItem(QStringLiteral("PID"), static_cast<int>(SORT_PID));

  m_sortCombo->addItem(QStringLiteral("RSS"), static_cast<int>(SORT_RSS));

  m_sortCombo->addItem(QStringLiteral("I/O Read"),
                       static_cast<int>(SORT_IO_READ));

  m_sortCombo->addItem(QStringLiteral("I/O Write"),
                       static_cast<int>(SORT_IO_WRITE));

  m_treeCheck = new QCheckBox(QStringLiteral("Tree"), group);

  m_refreshInterval = new QSpinBox(group);

  m_refreshInterval->setRange(100, 60000);

  m_refreshInterval->setValue(1000);

  m_refreshInterval->setSuffix(QStringLiteral(" ms"));

  layout->addWidget(filterLabel);
  layout->addWidget(m_filterEdit, 1);

  layout->addWidget(stateLabel);
  layout->addWidget(m_stateCombo);

  layout->addWidget(sortLabel);
  layout->addWidget(m_sortCombo);

  layout->addWidget(m_treeCheck);

  layout->addWidget(new QLabel(QStringLiteral("Interval:"), group));

  layout->addWidget(m_refreshInterval);

  connect(m_refreshInterval, &QSpinBox::valueChanged, this, [this](int value) {
    m_monitorController->setInterval(static_cast<double>(value) / 1000.0);

    m_refreshTimer->setInterval(value);
  });

  return group;
}

void NeoQtMainWindow::setupProcessView() {
  m_processView = new QTableView(m_centralWidget);

  m_processView->setModel(m_processModel);

  m_processView->setSelectionBehavior(QAbstractItemView::SelectRows);

  m_processView->setSelectionMode(QAbstractItemView::SingleSelection);

  m_processView->setAlternatingRowColors(true);

  m_processView->setShowGrid(false);

  m_processView->verticalHeader()->setVisible(false);

  m_processView->horizontalHeader()->setStretchLastSection(true);

  m_processView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);

  m_processView->setMinimumHeight(400);

  m_processView->setContextMenuPolicy(Qt::CustomContextMenu);

  connect(m_processView, &QWidget::customContextMenuRequested, this,
          &NeoQtMainWindow::showProcessContextMenu);
}

void NeoQtMainWindow::setupDetails() {
  m_detailText = new QPlainTextEdit(m_centralWidget);

  m_detailText->setReadOnly(true);

  m_detailText->setMaximumHeight(180);

  m_detailText->setPlaceholderText(
      QStringLiteral("Select a process to display details."));
}

void NeoQtMainWindow::refresh() {
  if (!m_monitoring)
    return;

  m_monitorController->refresh();

  m_processModel->setProcesses(m_monitorController->processes());

  updateStats();
  updateDetails();

  statusBar()->showMessage(
      QStringLiteral("Last refresh: %1")
          .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
}

void NeoQtMainWindow::toggleMonitoring() {
  m_monitoring = !m_monitoring;

  if (m_monitoring) {
    m_pauseButton->setText(QStringLiteral("Pause"));

    m_monitorController->start();

    m_refreshTimer->start(m_refreshInterval->value());

    refresh();

    statusBar()->showMessage(QStringLiteral("Monitoring resumed"));
  } else {
    m_pauseButton->setText(QStringLiteral("Resume"));

    m_refreshTimer->stop();

    m_monitorController->stop();

    statusBar()->showMessage(QStringLiteral("Monitoring paused"));
  }
}

void NeoQtMainWindow::showSettings() {
  NeoQtSettingsDialog dialog(this);

  dialog.setConfig(m_monitorController->config());

  dialog.exec();
}

void NeoQtMainWindow::showRemoteDialog() {
  if (m_remoteDialog == nullptr) {
    m_remoteDialog = new NeoQtRemoteDialog(this);

    connect(m_remoteDialog, &QObject::destroyed, this,
            [this]() { m_remoteDialog = nullptr; });

    connect(m_remoteDialog, &NeoQtRemoteDialog::processesFetched, this,
            &NeoQtMainWindow::showRemoteProcesses);
  }

  m_remoteDialog->show();
  m_remoteDialog->raise();
  m_remoteDialog->activateWindow();
}

void NeoQtMainWindow::showRemoteProcesses(QVector<NeoProcess> processes,
                                          NeoSystemInfo systemInfo,
                                          QString sourceLabel) {
  Q_UNUSED(systemInfo);

  /*
   * Pause local polling while showing remote data so the refresh
   * timer doesn't immediately overwrite it with local /proc results.
   */
  if (m_monitoring) {
    m_refreshTimer->stop();
    m_monitorController->stop();

    m_monitoring = false;

    m_pauseButton->setText(QStringLiteral("Resume"));
  }

  m_processModel->setProcesses(processes);

  updateStats();
  updateDetails();

  m_viewingRemote = true;

  setWindowTitle(
      QStringLiteral("%1  —  Remote: %2").arg(baseWindowTitle(), sourceLabel));

  statusBar()->showMessage(QStringLiteral("Showing %1 process(es) from %2")
                               .arg(processes.count())
                               .arg(sourceLabel));

  m_refreshButton->setEnabled(false);
  m_backToLocalButton->setVisible(true);
}

void NeoQtMainWindow::backToLocalMonitoring() {
  if (!m_viewingRemote)
    return;

  m_viewingRemote = false;

  setWindowTitle(baseWindowTitle());

  m_backToLocalButton->setVisible(false);
  m_refreshButton->setEnabled(true);

  m_monitoring = true;

  m_pauseButton->setText(QStringLiteral("Pause"));

  m_monitorController->start();

  m_refreshTimer->start(m_refreshInterval->value());

  refresh();

  statusBar()->showMessage(QStringLiteral("Back to local monitoring"));
}

void NeoQtMainWindow::showGraphsWindow() {
  if (m_graphsWindow == nullptr) {
    m_graphsWindow = new NeoQtGraphsWindow(this);

    connect(m_graphsWindow, &QObject::destroyed, this,
            [this]() { m_graphsWindow = nullptr; });
  }

  m_graphsWindow->show();
  m_graphsWindow->raise();
  m_graphsWindow->activateWindow();
}

void NeoQtMainWindow::showCaptureWindow() {
  if (m_captureWindow == nullptr) {
    m_captureWindow = new NeoQtCaptureWindow(this);

    connect(m_captureWindow, &QObject::destroyed, this,
            [this]() { m_captureWindow = nullptr; });
  }

  m_captureWindow->show();
  m_captureWindow->raise();
  m_captureWindow->activateWindow();
}

void NeoQtMainWindow::showLogWindow() {
  if (m_logWindow == nullptr) {
    m_logWindow = new NeoQtLogWindow(this);

    connect(m_logWindow, &QObject::destroyed, this,
            [this]() { m_logWindow = nullptr; });
  }

  m_logWindow->show();
  m_logWindow->raise();
  m_logWindow->activateWindow();
}

void NeoQtMainWindow::showAlertSettingsDialog() {
  QDialog dialog(this);
  dialog.setWindowTitle(QStringLiteral("Alert Thresholds"));

  auto *form = new QFormLayout();

  auto *cpuSpin = new QDoubleSpinBox(&dialog);
  cpuSpin->setRange(0.0, 100.0);
  cpuSpin->setSuffix(QStringLiteral(" %"));
  cpuSpin->setSpecialValueText(QStringLiteral("Off"));
  cpuSpin->setValue(m_alertConfig.alert_cpu_percent);
  form->addRow(QStringLiteral("CPU threshold:"), cpuSpin);

  auto *memSpin = new QDoubleSpinBox(&dialog);
  memSpin->setRange(0.0, 100.0);
  memSpin->setSuffix(QStringLiteral(" %"));
  memSpin->setSpecialValueText(QStringLiteral("Off"));
  memSpin->setValue(m_alertConfig.alert_mem_percent);
  form->addRow(QStringLiteral("Memory threshold:"), memSpin);

  auto *durationSpin = new QDoubleSpinBox(&dialog);
  durationSpin->setRange(0.0, 3600.0);
  durationSpin->setSuffix(QStringLiteral(" s"));
  durationSpin->setValue(m_alertConfig.alert_sustain_seconds);
  form->addRow(QStringLiteral("Must persist for:"), durationSpin);

  auto *notifyCheck =
      new QCheckBox(QStringLiteral("Desktop notification"), &dialog);
  notifyCheck->setChecked(m_alertConfig.alert_notify);
  form->addRow(QString(), notifyCheck);

  auto *webhookEdit = new QLineEdit(&dialog);
  webhookEdit->setPlaceholderText(
      QStringLiteral("https://example.com/hook (optional)"));
  webhookEdit->setText(QString::fromUtf8(m_alertConfig.alert_webhook));
  form->addRow(QStringLiteral("Webhook URL:"), webhookEdit);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto *mainLayout = new QVBoxLayout(&dialog);
  mainLayout->addLayout(form);
  mainLayout->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  m_alertConfig.alert_cpu_percent = cpuSpin->value();
  m_alertConfig.alert_mem_percent = memSpin->value();
  m_alertConfig.alert_sustain_seconds = durationSpin->value();
  m_alertConfig.alert_notify = notifyCheck->isChecked();

  snprintf(m_alertConfig.alert_webhook, sizeof(m_alertConfig.alert_webhook),
           "%s", webhookEdit->text().toUtf8().constData());

  /* A changed configuration should get a clean slate rather than
   * possibly reporting a "sustained" breach measured partly against
   * the old thresholds. */
  alert_state_init(&m_alertState);

  statusBar()->showMessage(QStringLiteral("Alert thresholds updated"), 3000);
}

void NeoQtMainWindow::handleAlertTriggered(int mask, double cpuPercent,
                                           double memPercent, QString message) {
  statusBar()->showMessage(message, 8000);

  if (m_trayIcon != nullptr) {
    m_trayIcon->showMessage(
        QStringLiteral("app_top_monitoring: threshold alert"), message,
        QSystemTrayIcon::Warning, 8000);
  }

  if (m_alertConfig.alert_webhook[0] != '\0') {
    const QString program = QStringLiteral("curl");
    const QStringList args = {
        QStringLiteral("-s"),
        QStringLiteral("-m"),
        QStringLiteral("5"),
        QStringLiteral("-X"),
        QStringLiteral("POST"),
        QStringLiteral("-H"),
        QStringLiteral("Content-Type: application/json"),
        QStringLiteral("-d"),
        QStringLiteral("{\"alert\":\"%1\",\"cpu\":%2,\"memory\":%3}")
            .arg((mask & NEO_ALERT_CPU) ? QStringLiteral("cpu")
                                        : QStringLiteral("memory"))
            .arg(cpuPercent, 0, 'f', 2)
            .arg(memPercent, 0, 'f', 2),
        QString::fromUtf8(m_alertConfig.alert_webhook)};

    /* Fire-and-forget, off the UI thread's event loop, so a slow or
     * unreachable webhook endpoint never stalls the interface. */
    QProcess::startDetached(program, args);
  }
}

void NeoQtMainWindow::processSelectionChanged() { updateDetails(); }

void NeoQtMainWindow::updateStats() {
  const NeoSystemInfo &info = m_monitorController->systemInfo();

  Q_UNUSED(info);

  m_processCountLabel->setText(
      QStringLiteral("Processes: %1").arg(m_processModel->rowCount()));
}

void NeoQtMainWindow::updateDetails() {
  const QModelIndex index = m_processView->currentIndex();

  if (!index.isValid()) {
    m_detailText->clear();
    return;
  }

  const NeoProcess *process = m_processModel->processAt(index.row());

  if (process == nullptr) {
    m_detailText->clear();
    return;
  }

  m_detailText->setPlainText(
      QStringLiteral("PID: %1\n"
                     "User: %2\n"
                     "State: %3\n"
                     "Command: %4\n"
                     "CPU: %5%\n"
                     "Memory: %6%\n"
                     "RSS: %7 MB\n"
                     "VSZ: %8 MB\n"
                     "Threads: %9")
          .arg(process->pid)
          .arg(QString::fromLocal8Bit(process->user))
          .arg(QString(process->state))
          .arg(QString::fromLocal8Bit(process->comm))
          .arg(process->cpu_percent, 0, 'f', 1)
          .arg(process->mem_percent, 0, 'f', 1)
          .arg(process->rss_mb, 0, 'f', 1)
          .arg(process->vsz_mb, 0, 'f', 1)
          .arg(static_cast<qulonglong>(process->threads)));
}

void NeoQtMainWindow::updateThemeButtonLabel() {
  if (m_themeButton == nullptr)
    return;

  if (auto *application = qobject_cast<NeoQtApplication *>(qApp)) {
    const bool isDark = application->currentTheme() == NeoTheme::Dark;

    m_themeButton->setText(isDark ? QStringLiteral("\u2600  Light Mode")
                                  : QStringLiteral("\U0001F319  Dark Mode"));
  }
}

QString NeoQtMainWindow::baseWindowTitle() const {
  return QStringLiteral("NEO Monitoring Services v%1")
      .arg(qApp->applicationVersion());
}

void NeoQtMainWindow::showAboutDialog() {
  QMessageBox::about(
      this, QStringLiteral("About NEO Monitoring Services"),
      QStringLiteral(
          "<h3>NEO Monitoring Services</h3>"
          "<p>Version %1</p>"
          "<p>A top-like Linux process monitor with remote SSH/Telnet "
          "monitoring, FTP/TFTP deployment, and live capture graphing, "
          "sharing one C backend between the CLI and this Qt "
          "application.</p>"
          "<p>Built with Qt %2.</p>")
          .arg(qApp->applicationVersion(), QString::fromLatin1(qVersion())));
}

void NeoQtMainWindow::showProcessContextMenu(const QPoint &pos) {
  const QModelIndex index = m_processView->indexAt(pos);

  if (!index.isValid()) {
    return;
  }

  m_processView->setCurrentIndex(index);

  const NeoProcess *process = m_processModel->processAt(index.row());

  if (process == nullptr) {
    return;
  }

  QMenu menu(this);

  if (m_viewingRemote) {
    QAction *disabledAction =
        menu.addAction(QStringLiteral("Not available for remote processes"));
    disabledAction->setEnabled(false);

    menu.exec(m_processView->viewport()->mapToGlobal(pos));
    return;
  }

  QAction *titleAction =
      menu.addAction(QStringLiteral("PID %1 (%2)")
                         .arg(process->pid)
                         .arg(QString::fromLocal8Bit(process->comm)));
  titleAction->setEnabled(false);

  menu.addSeparator();

  QAction *termAction = menu.addAction(QStringLiteral("Send SIGTERM"));
  connect(termAction, &QAction::triggered, this,
          [this]() { sendSignalToSelectedProcess(SIGTERM); });

  QAction *killAction = menu.addAction(QStringLiteral("Send SIGKILL"));
  connect(killAction, &QAction::triggered, this,
          [this]() { sendSignalToSelectedProcess(SIGKILL); });

  menu.addSeparator();

  QAction *reniceAction = menu.addAction(QStringLiteral("Renice..."));
  connect(reniceAction, &QAction::triggered, this,
          &NeoQtMainWindow::reniceSelectedProcess);

  menu.exec(m_processView->viewport()->mapToGlobal(pos));
}

void NeoQtMainWindow::sendSignalToSelectedProcess(int signalNumber) {
  if (m_viewingRemote) {
    return;
  }

  const QModelIndex index = m_processView->currentIndex();

  if (!index.isValid()) {
    return;
  }

  const NeoProcess *process = m_processModel->processAt(index.row());

  if (process == nullptr) {
    return;
  }

  const pid_t pid = process->pid;
  const QString comm = QString::fromLocal8Bit(process->comm);
  const QString signalName = signalNumber == SIGKILL
                                 ? QStringLiteral("SIGKILL")
                                 : QStringLiteral("SIGTERM");

  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, QStringLiteral("Confirm %1").arg(signalName),
      QStringLiteral("Send %1 to PID %2 (%3)?")
          .arg(signalName)
          .arg(pid)
          .arg(comm),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    return;
  }

  char message[256];

  if (process_send_signal(pid, signalNumber, message, sizeof(message)) != 0) {
    statusBar()->showMessage(QString::fromUtf8(message), 6000);
  } else {
    statusBar()->showMessage(
        QStringLiteral("Sent %1 to PID %2").arg(signalName).arg(pid), 4000);
  }

  refresh();
}

void NeoQtMainWindow::reniceSelectedProcess() {
  if (m_viewingRemote) {
    return;
  }

  const QModelIndex index = m_processView->currentIndex();

  if (!index.isValid()) {
    return;
  }

  const NeoProcess *process = m_processModel->processAt(index.row());

  if (process == nullptr) {
    return;
  }

  const pid_t pid = process->pid;
  const QString comm = QString::fromLocal8Bit(process->comm);

  bool ok = false;

  const int value = QInputDialog::getInt(
      this, QStringLiteral("Renice PID %1").arg(pid),
      QStringLiteral("New niceness for %1 (-20 = highest priority, "
                     "19 = lowest):")
          .arg(comm),
      0, -20, 19, 1, &ok);

  if (!ok) {
    return;
  }

  char message[256];

  if (process_renice(pid, value, message, sizeof(message)) != 0) {
    statusBar()->showMessage(QString::fromUtf8(message), 6000);
  } else {
    statusBar()->showMessage(
        QStringLiteral("Reniced PID %1 to %2").arg(pid).arg(value), 4000);
  }

  refresh();
}

void NeoQtMainWindow::setupTrayIcon() {
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    m_trayIcon = nullptr;
    return;
  }

  QPixmap pixmap(32, 32);
  pixmap.fill(Qt::transparent);

  {
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(0x3c, 0x9e, 0xe0));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(2, 2, 28, 28);
    painter.setPen(QPen(Qt::white, 2));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("N"));
  }

  m_trayIcon = new QSystemTrayIcon(QIcon(pixmap), this);
  m_trayIcon->setToolTip(QStringLiteral("NEO Monitoring Services"));

  m_trayMenu = new QMenu(this);

  QAction *showHideAction = m_trayMenu->addAction(QStringLiteral("Show/Hide"));

  connect(showHideAction, &QAction::triggered, this, [this]() {
    if (isVisible()) {
      hide();
    } else {
      showNormal();
      raise();
      activateWindow();
    }
  });

  m_trayMenu->addSeparator();

  QAction *quitAction = m_trayMenu->addAction(QStringLiteral("Quit"));

  connect(quitAction, &QAction::triggered, this, [this]() {
    m_quitting = true;
    close();
  });

  m_trayIcon->setContextMenu(m_trayMenu);

  connect(m_trayIcon, &QSystemTrayIcon::activated, this,
          &NeoQtMainWindow::trayIconActivated);

  m_trayIcon->show();
}

void NeoQtMainWindow::updateTrayTooltip(double cpuPercent, double memPercent) {
  if (m_trayIcon == nullptr) {
    return;
  }

  m_trayIcon->setToolTip(
      QStringLiteral("NEO Monitoring Services\nCPU: %1%   Memory: %2%")
          .arg(cpuPercent, 0, 'f', 1)
          .arg(memPercent, 0, 'f', 1));
}

void NeoQtMainWindow::trayIconActivated(
    QSystemTrayIcon::ActivationReason reason) {
  if (reason == QSystemTrayIcon::Trigger ||
      reason == QSystemTrayIcon::DoubleClick) {
    if (isVisible() && !isMinimized()) {
      hide();
    } else {
      showNormal();
      raise();
      activateWindow();
    }
  }
}

void NeoQtMainWindow::closeEvent(QCloseEvent *event) {
  if (!m_quitting && m_trayIcon != nullptr && m_trayIcon->isVisible()) {
    hide();
    event->ignore();
    return;
  }

  QMainWindow::closeEvent(event);
}

void NeoQtMainWindow::changeEvent(QEvent *event) {
  QMainWindow::changeEvent(event);

  if (event->type() == QEvent::WindowStateChange && isMinimized() &&
      m_trayIcon != nullptr && m_trayIcon->isVisible()) {
    QTimer::singleShot(0, this, &QWidget::hide);
  }
}
