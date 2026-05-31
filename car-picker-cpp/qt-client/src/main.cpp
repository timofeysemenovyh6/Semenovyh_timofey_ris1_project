#include "MainWindow.h"

#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QMessageBox>
#include <QProcess>
#include <QTcpSocket>
#include <QThread>

#ifdef Q_OS_WIN
static QString serverExeName() { return QStringLiteral("car_picker.exe"); }
#else
static QString serverExeName() { return QStringLiteral("car_picker"); }
#endif

static bool port8080Listening() {
  QTcpSocket s;
  s.connectToHost(QHostAddress::LocalHost, 8080);
  const bool ok = s.waitForConnected(400);
  if (ok) s.disconnectFromHost();
  return ok;
}

/// Поднимаемся от каталога .app/…/MacOS (или build/Release) вверх, пока не найдём cars.db и car_picker рядом (корень car-picker-cpp после make).
static QString findCarPickerRepoRoot() {
  QDir d(QCoreApplication::applicationDirPath());
  for (int i = 0; i < 16; ++i) {
    const QString db = d.absoluteFilePath(QStringLiteral("cars.db"));
    const QString bin = d.absoluteFilePath(serverExeName());
    if (QFile::exists(db) && QFile::exists(bin)) return d.absolutePath();
    if (!d.cdUp()) break;
  }
  return {};
}

static bool waitPort8080(int maxMs) {
  QElapsedTimer t;
  t.start();
  while (t.elapsed() < maxMs) {
    if (port8080Listening()) return true;
    QThread::msleep(80);
  }
  return port8080Listening();
}

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) app.setStyle(fusion);

  QProcess* serverProc = nullptr;

  if (!port8080Listening()) {
    const QString root = findCarPickerRepoRoot();
    if (root.isEmpty()) {
      QMessageBox::warning(
          nullptr, QStringLiteral("Car Picker"),
          QStringLiteral("Порт 8080 свободен, но рядом с программой не найдены cars.db и car_picker.\n"
                         "Соберите сервер: в папке car-picker-cpp выполните make.\n"
                         "Либо запустите ./car_picker вручную, затем снова откройте это приложение."));
    } else {
      serverProc = new QProcess(&app);
      serverProc->setWorkingDirectory(root);
      serverProc->setProgram(QDir(root).absoluteFilePath(serverExeName()));
      serverProc->start();
      if (!serverProc->waitForStarted(5000)) {
        QMessageBox::critical(nullptr, QStringLiteral("Car Picker"),
                              QStringLiteral("Не удалось запустить car_picker:\n") + serverProc->errorString());
        return 1;
      }
      if (!waitPort8080(10000)) {
        QMessageBox::critical(nullptr, QStringLiteral("Car Picker"),
                              QStringLiteral("Сервер не открыл порт 8080. Возможно, ошибка при старте car_picker."));
        serverProc->kill();
        serverProc->waitForFinished(2000);
        return 1;
      }
      QObject::connect(&app, &QCoreApplication::aboutToQuit, serverProc, [serverProc]() {
        if (serverProc->state() == QProcess::Running) {
          serverProc->terminate();
          if (!serverProc->waitForFinished(3000)) serverProc->kill();
        }
      });
    }
  }

  MainWindow w;
  w.show();
  return app.exec();
}
