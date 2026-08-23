#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QTimer>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

namespace {
constexpr quint8 RegCtrl = 0x05, RegDcVideo = 0x09, RegDcHscale = 0x0a, RegDcVscale = 0x0b;
constexpr quint8 RegDcBorder = 0x0c, RegL0Config = 0x0d, RegL0Mapbase = 0x0e, RegL0Tilebase = 0x0f;
constexpr quint8 RegL0HscrollL = 0x10, RegL0HscrollH = 0x11, RegL0VscrollL = 0x12, RegL0VscrollH = 0x13;
constexpr quint32 PaletteBase = 0x1fa00;
constexpr quint32 SpriteAttrBase = 0x1fc00;
constexpr quint32 SpritePixelsBase = 0x8000;
constexpr quint32 AudioPsgBase = 0x1f9c0;
constexpr quint8 VideoModeVga = 0x01, VideoLayer0 = 0x10;
constexpr quint8 RegIrqEnable = 0x06, RegIrqStatus = 0x07, RegIrqLine = 0x08;
constexpr quint8 RegSpiData = 0x1e, RegSpiControl = 0x1f;
constexpr int ScreenWidth = 640, ScreenHeight = 480;

struct SongNote {
  int leadFrequency;
  int bassFrequency;
  int duration;
  bool percussion;
};

// Korobeiniki, commonly known as the Game Boy Tetris Type A melody. Durations
// are in eighth notes. The second pulse voice follows the lead a fifth below,
// while the triangle bass and gated noise recreate the GB's four-part texture.
constexpr SongNote TetrisSong[] = {
    {659, 165, 2, true},  {494, 247, 1, false}, {523, 165, 1, true},  {587, 247, 2, false},
    {523, 220, 1, true},  {494, 165, 1, false}, {440, 220, 2, true},  {440, 220, 1, false},
    {523, 165, 1, true},  {659, 247, 2, false}, {587, 220, 1, true},  {523, 165, 1, false},
    {494, 247, 3, true},  {523, 165, 1, false}, {587, 247, 2, true},  {659, 220, 2, false},
    {523, 165, 2, true},  {440, 220, 2, false}, {440, 220, 2, true},  {587, 196, 2, false},
    {698, 294, 1, true},  {880, 196, 2, false}, {784, 294, 1, true},  {698, 247, 1, false},
    {659, 165, 3, true},  {523, 247, 1, false}, {659, 165, 2, true},  {587, 247, 1, false},
    {523, 220, 1, true},  {494, 165, 3, false}, {494, 247, 1, true},  {523, 165, 1, false},
    {587, 247, 2, true},  {659, 220, 2, false}, {523, 165, 2, true},  {440, 220, 2, false},
    {440, 220, 2, true},
};
constexpr int TetrisSongLength = sizeof(TetrisSong) / sizeof(TetrisSong[0]);
constexpr int SongEighthNoteMs = 200;

// Super Mario Bros. ground-level layout follows the NES disassembly: two
// square (pulse) voices, triangle bass, and noise. Durations are sixteenth-note
// units, including dotted-note timing from the decoded ground-theme melody.
constexpr SongNote marioNote(int frequency, int duration) {
  return {frequency, frequency == 0 ? 0 : frequency / 4, duration, false};
}

constexpr SongNote MarioSong[] = {
    marioNote(659, 2), marioNote(659, 2), marioNote(0, 2), marioNote(659, 2),
    marioNote(0, 2), marioNote(523, 2), marioNote(659, 2), marioNote(784, 4),
    marioNote(0, 4), marioNote(392, 2), marioNote(0, 4), marioNote(523, 6),
    marioNote(392, 2), marioNote(0, 4), marioNote(330, 6), marioNote(440, 4),
    marioNote(494, 4), marioNote(466, 2), marioNote(440, 4), marioNote(392, 3),
    marioNote(659, 3), marioNote(784, 3), marioNote(880, 4), marioNote(698, 2),
    marioNote(784, 2), marioNote(0, 2), marioNote(659, 4), marioNote(523, 2),
    marioNote(587, 2), marioNote(494, 6), marioNote(523, 6), marioNote(392, 2),
    marioNote(0, 4), marioNote(330, 6), marioNote(440, 4), marioNote(494, 4),
    marioNote(466, 2), marioNote(440, 4), marioNote(392, 3), marioNote(659, 3),
    marioNote(784, 3), marioNote(880, 4), marioNote(698, 2), marioNote(784, 2),
    marioNote(0, 2), marioNote(659, 4), marioNote(523, 2), marioNote(587, 2),
    marioNote(494, 6), marioNote(0, 4), marioNote(784, 2), marioNote(740, 2),
    marioNote(698, 2), marioNote(622, 4), marioNote(659, 2), marioNote(0, 2),
    marioNote(415, 2), marioNote(440, 2), marioNote(262, 2), marioNote(0, 2),
    marioNote(440, 2), marioNote(523, 2), marioNote(587, 2), marioNote(0, 4),
    marioNote(622, 4), marioNote(0, 2), marioNote(587, 6), marioNote(523, 8),
    marioNote(0, 8), marioNote(0, 4), marioNote(784, 2), marioNote(740, 2),
    marioNote(698, 2), marioNote(622, 4), marioNote(659, 2), marioNote(0, 2),
    marioNote(415, 2), marioNote(440, 2), marioNote(262, 2), marioNote(0, 2),
    marioNote(440, 2), marioNote(523, 2), marioNote(587, 2), marioNote(0, 4),
    marioNote(622, 4), marioNote(0, 2), marioNote(587, 6), marioNote(523, 8),
    marioNote(0, 8), marioNote(523, 2), marioNote(523, 4), marioNote(523, 2),
    marioNote(0, 2), marioNote(523, 2), marioNote(587, 4), marioNote(659, 2),
    marioNote(523, 4), marioNote(440, 2), marioNote(392, 8), marioNote(523, 2),
    marioNote(523, 4), marioNote(523, 2), marioNote(0, 2), marioNote(523, 2),
    marioNote(587, 2), marioNote(659, 2), marioNote(0, 16), marioNote(523, 2),
    marioNote(523, 4), marioNote(523, 2), marioNote(0, 2), marioNote(523, 2),
    marioNote(587, 4), marioNote(659, 2), marioNote(523, 4), marioNote(440, 2),
    marioNote(392, 8), marioNote(659, 2), marioNote(659, 2), marioNote(0, 2),
    marioNote(659, 2), marioNote(0, 2), marioNote(523, 2), marioNote(659, 4),
    marioNote(784, 4), marioNote(0, 4), marioNote(392, 4), marioNote(0, 4),
    marioNote(523, 6), marioNote(392, 2), marioNote(0, 4), marioNote(330, 6),
    marioNote(440, 4), marioNote(494, 4), marioNote(466, 2), marioNote(440, 4),
    marioNote(392, 3), marioNote(659, 3), marioNote(784, 3), marioNote(880, 4),
    marioNote(698, 2), marioNote(784, 2), marioNote(0, 2), marioNote(659, 4),
    marioNote(523, 2), marioNote(587, 2), marioNote(494, 6), marioNote(523, 6),
    marioNote(392, 2), marioNote(0, 4), marioNote(330, 6), marioNote(440, 4),
    marioNote(494, 4), marioNote(466, 2), marioNote(440, 4), marioNote(392, 3),
    marioNote(659, 3), marioNote(784, 3), marioNote(880, 4), marioNote(698, 2),
    marioNote(784, 2), marioNote(0, 2), marioNote(659, 4), marioNote(523, 2),
    marioNote(587, 2), marioNote(494, 6), marioNote(659, 2), marioNote(523, 4),
    marioNote(392, 2), marioNote(0, 4), marioNote(415, 4), marioNote(440, 2),
    marioNote(698, 4), marioNote(698, 2), marioNote(440, 8), marioNote(587, 3),
    marioNote(880, 3), marioNote(880, 3), marioNote(880, 3), marioNote(784, 3),
    marioNote(698, 3), marioNote(659, 2), marioNote(523, 4), marioNote(440, 2),
    marioNote(392, 8), marioNote(659, 2), marioNote(523, 4), marioNote(392, 2),
    marioNote(0, 4), marioNote(415, 4), marioNote(440, 2), marioNote(698, 4),
    marioNote(698, 2), marioNote(440, 8), marioNote(494, 2), marioNote(698, 4),
    marioNote(698, 2), marioNote(698, 3), marioNote(659, 3), marioNote(587, 3),
    marioNote(523, 2), marioNote(330, 4), marioNote(330, 2), marioNote(262, 8),
    marioNote(659, 2), marioNote(523, 4), marioNote(392, 2), marioNote(0, 4),
    marioNote(415, 4), marioNote(440, 2), marioNote(698, 4), marioNote(698, 2),
    marioNote(440, 8), marioNote(587, 3), marioNote(880, 3), marioNote(880, 3),
    marioNote(880, 3), marioNote(784, 3), marioNote(698, 3), marioNote(659, 2),
    marioNote(523, 4), marioNote(440, 2), marioNote(392, 8), marioNote(659, 2),
    marioNote(523, 4), marioNote(392, 2), marioNote(0, 4), marioNote(415, 4),
    marioNote(440, 2), marioNote(698, 4), marioNote(698, 2), marioNote(440, 8),
    marioNote(494, 2), marioNote(698, 4), marioNote(698, 2), marioNote(698, 3),
    marioNote(659, 3), marioNote(587, 3), marioNote(523, 2), marioNote(330, 4),
    marioNote(330, 2), marioNote(262, 8), marioNote(523, 2), marioNote(523, 4),
    marioNote(523, 2), marioNote(0, 2), marioNote(523, 2), marioNote(587, 2),
    marioNote(659, 2), marioNote(0, 16), marioNote(523, 2), marioNote(523, 4),
    marioNote(523, 2), marioNote(0, 2), marioNote(523, 2), marioNote(587, 4),
    marioNote(659, 2), marioNote(523, 4), marioNote(440, 2), marioNote(392, 8),
    marioNote(659, 2), marioNote(659, 2), marioNote(0, 2), marioNote(659, 2),
    marioNote(0, 2), marioNote(523, 2), marioNote(659, 4), marioNote(784, 4),
    marioNote(0, 4), marioNote(392, 4), marioNote(0, 4), marioNote(659, 2),
    marioNote(523, 4), marioNote(392, 2), marioNote(0, 4), marioNote(415, 4),
    marioNote(440, 2), marioNote(698, 4), marioNote(698, 2), marioNote(440, 8),
    marioNote(587, 3), marioNote(880, 3), marioNote(880, 3), marioNote(880, 3),
    marioNote(784, 3), marioNote(698, 3), marioNote(659, 2), marioNote(523, 4),
    marioNote(440, 2), marioNote(392, 8), marioNote(659, 2), marioNote(523, 4),
    marioNote(392, 2), marioNote(0, 4), marioNote(415, 4), marioNote(440, 2),
    marioNote(698, 4), marioNote(698, 2), marioNote(440, 8), marioNote(494, 2),
    marioNote(698, 4), marioNote(698, 2), marioNote(698, 3), marioNote(659, 3),
    marioNote(587, 3), marioNote(523, 2), marioNote(330, 4), marioNote(330, 2),
    marioNote(262, 8), marioNote(523, 6), marioNote(392, 6), marioNote(330, 4),
};
constexpr int MarioSongLength = sizeof(MarioSong) / sizeof(MarioSong[0]);
constexpr int MarioSixteenthNoteMs = 75;  // NES source tempo: 200 BPM.
constexpr double PsgSampleRate = 48828.125;

quint16 psgPhaseIncrement(int frequency) {
  return static_cast<quint16>(std::lround(frequency * 131072.0 / PsgSampleRate));
}

void setPsgChannel(QByteArray *channels, int channel, int frequency, quint8 volume,
                   quint8 waveform, quint8 pulseWidth = 0) {
  const int offset = channel * 4;
  if (frequency == 0 || volume == 0) return;
  const quint16 phase = psgPhaseIncrement(frequency);
  (*channels)[offset] = static_cast<char>(phase & 0xff);
  (*channels)[offset + 1] = static_cast<char>(phase >> 8);
  (*channels)[offset + 2] = static_cast<char>((volume & 0x3f) | 0xc0);
  (*channels)[offset + 3] = static_cast<char>((waveform << 6) | (pulseWidth & 0x3f));
}

QByteArray makeMonoPattern() {
  QByteArray bytes(ScreenWidth * ScreenHeight / 8, '\0');
  for (int y = 0; y < ScreenHeight; ++y) for (int x = 0; x < ScreenWidth; ++x) {
    const bool border = x < 8 || x >= ScreenWidth - 8 || y < 8 || y >= ScreenHeight - 8;
    const bool grid = (x % 64 == 0) || (y % 48 == 0);
    const bool checker = ((x / 32) + (y / 32)) % 2 == 0;
    if (border || grid || checker) bytes[y * 80 + x / 8] |= static_cast<char>(0x80 >> (x & 7));
  }
  return bytes;
}

QByteArray makeColorPattern() {
  QByteArray bytes(ScreenWidth * ScreenHeight / 4, '\0');
  for (int y = 0; y < ScreenHeight; ++y) for (int x = 0; x < ScreenWidth; ++x) {
    quint8 index = static_cast<quint8>((x * 4) / ScreenWidth);
    if ((x / 24 + y / 24) % 2 == 0 && y > 320) index = static_cast<quint8>((index + 1) & 3);
    bytes[y * 160 + x / 4] |= static_cast<char>(index << (6 - 2 * (x & 3)));
  }
  return bytes;
}

QByteArray makeTextPattern() {
  QImage canvas(ScreenWidth, ScreenHeight, QImage::Format_RGB32);
  canvas.fill(QColor(4, 16, 36));
  QPainter painter(&canvas);
  painter.setPen(QColor(140, 255, 220));
  QFont title = painter.font(); title.setPixelSize(60); title.setBold(true); painter.setFont(title);
  painter.drawText(QRect(0, 125, ScreenWidth, 80), Qt::AlignCenter, QStringLiteral("VERA"));
  QFont subtitle = painter.font(); subtitle.setPixelSize(24); subtitle.setBold(false); painter.setFont(subtitle);
  painter.drawText(QRect(0, 215, ScreenWidth, 48), Qt::AlignCenter, QStringLiteral("640 × 480  •  BITMAP TEXT TEST"));
  painter.setPen(QColor(220, 255, 245)); painter.drawLine(128, 292, 512, 292);
  painter.drawText(QRect(0, 314, ScreenWidth, 40), Qt::AlignCenter, QStringLiteral("USB BRIDGE  /  FPGA VIDEO"));
  painter.end();
  QByteArray bytes(ScreenWidth * ScreenHeight / 8, '\0');
  for (int y = 0; y < ScreenHeight; ++y) for (int x = 0; x < ScreenWidth; ++x)
    if (qGray(canvas.pixel(x, y)) > 64) bytes[y * 80 + x / 8] |= static_cast<char>(0x80 >> (x & 7));
  return bytes;
}
}

