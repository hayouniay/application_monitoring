#include "qt/qt_application.h"
#include "qt/qt_main_window.h"

#include <QApplication>
#include <QSettings>
#include <QStyleFactory>

NeoQtApplication::NeoQtApplication(int &argc, char **argv)
    : QApplication(argc, argv), m_theme(NeoTheme::Light) {
  setApplicationName(QStringLiteral("NEO Monitoring Services"));

  setApplicationDisplayName(QStringLiteral("NEO Monitoring Services"));

  setApplicationVersion(QStringLiteral(VERSION));

  setOrganizationName(QStringLiteral("NEO"));

  setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

  /*
   * Restore the user's last chosen theme (defaults to Light on
   * first launch).
   */
  const QSettings settings;

  const NeoTheme savedTheme = neoThemeFromName(
      settings
          .value(QStringLiteral("appearance/theme"), QStringLiteral("light"))
          .toString());

  setTheme(savedTheme);
}

NeoQtApplication::~NeoQtApplication() = default;

NeoTheme NeoQtApplication::currentTheme() const { return m_theme; }

void NeoQtApplication::setTheme(NeoTheme theme) {
  m_theme = theme;

  setStyleSheet(neoThemeStyleSheet(theme));

  QSettings settings;

  settings.setValue(QStringLiteral("appearance/theme"), neoThemeName(theme));

  emit themeChanged(theme);
}

void NeoQtApplication::toggleTheme() {
  setTheme(m_theme == NeoTheme::Light ? NeoTheme::Dark : NeoTheme::Light);
}

int main(int argc, char **argv) {
  NeoQtApplication application(argc, argv);

  NeoQtMainWindow window;
  window.show();

  return application.exec();
}
