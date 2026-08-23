#pragma once

#include "bridgeclient.h"

#include <QMainWindow>
#include <QVector>

class QComboBox;
class QLabel;
class QProgressBar;
class QProgressDialog;
class QTimer;

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();

private slots:
  void refreshPorts();
  void connectBridge();
  void resetVera();
  void enableVga();
  void uploadPm5544();
  void runMonochromeTest();
  void runColorTest();
  void runTileTest();
  void runTextTest();
  void runSpriteTest();
  void stopSpriteMotion();
  void advanceSprites();
  void startAudioTest();
  void stopAudio();
  void runTimingInterruptTest();
  void runSdCardTest();

private:
  bool requireBridge();
  bool writeRegister(quint8 reg, quint8 value);
  bool readRegister(quint8 reg, quint8 *value);
  QString readVersion();
  bool writePalette(const QVector<QRgb> &colors);
  bool writeBuffer(quint32 address, const QByteArray &data, const QString &label);
  bool configureBitmap(quint8 colorDepth, bool wide);
  bool configureTiles();
  bool uploadImage(const QString &resourcePath);
  bool spiTransfer(const QByteArray &transmit, QByteArray *receive);
  bool spiTransfer(quint8 transmit, quint8 *receive);
  bool sdCommand(quint8 command, quint32 argument, quint8 crc, quint8 *r1,
                 QByteArray *trailing = nullptr);
  bool initializeSdCard(bool *highCapacity, quint32 *ocr);
  bool sdReadSector(quint32 lba, bool highCapacity, QByteArray *sector);
  void showReport(const QString &title, const QString &report, bool success);
  void setOperationProgress(const QString &text, int value = -1, int maximum = 0);
  void setStatus(const QString &text, bool error = false);

  struct SpriteMotion {
    int x;
    int y;
    int dx;
    int dy;
  };

  BridgeClient bridge_;
  QComboBox *ports_;
  QLabel *version_;
  QLabel *status_;
  QProgressBar *progress_;
  QProgressDialog *operationProgress_ = nullptr;
  QTimer *spriteTimer_;
  QVector<SpriteMotion> sprites_;
};