MainWindow::MainWindow() {
  setWindowTitle(QStringLiteral("VERA Test Console"));
  setWindowIcon(QIcon(QStringLiteral(":/icons/vera-gui-icon.png")));
  setMinimumWidth(520);
  auto *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
  auto *exitAction = fileMenu->addAction(QIcon(QStringLiteral(":/adwaita/exit.svg")), QStringLiteral("E&xit"));
  exitAction->setShortcut(QKeySequence::Quit);
  connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
  auto *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
  auto *aboutAction = helpMenu->addAction(QIcon(QStringLiteral(":/adwaita/about.svg")), QStringLiteral("&About"));
  connect(aboutAction, &QAction::triggered, this, [this] { QMessageBox::about(this,
    QStringLiteral("About VERA Test Console"), QStringLiteral("VERA Test Console\n\nQt 6 desktop tools for the VERA FPGA video module.\nThe Arduino Mega is used only as a USB-to-parallel bridge.")); });

  auto *central = new QWidget(this); auto *layout = new QVBoxLayout(central); layout->setSpacing(10);
  auto *connection = new QGroupBox(QStringLiteral("Connection"), central); auto *connectionLayout = new QFormLayout(connection);
  auto *portRow = new QHBoxLayout; ports_ = new QComboBox(connection);
  auto *refreshButton = new QPushButton(QStringLiteral("Refresh"), connection); auto *connectButton = new QPushButton(QStringLiteral("Connect"), connection);
  portRow->addWidget(ports_, 1); portRow->addWidget(refreshButton); portRow->addWidget(connectButton);
  version_ = new QLabel(QStringLiteral("Not connected"), connection); connectionLayout->addRow(QStringLiteral("Serial port"), portRow); connectionLayout->addRow(QStringLiteral("FPGA version"), version_); layout->addWidget(connection);

  auto *control = new QGroupBox(QStringLiteral("Control"), central); auto *controlLayout = new QHBoxLayout(control);
  auto *resetButton = new QPushButton(QStringLiteral("Reset VERA"), control); auto *vgaButton = new QPushButton(QStringLiteral("VGA signal only"), control);
  controlLayout->addWidget(resetButton); controlLayout->addWidget(vgaButton); layout->addWidget(control);

  auto *bitmapTests = new QGroupBox(QStringLiteral("640 × 480 bitmap tests"), central); auto *bitmapLayout = new QGridLayout(bitmapTests);
  auto *monoButton = new QPushButton(QStringLiteral("Monochrome"), bitmapTests); auto *colorButton = new QPushButton(QStringLiteral("Four colours"), bitmapTests);
  auto *textButton = new QPushButton(QStringLiteral("Text"), bitmapTests); auto *cardButton = new QPushButton(QStringLiteral("PM5544 card"), bitmapTests);
  bitmapLayout->addWidget(monoButton, 0, 0); bitmapLayout->addWidget(colorButton, 0, 1); bitmapLayout->addWidget(textButton, 1, 0); bitmapLayout->addWidget(cardButton, 1, 1); layout->addWidget(bitmapTests);
  auto *tileTests = new QGroupBox(QStringLiteral("Tile tests"), central); auto *tileLayout = new QHBoxLayout(tileTests); auto *tileButton = new QPushButton(QStringLiteral("16-colour tile pattern"), tileTests); tileLayout->addWidget(tileButton); layout->addWidget(tileTests);
  auto *spriteTests = new QGroupBox(QStringLiteral("Sprite tests"), central); auto *spriteLayout = new QHBoxLayout(spriteTests); auto *spriteButton = new QPushButton(QStringLiteral("Bouncing sprites"), spriteTests); auto *stopSpriteButton = new QPushButton(QStringLiteral("Stop motion"), spriteTests); spriteLayout->addWidget(spriteButton); spriteLayout->addWidget(stopSpriteButton); layout->addWidget(spriteTests);
  auto *audioTests = new QGroupBox(QStringLiteral("Audio tests"), central); auto *audioLayout = new QHBoxLayout(audioTests); auto *stereoButton = new QPushButton(QStringLiteral("Test left / right stereo"), audioTests); auto *tetrisButton = new QPushButton(QStringLiteral("Play Game Boy Tetris song"), audioTests); auto *marioButton = new QPushButton(QStringLiteral("Play NES Mario Ground theme"), audioTests); auto *stopAudioButton = new QPushButton(QStringLiteral("Stop audio"), audioTests); audioLayout->addWidget(stereoButton); audioLayout->addWidget(tetrisButton); audioLayout->addWidget(marioButton); audioLayout->addWidget(stopAudioButton); layout->addWidget(audioTests);
  auto *systemTests = new QGroupBox(QStringLiteral("System tests"), central); auto *systemLayout = new QHBoxLayout(systemTests); auto *irqButton = new QPushButton(QStringLiteral("Timing and interrupts"), systemTests); auto *sdButton = new QPushButton(QStringLiteral("Probe SD card"), systemTests); systemLayout->addWidget(irqButton); systemLayout->addWidget(sdButton); layout->addWidget(systemTests);
  progress_ = new QProgressBar(central); progress_->setRange(0, 100); progress_->setTextVisible(false); status_ = new QLabel(QStringLiteral("Ready."), central); status_->setWordWrap(true); layout->addWidget(progress_); layout->addWidget(status_); setCentralWidget(central);
  const auto setAdwaitaIcon = [](QPushButton *button, const char *resource) {
    button->setIcon(QIcon(QString::fromLatin1(resource)));
    button->setIconSize(QSize(18, 18));
  };
  setAdwaitaIcon(refreshButton, ":/adwaita/refresh.svg");
  setAdwaitaIcon(connectButton, ":/adwaita/network.svg");
  setAdwaitaIcon(resetButton, ":/adwaita/clear.svg");
  setAdwaitaIcon(vgaButton, ":/adwaita/run.svg");
  setAdwaitaIcon(monoButton, ":/adwaita/video.svg");
  setAdwaitaIcon(colorButton, ":/adwaita/video.svg");
  setAdwaitaIcon(textButton, ":/adwaita/video.svg");
  setAdwaitaIcon(cardButton, ":/adwaita/video.svg");
  setAdwaitaIcon(tileButton, ":/adwaita/video.svg");
  setAdwaitaIcon(spriteButton, ":/adwaita/run.svg");
  setAdwaitaIcon(stopSpriteButton, ":/adwaita/stop.svg");
  setAdwaitaIcon(stereoButton, ":/adwaita/speakers.svg");
  setAdwaitaIcon(tetrisButton, ":/adwaita/music.svg");
  setAdwaitaIcon(marioButton, ":/adwaita/music.svg");
  setAdwaitaIcon(stopAudioButton, ":/adwaita/stop.svg");
  setAdwaitaIcon(irqButton, ":/adwaita/run.svg");
  setAdwaitaIcon(sdButton, ":/adwaita/network.svg");
  spriteTimer_ = new QTimer(this); spriteTimer_->setInterval(1000 / 60);
  songTimer_ = new QTimer(this); songTimer_->setSingleShot(true);
  stereoTimer_ = new QTimer(this); stereoTimer_->setInterval(1500);
  marioTimer_ = new QTimer(this); marioTimer_->setSingleShot(true);
  connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPorts); connect(connectButton, &QPushButton::clicked, this, &MainWindow::connectBridge); connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetVera); connect(vgaButton, &QPushButton::clicked, this, &MainWindow::enableVga); connect(monoButton, &QPushButton::clicked, this, &MainWindow::runMonochromeTest); connect(colorButton, &QPushButton::clicked, this, &MainWindow::runColorTest); connect(textButton, &QPushButton::clicked, this, &MainWindow::runTextTest); connect(cardButton, &QPushButton::clicked, this, &MainWindow::uploadPm5544); connect(tileButton, &QPushButton::clicked, this, &MainWindow::runTileTest); connect(spriteButton, &QPushButton::clicked, this, &MainWindow::runSpriteTest); connect(stopSpriteButton, &QPushButton::clicked, this, &MainWindow::stopSpriteMotion); connect(spriteTimer_, &QTimer::timeout, this, &MainWindow::advanceSprites); connect(stereoButton, &QPushButton::clicked, this, &MainWindow::startStereoTest); connect(stereoTimer_, &QTimer::timeout, this, &MainWindow::advanceStereoTest); connect(tetrisButton, &QPushButton::clicked, this, &MainWindow::startTetrisSong); connect(songTimer_, &QTimer::timeout, this, &MainWindow::advanceTetrisSong); connect(marioButton, &QPushButton::clicked, this, &MainWindow::startMarioSong); connect(marioTimer_, &QTimer::timeout, this, &MainWindow::advanceMarioSong); connect(stopAudioButton, &QPushButton::clicked, this, &MainWindow::stopAudio); connect(irqButton, &QPushButton::clicked, this, &MainWindow::runTimingInterruptTest); connect(sdButton, &QPushButton::clicked, this, &MainWindow::runSdCardTest); refreshPorts();
  QTimer::singleShot(0, this, [this] { if (ports_->count() != 0) connectBridge(); });
}

