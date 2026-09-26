#ifndef QT_MAIN_WINDOW_H
#define QT_MAIN_WINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QVector>

extern "C" {
#include "monitoring_alert.h"
#include "monitoring_process_control.h"
#include "monitoring_services.h"
}

class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QCheckBox;
class QSpinBox;
class QTableView;
class QPlainTextEdit;
class QTimer;
class QGroupBox;
class QMenu;
class QCloseEvent;

class NeoQtProcessModel;
class NeoQtMonitorController;
class NeoQtRemoteDialog;
class NeoQtGraphsWindow;
class NeoQtCaptureWindow;
class NeoQtLogWindow;

class NeoQtMainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit NeoQtMainWindow(QWidget *parent = nullptr);
  ~NeoQtMainWindow() override = default;

private slots:
  void refresh();
  void toggleMonitoring();
  void showSettings();
  void showRemoteDialog();
  void showRemoteProcesses(QVector<NeoProcess> processes,
                           NeoSystemInfo systemInfo, QString sourceLabel);
  void backToLocalMonitoring();
  void showGraphsWindow();
  void showCaptureWindow();
  void showLogWindow();
  void showAboutDialog();
  void processSelectionChanged();
  void showProcessContextMenu(const QPoint &pos);
  void sendSignalToSelectedProcess(int signalNumber);
  void reniceSelectedProcess();
  void showAlertSettingsDialog();
  void handleAlertTriggered(int mask, double cpuPercent, double memPercent,
                            QString message);
  void trayIconActivated(QSystemTrayIcon::ActivationReason reason);

protected:
  void closeEvent(QCloseEvent *event) override;
  void changeEvent(QEvent *event) override;

private:
  void setupUi();
  void setupToolbar();
  void setupMenuBar();
  void setupStats();
  QGroupBox *setupFilters();
  void setupProcessView();
  void setupDetails();
  void setupTrayIcon();

  void updateStats();
  void updateDetails();
  void updateThemeButtonLabel();
  void updateTrayTooltip(double cpuPercent, double memPercent);
  QString baseWindowTitle() const;

  NeoQtProcessModel *m_processModel;
  NeoQtMonitorController *m_monitorController;

  QTimer *m_refreshTimer;

  QWidget *m_centralWidget;

  QLabel *m_cpuLabel;
  QLabel *m_memoryLabel;
  QLabel *m_swapLabel;
  QLabel *m_processCountLabel;
  QLabel *m_loadLabel;

  QLineEdit *m_filterEdit;
  QComboBox *m_sortCombo;
  QComboBox *m_stateCombo;
  QCheckBox *m_treeCheck;
  QSpinBox *m_refreshInterval;

  QTableView *m_processView;

  QPlainTextEdit *m_detailText;

  QPushButton *m_refreshButton;
  QPushButton *m_pauseButton;
  QPushButton *m_settingsButton;
  QPushButton *m_themeButton;
  QPushButton *m_remoteButton;
  QPushButton *m_backToLocalButton;

  NeoQtRemoteDialog *m_remoteDialog;
  NeoQtGraphsWindow *m_graphsWindow;
  NeoQtCaptureWindow *m_captureWindow;
  NeoQtLogWindow *m_logWindow;

  QSystemTrayIcon *m_trayIcon;
  QMenu *m_trayMenu;
  bool m_quitting;

  /* Threshold alerting (View menu -> Alerts... / tray notifications).
   * A full NeoConfig is used purely as a convenient bag of the fields
   * alert_evaluate()/alert_dispatch() need - only the alert_* members
   * are ever populated, so there is nothing for config_free() to
   * release. Evaluated against the *local* machine's CPU/memory
   * (there's no equivalent system-wide reading for a remote target),
   * regardless of which process list is currently being viewed. */
  NeoConfig m_alertConfig;
  NeoAlertState m_alertState;

  bool m_monitoring;
  bool m_viewingRemote;
};

#endif // QT_MAIN_WINDOW_H
