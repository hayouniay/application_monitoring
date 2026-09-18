#ifndef QT_APPLICATION_H
#define QT_APPLICATION_H

#include <QApplication>

extern "C" {
#include "monitoring_services.h"
}

class NeoQtApplication : public QApplication {
public:
  NeoQtApplication(int &argc, char **argv);
  ~NeoQtApplication() override;

  NeoQtApplication(const NeoQtApplication &) = delete;
  NeoQtApplication &operator=(const NeoQtApplication &) = delete;
};

#endif /* QT_APPLICATION_H */