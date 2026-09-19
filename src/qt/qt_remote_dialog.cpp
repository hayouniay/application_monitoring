#include "qt/qt_remote_dialog.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTime>
#include <QVBoxLayout>

#include <cstdio>
#include <string>
#include <thread>

NeoQtRemoteDialog::NeoQtRemoteDialog(QWidget *parent)
    : QDialog(parent), m_protocolCombo(nullptr), m_hostEdit(nullptr),
      m_portSpin(nullptr), m_stack(nullptr), m_loginPage(nullptr),
      m_userEdit(nullptr), m_passwordEdit(nullptr), m_identityEdit(nullptr),
      m_identityBrowseButton(nullptr), m_remoteBinEdit(nullptr),
      m_deployPage(nullptr), m_deployUserEdit(nullptr),
      m_deployPasswordEdit(nullptr), m_localFileEdit(nullptr),
      m_localFileBrowseButton(nullptr), m_remoteFileEdit(nullptr),
      m_actionButton(nullptr), m_logView(nullptr), m_busy(false) {
  setWindowTitle(QStringLiteral("Connect to Remote Card"));

  /*
   * Non-modal and self-deleting: the person can keep monitoring the
   * local card while this is open, and closing it always leaves a
   * clean slate for next time rather than stale field values.
   */
  setModal(false);
  setAttribute(Qt::WA_DeleteOnClose);

  resize(560, 560);

  setupUi();
}

NeoQtRemoteDialog::~NeoQtRemoteDialog() = default;

void NeoQtRemoteDialog::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);

  /* -------------------------------------------------------------- */
  /* Common: protocol / host / port                                  */
  /* -------------------------------------------------------------- */

  auto *connectionGroup = new QGroupBox(QStringLiteral("Remote Card"), this);

  auto *connectionLayout = new QFormLayout(connectionGroup);

  m_protocolCombo = new QComboBox(connectionGroup);

  m_protocolCombo->addItem(QStringLiteral("SSH (monitor)"),
                           static_cast<int>(PROTOCOL_SSH));
  m_protocolCombo->addItem(QStringLiteral("Telnet (monitor)"),
                           static_cast<int>(PROTOCOL_TELNET));
  m_protocolCombo->addItem(QStringLiteral("FTP (deploy)"),
                           static_cast<int>(PROTOCOL_FTP));
  m_protocolCombo->addItem(QStringLiteral("TFTP (deploy)"),
                           static_cast<int>(PROTOCOL_TFTP));

  m_hostEdit = new QLineEdit(connectionGroup);
  m_hostEdit->setPlaceholderText(QStringLiteral("192.168.1.50"));

  m_portSpin = new QSpinBox(connectionGroup);
  m_portSpin->setRange(1, 65535);

  connectionLayout->addRow(QStringLiteral("Protocol:"), m_protocolCombo);
  connectionLayout->addRow(QStringLiteral("Host:"), m_hostEdit);
  connectionLayout->addRow(QStringLiteral("Port:"), m_portSpin);

  mainLayout->addWidget(connectionGroup);

  /* -------------------------------------------------------------- */
  /* SSH / Telnet monitor page                                       */
  /* -------------------------------------------------------------- */

  m_stack = new QStackedWidget(this);

  m_loginPage = new QWidget(m_stack);

  auto *loginLayout = new QFormLayout(m_loginPage);

  m_userEdit = new QLineEdit(m_loginPage);
  m_userEdit->setPlaceholderText(QStringLiteral("root"));

  m_passwordEdit = new QLineEdit(m_loginPage);
  m_passwordEdit->setEchoMode(QLineEdit::Password);

  auto *identityRow = new QWidget(m_loginPage);
  auto *identityRowLayout = new QHBoxLayout(identityRow);
  identityRowLayout->setContentsMargins(0, 0, 0, 0);

  m_identityEdit = new QLineEdit(identityRow);
  m_identityEdit->setPlaceholderText(
      QStringLiteral("~/.ssh/id_ed25519 (optional)"));

  m_identityBrowseButton =
      new QPushButton(QStringLiteral("Browse..."), identityRow);

  identityRowLayout->addWidget(m_identityEdit, 1);
  identityRowLayout->addWidget(m_identityBrowseButton);

  m_remoteBinEdit = new QLineEdit(m_loginPage);
  m_remoteBinEdit->setText(QStringLiteral("app_top_monitoring"));

  loginLayout->addRow(QStringLiteral("User:"), m_userEdit);
  loginLayout->addRow(QStringLiteral("Password (Telnet only):"),
                      m_passwordEdit);
  loginLayout->addRow(QStringLiteral("SSH key (SSH only):"), identityRow);
  loginLayout->addRow(QStringLiteral("Remote binary:"), m_remoteBinEdit);

  m_stack->addWidget(m_loginPage);

  /* -------------------------------------------------------------- */
  /* FTP / TFTP deploy page                                          */
  /* -------------------------------------------------------------- */

  m_deployPage = new QWidget(m_stack);

  auto *deployLayout = new QFormLayout(m_deployPage);

  m_deployUserEdit = new QLineEdit(m_deployPage);
  m_deployUserEdit->setPlaceholderText(
      QStringLiteral("leave empty for anonymous"));

  m_deployPasswordEdit = new QLineEdit(m_deployPage);
  m_deployPasswordEdit->setEchoMode(QLineEdit::Password);

  auto *localFileRow = new QWidget(m_deployPage);
  auto *localFileRowLayout = new QHBoxLayout(localFileRow);
  localFileRowLayout->setContentsMargins(0, 0, 0, 0);

  m_localFileEdit = new QLineEdit(localFileRow);

  m_localFileBrowseButton =
      new QPushButton(QStringLiteral("Browse..."), localFileRow);

  localFileRowLayout->addWidget(m_localFileEdit, 1);
  localFileRowLayout->addWidget(m_localFileBrowseButton);

  m_remoteFileEdit = new QLineEdit(m_deployPage);
  m_remoteFileEdit->setPlaceholderText(
      QStringLiteral("Defaults to the local filename"));

  deployLayout->addRow(QStringLiteral("User (FTP only):"), m_deployUserEdit);
  deployLayout->addRow(QStringLiteral("Password (FTP only):"),
                       m_deployPasswordEdit);
  deployLayout->addRow(QStringLiteral("Local file:"), localFileRow);
  deployLayout->addRow(QStringLiteral("Remote filename:"), m_remoteFileEdit);

  m_stack->addWidget(m_deployPage);

  mainLayout->addWidget(m_stack);

  /* -------------------------------------------------------------- */
  /* Action + log                                                    */
  /* -------------------------------------------------------------- */

  m_actionButton = new QPushButton(this);
  mainLayout->addWidget(m_actionButton);

  m_logView = new QPlainTextEdit(this);
  m_logView->setReadOnly(true);
  m_logView->setPlaceholderText(
      QStringLiteral("Connection log will appear here."));

  mainLayout->addWidget(m_logView, 1);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);

  mainLayout->addWidget(buttons);

  connect(m_protocolCombo, &QComboBox::currentIndexChanged, this,
          &NeoQtRemoteDialog::protocolChanged);

  connect(m_identityBrowseButton, &QPushButton::clicked, this,
          &NeoQtRemoteDialog::browseIdentityFile);

  connect(m_localFileBrowseButton, &QPushButton::clicked, this,
          &NeoQtRemoteDialog::browseLocalFile);

  connect(m_actionButton, &QPushButton::clicked, this,
          &NeoQtRemoteDialog::runAction);

  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  /* Apply the initial (SSH) protocol's field visibility/defaults. */
  protocolChanged(m_protocolCombo->currentIndex());
}

