#include "qt/qt_process_model.h"

#include <QString>

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