void MainWindow::closeEvent(QCloseEvent *event) {
  spriteTimer_->stop();
  songTimer_->stop();
  stereoTimer_->stop();
  marioTimer_->stop();
  if (bridge_.isOpen()) {
    QString error;
    bridge_.reset(&error);
    bridge_.close();
  }
  event->accept();
}

void MainWindow::refreshPorts() { const QString previous = ports_->currentData().toString(); ports_->clear(); for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) ports_->addItem(QStringLiteral("%1 — %2").arg(info.portName(), info.description()), info.portName()); const int index = ports_->findData(previous); if (index >= 0) ports_->setCurrentIndex(index); }
void MainWindow::connectBridge() { const QString port = ports_->currentData().toString(); if (port.isEmpty()) { setStatus(QStringLiteral("No serial ports found."), true); return; } QString error; if (!bridge_.open(port, &error) || !bridge_.ping(&error)) { bridge_.close(); version_->setText(QStringLiteral("Not connected")); setStatus(error, true); return; } version_->setText(readVersion()); setStatus(QStringLiteral("Connected to bridge on %1.").arg(port)); }
bool MainWindow::requireBridge() { if (bridge_.isOpen()) return true; setStatus(QStringLiteral("Connect to the VERA bridge first."), true); return false; }
bool MainWindow::writeRegister(quint8 reg, quint8 value) { QString error; if (bridge_.writeRegister(reg, value, &error)) return true; setStatus(error, true); return false; }
bool MainWindow::readRegister(quint8 reg, quint8 *value) { QString error; if (bridge_.readRegister(reg, value, &error)) return true; setStatus(error, true); return false; }

