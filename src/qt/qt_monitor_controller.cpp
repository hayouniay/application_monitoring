#include "qt/qt_monitor_controller.h"

#include <QTimer>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace {

bool qt_parse_pid_list(const QString &value, pid_t **items, size_t *count) {
  if (items == nullptr || count == nullptr)
    return false;

  *items = nullptr;
  *count = 0;

  const QStringList values = value.split(',', Qt::SkipEmptyParts);

  if (values.isEmpty())
    return true;

  pid_t *result = static_cast<pid_t *>(
      std::calloc(static_cast<size_t>(values.size()), sizeof(pid_t)));

  if (result == nullptr)
    return false;

  for (int i = 0; i < values.size(); ++i) {
    bool ok = false;

    const qlonglong parsed = values.at(i).trimmed().toLongLong(&ok);

    if (!ok || parsed <= 0 || parsed > 2147483647LL) {
      std::free(result);
      return false;
    }

    result[i] = static_cast<pid_t>(parsed);
  }

  *items = result;
  *count = static_cast<size_t>(values.size());

  return true;
}

} // namespace

NeoQtMonitorController::NeoQtMonitorController(QObject *parent)
    : QObject(parent), m_timer(new QTimer(this)), m_running(false) {
  config_init(&m_config);
  process_list_init(&m_processList);
  previous_list_init(&m_previousList);

  std::memset(&m_systemInfo, 0, sizeof(m_systemInfo));

  connect(m_timer, &QTimer::timeout, this, &NeoQtMonitorController::refresh);
}

NeoQtMonitorController::~NeoQtMonitorController() {
  stop();

  process_list_free(&m_processList);

  previous_list_free(&m_previousList);

  config_free(&m_config);
}

const NeoProcessList &NeoQtMonitorController::processes() const {
  return m_processList;
}

const NeoSystemInfo &NeoQtMonitorController::systemInfo() const {
  return m_systemInfo;
}

const NeoConfig &NeoQtMonitorController::config() const { return m_config; }

bool NeoQtMonitorController::isRunning() const { return m_running; }

void NeoQtMonitorController::start() {
  if (m_running)
    return;

  m_running = true;

  refresh();

  const int intervalMs =
      std::max(100, static_cast<int>(m_config.interval * 1000.0));

  m_timer->start(intervalMs);

  emit monitoringStarted();
}

void NeoQtMonitorController::stop() {
  if (!m_running)
    return;

  m_timer->stop();

  m_running = false;

  emit monitoringStopped();
}

void NeoQtMonitorController::refresh() {
  if (read_system_info(&m_systemInfo) != 0) {
    emit errorOccurred(QStringLiteral("Unable to read system information"));
    return;
  }

  const int result = scan_processes(&m_config, &m_systemInfo, &m_processList,
                                    &m_previousList, m_config.interval);

  if (result != 0) {
    emit errorOccurred(QStringLiteral("Unable to scan processes"));
    return;
  }

  sort_processes(&m_processList, m_config.sort_mode, m_config.reverse);

  emit systemInfoUpdated();
  emit processesUpdated();
}

void NeoQtMonitorController::setInterval(double seconds) {
  seconds = std::clamp(seconds, MIN_INTERVAL, MAX_INTERVAL);

  m_config.interval = seconds;

  if (m_running) {
    const int intervalMs = std::max(100, static_cast<int>(seconds * 1000.0));

    m_timer->start(intervalMs);
  }
}

void NeoQtMonitorController::setSearchPattern(const QString &pattern) {
  if (!setSinglePattern(m_config.patterns, m_config.pattern_count, pattern)) {
    emit errorOccurred(QStringLiteral("Unable to allocate search pattern"));
    return;
  }

  refresh();
}

void NeoQtMonitorController::setExcludePattern(const QString &pattern) {
  if (!setSinglePattern(m_config.exclude_patterns, m_config.exclude_count,
                        pattern)) {
    emit errorOccurred(QStringLiteral("Unable to allocate exclude pattern"));
    return;
  }

  refresh();
}

void NeoQtMonitorController::setUserFilter(const QString &user) {
  const QByteArray value = user.trimmed().toLocal8Bit();

  if (value.isEmpty()) {
    m_config.filter_user[0] = '\0';
    m_config.filter_user_enabled = false;
  } else {
    std::strncpy(m_config.filter_user, value.constData(), MAX_USER - 1);

    m_config.filter_user[MAX_USER - 1] = '\0';

    m_config.filter_user_enabled = true;
  }

  refresh();
}

void NeoQtMonitorController::setStateFilter(const QString &states) {
  if (!setStateList(states)) {
    emit errorOccurred(QStringLiteral("Invalid state filter"));
    return;
  }

  refresh();
}

void NeoQtMonitorController::setPidFilter(const QString &pids) {
  pid_t *items = nullptr;
  size_t count = 0;

  if (!qt_parse_pid_list(pids, &items, &count)) {
    emit errorOccurred(QStringLiteral("Invalid PID filter"));
    return;
  }

  std::free(m_config.pids);

  m_config.pids = items;
  m_config.pid_count = count;

  refresh();
}

