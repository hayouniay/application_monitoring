#ifndef QT_MAIN_WINDOW_H
#define QT_MAIN_WINDOW_H

#include <QMainWindow>
#include <QVector>

extern "C" {
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

class NeoQtProcessModel;
class NeoQtMonitorController;
class NeoQtRemoteDialog;
class NeoQtGraphsWindow;
class NeoQtCaptureWindow;

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
  void showAboutDialog();
  void processSelectionChanged();

private:
  void setupUi();
  void setupToolbar();
  void setupMenuBar();
  void setupStats();
  QGroupBox *setupFilters();
  void setupProcessView();
  void setupDetails();

  void updateStats();
  void updateDetails();
  void updateThemeButtonLabel();
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

  bool m_monitoring;
  bool m_viewingRemote;
};

#endif // QT_MAIN_WINDOW_H
