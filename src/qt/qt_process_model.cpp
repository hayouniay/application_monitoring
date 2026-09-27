#include "qt/qt_process_model.h"

#include <QDateTime>
#include <QString>

#include <ctime>

namespace {

/* Mirrors format_start_time() in monitoring_output.c (kept separate
 * since that one is a static CLI-only helper) - "-" for a process
 * whose start time couldn't be determined. */
QString formatStartTime(time_t startTime) {
  if (startTime <= 0) {
    return QStringLiteral("-");
  }

  return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(startTime))
      .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

/* Mirrors format_elapsed() in monitoring_output.c: "HH:MM:SS", or
 * "Nd HH:MM:SS" once a process has been running for over a day. */
QString formatElapsed(double seconds) {
  if (seconds < 0.0) {
    seconds = 0.0;
  }

  unsigned long total = static_cast<unsigned long>(seconds);

  const unsigned long days = total / 86400UL;
  total %= 86400UL;

  const unsigned long hours = total / 3600UL;
  total %= 3600UL;

  const unsigned long minutes = total / 60UL;
  const unsigned long secs = total % 60UL;

  const QString hms = QStringLiteral("%1:%2:%3")
                          .arg(hours, 2, 10, QLatin1Char('0'))
                          .arg(minutes, 2, 10, QLatin1Char('0'))
                          .arg(secs, 2, 10, QLatin1Char('0'));

  if (days > 0) {
    return QStringLiteral("%1d %2").arg(days).arg(hms);
  }

  return hms;
}

} // namespace

NeoQtProcessModel::NeoQtProcessModel(QObject *parent)
    : QAbstractTableModel(parent) {}

NeoQtProcessModel::~NeoQtProcessModel() = default;

int NeoQtProcessModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;

  return m_processes.size();
}

int NeoQtProcessModel::columnCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;

  return ColumnCount;
}

QVariant NeoQtProcessModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid())
    return {};

  if (index.row() < 0 || index.row() >= m_processes.size())
    return {};

  const NeoProcess &process = m_processes.at(index.row());

  if (role == Qt::TextAlignmentRole) {
    switch (index.column()) {
    case ColumnPid:
    case ColumnCpu:
    case ColumnMemory:
    case ColumnRss:
    case ColumnVsz:
    case ColumnSwap:
    case ColumnRead:
    case ColumnWrite:
    case ColumnThreads:
    case ColumnElapsed:
      return QVariant::fromValue(
          Qt::Alignment(Qt::AlignRight | Qt::AlignVCenter));

    default:
      return QVariant::fromValue(
          Qt::Alignment(Qt::AlignLeft | Qt::AlignVCenter));
    }
  }

  if (role == Qt::DisplayRole) {
    switch (index.column()) {
    case ColumnPid:
      return process.pid;

    case ColumnUser:
      return QString::fromLocal8Bit(process.user);

    case ColumnState:
      return QString(process.state);

    case ColumnCpu:
      return QString::number(process.cpu_percent, 'f', 1);

    case ColumnMemory:
      return QString::number(process.mem_percent, 'f', 1);

    case ColumnRss:
      return QString::number(process.rss_mb, 'f', 1) + " MB";

    case ColumnVsz:
      return QString::number(process.vsz_mb, 'f', 1) + " MB";

    case ColumnSwap:
      return QString::number(process.swap_mb, 'f', 1) + " MB";

    case ColumnRead:
      return QString::number(process.io_read_mb_s, 'f', 2) + " MB/s";

    case ColumnWrite:
      return QString::number(process.io_write_mb_s, 'f', 2) + " MB/s";

    case ColumnThreads:
      return static_cast<qulonglong>(process.threads);

    case ColumnStartTime:
      return formatStartTime(process.start_time);

    case ColumnElapsed:
      return formatElapsed(process.elapsed_seconds);

    case ColumnCommand:
      return QString::fromLocal8Bit(process.comm);

    default:
      return {};
    }
  }

  if (role == Qt::UserRole) {
    switch (index.column()) {
    case ColumnPid:
      return static_cast<qlonglong>(process.pid);

    case ColumnCpu:
      return process.cpu_percent;

    case ColumnMemory:
      return process.mem_percent;

    case ColumnRss:
      return static_cast<qulonglong>(process.rss_kb);

    case ColumnVsz:
      return static_cast<qulonglong>(process.vsz_kb);

    case ColumnSwap:
      return static_cast<qulonglong>(process.swap_kb);

    case ColumnRead:
      return static_cast<qulonglong>(process.read_bytes);

    case ColumnWrite:
      return static_cast<qulonglong>(process.write_bytes);

    case ColumnThreads:
      return static_cast<qulonglong>(process.threads);

    case ColumnStartTime:
      return static_cast<qlonglong>(process.start_time);

    case ColumnElapsed:
      return process.elapsed_seconds;

    default:
      return {};
    }
  }

  return {};
}

QVariant NeoQtProcessModel::headerData(int section, Qt::Orientation orientation,
                                       int role) const {
  if (role != Qt::DisplayRole)
    return {};

  if (orientation == Qt::Vertical)
    return section + 1;

  switch (section) {
  case ColumnPid:
    return "PID";

  case ColumnUser:
    return "USER";

  case ColumnState:
    return "STATE";

  case ColumnCpu:
    return "CPU %";

  case ColumnMemory:
    return "MEM %";

  case ColumnRss:
    return "RSS";

  case ColumnVsz:
    return "VSZ";

  case ColumnSwap:
    return "SWAP";

  case ColumnRead:
    return "READ";

  case ColumnWrite:
    return "WRITE";

  case ColumnThreads:
    return "THREADS";

  case ColumnStartTime:
    return "START";

  case ColumnElapsed:
    return "ELAPSED";

  case ColumnCommand:
    return "COMMAND";

  default:
    return {};
  }
}

void NeoQtProcessModel::setProcesses(const NeoProcessList &processes) {
  beginResetModel();

  m_processes.clear();
  m_processes.reserve(static_cast<int>(processes.count));

  for (size_t i = 0; i < processes.count; ++i)
    m_processes.append(processes.items[i]);

  endResetModel();
}

void NeoQtProcessModel::setProcesses(const QVector<NeoProcess> &processes) {
  beginResetModel();

  m_processes = processes;

  endResetModel();
}

const NeoProcess *NeoQtProcessModel::processAt(int row) const {
  if (row < 0 || row >= m_processes.size())
    return nullptr;

  return &m_processes.at(row);
}

void NeoQtProcessModel::clear() {
  beginResetModel();
  m_processes.clear();
  endResetModel();
}
