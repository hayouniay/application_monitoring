#ifndef QT_MONITOR_CONTROLLER_H
#define QT_MONITOR_CONTROLLER_H

#include <QObject>
#include <QString>

class QTimer;

extern "C" {
#include "monitoring_services.h"
}

class NeoQtMonitorController : public QObject {
  Q_OBJECT

public:
  explicit NeoQtMonitorController(QObject *parent = nullptr);
  ~NeoQtMonitorController() override;

  NeoQtMonitorController(const NeoQtMonitorController &) = delete;
  NeoQtMonitorController &operator=(const NeoQtMonitorController &) = delete;

  const NeoProcessList &processes() const;
  const NeoSystemInfo &systemInfo() const;
  const NeoConfig &config() const;

  bool isRunning() const;

public slots:
  void start();
  void stop();
  void refresh();

  void setInterval(double seconds);

  void setSearchPattern(const QString &pattern);

  void setExcludePattern(const QString &pattern);

  void setUserFilter(const QString &user);

  void setStateFilter(const QString &states);

  void setPidFilter(const QString &pids);

  void setPpidFilter(const QString &ppids);

  void setCpuThreshold(double threshold);

  void setRamThresholdMb(double threshold);

  void setRamThresholdPercent(double threshold);

  void setProcessLimit(int limit);

  void setSortMode(NeoSortMode mode);

  void setReverse(bool reverse);

  void setShowLongArgs(bool enabled);

  void setShowVsz(bool enabled);

  void setShowSwap(bool enabled);

  void setShowIo(bool enabled);

  void setShowThreads(bool enabled);

  void setShowStartTime(bool enabled);

  void setShowElapsed(bool enabled);

signals:
  void processesUpdated();
  void systemInfoUpdated();
  void monitoringStarted();
  void monitoringStopped();

  void errorOccurred(const QString &message);

private:
  void clearPatterns(char **&patterns, size_t &count);

  bool setSinglePattern(char **&patterns, size_t &count,
                        const QString &pattern);

  bool setPidList(pid_t *&items, size_t &count, const QString &value);

  bool setStateList(const QString &value);

  QTimer *m_timer;

  NeoConfig m_config;
  NeoSystemInfo m_systemInfo;
  NeoProcessList m_processList;
  NeoPreviousList m_previousList;

  bool m_running;
};

#endif /* QT_MONITOR_CONTROLLER_H */