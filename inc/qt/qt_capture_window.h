#ifndef QT_CAPTURE_WINDOW_H
#define QT_CAPTURE_WINDOW_H

#include <QDialog>

extern "C" {
#include "monitoring_capture.h"
}

class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QTimer;
class NeoQtWaveWidget;

/*
 * The Qt equivalent of the CLI's --capture: records CPU/Memory/Swap/IO
 * over a chosen duration (or until manually stopped), shown live with
 * a real wall-clock X axis, and can export the same CSV/HTML pair the
 * CLI produces via the shared monitoring_capture backend.
 *
 * Unlike the always-on "Live Graphs" window, this keeps the FULL
 * session's samples (no rolling cap) so the exported report matches
 * what the CLI would have captured for the same session.
 */
class NeoQtCaptureWindow : public QDialog {
  Q_OBJECT

public:
  explicit NeoQtCaptureWindow(QWidget *parent = nullptr);
  ~NeoQtCaptureWindow() override;

  NeoQtCaptureWindow(const NeoQtCaptureWindow &) = delete;
  NeoQtCaptureWindow &operator=(const NeoQtCaptureWindow &) = delete;

  /* Called by the main window on every local refresh tick; a no-op
   * when a capture isn't currently running. */
  void feedSample(double cpuPercent, double memPercent, double swapPercent,
                  double ioReadMbS, double ioWriteMbS, size_t processCount);

  bool isCapturing() const;

private slots:
  void startCapture();
  void stopCapture();
  void saveReport();
  void graphSelectionChanged(int index);
  void tick();

private:
  void setupUi();
  void updateStatusLabel();
  void updateButtonStates();

  QComboBox *m_graphSelect;
  QSpinBox *m_durationSpin;
  QPushButton *m_startButton;
  QPushButton *m_stopButton;
  QPushButton *m_saveButton;
  QLabel *m_statusLabel;

  NeoQtWaveWidget *m_cpuGraph;
  NeoQtWaveWidget *m_memGraph;
  NeoQtWaveWidget *m_swapGraph;
  NeoQtWaveWidget *m_ioGraph;

  QTimer *m_tickTimer;

  bool m_capturing;
  int m_elapsedSeconds;
  int m_durationLimit; /* 0 = run until manually stopped */

  NeoCaptureSeries m_series;
};

#endif /* QT_CAPTURE_WINDOW_H */