QString MainWindow::readVersion() { quint8 originalCtrl = 0; QString error; if (!bridge_.readRegister(RegCtrl, &originalCtrl, &error)) return QStringLiteral("Read failed"); QByteArray id; for (quint8 bank = 60; bank <= 63; ++bank) { if (!bridge_.writeRegister(RegCtrl, static_cast<quint8>((bank << 1) | (originalCtrl & 1)), &error)) break; for (quint8 offset = 0; offset < 4; ++offset) { quint8 value = 0; const quint8 physicalOffset = (offset + 3) & 3; if (!bridge_.readRegister(RegDcVideo + physicalOffset, &value, &error) || value == 0) goto done; id.append(static_cast<char>(value)); } } done: bridge_.writeRegister(RegCtrl, originalCtrl, &error); return id.isEmpty() ? QStringLiteral("Read failed") : QString::fromLatin1(id); }
void MainWindow::resetVera() { if (!requireBridge()) return; songTimer_->stop(); stereoTimer_->stop(); marioTimer_->stop(); QString error; if (!bridge_.reset(&error)) { setStatus(error, true); return; } version_->setText(readVersion()); setStatus(QStringLiteral("VERA reset complete.")); }
void MainWindow::enableVga() { if (!requireBridge()) return; if (writeRegister(RegCtrl, 0) && writeRegister(RegDcVideo, VideoModeVga)) setStatus(QStringLiteral("640 × 480 @ 60 Hz VGA enabled; layers are off.")); }

bool MainWindow::writePalette(const QVector<QRgb> &colors) { QByteArray palette; palette.reserve(colors.size() * 2); for (QRgb color : colors) { const QColor rgb(color); palette.append(static_cast<char>(((rgb.green() >> 4) << 4) | (rgb.blue() >> 4))); palette.append(static_cast<char>(rgb.red() >> 4)); } return writeBuffer(PaletteBase, palette, QStringLiteral("palette")); }
bool MainWindow::writeBuffer(quint32 address, const QByteArray &data, const QString &label) { QString error; for (int offset = 0; offset < data.size(); offset += BridgeClient::MaxVramChunk) { if (!bridge_.writeVram(address + static_cast<quint32>(offset), data.mid(offset, BridgeClient::MaxVramChunk), &error)) { setStatus(error, true); return false; } progress_->setValue((100 * qMin(offset + BridgeClient::MaxVramChunk, data.size())) / data.size()); status_->setText(QStringLiteral("Uploading %1… %2%").arg(label).arg(progress_->value())); QApplication::processEvents(); } return true; }
bool MainWindow::configureBitmap(quint8 colorDepth, bool wide) { const quint8 scale = wide ? 128 : 64; return writeRegister(RegCtrl, 0) && writeRegister(RegDcHscale, scale) && writeRegister(RegDcVscale, scale) && writeRegister(RegDcBorder, 0) && writeRegister(RegL0Config, static_cast<quint8>(0x04 | colorDepth)) && writeRegister(RegL0Mapbase, 0) && writeRegister(RegL0Tilebase, wide ? 1 : 0) && writeRegister(RegL0HscrollL, 0) && writeRegister(RegL0HscrollH, 0) && writeRegister(RegL0VscrollL, 0) && writeRegister(RegL0VscrollH, 0) && writeRegister(RegDcVideo, VideoLayer0 | VideoModeVga); }
bool MainWindow::configureTiles() { return writeRegister(RegCtrl, 0) && writeRegister(RegDcHscale, 128) && writeRegister(RegDcVscale, 128) && writeRegister(RegDcBorder, 0) && writeRegister(RegL0Config, 0x62) && writeRegister(RegL0Mapbase, 0) && writeRegister(RegL0Tilebase, 0x20) && writeRegister(RegL0HscrollL, 0) && writeRegister(RegL0HscrollH, 0) && writeRegister(RegL0VscrollL, 0) && writeRegister(RegL0VscrollH, 0) && writeRegister(RegDcVideo, VideoLayer0 | VideoModeVga); }

void MainWindow::runMonochromeTest() { if (!requireBridge()) return; progress_->setValue(0); if (writePalette({qRgb(3, 12, 28), qRgb(230, 255, 245)}) && writeBuffer(0, makeMonoPattern(), QStringLiteral("monochrome test")) && configureBitmap(0, true)) setStatus(QStringLiteral("640 × 480 1bpp monochrome pattern loaded.")); }
void MainWindow::runColorTest() { if (!requireBridge()) return; progress_->setValue(0); if (writePalette({qRgb(8, 12, 28), qRgb(0, 220, 255), qRgb(255, 48, 80), qRgb(255, 235, 40)}) && writeBuffer(0, makeColorPattern(), QStringLiteral("four-colour test")) && configureBitmap(1, true)) setStatus(QStringLiteral("640 × 480 2bpp four-colour pattern loaded.")); }
void MainWindow::runTextTest() { if (!requireBridge()) return; progress_->setValue(0); if (writePalette({qRgb(4, 16, 36), qRgb(150, 255, 220)}) && writeBuffer(0, makeTextPattern(), QStringLiteral("text test")) && configureBitmap(0, true)) setStatus(QStringLiteral("640 × 480 monochrome bitmap text test loaded.")); }
void MainWindow::runTileTest() { if (!requireBridge()) return; progress_->setValue(0); QVector<QRgb> colors = {qRgb(5, 10, 28)}; for (int i = 0; i < 15; ++i) colors.append(QColor::fromHsv((i * 23 + 185) % 360, 210, 255).rgb()); QByteArray tiles(16 * 32, '\0'); for (int tile = 0; tile < 16; ++tile) for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) { const quint8 pixel = static_cast<quint8>(1 + ((tile + x / 2 + y / 2) % 15)); tiles[tile * 32 + y * 4 + x / 2] |= static_cast<char>(pixel << ((x & 1) ? 0 : 4)); } QByteArray map(128 * 64 * 2, '\0'); for (int y = 0; y < 60; ++y) for (int x = 0; x < 80; ++x) map[(y * 128 + x) * 2] = static_cast<char>(1 + ((x / 5 + y / 4) % 15)); if (writePalette(colors) && writeBuffer(0x4000, tiles, QStringLiteral("tile set")) && writeBuffer(0, map, QStringLiteral("tile map")) && configureTiles()) setStatus(QStringLiteral("640 × 480 tiled 4bpp pattern loaded.")); }

