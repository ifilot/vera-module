#include "mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
  QApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("VERA Test Console"));
  application.setWindowIcon(QIcon(QStringLiteral(":/icons/vera-gui-icon.png")));
  MainWindow window;
  window.show();
  return application.exec();
}
