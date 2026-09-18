#ifndef QT_MAIN_WINDOW_H
#define QT_MAIN_WINDOW_H

#include <QMainWindow>

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

class NeoQtMainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit NeoQtMainWindow(QWidget *parent = nullptr);
  ~NeoQtMainWindow() override = default;

private slots:
  void refresh();
  void toggleMonitoring();
  void showSettings();
  void processSelectionChanged();

private:
  void setupUi();
  void setupToolbar();
  void setupStats();
  QGroupBox *setupFilters();
  void setupProcessView();
  void setupDetails();

  void updateStats();
  void updateDetails();

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

  bool m_monitoring;
};

#endif // QT_MAIN_WINDOW_H