NeoProtocol NeoQtRemoteDialog::currentProtocol() const {
  return static_cast<NeoProtocol>(m_protocolCombo->currentData().toInt());
}

int NeoQtRemoteDialog::defaultPortForProtocol(NeoProtocol protocol) const {
  switch (protocol) {
  case PROTOCOL_SSH:
    return 22;

  case PROTOCOL_TELNET:
    return 23;

  case PROTOCOL_FTP:
    return 21;

  case PROTOCOL_TFTP:
    return 69;

  case PROTOCOL_LOCAL:
  default:
    return 0;
  }
}

void NeoQtRemoteDialog::protocolChanged(int index) {
  const NeoProtocol protocol =
      static_cast<NeoProtocol>(m_protocolCombo->itemData(index).toInt());

  const bool isMonitorMode =
      (protocol == PROTOCOL_SSH || protocol == PROTOCOL_TELNET);

  m_stack->setCurrentWidget(isMonitorMode ? m_loginPage : m_deployPage);

  /* Password only makes sense for Telnet; SSH here is key-only. */
  m_passwordEdit->setEnabled(protocol == PROTOCOL_TELNET);

  /* Identity file only applies to SSH. */
  m_identityEdit->setEnabled(protocol == PROTOCOL_SSH);
  m_identityBrowseButton->setEnabled(protocol == PROTOCOL_SSH);

  /* TFTP has no authentication at all. */
  m_deployUserEdit->setEnabled(protocol == PROTOCOL_FTP);
  m_deployPasswordEdit->setEnabled(protocol == PROTOCOL_FTP);

  m_portSpin->setValue(defaultPortForProtocol(protocol));

  m_actionButton->setText(isMonitorMode ? QStringLiteral("Connect && Fetch")
                                        : QStringLiteral("Deploy"));
}