void MainWindow::runSpriteTest() {
  if (!requireBridge()) return;
  spriteTimer_->stop();
  progress_->setValue(0);
  QVector<QRgb> colors = {qRgb(4, 12, 28), qRgb(0, 235, 255), qRgb(255, 65, 100), qRgb(255, 235, 55), qRgb(170, 110, 255)};
  QByteArray pixels(128, '\0');  // 16 × 16 at 4bpp.
  for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
    const int dx = x - 7, dy = y - 7;
    quint8 pixel = 0;
    if (dx * dx + dy * dy < 57) pixel = static_cast<quint8>(1 + ((x / 4 + y / 4) % 4));
    if (x == 7 || y == 7) pixel = 3;
    pixels[y * 8 + x / 2] |= static_cast<char>(pixel << ((x & 1) ? 0 : 4));
  }
  sprites_ = {{40, 48, 3, 2}, {584, 80, -2, 3}, {220, 392, 4, -2}, {408, 276, -3, -3}};
  QByteArray attributes(1024, '\0');
  const quint16 spriteAddress = static_cast<quint16>(SpritePixelsBase / 32);
  for (int i = 0; i < sprites_.size(); ++i) {
    const int offset = i * 8;
    attributes[offset + 0] = static_cast<char>(spriteAddress & 0xff);
    attributes[offset + 1] = static_cast<char>(spriteAddress >> 8);
    attributes[offset + 2] = static_cast<char>(sprites_[i].x & 0xff);
    attributes[offset + 3] = static_cast<char>(sprites_[i].x >> 8);
    attributes[offset + 4] = static_cast<char>(sprites_[i].y & 0xff);
    attributes[offset + 5] = static_cast<char>(sprites_[i].y >> 8);
    attributes[offset + 6] = 0x0c;  // z-depth 3.
    attributes[offset + 7] = 0x50;  // 16-pixel width and height.
  }
  if (writePalette(colors) && writeBuffer(SpritePixelsBase, pixels, QStringLiteral("sprite pixels")) &&
      writeBuffer(SpriteAttrBase, attributes, QStringLiteral("sprite attributes")) &&
      writeRegister(RegCtrl, 0) && writeRegister(RegDcBorder, 0) &&
      writeRegister(RegDcVideo, static_cast<quint8>(0x40 | VideoModeVga))) {
    spriteTimer_->start();
    setStatus(QStringLiteral("Four 16 × 16 sprites are bouncing at 60 Hz."));
  }
}

void MainWindow::stopSpriteMotion() {
  spriteTimer_->stop();
  setStatus(QStringLiteral("Sprite motion paused."));
}

void MainWindow::advanceSprites() {
  if (!bridge_.isOpen() || sprites_.isEmpty()) { spriteTimer_->stop(); return; }
  QByteArray attributes(sprites_.size() * 8, '\0');
  const quint16 spriteAddress = static_cast<quint16>(SpritePixelsBase / 32);
  for (int i = 0; i < sprites_.size(); ++i) {
    SpriteMotion &sprite = sprites_[i];
    sprite.x += sprite.dx; sprite.y += sprite.dy;
    if (sprite.x <= 0 || sprite.x >= ScreenWidth - 16) { sprite.dx = -sprite.dx; sprite.x = qBound(0, sprite.x, ScreenWidth - 16); }
    if (sprite.y <= 0 || sprite.y >= ScreenHeight - 16) { sprite.dy = -sprite.dy; sprite.y = qBound(0, sprite.y, ScreenHeight - 16); }
    const int offset = i * 8;
    attributes[offset + 0] = static_cast<char>(spriteAddress & 0xff);
    attributes[offset + 1] = static_cast<char>(spriteAddress >> 8);
    attributes[offset + 2] = static_cast<char>(sprite.x & 0xff);
    attributes[offset + 3] = static_cast<char>(sprite.x >> 8);
    attributes[offset + 4] = static_cast<char>(sprite.y & 0xff);
    attributes[offset + 5] = static_cast<char>(sprite.y >> 8);
    attributes[offset + 6] = 0x0c;
    attributes[offset + 7] = 0x50;
  }
  QString error;
  if (!bridge_.writeVram(SpriteAttrBase, attributes, &error)) {
    spriteTimer_->stop();
    setStatus(error, true);
  }
}

void MainWindow::startStereoTest() {
  if (!requireBridge()) return;
  songTimer_->stop();
  marioTimer_->stop();
  stereoTimer_->stop();
  progress_->setValue(0);
  if (!writeBuffer(AudioPsgBase, QByteArray(64, '\0'), QStringLiteral("stereo PSG setup"))) return;
  stereoLeft_ = true;
  stereoTimer_->start();
  advanceStereoTest();
}

void MainWindow::advanceStereoTest() {
  if (!bridge_.isOpen()) { stereoTimer_->stop(); return; }
  QByteArray channel(4, '\0');
  const quint16 phase = psgPhaseIncrement(440);
  channel[0] = static_cast<char>(phase & 0xff);
  channel[1] = static_cast<char>(phase >> 8);
  channel[2] = static_cast<char>(0x3f | (stereoLeft_ ? 0x40 : 0x80));
  channel[3] = static_cast<char>(0x80);  // Triangle waveform.
  QString error;
  if (!bridge_.writeVram(AudioPsgBase, channel, &error)) {
    stereoTimer_->stop();
    setStatus(error, true);
    return;
  }
  setStatus(QStringLiteral("Stereo test: 440 Hz on the %1 channel.").arg(stereoLeft_ ? QStringLiteral("LEFT") : QStringLiteral("RIGHT")));
  stereoLeft_ = !stereoLeft_;
}

void MainWindow::startTetrisSong() {
  if (!requireBridge()) return;
  songTimer_->stop();
  stereoTimer_->stop();
  marioTimer_->stop();
  progress_->setValue(0);
  if (!writeBuffer(AudioPsgBase, QByteArray(64, '\0'), QStringLiteral("PSG setup"))) return;
  songNoteIndex_ = 0;
  advanceTetrisSong();
}

void MainWindow::advanceTetrisSong() {
  if (!bridge_.isOpen()) { songTimer_->stop(); return; }
  const SongNote &note = TetrisSong[songNoteIndex_];
  QByteArray channels(16, '\0');
  const int harmonyFrequency = static_cast<int>(std::lround(note.leadFrequency * 2.0 / 3.0));
  setPsgChannel(&channels, 0, note.leadFrequency, 48, 0, 32);       // 50% pulse lead.
  setPsgChannel(&channels, 1, harmonyFrequency, 27, 0, 16);          // 25% pulse harmony.
  setPsgChannel(&channels, 2, note.bassFrequency, 42, 2);            // Triangle bass.
  if (note.percussion) setPsgChannel(&channels, 3, 4096, 19, 3);     // Short noise hit.
  QString error;
  if (!bridge_.writeVram(AudioPsgBase, channels, &error)) {
    songTimer_->stop();
    setStatus(error, true);
    return;
  }
  songNoteIndex_ = (songNoteIndex_ + 1) % TetrisSongLength;
  songTimer_->start(note.duration * SongEighthNoteMs);
  setStatus(QStringLiteral("Game Boy-style Tetris Type A is playing (loops). Use Stop audio to mute."));
}

