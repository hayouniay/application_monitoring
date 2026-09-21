#include "qt/qt_capture_window.h"
#include "qt/qt_wave_widget.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

NeoQtCaptureWindow::NeoQtCaptureWindow(QWidget *parent)
    : QDialog(parent), m_graphSelect(nullptr), m_durationSpin(nullptr),
      m_startButton(nullptr), m_stopButton(nullptr), m_saveButton(nullptr),
      m_statusLabel(nullptr), m_cpuGraph(nullptr), m_memGraph(nullptr),
      m_swapGraph(nullptr), m_ioGraph(nullptr), m_tickTimer(new QTimer(this)),
      m_capturing(false), m_elapsedSeconds(0), m_durationLimit(0) {
  setWindowTitle(QStringLiteral("Capture"));

  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose);

  resize(760, 660);

  capture_series_init(&m_series);

  setupUi();

  connect(m_tickTimer, &QTimer::timeout, this, &NeoQtCaptureWindow::tick);

  updateButtonStates();
  updateStatusLabel();
}

NeoQtCaptureWindow::~NeoQtCaptureWindow() { capture_series_free(&m_series); }

void NeoQtCaptureWindow::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);

  auto *controlsLayout = new QHBoxLayout();

  auto *durationLabel = new QLabel(QStringLiteral("Duration:"), this);

  m_durationSpin = new QSpinBox(this);
  m_durationSpin->setRange(0, 86400);
  m_durationSpin->setSuffix(QStringLiteral(" s (0 = until stopped)"));
  m_durationSpin->setValue(30);
  m_durationSpin->setMinimumWidth(190);

  m_startButton = new QPushButton(QStringLiteral("Start Capture"), this);
  m_stopButton = new QPushButton(QStringLiteral("Stop"), this);
  m_saveButton = new QPushButton(QStringLiteral("Save Report..."), this);

  auto *graphSelectLabel = new QLabel(QStringLiteral("Show:"), this);

  m_graphSelect = new QComboBox(this);
  m_graphSelect->addItem(QStringLiteral("All graphs"), QStringLiteral("all"));
  m_graphSelect->addItem(QStringLiteral("CPU only"), QStringLiteral("cpu"));
  m_graphSelect->addItem(QStringLiteral("Memory only"), QStringLiteral("mem"));
  m_graphSelect->addItem(QStringLiteral("Swap only"), QStringLiteral("swap"));
  m_graphSelect->addItem(QStringLiteral("I/O only"), QStringLiteral("io"));

  controlsLayout->addWidget(durationLabel);
  controlsLayout->addWidget(m_durationSpin);
  controlsLayout->addWidget(m_startButton);
  controlsLayout->addWidget(m_stopButton);
  controlsLayout->addWidget(m_saveButton);
  controlsLayout->addStretch(1);
  controlsLayout->addWidget(graphSelectLabel);
  controlsLayout->addWidget(m_graphSelect);

  mainLayout->addLayout(controlsLayout);

  m_statusLabel = new QLabel(this);
  mainLayout->addWidget(m_statusLabel);

  m_cpuGraph = new NeoQtWaveWidget(this);
  m_cpuGraph->setSeries(QStringLiteral("CPU"), QColor(0x2f, 0x6f, 0xed));
  m_cpuGraph->setUnit(QStringLiteral("%"));
  m_cpuGraph->setFixedRange(0.0, 100.0);
  m_cpuGraph->setShowTimeAxis(true);
  m_cpuGraph->setMaxSamples(
      100000); /* keep the whole session, no rolling cap */

  m_memGraph = new NeoQtWaveWidget(this);
  m_memGraph->setSeries(QStringLiteral("Memory"), QColor(0xe0, 0x57, 0x4c));
  m_memGraph->setUnit(QStringLiteral("%"));
  m_memGraph->setFixedRange(0.0, 100.0);
  m_memGraph->setShowTimeAxis(true);
  m_memGraph->setMaxSamples(100000);

  m_swapGraph = new NeoQtWaveWidget(this);
  m_swapGraph->setSeries(QStringLiteral("Swap"), QColor(0xd9, 0xa4, 0x41));
  m_swapGraph->setUnit(QStringLiteral("%"));
  m_swapGraph->setFixedRange(0.0, 100.0);
  m_swapGraph->setShowTimeAxis(true);
  m_swapGraph->setMaxSamples(100000);

  m_ioGraph = new NeoQtWaveWidget(this);
  m_ioGraph->setSeries(QStringLiteral("Read"), QColor(0x2f, 0xa8, 0x4f));
  m_ioGraph->setSecondSeries(QStringLiteral("Write"), QColor(0x8a, 0x4f, 0xe0));
  m_ioGraph->setUnit(QStringLiteral(" MB/s"));
  m_ioGraph->setAutoRange();
  m_ioGraph->setShowTimeAxis(true);
  m_ioGraph->setMaxSamples(100000);

  mainLayout->addWidget(m_cpuGraph, 1);
  mainLayout->addWidget(m_memGraph, 1);
  mainLayout->addWidget(m_swapGraph, 1);
  mainLayout->addWidget(m_ioGraph, 1);

  connect(m_startButton, &QPushButton::clicked, this,
          &NeoQtCaptureWindow::startCapture);
  connect(m_stopButton, &QPushButton::clicked, this,
          &NeoQtCaptureWindow::stopCapture);
  connect(m_saveButton, &QPushButton::clicked, this,
          &NeoQtCaptureWindow::saveReport);
  connect(m_graphSelect, &QComboBox::currentIndexChanged, this,
          &NeoQtCaptureWindow::graphSelectionChanged);
}