void NeoQtRemoteDialog::browseIdentityFile() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Select SSH private key"));

  if (!path.isEmpty()) {
    m_identityEdit->setText(path);
  }
}

void NeoQtRemoteDialog::browseLocalFile() {
  const QString path = QFileDialog::getOpenFileName(
      this, QStringLiteral("Select file to deploy"));

  if (!path.isEmpty()) {
    m_localFileEdit->setText(path);
  }
}

void NeoQtRemoteDialog::appendLog(const QString &message) {
  m_logView->appendPlainText(
      QTime::currentTime().toString(QStringLiteral("HH:mm:ss")) +
      QStringLiteral("  ") + message);
}

void NeoQtRemoteDialog::setBusy(bool busy) {
  m_busy = busy;

  m_actionButton->setEnabled(!busy);
  m_protocolCombo->setEnabled(!busy);
}

void NeoQtRemoteDialog::runAction() {
  if (m_busy)
    return;

  if (m_hostEdit->text().trimmed().isEmpty()) {
    appendLog(QStringLiteral("Host is required."));
    return;
  }

  const NeoProtocol protocol = currentProtocol();

  if (protocol == PROTOCOL_SSH || protocol == PROTOCOL_TELNET) {
    runSshOrTelnet(protocol);
  } else {
    runFtpOrTftp(protocol);
  }
}

void NeoQtRemoteDialog::reportSshOrTelnetResult(
    bool success, const QString &logText, const QVector<NeoProcess> &processes,
    const NeoSystemInfo &systemInfo, const QString &sourceLabel) {
  appendLog(logText);
  setBusy(false);

  if (success) {
    emit processesFetched(processes, systemInfo, sourceLabel);
  }
}

void NeoQtRemoteDialog::runSshOrTelnet(NeoProtocol protocol) {
  NeoRemoteTarget target;
  remote_target_init(&target);

  const QByteArray hostBytes = m_hostEdit->text().trimmed().toLocal8Bit();
  std::snprintf(target.host, sizeof(target.host), "%s", hostBytes.constData());

  const QByteArray userBytes = m_userEdit->text().trimmed().toLocal8Bit();
  std::snprintf(target.user, sizeof(target.user), "%s", userBytes.constData());

  const QByteArray passwordBytes = m_passwordEdit->text().toLocal8Bit();
  std::snprintf(target.password, sizeof(target.password), "%s",
                passwordBytes.constData());

  const QByteArray identityBytes =
      m_identityEdit->text().trimmed().toLocal8Bit();
  std::snprintf(target.identity_file, sizeof(target.identity_file), "%s",
                identityBytes.constData());

  const QByteArray remoteBinBytes =
      m_remoteBinEdit->text().trimmed().toLocal8Bit();
  std::snprintf(target.remote_binary, sizeof(target.remote_binary), "%s",
                remoteBinBytes.isEmpty() ? "app_top_monitoring"
                                         : remoteBinBytes.constData());

  target.port = m_portSpin->value();

  appendLog(QStringLiteral("Connecting to %1 via %2...")
                .arg(m_hostEdit->text().trimmed())
                .arg(protocol == PROTOCOL_SSH ? QStringLiteral("SSH")
                                              : QStringLiteral("Telnet")));

  setBusy(true);

  QPointer<NeoQtRemoteDialog> guard(this);

  std::thread([guard, protocol, target]() {
    NeoProcessList list;
    NeoSystemInfo systemInfo;
    char message[REMOTE_MESSAGE_MAX];

    process_list_init(&list);

    const int result = (protocol == PROTOCOL_SSH)
                           ? remote_ssh_scan(&target, &list, &systemInfo,
                                             message, sizeof(message))
                           : remote_telnet_scan(&target, &list, &systemInfo,
                                                message, sizeof(message));

    QString logText;
    QVector<NeoProcess> processes;

    if (result == 0) {
      /* Sort before handing off, so the table opens in a useful
       * order rather than raw /proc scan order. */
      sort_processes(&list, SORT_CPU, false);

      processes.reserve(static_cast<int>(list.count));

      for (size_t i = 0; i < list.count; ++i) {
        processes.append(list.items[i]);
      }

      logText = QStringLiteral("Connected. %1 process(es) found. "
                               "CPUs: %2, Memory: %3 MB.")
                    .arg(static_cast<qulonglong>(list.count))
                    .arg(systemInfo.cpu_count)
                    .arg(systemInfo.total_memory_kb / 1024.0, 0, 'f', 1);
    } else {
      logText =
          QStringLiteral("Failed: %1").arg(QString::fromLocal8Bit(message));
    }

    process_list_free(&list);

    const QString sourceLabel =
        QStringLiteral("%1%2%3 (%4)")
            .arg(target.user[0] ? QString::fromLocal8Bit(target.user)
                                : QString())
            .arg(target.user[0] ? QStringLiteral("@") : QString())
            .arg(QString::fromLocal8Bit(target.host))
            .arg(protocol == PROTOCOL_SSH ? QStringLiteral("SSH")
                                          : QStringLiteral("Telnet"));

    QMetaObject::invokeMethod(
        qApp,
        [guard, result, logText, processes, systemInfo, sourceLabel]() {
          if (guard) {
            guard->reportSshOrTelnetResult(result == 0, logText, processes,
                                           systemInfo, sourceLabel);
          }
        },
        Qt::QueuedConnection);
  }).detach();
}

