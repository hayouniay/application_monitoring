#include "qt/qt_application.h"
#include "qt/qt_main_window.h"

#include <QApplication>
#include <QStyleFactory>

NeoQtApplication::NeoQtApplication(int &argc, char **argv)
    : QApplication(argc, argv) {
  setApplicationName(QStringLiteral("NEO Monitoring Services"));

  setApplicationDisplayName(QStringLiteral("NEO Monitoring Services"));

  setApplicationVersion(QStringLiteral(VERSION));

  setOrganizationName(QStringLiteral("NEO"));

  setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
}

NeoQtApplication::~NeoQtApplication() = default;

int main(int argc, char **argv) {
  NeoQtApplication application(argc, argv);

  NeoQtMainWindow window;
  window.show();

  return application.exec();
}
