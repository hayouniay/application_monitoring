#ifndef QT_REMOTE_DIALOG_H
#define QT_REMOTE_DIALOG_H

#include <QDialog>
#include <QVector>

extern "C" {
#include "monitoring_remote.h"
#include "monitoring_targets.h"
}

class QComboBox;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QPlainTextEdit;
class QStackedWidget;

/*
 * Lets the user connect to a remote card over SSH or Telnet (fetches
 * and logs a process-count/CPU/memory summary - it does not yet push
 * results into the main window's table, matching the current scope),
 * or deploy a file to the card over FTP or TFTP.
 *
 * All network calls run on a detached std::thread so the UI is never
 * blocked; results are marshaled back to the UI thread via
 * QMetaObject::invokeMethod, guarded by a QPointer in case the dialog
 * is closed (it's WA_DeleteOnClose) while a call is still in flight.
 */
class NeoQtRemoteDialog : public QDialog {
  Q_OBJECT

public:
  explicit NeoQtRemoteDialog(QWidget *parent = nullptr);
  ~NeoQtRemoteDialog() override;

  NeoQtRemoteDialog(const NeoQtRemoteDialog &) = delete;
  NeoQtRemoteDialog &operator=(const NeoQtRemoteDialog &) = delete;

signals:
  /*
   * Emitted after a successful SSH/Telnet fetch. Connected by the
   * main window to push results into the main process table.
   */
  void processesFetched(QVector<NeoProcess> processes, NeoSystemInfo systemInfo,
                        QString sourceLabel);

private slots:
  void protocolChanged(int index);
  void browseIdentityFile();
  void browseLocalFile();
  void runAction();
  void savedTargetSelected(int index);
  void saveCurrentAsTarget();
  void deleteSelectedTarget();

private:
  void setupUi();
  void appendLog(const QString &message);
  void setBusy(bool busy);

  NeoProtocol currentProtocol() const;
  int defaultPortForProtocol(NeoProtocol protocol) const;

  void runSshOrTelnet(NeoProtocol protocol);
  void runFtpOrTftp(NeoProtocol protocol);

  /*
   * Called (already on the UI thread) once a background ssh/telnet
   * fetch completes, to update the log/busy state and, on success,
   * emit processesFetched() for the main window to consume.
   */
  void reportSshOrTelnetResult(bool success, const QString &logText,
                               const QVector<NeoProcess> &processes,
                               const NeoSystemInfo &systemInfo,
                               const QString &sourceLabel);

  /* Saved targets (~/.config/neo-monitoring/targets.json, shared with
   * the CLI's --target/--save-target/--list-targets/--delete-target). */
  void reloadSavedTargetsCombo(const QString &selectName = QString());
  void applyTarget(const NeoTarget &target);
  NeoTarget targetFromCurrentFields(const QString &name) const;

  /* Common */
  QComboBox *m_protocolCombo;
  QLineEdit *m_hostEdit;
  QSpinBox *m_portSpin;
  QStackedWidget *m_stack;

  /* Saved targets row */
  QComboBox *m_savedTargetCombo;
  QPushButton *m_saveTargetButton;
  QPushButton *m_deleteTargetButton;
  bool m_populatingSavedTargetCombo;

  /* SSH / Telnet monitor page */
  QWidget *m_loginPage;
  QLineEdit *m_userEdit;
  QLineEdit *m_passwordEdit;
  QLineEdit *m_identityEdit;
  QPushButton *m_identityBrowseButton;
  QLineEdit *m_remoteBinEdit;

  /* FTP / TFTP deploy page */
  QWidget *m_deployPage;
  QLineEdit *m_deployUserEdit;
  QLineEdit *m_deployPasswordEdit;
  QLineEdit *m_localFileEdit;
  QPushButton *m_localFileBrowseButton;
  QLineEdit *m_remoteFileEdit;

  QPushButton *m_actionButton;
  QPlainTextEdit *m_logView;

  bool m_busy;
};

#endif /* QT_REMOTE_DIALOG_H */
