#include "qt/qt_log_window.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

#include <array>

namespace {

/*
 * Bridges the core monitoring_log module's sink callback (which can
 * fire on any thread - notably the background std::thread that
 * NeoQtRemoteDialog uses for SSH/FTP/TFTP work) onto the Qt UI thread.
 *
 * This is a process-lifetime singleton, deliberately independent of
 * NeoQtLogWindow's own lifetime: the window is created/destroyed each
 * time View -> Logs... is opened/closed (same pattern as the graphs
 * and capture windows), but log messages that arrive while no window
 * is open must not be lost - log_copy_recent() lets a freshly-opened
 * window catch up on history it missed.
 */
class NeoLogBridge : public QObject {
  Q_OBJECT

public:
  static NeoLogBridge *instance() {
    static NeoLogBridge bridge;
    return &bridge;
  }

signals:
  void entryLogged(int level, qlonglong when, QString message);

private:
  NeoLogBridge() { log_add_sink(&NeoLogBridge::sink, this); }

  ~NeoLogBridge() override { log_remove_sink(&NeoLogBridge::sink, this); }

  static void sink(const NeoLogEntry *entry, void *user_data) {
    auto *bridge = static_cast<NeoLogBridge *>(user_data);

    /* entry is only valid for the duration of this call (it points at
     * a stack-local copy inside log_write()), so everything needed
     * from it must be copied out before crossing threads. */
    const int level = static_cast<int>(entry->level);
    const qlonglong when = static_cast<qlonglong>(entry->when);
    const QString message = QString::fromUtf8(entry->message);

    QMetaObject::invokeMethod(
        bridge,
        [bridge, level, when, message]() {
          emit bridge->entryLogged(level, when, message);
        },
        Qt::QueuedConnection);
  }
};

const char *levelLabel(NeoLogLevel level) {
  switch (level) {
  case NEO_LOG_ERROR:
    return "ERROR";
  case NEO_LOG_WARN:
    return "WARN";
  case NEO_LOG_INFO:
    return "INFO";
  case NEO_LOG_DEBUG:
    return "DEBUG";
  default:
    return "?";
  }
}

const char *levelColor(NeoLogLevel level) {
  switch (level) {
  case NEO_LOG_ERROR:
    return "#e05c5c";
  case NEO_LOG_WARN:
    return "#e0a83c";
  case NEO_LOG_INFO:
    return "#5c9ee0";
  case NEO_LOG_DEBUG:
  default:
    return "#8c8c8c";
  }
}

} // namespace

NeoQtLogWindow::NeoQtLogWindow(QWidget *parent)
    : QDialog(parent), m_view(nullptr), m_levelFilter(nullptr),
      m_autoScroll(nullptr) {
  setWindowTitle(QStringLiteral("Logs"));

  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose);

  resize(760, 480);

  setupUi();

  /* Catch up on anything logged before this window existed. */
  std::array<NeoLogEntry, 500> history{};
  const size_t count = log_copy_recent(history.data(), history.size());

  for (size_t i = 0; i < count; ++i) {
    Entry entry;
    entry.level = history[i].level;
    entry.when = history[i].when;
    entry.message = QString::fromUtf8(history[i].message);
    m_entries.append(entry);

    if (passesFilter(entry.level)) {
      appendFormatted(entry.level, entry.when, entry.message);
    }
  }

  connect(NeoLogBridge::instance(), &NeoLogBridge::entryLogged, this,
          &NeoQtLogWindow::appendEntry);
}

NeoQtLogWindow::~NeoQtLogWindow() = default;

void NeoQtLogWindow::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);

  auto *toolbarLayout = new QHBoxLayout();

  toolbarLayout->addWidget(new QLabel(QStringLiteral("Minimum level:"), this));

  m_levelFilter = new QComboBox(this);
  m_levelFilter->addItem(QStringLiteral("Debug"), (int)NEO_LOG_DEBUG);
  m_levelFilter->addItem(QStringLiteral("Info"), (int)NEO_LOG_INFO);
  m_levelFilter->addItem(QStringLiteral("Warning"), (int)NEO_LOG_WARN);
  m_levelFilter->addItem(QStringLiteral("Error"), (int)NEO_LOG_ERROR);
  m_levelFilter->setCurrentIndex(1); /* Info by default. */

  toolbarLayout->addWidget(m_levelFilter);

  m_autoScroll = new QCheckBox(QStringLiteral("Auto-scroll"), this);
  m_autoScroll->setChecked(true);

  toolbarLayout->addWidget(m_autoScroll);

  toolbarLayout->addStretch(1);

  auto *clearButton = new QPushButton(QStringLiteral("Clear"), this);

  toolbarLayout->addWidget(clearButton);

  mainLayout->addLayout(toolbarLayout);

  m_view = new QPlainTextEdit(this);
  m_view->setReadOnly(true);
  m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
  m_view->setMaximumBlockCount(5000);

  QFont monoFont(QStringLiteral("Monospace"));
  monoFont.setStyleHint(QFont::TypeWriter);
  m_view->setFont(monoFont);

  mainLayout->addWidget(m_view, 1);

  connect(m_levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NeoQtLogWindow::filterChanged);

  connect(clearButton, &QPushButton::clicked, this,
          &NeoQtLogWindow::clearView);
}

bool NeoQtLogWindow::passesFilter(NeoLogLevel level) const {
  if (m_levelFilter == nullptr) {
    return true;
  }

  const int minimum = m_levelFilter->currentData().toInt();

  /* Lower numeric value = more severe; "at least as severe as the
   * selected minimum" is level <= minimum. */
  return static_cast<int>(level) <= minimum;
}

void NeoQtLogWindow::appendFormatted(NeoLogLevel level, time_t when,
                                     const QString &message) {
  const QDateTime timestamp = QDateTime::fromSecsSinceEpoch((qint64)when);

  const QString line =
      QStringLiteral("<span style=\"color:#888;\">%1</span> "
                     "<span style=\"color:%2; font-weight:bold;\">[%3]</span> "
                     "%4")
          .arg(timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
          .arg(QString::fromUtf8(levelColor(level)))
          .arg(QString::fromUtf8(levelLabel(level)))
          .arg(message.toHtmlEscaped());

  m_view->appendHtml(line);

  if (m_autoScroll != nullptr && m_autoScroll->isChecked()) {
    QScrollBar *bar = m_view->verticalScrollBar();
    bar->setValue(bar->maximum());
  }
}

void NeoQtLogWindow::appendEntry(int level, qlonglong when, QString message) {
  Entry entry;
  entry.level = (NeoLogLevel)level;
  entry.when = (time_t)when;
  entry.message = message;

  m_entries.append(entry);

  if (passesFilter(entry.level)) {
    appendFormatted(entry.level, entry.when, entry.message);
  }
}

void NeoQtLogWindow::filterChanged() {
  m_view->clear();

  for (const Entry &entry : m_entries) {
    if (passesFilter(entry.level)) {
      appendFormatted(entry.level, entry.when, entry.message);
    }
  }
}

void NeoQtLogWindow::clearView() {
  m_entries.clear();
  m_view->clear();
}

#include "qt_log_window.moc"
