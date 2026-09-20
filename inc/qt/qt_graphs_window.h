#ifndef QT_GRAPHS_WINDOW_H
#define QT_GRAPHS_WINDOW_H

#include <QDialog>

class QComboBox;
class NeoQtWaveWidget;

/*
 * A single window holding live CPU / Memory / Swap / I/O waveforms,
 * fed one sample at a time by the main window on every local refresh.
 * A dropdown selects between showing all graphs at once or isolating
 * one - the native-Qt equivalent of the HTML capture report's toggle.
 */
class NeoQtGraphsWindow : public QDialog {
  Q_OBJECT

public:
  explicit NeoQtGraphsWindow(QWidget *parent = nullptr);
  ~NeoQtGraphsWindow() override;

  NeoQtGraphsWindow(const NeoQtGraphsWindow &) = delete;
  NeoQtGraphsWindow &operator=(const NeoQtGraphsWindow &) = delete;

public slots:
  void addSample(double cpuPercent, double memPercent, double swapPercent,
                 double ioReadMbS, double ioWriteMbS);
  void clearAll();

private slots:
  void graphSelectionChanged(int index);

private:
  void setupUi();

  QComboBox *m_graphSelect;

  NeoQtWaveWidget *m_cpuGraph;
  NeoQtWaveWidget *m_memGraph;
  NeoQtWaveWidget *m_swapGraph;
  NeoQtWaveWidget *m_ioGraph;
};

#endif /* QT_GRAPHS_WINDOW_H */