void NeoQtRemoteDialog::runFtpOrTftp(NeoProtocol protocol) {
  const QString localFile = m_localFileEdit->text().trimmed();

  if (localFile.isEmpty()) {
    appendLog(QStringLiteral("A local file to deploy is required."));
    return;
  }

  appendLog(QStringLiteral("Deploying %1 to %2 via %3...")
                .arg(localFile, m_hostEdit->text().trimmed(),
                     protocol == PROTOCOL_FTP ? QStringLiteral("FTP")
                                              : QStringLiteral("TFTP")));

  setBusy(true);

  QPointer<NeoQtRemoteDialog> guard(this);

  /* Kept as std::string (not QByteArray) so it survives safely as a
   * plain value captured into the detached worker thread. */
  const std::string localFileStd = localFile.toLocal8Bit().toStdString();

  if (protocol == PROTOCOL_FTP) {
    NeoFtpTarget target;
    ftp_target_init(&target);

    const QByteArray hostBytes = m_hostEdit->text().trimmed().toLocal8Bit();
    std::snprintf(target.host, sizeof(target.host), "%s",
                  hostBytes.constData());

    const QByteArray userBytes =
        m_deployUserEdit->text().trimmed().toLocal8Bit();
    std::snprintf(target.user, sizeof(target.user), "%s",
                  userBytes.constData());

    const QByteArray passwordBytes = m_deployPasswordEdit->text().toLocal8Bit();
    std::snprintf(target.password, sizeof(target.password), "%s",
                  passwordBytes.constData());

    const QByteArray remoteFileBytes =
        m_remoteFileEdit->text().trimmed().toLocal8Bit();
    std::snprintf(target.remote_filename, sizeof(target.remote_filename), "%s",
                  remoteFileBytes.constData());

    target.port = m_portSpin->value();

    std::thread([guard, target, localFileStd]() {
      char message[REMOTE_MESSAGE_MAX];

      const int result = remote_ftp_deploy(&target, localFileStd.c_str(),
                                           message, sizeof(message));

      const QString logText =
          (result == 0) ? QStringLiteral("Deploy finished successfully.")
                        : QStringLiteral("Deploy failed: %1")
                              .arg(QString::fromLocal8Bit(message));

      QMetaObject::invokeMethod(
          qApp,
          [guard, logText]() {
            if (guard) {
              guard->appendLog(logText);
              guard->setBusy(false);
            }
          },
          Qt::QueuedConnection);
    }).detach();
  } else {
    NeoTftpTarget target;
    tftp_target_init(&target);

    const QByteArray hostBytes = m_hostEdit->text().trimmed().toLocal8Bit();
    std::snprintf(target.host, sizeof(target.host), "%s",
                  hostBytes.constData());

    const QByteArray remoteFileBytes =
        m_remoteFileEdit->text().trimmed().toLocal8Bit();
    std::snprintf(target.remote_filename, sizeof(target.remote_filename), "%s",
                  remoteFileBytes.constData());

    target.port = m_portSpin->value();

    std::thread([guard, target, localFileStd]() {
      char message[REMOTE_MESSAGE_MAX];

      const int result = remote_tftp_deploy(&target, localFileStd.c_str(),
                                            message, sizeof(message));

      const QString logText =
          (result == 0) ? QStringLiteral("Deploy finished successfully.")
                        : QStringLiteral("Deploy failed: %1")
                              .arg(QString::fromLocal8Bit(message));

      QMetaObject::invokeMethod(
          qApp,
          [guard, logText]() {
            if (guard) {
              guard->appendLog(logText);
              guard->setBusy(false);
            }
          },
          Qt::QueuedConnection);
    }).detach();
  }
}
