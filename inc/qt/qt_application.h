#ifndef QT_APPLICATION_H
#define QT_APPLICATION_H

#include <QApplication>

#include "qt/qt_theme.h"

extern "C" {
#include "monitoring_services.h"
}

class NeoQtApplication : public QApplication {
  Q_OBJECT

public:
  NeoQtApplication(int &argc, char **argv);
  ~NeoQtApplication() override;

  NeoQtApplication(const NeoQtApplication &) = delete;
  NeoQtApplication &operator=(const NeoQtApplication &) = delete;

  NeoTheme currentTheme() const;

public slots:
  void setTheme(NeoTheme theme);
  void toggleTheme();

signals:
  void themeChanged(NeoTheme theme);

private:
  NeoTheme m_theme;
};

#endif /* QT_APPLICATION_H */