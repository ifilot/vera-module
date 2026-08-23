#pragma once

#include <QByteArray>
#include <QSerialPort>

class BridgeClient final {
public:
  static constexpr int MaxVramChunk = 1024;

  bool open(const QString &portName, QString *error);
  void close();
  bool isOpen() const;

  bool ping(QString *error);
  bool reset(QString *error);
  bool writeRegister(quint8 reg, quint8 value, QString *error);
  bool readRegister(quint8 reg, quint8 *value, QString *error);
  bool writeVram(quint32 address, const QByteArray &data, QString *error);
  bool readVram(quint32 address, quint8 *value, QString *error);
  // Clocks bytes through VERA's SPI controller.  The returned array contains
  // the byte received during each corresponding clock.
  bool spiTransfer(const QByteArray &transmit, QByteArray *receive, QString *error);

private:
  static constexpr quint8 RequestMarker = 0xA5;
  static constexpr quint8 ResponseMarker = 0x5A;

  bool request(quint8 opcode, const QByteArray &payload, QByteArray *response,
               QString *error);
  bool readResponse(quint8 expectedOpcode, QByteArray *payload, QString *error);
  static quint8 checksum(const QByteArray &bytes);

  QSerialPort serial_;
  QByteArray received_;
};
