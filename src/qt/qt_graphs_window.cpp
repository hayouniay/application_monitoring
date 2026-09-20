#include "qt/qt_graphs_window.h"
#include "qt/qt_application.h"
#include "qt/qt_wave_widget.h"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

NeoQtGraphsWindow::NeoQtGraphsWindow(QWidget *parent)
    : QDialog(parent), m_graphSelect(nullptr), m_cpuGraph(nullptr),
      m_memGraph(nullptr), m_swapGraph(nullptr), m_ioGraph(nullptr) {
  setWindowTitle(QStringLiteral("Live Graphs"));

  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose);

  resize(720, 640);

  setupUi();

  /* Repaint every graph when the theme flips, so colors stay
   * consistent with the rest of the app. */
  if (auto *application = qobject_cast<NeoQtApplication *>(qApp)) {
    connect(application, &NeoQtApplication::themeChanged, this,
            [this](NeoTheme) {
              m_cpuGraph->update();
              m_memGraph->update();
              m_swapGraph->update();
              m_ioGraph->update();
            });
  }
}

NeoQtGraphsWindow::~NeoQtGraphsWindow() = default;

void NeoQtGraphsWindow::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);

  auto *controlsLayout = new QHBoxLayout();

  auto *selectLabel = new QLabel(QStringLiteral("Show:"), this);

  m_graphSelect = new QComboBox(this);
  m_graphSelect->addItem(QStringLiteral("All graphs"), QStringLiteral("all"));
  m_graphSelect->addItem(QStringLiteral("CPU only"), QStringLiteral("cpu"));
  m_graphSelect->addItem(QStringLiteral("Memory only"), QStringLiteral("mem"));
  m_graphSelect->addItem(QStringLiteral("Swap only"), QStringLiteral("swap"));
  m_graphSelect->addItem(QStringLiteral("I/O only"), QStringLiteral("io"));

  auto *clearButton = new QPushButton(QStringLiteral("Clear"), this);

  controlsLayout->addWidget(selectLabel);
  controlsLayout->addWidget(m_graphSelect);
  controlsLayout->addStretch(1);
  controlsLayout->addWidget(clearButton);

  mainLayout->addLayout(controlsLayout);

  m_cpuGraph = new NeoQtWaveWidget(this);
  m_cpuGraph->setSeries(QStringLiteral("CPU"), QColor(0x2f, 0x6f, 0xed));
  m_cpuGraph->setUnit(QStringLiteral("%"));
  m_cpuGraph->setFixedRange(0.0, 100.0);

  m_memGraph = new NeoQtWaveWidget(this);
  m_memGraph->setSeries(QStringLiteral("Memory"), QColor(0xe0, 0x57, 0x4c));
  m_memGraph->setUnit(QStringLiteral("%"));
  m_memGraph->setFixedRange(0.0, 100.0);

  m_swapGraph = new NeoQtWaveWidget(this);
  m_swapGraph->setSeries(QStringLiteral("Swap"), QColor(0xd9, 0xa4, 0x41));
  m_swapGraph->setUnit(QStringLiteral("%"));
  m_swapGraph->setFixedRange(0.0, 100.0);

  m_ioGraph = new NeoQtWaveWidget(this);
  m_ioGraph->setSeries(QStringLiteral("Read"), QColor(0x2f, 0xa8, 0x4f));
  m_ioGraph->setSecondSeries(QStringLiteral("Write"), QColor(0x8a, 0x4f, 0xe0));
  m_ioGraph->setUnit(QStringLiteral(" MB/s"));
  m_ioGraph->setAutoRange();

  mainLayout->addWidget(m_cpuGraph, 1);
  mainLayout->addWidget(m_memGraph, 1);
  mainLayout->addWidget(m_swapGraph, 1);
  mainLayout->addWidget(m_ioGraph, 1);

  connect(m_graphSelect, &QComboBox::currentIndexChanged, this,
          &NeoQtGraphsWindow::graphSelectionChanged);

  connect(clearButton, &QPushButton::clicked, this,
          &NeoQtGraphsWindow::clearAll);
}

void NeoQtGraphsWindow::graphSelectionChanged(int index) {
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

void NeoQtGraphsWindow::addSample(double cpuPercent, double memPercent,
                                  double swapPercent, double ioReadMbS,
                                  double ioWriteMbS) {
  m_cpuGraph->addSample(cpuPercent);
  m_memGraph->addSample(memPercent);
  m_swapGraph->addSample(swapPercent);
  m_ioGraph->addSample(ioReadMbS, ioWriteMbS);
}

void NeoQtGraphsWindow::clearAll() {
  m_cpuGraph->clear();
  m_memGraph->clear();
  m_swapGraph->clear();
  m_ioGraph->clear();
}