void MainWindow::startMarioSong() {
  if (!requireBridge()) return;
  songTimer_->stop();
  stereoTimer_->stop();
  marioTimer_->stop();
  progress_->setValue(0);
  if (!writeBuffer(AudioPsgBase, QByteArray(64, '\0'), QStringLiteral("NES PSG setup"))) return;
  marioNoteIndex_ = 0;
  advanceMarioSong();
}

void MainWindow::advanceMarioSong() {
  if (!bridge_.isOpen()) { marioTimer_->stop(); return; }
  const SongNote &note = MarioSong[marioNoteIndex_];
  QByteArray channels(16, '\0');
  const int harmonyFrequency = note.leadFrequency == 0 ? 0 : static_cast<int>(std::lround(note.leadFrequency * 2.0 / 3.0));
  setPsgChannel(&channels, 0, note.leadFrequency, 50, 0, 32);       // NES pulse 1 lead.
  setPsgChannel(&channels, 1, harmonyFrequency, 30, 0, 16);          // NES pulse 2 accompaniment.
  setPsgChannel(&channels, 2, note.bassFrequency, 43, 2);            // NES triangle bass.
  const bool percussion = note.leadFrequency != 0 &&
                           (marioNoteIndex_ % 4 == 0 || note.duration >= 4);
  if (percussion) setPsgChannel(&channels, 3, 4096, 20, 3);           // NES-style noise hit.
  QString error;
  if (!bridge_.writeVram(AudioPsgBase, channels, &error)) {
    marioTimer_->stop();
    setStatus(error, true);
    return;
  }
  marioNoteIndex_ = (marioNoteIndex_ + 1) % MarioSongLength;
  marioTimer_->start(note.duration * MarioSixteenthNoteMs);
  setStatus(QStringLiteral("NES-style Super Mario Bros. Ground Level theme is playing (loops). Use Stop audio to mute."));
}

void MainWindow::stopAudio() {
  if (!requireBridge()) return;
  songTimer_->stop();
  stereoTimer_->stop();
  marioTimer_->stop();
  progress_->setValue(0);
  if (writeBuffer(AudioPsgBase, QByteArray(64, '\0'), QStringLiteral("audio stop")))
    setStatus(QStringLiteral("PSG channels muted."));
}

void MainWindow::runTimingInterruptTest() {
  if (!requireBridge()) return;
  spriteTimer_->stop();
  // IRQ_STATUS bits 0 and 1 latch VSync and the requested line respectively.
  if (!writeRegister(RegCtrl, 0) || !writeRegister(RegDcVideo, VideoModeVga) ||
      !writeRegister(RegIrqEnable, 0) || !writeRegister(RegIrqStatus, 0x03) ||
      !writeRegister(RegIrqLine, 240) || !writeRegister(RegIrqEnable, 0x03)) return;
  QVector<double> vsyncTimes;
  int lineIrqCount = 0, scanlineWraps = 0, scanlineMinimum = 512, scanlineMaximum = -1, samples = 0;
  int previousScanline = -1;
  QElapsedTimer timer;
  timer.start();
  bool transportOk = true;
  while (timer.elapsed() < 1000 && transportOk) {
    quint8 scanline = 0, status = 0;
    transportOk = readRegister(RegIrqLine, &scanline) && readRegister(RegIrqStatus, &status);
    if (!transportOk) break;
    ++samples;
    scanlineMinimum = qMin(scanlineMinimum, static_cast<int>(scanline));
    scanlineMaximum = qMax(scanlineMaximum, static_cast<int>(scanline));
    if (previousScanline >= 0 && scanline + 16 < previousScanline) ++scanlineWraps;
    previousScanline = scanline;
    if (status & 0x01) vsyncTimes.append(timer.nsecsElapsed() / 1'000'000.0);
    if (status & 0x02) ++lineIrqCount;
    if (status & 0x03) transportOk = writeRegister(RegIrqStatus, status & 0x03);
    QThread::usleep(750);
  }
  writeRegister(RegIrqEnable, 0);
  writeRegister(RegIrqStatus, 0x03);
  QVector<double> intervals;
  for (int i = 1; i < vsyncTimes.size(); ++i) intervals.append(vsyncTimes[i] - vsyncTimes[i - 1]);
  double mean = 0, minimum = 0, maximum = 0, deviation = 0;
  if (!intervals.isEmpty()) {
    minimum = maximum = intervals.first();
    for (double value : intervals) { mean += value; minimum = qMin(minimum, value); maximum = qMax(maximum, value); }
    mean /= intervals.size();
    for (double value : intervals) deviation += (value - mean) * (value - mean);
    deviation = std::sqrt(deviation / intervals.size());
  }
  const bool passed = transportOk && vsyncTimes.size() >= 20 && lineIrqCount >= 20 && !intervals.isEmpty();
  const QString report = QStringLiteral(
      "Measurement duration: %1 ms\n"
      "Register samples: %2\n"
      "Scanline range: %3–%4\n"
      "Observed scanline wraps: %5\n\n"
      "VSync IRQ events: %6\n"
      "Line-240 IRQ events: %7\n"
      "Frame interval mean: %8 ms\n"
      "Frame interval min/max: %9 / %10 ms\n"
      "Host-observed deviation: %11 ms\n\n"
      "Note: interval values include USB/host polling jitter; IRQ counts and scanline wraps are the primary hardware checks.")
      .arg(timer.elapsed()).arg(samples).arg(scanlineMinimum).arg(scanlineMaximum).arg(scanlineWraps)
      .arg(vsyncTimes.size()).arg(lineIrqCount).arg(mean, 0, 'f', 3).arg(minimum, 0, 'f', 3)
      .arg(maximum, 0, 'f', 3).arg(deviation, 0, 'f', 3);
  showReport(QStringLiteral("Timing and interrupt diagnostics"), report, passed);
  setStatus(passed ? QStringLiteral("PASS: timing and interrupt diagnostics complete.") : QStringLiteral("FAIL: timing diagnostic did not meet expected event counts."), !passed);
}

bool MainWindow::spiTransfer(const QByteArray &transmit, QByteArray *receive) {
  QString error;
  if (bridge_.spiTransfer(transmit, receive, &error)) return true;
  setStatus(QStringLiteral("SPI transfer failed: %1").arg(error), true);
  return false;
}

bool MainWindow::spiTransfer(quint8 transmit, quint8 *receive) {
  QByteArray received;
  if (!spiTransfer(QByteArray(1, static_cast<char>(transmit)), &received)) return false;
  *receive = static_cast<quint8>(received.at(0));
  return true;
}

bool MainWindow::sdCommand(quint8 command, quint32 argument, quint8 crc, quint8 *r1,
                           QByteArray *trailing) {
  QByteArray clocked;
  clocked.reserve(22);
  clocked.append(static_cast<char>(0x40 | command));
  clocked.append(static_cast<char>(argument >> 24));
  clocked.append(static_cast<char>(argument >> 16));
  clocked.append(static_cast<char>(argument >> 8));
  clocked.append(static_cast<char>(argument));
  clocked.append(static_cast<char>(crc));
  clocked.append(QByteArray(16, static_cast<char>(0xff)));
  QByteArray received;
  if (!spiTransfer(clocked, &received)) return false;
  for (int index = 6; index < received.size(); ++index) {
    const quint8 response = static_cast<quint8>(received.at(index));
    if ((response & 0x80) == 0) {
      *r1 = response;
      if (trailing) *trailing = received.mid(index + 1);
      return true;
    }
  }
  setStatus(QStringLiteral("SD command %1 did not return an R1 response.").arg(command), true);
  return false;
}

