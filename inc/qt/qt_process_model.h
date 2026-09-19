#ifndef QT_PROCESS_MODEL_H
#define QT_PROCESS_MODEL_H

#include <QAbstractTableModel>
#include <QVector>

extern "C" {
#include "monitoring_services.h"
}

class NeoQtProcessModel : public QAbstractTableModel {
  Q_OBJECT

public:
  enum Column {
    ColumnPid = 0,
    ColumnUser,
    ColumnState,
    ColumnCpu,
    ColumnMemory,
    ColumnRss,
    ColumnVsz,
    ColumnSwap,
    ColumnRead,
    ColumnWrite,
    ColumnThreads,
    ColumnCommand,
    ColumnCount
  };

  explicit NeoQtProcessModel(QObject *parent = nullptr);
  ~NeoQtProcessModel() override;

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;

  int columnCount(const QModelIndex &parent = QModelIndex()) const override;

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;

  void setProcesses(const NeoProcessList &processes);

  /*
   * Overload used by remote monitoring (SSH/Telnet), which builds
   * its result as a QVector rather than a NeoProcessList since the
   * data is assembled on a background thread and marshaled back to
   * the UI thread as a plain Qt value type.
   */
  void setProcesses(const QVector<NeoProcess> &processes);

  const NeoProcess *processAt(int row) const;

  void clear();

private:
  QVector<NeoProcess> m_processes;
};

#endif /* QT_PROCESS_MODEL_H */
