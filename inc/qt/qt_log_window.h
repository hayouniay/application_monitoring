#ifndef QT_LOG_WINDOW_H
#define QT_LOG_WINDOW_H

#include <QDialog>
#include <QString>
#include <QVector>

extern "C" {
#include "monitoring_log.h"
}

class QPlainTextEdit;
class QComboBox;
class QCheckBox;

/*
 * Shows the same log stream the CLI writes to --log-file, live, inside
 * the Qt app - including entries produced by background threads (the
 * SSH/FTP/TFTP work in NeoQtRemoteDialog) that would otherwise only be
 * visible in that dialog's own log box while it happens to be open.
 *
 * Registers a sink with the core monitoring_log module on first use and
 * keeps it registered for the lifetime of the process (there's only
 * ever one of these needed), rather than tying it to this window's own
 * lifetime, so no messages are lost while the window is closed.
 */
class NeoQtLogWindow : public QDialog {
  Q_OBJECT

public:
  explicit NeoQtLogWindow(QWidget *parent = nullptr);
  ~NeoQtLogWindow() override;

  NeoQtLogWindow(const NeoQtLogWindow &) = delete;
  NeoQtLogWindow &operator=(const NeoQtLogWindow &) = delete;

private slots:
  void appendEntry(int level, qlonglong when, QString message);
  void filterChanged();
  void clearView();

private:
  void setupUi();
  bool passesFilter(NeoLogLevel level) const;
  void appendFormatted(NeoLogLevel level, time_t when, const QString &message);

  QPlainTextEdit *m_view;
  QComboBox *m_levelFilter;
  QCheckBox *m_autoScroll;

  struct Entry {
    NeoLogLevel level;
    time_t when;
    QString message;
  };

  QVector<Entry> m_entries;
};

#endif /* QT_LOG_WINDOW_H */