bool NeoQtCaptureWindow::isCapturing() const { return m_capturing; }

void NeoQtCaptureWindow::updateButtonStates() {
  m_startButton->setEnabled(!m_capturing);
  m_durationSpin->setEnabled(!m_capturing);
  m_stopButton->setEnabled(m_capturing);
  m_saveButton->setEnabled(!m_capturing && m_series.count > 0);
}

void NeoQtCaptureWindow::updateStatusLabel() {
  if (m_capturing) {
    const QString limitText =
        (m_durationLimit > 0) ? QStringLiteral(" / %1s").arg(m_durationLimit)
                              : QString();

    m_statusLabel->setText(
        QStringLiteral("Capturing... %1%2 elapsed, %3 sample(s)")
            .arg(m_elapsedSeconds)
            .arg(limitText)
            .arg(m_series.count));
  } else if (m_series.count > 0) {
    m_statusLabel->setText(
        QStringLiteral("Stopped. %1 sample(s) captured over %2s.")
            .arg(m_series.count)
            .arg(m_elapsedSeconds));
  } else {
    m_statusLabel->setText(
        QStringLiteral("Not capturing. Set a duration (0 = until "
                       "stopped) and click Start Capture."));
  }
}

void NeoQtCaptureWindow::startCapture() {
  capture_series_free(&m_series);
  capture_series_init(&m_series);

  m_cpuGraph->clear();
  m_memGraph->clear();
  m_swapGraph->clear();
  m_ioGraph->clear();

  m_capturing = true;
  m_elapsedSeconds = 0;
  m_durationLimit = m_durationSpin->value();

  m_tickTimer->start(1000);

  updateButtonStates();
  updateStatusLabel();
}

void NeoQtCaptureWindow::stopCapture() {
  m_capturing = false;
  m_tickTimer->stop();

  updateButtonStates();
  updateStatusLabel();
}

void NeoQtCaptureWindow::tick() {
  ++m_elapsedSeconds;

  updateStatusLabel();

  if (m_durationLimit > 0 && m_elapsedSeconds >= m_durationLimit) {
    stopCapture();
  }
}

void NeoQtCaptureWindow::feedSample(double cpuPercent, double memPercent,
                                    double swapPercent, double ioReadMbS,
                                    double ioWriteMbS, size_t processCount) {
  if (!m_capturing) {
    return;
  }

  capture_append_sample(&m_series, cpuPercent, memPercent, swapPercent,
                        ioReadMbS, ioWriteMbS, processCount);

  m_cpuGraph->addSample(cpuPercent);
  m_memGraph->addSample(memPercent);
  m_swapGraph->addSample(swapPercent);
  m_ioGraph->addSample(ioReadMbS, ioWriteMbS);

  updateStatusLabel();
}

void NeoQtCaptureWindow::graphSelectionChanged(int index) {
  const QString which = m_graphSelect->itemData(index).toString();

  m_cpuGraph->setVisible(which == QStringLiteral("all") ||
                         which == QStringLiteral("cpu"));
  m_memGraph->setVisible(which == QStringLiteral("all") ||
                         which == QStringLiteral("mem"));
  m_swapGraph->setVisible(which == QStringLiteral("all") ||
                          which == QStringLiteral("swap"));
  m_ioGraph->setVisible(which == QStringLiteral("all") ||
                        which == QStringLiteral("io"));
}

void NeoQtCaptureWindow::saveReport() {
  if (m_series.count == 0) {
    return;
  }

  QString basePath = QFileDialog::getSaveFileName(
      this, QStringLiteral("Save Capture Report"),
      QStringLiteral("capture_report"),
      QStringLiteral("Base name, .csv and .html are added (*)"));

  if (basePath.isEmpty()) {
    return;
  }

  if (basePath.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive) ||
      basePath.endsWith(QStringLiteral(".html"), Qt::CaseInsensitive)) {
    basePath = basePath.left(basePath.lastIndexOf(QLatin1Char('.')));
  }

  const QByteArray csvPath = (basePath + QStringLiteral(".csv")).toLocal8Bit();
  const QByteArray htmlPath =
      (basePath + QStringLiteral(".html")).toLocal8Bit();

  const int csvResult = capture_write_csv(&m_series, csvPath.constData());
  const int htmlResult =
      capture_write_html_report(&m_series, htmlPath.constData());

  if (csvResult == 0 && htmlResult == 0) {
    m_statusLabel->setText(
        QStringLiteral("Saved %1.csv and %1.html").arg(basePath));
  } else {
    m_statusLabel->setText(
        QStringLiteral("Failed to save the report - check the path is "
                       "writable."));
  }
}