bool MainWindow::initializeSdCard(bool *highCapacity, quint32 *ocrValue) {
  quint8 r1 = 0;
  // CS high, slow startup clock: at least 80 clocks with the card deselected.
  setOperationProgress(QStringLiteral("SD: deselecting card and sending startup clocks…"));
  if (!writeRegister(RegSpiControl, 0x02)) return false;
  QThread::msleep(2);
  QByteArray received;
  if (!spiTransfer(QByteArray(10, static_cast<char>(0xff)), &received)) return false;
  // Select card, keep the initialisation clock at approximately 390 kHz.
  setOperationProgress(QStringLiteral("SD: issuing CMD0 (reset)…"));
  if (!writeRegister(RegSpiControl, 0x03)) return false;
  QThread::msleep(2);  // Allow CS to settle before the first command frame.
  if (!sdCommand(0, 0, 0x95, &r1)) return false;
  if (r1 != 0x01) { setStatus(QStringLiteral("SD CMD0 returned 0x%1 (no card or wiring issue).").arg(r1, 2, 16, QLatin1Char('0')), true); return false; }
  setOperationProgress(QStringLiteral("SD: checking CMD8 voltage pattern…"));
  QByteArray commandTail;
  if (!sdCommand(8, 0x000001aa, 0x87, &r1, &commandTail)) return false;
  const bool v2Card = r1 == 0x01;
  if (v2Card) {
    if (commandTail.size() < 4) { setStatus(QStringLiteral("SD CMD8 response was truncated."), true); return false; }
    const quint8 r7[] = {static_cast<quint8>(commandTail.at(0)), static_cast<quint8>(commandTail.at(1)),
                          static_cast<quint8>(commandTail.at(2)), static_cast<quint8>(commandTail.at(3))};
    if (r7[2] != 0x01 || r7[3] != 0xaa) { setStatus(QStringLiteral("SD CMD8 voltage/check pattern mismatch."), true); return false; }
  } else if ((r1 & 0x04) == 0) { setStatus(QStringLiteral("SD CMD8 returned 0x%1.").arg(r1, 2, 16, QLatin1Char('0')), true); return false; }
  bool ready = false;
  for (int attempt = 0; attempt < 100 && !ready; ++attempt) {
    setOperationProgress(QStringLiteral("SD: waiting for ACMD41 readiness (%1/100)…").arg(attempt + 1));
    if (!sdCommand(55, 0, 0x01, &r1) || !sdCommand(41, v2Card ? 0x40000000 : 0, 0x01, &r1)) return false;
    ready = r1 == 0x00;
    if (!ready) QThread::msleep(10);
  }
  if (!ready) { setStatus(QStringLiteral("SD card did not leave idle state (ACMD41)."), true); return false; }
  setOperationProgress(QStringLiteral("SD: reading OCR (CMD58)…"));
  if (!sdCommand(58, 0, 0x01, &r1, &commandTail) || r1 != 0x00) { setStatus(QStringLiteral("SD CMD58 failed."), true); return false; }
  if (commandTail.size() < 4) { setStatus(QStringLiteral("SD CMD58 response was truncated."), true); return false; }
  const quint8 ocr[] = {static_cast<quint8>(commandTail.at(0)), static_cast<quint8>(commandTail.at(1)),
                         static_cast<quint8>(commandTail.at(2)), static_cast<quint8>(commandTail.at(3))};
  *highCapacity = (ocr[0] & 0x40) != 0;
  *ocrValue = (quint32(ocr[0]) << 24) | (quint32(ocr[1]) << 16) | (quint32(ocr[2]) << 8) | ocr[3];
  return true;
}

bool MainWindow::sdReadSector(quint32 lba, bool highCapacity, QByteArray *sector) {
  quint8 r1 = 0;
  setOperationProgress(QStringLiteral("SD: reading sector %1…").arg(lba), 0, 512);
  const quint32 argument = highCapacity ? lba : lba * 512U;
  QByteArray commandTail;
  if (!sdCommand(17, argument, 0x01, &r1, &commandTail) || r1 != 0x00) {
    setStatus(QStringLiteral("SD CMD17 failed for LBA %1.").arg(lba), true);
    return false;
  }
  QByteArray data;
  bool tokenFound = false;
  const int earlyToken = commandTail.indexOf(static_cast<char>(0xfe));
  if (earlyToken >= 0) {
    tokenFound = true;
    data.append(commandTail.mid(earlyToken + 1));
  }
  for (int attempt = 0; attempt < 32 && !tokenFound; ++attempt) {
    QByteArray received;
    if (!spiTransfer(QByteArray(64, static_cast<char>(0xff)), &received)) return false;
    const int token = received.indexOf(static_cast<char>(0xfe));
    if (token >= 0) {
      tokenFound = true;
      data.append(received.mid(token + 1));
    }
  }
  if (!tokenFound) { setStatus(QStringLiteral("SD sector data token timed out."), true); return false; }
  while (data.size() < 514) {
    const int remaining = 514 - data.size();
    const int chunkSize = qMin(remaining, BridgeClient::MaxVramChunk);
    QByteArray received;
    if (!spiTransfer(QByteArray(chunkSize, static_cast<char>(0xff)), &received)) return false;
    data.append(received);
    setOperationProgress(QStringLiteral("SD: reading sector %1…").arg(lba), qMin(data.size(), 512), 512);
  }
  *sector = data.left(512);
  setOperationProgress(QStringLiteral("SD: checking sector %1 transfer…").arg(lba), 512, 512);
  return true;  // The final two received bytes are the SD CRC; this diagnostic does not validate it.
}