void NeoQtMonitorController::setPpidFilter(const QString &ppids) {
  pid_t *items = nullptr;
  size_t count = 0;

  if (!qt_parse_pid_list(ppids, &items, &count)) {
    emit errorOccurred(QStringLiteral("Invalid PPID filter"));
    return;
  }

  std::free(m_config.ppids);

  m_config.ppids = items;
  m_config.ppid_count = count;

  refresh();
}

void NeoQtMonitorController::setCpuThreshold(double threshold) {
  m_config.cpu_threshold = std::max(0.0, threshold);

  refresh();
}

void NeoQtMonitorController::setRamThresholdMb(double threshold) {
  m_config.ram_threshold_mb = std::max(0.0, threshold);

  refresh();
}

void NeoQtMonitorController::setRamThresholdPercent(double threshold) {
  m_config.ram_threshold_percent = std::clamp(threshold, 0.0, 100.0);

  refresh();
}

void NeoQtMonitorController::setProcessLimit(int limit) {
  m_config.limit = limit <= 0 ? 0 : static_cast<size_t>(limit);

  refresh();
}

void NeoQtMonitorController::setSortMode(NeoSortMode mode) {
  m_config.sort_mode = mode;

  sort_processes(&m_processList, m_config.sort_mode, m_config.reverse);

  emit processesUpdated();
}

void NeoQtMonitorController::setReverse(bool reverse) {
  m_config.reverse = reverse;

  sort_processes(&m_processList, m_config.sort_mode, m_config.reverse);

  emit processesUpdated();
}

void NeoQtMonitorController::setShowLongArgs(bool enabled) {
  m_config.show_long_args = enabled;

  refresh();
}

void NeoQtMonitorController::setShowVsz(bool enabled) {
  m_config.show_vsz = enabled;

  refresh();
}

void NeoQtMonitorController::setShowSwap(bool enabled) {
  m_config.show_swap = enabled;

  refresh();
}

void NeoQtMonitorController::setShowIo(bool enabled) {
  m_config.show_io = enabled;

  refresh();
}

void NeoQtMonitorController::setShowThreads(bool enabled) {
  m_config.show_threads = enabled;

  refresh();
}

void NeoQtMonitorController::setShowStartTime(bool enabled) {
  m_config.show_start_time = enabled;

  refresh();
}

void NeoQtMonitorController::setShowElapsed(bool enabled) {
  m_config.show_elapsed = enabled;

  refresh();
}

void NeoQtMonitorController::clearPatterns(char **&patterns, size_t &count) {
  if (patterns != nullptr) {
    for (size_t i = 0; i < count; ++i)
      std::free(patterns[i]);

    std::free(patterns);
  }

  patterns = nullptr;
  count = 0;
}

bool NeoQtMonitorController::setSinglePattern(char **&patterns, size_t &count,
                                              const QString &pattern) {
  clearPatterns(patterns, count);

  const QByteArray value = pattern.trimmed().toLocal8Bit();

  if (value.isEmpty())
    return true;

  char **result = static_cast<char **>(std::calloc(1, sizeof(char *)));

  if (result == nullptr)
    return false;

  result[0] =
      static_cast<char *>(std::malloc(static_cast<size_t>(value.size()) + 1));

  if (result[0] == nullptr) {
    std::free(result);
    return false;
  }

  std::memcpy(result[0], value.constData(), static_cast<size_t>(value.size()));

  result[0][value.size()] = '\0';

  patterns = result;
  count = 1;

  return true;
}

bool NeoQtMonitorController::setPidList(pid_t *&items, size_t &count,
                                        const QString &value) {
  pid_t *result = nullptr;
  size_t resultCount = 0;

  if (!qt_parse_pid_list(value, &result, &resultCount)) {
    return false;
  }

  std::free(items);

  items = result;
  count = resultCount;

  return true;
}

bool NeoQtMonitorController::setStateList(const QString &value) {
  const QString normalized = value.trimmed().toUpper();

  if (normalized.isEmpty()) {
    m_config.state_count = 0;
    std::memset(m_config.states, 0, sizeof(m_config.states));
    return true;
  }

  if (normalized.size() > MAX_STATE_FILTER - 1) {
    return false;
  }

  std::memset(m_config.states, 0, sizeof(m_config.states));

  m_config.state_count = 0;

  for (const QChar character : normalized) {
    const char state = character.toLatin1();

    if (state != 'R' && state != 'S' && state != 'D' && state != 'T' &&
        state != 'Z' && state != 'I') {
      return false;
    }

    bool duplicate = false;

    for (size_t i = 0; i < m_config.state_count; ++i) {
      if (m_config.states[i] == state) {
        duplicate = true;
        break;
      }
    }

    if (!duplicate) {
      m_config.states[m_config.state_count++] = state;
    }
  }

  return true;
}
