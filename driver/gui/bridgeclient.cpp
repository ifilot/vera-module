#include "bridgeclient.h"

#include <QElapsedTimer>

namespace {
constexpr quint8 OpPing = 0x01;
constexpr quint8 OpReset = 0x02;
constexpr quint8 OpWriteRegister = 0x10;
constexpr quint8 OpReadRegister = 0x11;
constexpr quint8 OpWriteVram = 0x12;
constexpr quint8 OpReadVram = 0x13;
constexpr quint8 OpSpiTransfer = 0x14;
constexpr quint8 StatusOk = 0x00;
constexpr int ResponseTimeoutMs = 3000;
}

bool BridgeClient::open(const QString &portName, QString *error) {
  close();
  serial_.setPortName(portName);
  serial_.setBaudRate(500000);
  serial_.setDataBits(QSerialPort::Data8);
  serial_.setParity(QSerialPort::NoParity);
  serial_.setStopBits(QSerialPort::OneStop);
  serial_.setFlowControl(QSerialPort::NoFlowControl);
  serial_.setDataTerminalReady(false);
  serial_.setRequestToSend(false);
  if (!serial_.open(QIODevice::ReadWrite)) {
    *error = QStringLiteral("Cannot open %1: %2").arg(portName, serial_.errorString());
    return false;
  }
  serial_.clear(QSerialPort::AllDirections);
  received_.clear();
  return true;
}

void BridgeClient::close() {
  if (serial_.isOpen()) serial_.close();
  received_.clear();
}

bool BridgeClient::isOpen() const { return serial_.isOpen(); }

quint8 BridgeClient::checksum(const QByteArray &bytes) {
  quint8 result = 0;
  for (char byte : bytes) result ^= static_cast<quint8>(byte);
  return result;
}

bool BridgeClient::request(quint8 opcode, const QByteArray &payload,
                           QByteArray *response, QString *error) {
  if (!isOpen()) {
    *error = QStringLiteral("No bridge is connected.");
    return false;
  }
  if (payload.size() > MaxVramChunk + 3) {
    *error = QStringLiteral("Bridge request exceeds 1,027 bytes.");
    return false;
  }

  QByteArray frame;
  frame.reserve(payload.size() + 5);
  frame.append(static_cast<char>(RequestMarker));
  frame.append(static_cast<char>(opcode));
  frame.append(static_cast<char>(payload.size() & 0xff));
  frame.append(static_cast<char>((payload.size() >> 8) & 0xff));
  frame.append(payload);
  frame.append(static_cast<char>(checksum(frame.mid(1))));
  if (serial_.write(frame) != frame.size() || !serial_.waitForBytesWritten(ResponseTimeoutMs)) {
    *error = QStringLiteral("Serial write failed: %1").arg(serial_.errorString());
    return false;
  }
  return readResponse(opcode, response, error);
}

bool BridgeClient::readResponse(quint8 expectedOpcode, QByteArray *payload, QString *error) {
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < ResponseTimeoutMs) {
    if (serial_.waitForReadyRead(ResponseTimeoutMs - timer.elapsed()))
      received_.append(serial_.readAll());
    while (!received_.isEmpty() && static_cast<quint8>(received_.at(0)) != ResponseMarker)
      received_.remove(0, 1);
    if (received_.size() < 6) continue;

    const quint8 status = static_cast<quint8>(received_.at(1));
    const quint8 opcode = static_cast<quint8>(received_.at(2));
    const int length = static_cast<quint8>(received_.at(3)) |
                       (static_cast<quint8>(received_.at(4)) << 8);
    if (length > MaxVramChunk + 3) {
      received_.remove(0, 1);
      continue;
    }
    const int frameSize = 6 + length;
    if (received_.size() < frameSize) continue;
    const QByteArray protectedBytes = received_.mid(1, 4 + length);
    const quint8 frameChecksum = static_cast<quint8>(received_.at(frameSize - 1));
    if (checksum(protectedBytes) != frameChecksum) {
      received_.remove(0, 1);
      continue;
    }
    const QByteArray result = received_.mid(5, length);
    received_.remove(0, frameSize);
    if (opcode != expectedOpcode) {
      *error = QStringLiteral("Unexpected bridge response opcode 0x%1.")
                   .arg(opcode, 2, 16, QLatin1Char('0'));
      return false;
    }
    if (status != StatusOk) {
      *error = QStringLiteral("Bridge rejected the request (status 0x%1).")
                   .arg(status, 2, 16, QLatin1Char('0'));
      return false;
    }
    *payload = result;
    return true;
  }
  *error = QStringLiteral("Timed out waiting for the bridge response.");
  return false;
}

bool BridgeClient::ping(QString *error) {
  QByteArray response;
  return request(OpPing, {}, &response, error) && response == QByteArray(1, '\x01');
}

bool BridgeClient::reset(QString *error) {
  QByteArray response;
  return request(OpReset, {}, &response, error);
}

bool BridgeClient::writeRegister(quint8 reg, quint8 value, QString *error) {
  QByteArray response;
  QByteArray payload;
  payload.append(static_cast<char>(reg));
  payload.append(static_cast<char>(value));
  return request(OpWriteRegister, payload, &response, error);
}

bool BridgeClient::readRegister(quint8 reg, quint8 *value, QString *error) {
  QByteArray response;
  if (!request(OpReadRegister, QByteArray(1, static_cast<char>(reg)), &response, error)) return false;
  if (response.size() != 1) { *error = QStringLiteral("Malformed register response."); return false; }
  *value = static_cast<quint8>(response.at(0));
  return true;
}

bool BridgeClient::writeVram(quint32 address, const QByteArray &data, QString *error) {
  if (address > 0x1ffff || data.isEmpty()) { *error = QStringLiteral("Invalid VRAM write."); return false; }
  for (int offset = 0; offset < data.size(); offset += MaxVramChunk) {
    const QByteArray chunk = data.mid(offset, MaxVramChunk);
    const quint32 chunkAddress = address + static_cast<quint32>(offset);
    QByteArray payload;
    payload.append(static_cast<char>(chunkAddress & 0xff));
    payload.append(static_cast<char>((chunkAddress >> 8) & 0xff));
    payload.append(static_cast<char>((chunkAddress >> 16) & 0x01));
    payload.append(chunk);
    QByteArray response;
    if (!request(OpWriteVram, payload, &response, error)) return false;
  }
  return true;
}

bool BridgeClient::readVram(quint32 address, quint8 *value, QString *error) {
  QByteArray payload;
  payload.append(static_cast<char>(address & 0xff));
  payload.append(static_cast<char>((address >> 8) & 0xff));
  payload.append(static_cast<char>((address >> 16) & 0x01));
  QByteArray response;
  if (!request(OpReadVram, payload, &response, error)) return false;
  if (response.size() != 1) { *error = QStringLiteral("Malformed VRAM response."); return false; }
  *value = static_cast<quint8>(response.at(0));
  return true;
}

bool BridgeClient::spiTransfer(const QByteArray &transmit, QByteArray *receive, QString *error) {
  if (transmit.isEmpty() || transmit.size() > MaxVramChunk) {
    *error = QStringLiteral("SPI transfer must contain 1 to %1 bytes.").arg(MaxVramChunk);
    return false;
  }
  if (!request(OpSpiTransfer, transmit, receive, error)) return false;
  if (receive->size() != transmit.size()) {
    *error = QStringLiteral("Malformed SPI transfer response.");
    return false;
  }
  return true;
}