void MainWindow::runSdCardTest() {
  if (!requireBridge()) return;
  spriteTimer_->stop();
  if (QMessageBox::question(this, QStringLiteral("FAT32 diagnostics"),
      QStringLiteral("This reads the SD card's MBR, first FAT32 boot sector, and FSInfo sector. It does not write to the card. Continue?"),
      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
  QProgressDialog progress(QStringLiteral("Preparing SD diagnostics…"), QString(), 0, 0, this);
  progress.setWindowTitle(QStringLiteral("SD FAT32 diagnostics"));
  progress.setWindowModality(Qt::ApplicationModal);
  progress.setCancelButton(nullptr);
  progress.setMinimumDuration(0);
  progress.show();
  operationProgress_ = &progress;
  setOperationProgress(QStringLiteral("Preparing SD diagnostics…"));

  bool highCapacity = false;
  quint32 ocr = 0;
  QString report;
  bool passed = false;
  for (int attempt = 0; attempt < 2 && !passed; ++attempt) {
    setOperationProgress(QStringLiteral("SD initialization attempt %1 of 2…").arg(attempt + 1));
    passed = initializeSdCard(&highCapacity, &ocr);
    if (!passed) {
      setOperationProgress(QStringLiteral("SD: recovering bus before retry…"));
      writeRegister(RegSpiControl, 0x02);
      QThread::msleep(10);
      QByteArray received;
      spiTransfer(QByteArray(10, static_cast<char>(0xff)), &received);
      QThread::msleep(50);
    }
  }
  if (!passed) {
    report = QStringLiteral("SD initialization failed after two attempts.\n\n%1").arg(status_->text());
  } else {
    setOperationProgress(QStringLiteral("SD: switching to high-speed read clock…"));
    if (!writeRegister(RegSpiControl, 0x01)) {
      passed = false;
      report = QStringLiteral("Could not select the SD card for high-speed reads.\n\n%1").arg(status_->text());
    }
    QThread::msleep(1);
    QByteArray mbr;
    if (passed && !sdReadSector(0, highCapacity, &mbr)) {
      passed = false;
      report = QStringLiteral("Could not read sector 0.\n\n%1").arg(status_->text());
    }
    if (passed) {
      const auto u8 = [](const QByteArray &data, int offset) { return static_cast<quint8>(data.at(offset)); };
      const auto le16 = [&u8](const QByteArray &data, int offset) { return quint16(u8(data, offset)) | (quint16(u8(data, offset + 1)) << 8); };
      const auto le32 = [&u8](const QByteArray &data, int offset) { return quint32(u8(data, offset)) | (quint32(u8(data, offset + 1)) << 8) | (quint32(u8(data, offset + 2)) << 16) | (quint32(u8(data, offset + 3)) << 24); };
      int fat32Entry = -1;
      QStringList entries;
      for (int index = 0; index < 4; ++index) {
        const int offset = 446 + index * 16;
        const quint8 type = u8(mbr, offset + 4);
        const quint32 start = le32(mbr, offset + 8), sectors = le32(mbr, offset + 12);
        entries.append(QStringLiteral("%1: type 0x%2, LBA %3, %4 sectors").arg(index + 1).arg(type, 2, 16, QLatin1Char('0')).arg(start).arg(sectors));
        if (fat32Entry < 0 && sectors != 0 && (type == 0x0b || type == 0x0c || type == 0x1b || type == 0x1c)) fat32Entry = index;
      }
      const bool mbrSignature = u8(mbr, 510) == 0x55 && u8(mbr, 511) == 0xaa;
      report = QStringLiteral("Card: %1\nOCR: 0x%2\nMBR signature: %3\n\nPartitions\n%4\n")
          .arg(highCapacity ? QStringLiteral("SDHC/SDXC") : QStringLiteral("SDSC"))
          .arg(ocr, 8, 16, QLatin1Char('0')).arg(mbrSignature ? QStringLiteral("valid") : QStringLiteral("INVALID"))
          .arg(entries.join(QLatin1Char('\n')));
      passed = mbrSignature && fat32Entry >= 0;
      if (passed) {
        const int offset = 446 + fat32Entry * 16;
        const quint32 startLba = le32(mbr, offset + 8), partitionSectors = le32(mbr, offset + 12);
        QByteArray boot;
        passed = sdReadSector(startLba, highCapacity, &boot);
        if (passed) {
          const quint16 bytesPerSector = le16(boot, 11), reserved = le16(boot, 14), rootEntries = le16(boot, 17);
          const quint8 sectorsPerCluster = u8(boot, 13), fats = u8(boot, 16);
          const quint32 totalSectors = le16(boot, 19) ? le16(boot, 19) : le32(boot, 32);
          const quint32 fatSectors = le16(boot, 22) ? le16(boot, 22) : le32(boot, 36);
          const quint32 rootCluster = le32(boot, 44), fsInfo = le16(boot, 48), hidden = le32(boot, 28);
          const quint32 dataSectors = totalSectors - reserved - quint32(fats) * fatSectors;
          const quint32 clusters = sectorsPerCluster ? dataSectors / sectorsPerCluster : 0;
          const bool fat32 = bytesPerSector == 512 && rootEntries == 0 && fatSectors != 0 && clusters >= 65525;
          report += QStringLiteral("\nFirst FAT32 partition\nStart LBA: %1\nPartition sectors: %2\nBytes/sector: %3\nSectors/cluster: %4\nReserved sectors: %5\nFATs: %6 × %7 sectors\nRoot cluster: %8\nCluster count: %9\nHidden-sector match: %10\nFAT32 validation: %11\n")
              .arg(startLba).arg(partitionSectors).arg(bytesPerSector).arg(sectorsPerCluster).arg(reserved).arg(fats).arg(fatSectors).arg(rootCluster).arg(clusters)
              .arg(hidden == startLba ? QStringLiteral("yes") : QStringLiteral("no"))
              .arg(fat32 ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
          passed = fat32 && hidden == startLba && u8(boot, 510) == 0x55 && u8(boot, 511) == 0xaa;
          if (passed && fsInfo != 0 && fsInfo < reserved) {
            QByteArray info;
            if (sdReadSector(startLba + fsInfo, highCapacity, &info)) {
              const bool fsInfoValid = le32(info, 0) == 0x41615252 && le32(info, 484) == 0x61417272 && le16(info, 510) == 0xaa55;
              report += QStringLiteral("\nFSInfo sector: %1\nFSInfo signatures: %2\nFree clusters: %3\nNext free cluster: %4\n")
                  .arg(fsInfo).arg(fsInfoValid ? QStringLiteral("valid") : QStringLiteral("INVALID")).arg(le32(info, 488)).arg(le32(info, 492));
              passed = passed && fsInfoValid;
            } else {
              report += QStringLiteral("\nFSInfo sector could not be read.\n");
              passed = false;
            }
          }
        } else {
          report += QStringLiteral("\nCould not read the first FAT32 boot sector.\n");
        }
      } else {
        report += QStringLiteral("\nNo valid MBR/FAT32 partition was found.\n");
      }
    }
  }
  quint8 ignored = 0;
  writeRegister(RegSpiControl, 0x02);  // Deselect card.
  spiTransfer(0xff, &ignored);
  operationProgress_ = nullptr;
  progress.close();
  showReport(QStringLiteral("SD FAT32 diagnostics"), report, passed);
  setStatus(passed ? QStringLiteral("PASS: SD FAT32 diagnostics complete.") : QStringLiteral("FAIL: SD FAT32 diagnostics reported an error."), !passed);
}

void MainWindow::showReport(const QString &title, const QString &report, bool success) {
  QDialog dialog(this);
  dialog.setWindowTitle(title);
  dialog.setModal(true);
  dialog.resize(600, 500);
  auto *layout = new QVBoxLayout(&dialog);
  auto *summary = new QLabel(success ? QStringLiteral("PASS") : QStringLiteral("FAIL"), &dialog);
  summary->setStyleSheet(success ? QStringLiteral("color: #167d37; font-weight: 600;") : QStringLiteral("color: #b00020; font-weight: 600;"));
  auto *details = new QPlainTextEdit(&dialog);
  details->setReadOnly(true);
  details->setPlainText(report);
  details->setLineWrapMode(QPlainTextEdit::NoWrap);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  layout->addWidget(summary);
  layout->addWidget(details, 1);
  layout->addWidget(buttons);
  dialog.exec();
}

void MainWindow::setOperationProgress(const QString &text, int value, int maximum) {
  if (!operationProgress_) return;
  if (maximum > 0 && operationProgress_->maximum() != maximum) operationProgress_->setRange(0, maximum);
  if (maximum == 0 && operationProgress_->maximum() != 0) operationProgress_->setRange(0, 0);
  operationProgress_->setLabelText(text);
  if (value >= 0) operationProgress_->setValue(value);
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void MainWindow::uploadPm5544() { if (!requireBridge()) return; uploadImage(QStringLiteral(":/images/philips-pm5544-320x240-indexed.png")); }
bool MainWindow::uploadImage(const QString &resourcePath) { QImage image(resourcePath); if (image.isNull() || image.size() != QSize(320, 240) || image.format() != QImage::Format_Indexed8) { setStatus(QStringLiteral("Embedded PM5544 image is not a 320 × 240 indexed PNG."), true); return false; } QVector<QRgb> colors = image.colorTable(); QByteArray pixels; pixels.reserve(image.width() * image.height()); for (int y = 0; y < image.height(); ++y) pixels.append(reinterpret_cast<const char *>(image.constScanLine(y)), image.width()); progress_->setValue(0); if (writePalette(colors) && writeBuffer(0, pixels, QStringLiteral("PM5544 card")) && configureBitmap(3, false)) setStatus(QStringLiteral("PM5544 card loaded: 320 × 240 8bpp, scaled to 640 × 480.")); return true; }
void MainWindow::setStatus(const QString &text, bool error) { status_->setText(text); status_->setStyleSheet(error ? QStringLiteral("color: #b00020;") : QString()); }
