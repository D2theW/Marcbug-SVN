// ======================================================================== //
// Copyright 2009-2010 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

// This Widget creates one tab for each available/selected core and invokes
// sccVirtualDisplay as actual "screen". It also opens sockets for each
// core and forwards the events that sccVirtualDisplay detects...

#include "sccDisplayWidget.h"

// Constructor and destructor
sccDisplayWidget::sccDisplayWidget(sccExtApi *sccAccess, messageHandler *log, QStatusBar* statusBar, uInt64 pFrameOffset, coreMask* coresEnabled, uInt32 socketMode, sccDisplay* displayApp) : QWidget(displayApp) {
  pollServerRunning = false;
  pollTimer = NULL;
  showDynamicContent = false;
  this->sccAccess = sccAccess;
  this->log = log;
  this->statusBar = statusBar;
  this->pFrameOffset = pFrameOffset;
  this->coresEnabled = coresEnabled;
  this->socketMode = socketMode;
  this->displayApp = displayApp;
  sifInUse = false;
  fpsValue = 0;
  previewActive = false;
  requestSwitch = false;
  broadcastingEnabled = false;
  showDisplayRetry = NULL;
  showPreviewRetry = NULL;
  activeCoreIndex = 0;
  CONNECT_MESSAGE_HANDLER;

  // Find out where the display buffers are located and fill core selection tab bar with life
  int index=0;
  coreSelection = new QTabBar();
  coreSelection->setFixedHeight(30);
  coreSelection->setShape(QTabBar::RoundedSouth);
  coreSelection->setFocusPolicy(Qt::NoFocus);
  connect(coreSelection, SIGNAL(currentChanged(const int &)), this, SLOT(selectCore( const int &)));
  int timeout = 10;
  uInt64 frameOffset = 0;
  while ((frameOffset==0) && (timeout!=0)) {
    frameOffset = sccAccess->readFlit(0x00, PERIW,  pFrameOffset);
    if (timeout < 10) sleep(1);
    timeout--;
  }
  if (!timeout) {
    printError("Unable to find out framebuffer offset. Did you boot a framebuffer enabled Linux image? Aborting...");
    exit(-1);
  }
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (coresEnabled->getEnabled(loop_x, loop_y, loop_core)) {
          int lutEntry = sccAccess->readFlit(TID(loop_x, loop_y), CRB,  0x800+0x800*loop_core);
          coreHostName[index] = sccAccess->getSccIp(PID(loop_x, loop_y, loop_core));
          if (socketMode==HID_TCP) {
            hidTcpSocket[index] = new QTcpSocket(this);
            hidTcpSocket[index]->connectToHost(coreHostName[index],5678);
            if (!hidTcpSocket[index]->waitForConnected(3000)) {
              printError("Unable to connect to keyboard server of ", coreHostName[index], " via TCP (try --udp option). Keyboard entries won't be forwarded... :-(");
              hidTcpSocket[index] = NULL;
            }
          } else {
            hidUdpSocket[index] = new QUdpSocket(this);
            hidUdpSocket[index]->connectToHost(coreHostName[index],5678);
            if (!hidUdpSocket[index]->waitForConnected(3000)) {
              printError("Unable to connect to keyboard server of ", coreHostName[index], " via UDP (try --tcp option). Keyboard entries won't be forwarded... :-(");
              hidUdpSocket[index] = NULL;
            }
          }
          frameAddress[index] = ((((uInt64)lutEntry&0x3ff))<<24) + frameOffset + FB_OFFSET; // Offset of one page...
          frameRoute[index] = (lutEntry>>13)&0x0ff;
          frameDestId[index] = (lutEntry>>10)&0x07;
          coreSelection->insertTab(index,(QString)"rck"+messageHandler::toString(PID(loop_x, loop_y, loop_core),10,2));
          pid[index++] = PID(loop_x, loop_y, loop_core);
        }
      }
    }
  }

  // Read picture format from first framebuffer... For simplification we defined that all framebuffers need to have the same size and format!
  uInt64 tmp = sccAccess->readFlit(frameRoute[0], frameDestId[0], frameAddress[0]-FB_OFFSET);
  frameWidth = (uInt32)tmp;
  frameHeight = (uInt32)(tmp>>32);
  frameColorMode = (uInt32)sccAccess->readFlit(frameRoute[0], frameDestId[0], frameAddress[0]-FB_OFFSET+0x08);

  // Create the widget
  viewerWidget = this;
  imageLabel = new sccVirtualDisplay(coreSelection, this);
  imageLabel->setGeometry(0,0,frameWidth,frameHeight);
  imagePreview = NULL;
  scrollArea = new QScrollArea;
  scrollArea->setBackgroundRole(QPalette::Dark);
  scrollArea->setWidget(imageLabel);
  scrollArea->setFocusPolicy(Qt::NoFocus);
  viewerLayout = new QVBoxLayout;
  viewerLayout->addWidget(scrollArea);
  viewerLayout->addWidget(coreSelection);
  viewerWidget->setLayout(viewerLayout);
  viewerWidget->setFocusPolicy(Qt::NoFocus);

  // Find out what size and colormode we want to use...
  if (frameColorMode == 8) {
    // Prepare image including colortable...
    colormap += qRgb(0, 0, 0); // Black
    colormap += qRgb(0x0000, 0x0000, 0xAAAA);
    colormap += qRgb(0x0000, 0xAAAA, 0x0000);
    colormap += qRgb(0x0000, 0xAAAA, 0xAAAA);
    colormap += qRgb(0xAAAA, 0x0000, 0x0000);
    colormap += qRgb(0xAAAA, 0x0000, 0xAAAA);
    colormap += qRgb(0xAAAA, 0x5555, 0x0000);
    colormap += qRgb(0xAAAA, 0xAAAA, 0xAAAA);
    colormap += qRgb(0x5555, 0x5555, 0x5555);
    colormap += qRgb(0x5555, 0x5555, 0xFFFF);
    colormap += qRgb(0x5555, 0xFFFF, 0x5555);
    colormap += qRgb(0x5555, 0xFFFF, 0xFFFF);
    colormap += qRgb(0xFFFF, 0x5555, 0x5555);
    colormap += qRgb(0xFFFF, 0x5555, 0xFFFF);
    colormap += qRgb(0xFFFF, 0xFFFF, 0x5555);
    colormap += qRgb(0xFFFF, 0xFFFF, 0xFFFF);
    for (int i = 16; i < 253; i++) {
      colormap += qRgb( (((i>>5)&0x07)*255/7),(((i>>2)&0x07)*255/7),(((i>>0)&0x03)*255/3));
    }
    colormap += qRgb(0xFFFF, 0xFFFF, 0xFFFF);
  } else if ((frameColorMode != 16)&&(frameColorMode != 24)&&(frameColorMode != 32)) {
    printError("The detected color depth (",QString::number(frameColorMode),") isn't valid. Please check SCC Linux display driver...");
    exit(255);
  }

  // Prepare image
  if (frameColorMode == 8) {
    // Prepare image including colortable...
    image = new QImage(QSize(frameWidth, frameHeight), QImage::Format_Indexed8);
    image->setColorTable(colormap);
  } else if (frameColorMode == 16) {
    image = new QImage(QSize(frameWidth, frameHeight), QImage::Format_RGB16);
  } else if (frameColorMode == 24) {
    image = new QImage(QSize(frameWidth, frameHeight), QImage::Format_RGB32);
  } else if (frameColorMode == 32) {
    image = new QImage(QSize(frameWidth, frameHeight), QImage::Format_ARGB32);
  }

  // Start w/ core 0x00 - The method also enables sound forwarding of that specific core...
  selectCore(0);

  // Add help test to status monitor
  statusBar->setFixedHeight(30);
  statusBar->showMessage("<Right CTRL>+<H> for help");
  statusBar->setFocusPolicy(Qt::NoFocus);

  // Create broadcasting status monitor
  broadCastingStatus = new QLabel();
  broadCastingStatus->setFocusPolicy(Qt::NoFocus);
  broadCastingStatus->setTextFormat(Qt::RichText);
  statusBar->addPermanentWidget(broadCastingStatus);
  setBroadcasting(false);

  // Create picture status monitor
  picFormat = new QLabel(QString::number(frameWidth)+"x"+QString::number(frameHeight)+" - "+QString::number(frameColorMode)+" bit");
  picFormat->setFocusPolicy(Qt::NoFocus);
  statusBar->addPermanentWidget(picFormat);

  // Create fps monitor
  gettimeofday(&fpsTimeval, NULL);
  fpsTimeval.tv_sec += 1;
  fpsStatus = new QLabel();
  setFramerate(0);
  statusBar->addPermanentWidget(fpsStatus);

  // Show widget...
  viewerWidget->show();
  viewerWidget->raise();
  viewerWidget->activateWindow();
  int height = frameHeight+89;
  int width = frameWidth+23;
  displayApp->setGeometry((int)(QApplication::desktop()->width() - width) / 2, (int)(QApplication::desktop()->height() - height) / 2, width, height);
  displayApp->setMaximumSize(width, height);

  // Create and start timer...
  pollTimer = new QTimer(this);
  pollTimer->setSingleShot(true);
  connect(pollTimer, SIGNAL(timeout()), this, SLOT(poll()));
  if (pollTimer) pollTimer->start(100); // Allow window to open before we start polling...
}

sccDisplayWidget::~sccDisplayWidget() {
  stopPolling();
  delete viewerLayout;
  delete imageLabel;
  delete imagePreview;
  delete viewerWidget;
}

void sccDisplayWidget::setHorizontalScrollValue(uInt32 value) {
  scrollArea->horizontalScrollBar()->setValue(value);
}

uInt32 sccDisplayWidget::getHorizontalScrollValue() {
  return scrollArea->horizontalScrollBar()->value();
}

void sccDisplayWidget::setVerticalScrollValue(uInt32 value) {
  scrollArea->verticalScrollBar()->setValue(value);
}

uInt32 sccDisplayWidget::getVerticalScrollValue() {
  return scrollArea->verticalScrollBar()->value();
}

void sccDisplayWidget::blockSif(bool block) {
  sifInUse = block;
}

bool sccDisplayWidget::isSifBlocked() {
  return sifInUse;
}

void sccDisplayWidget::poll() {
  // Don't start with polling before current block transfers are done...
  if (pollServerRunning) {
    return;
  }
  if (isSifBlocked() || requestSwitch) {
    if (pollTimer) pollTimer->start(POLLING_INT);
    return;
  }
  pollServerRunning = true;

  char tmp[ frameWidth*4 ];
  int myIndex;
#if RANDOM_PREVIEW == 1
    // Initialize random test-pattern...
    struct timeval current;
    QString gibtsSchon;
    gettimeofday(&current, NULL);
    srand(current.tv_sec);
#endif
  for (int loop = 0; loop < coreSelection->count(); loop++) {
#if RANDOM_PREVIEW == 1
    myIndex = rand()%48;
    while (gibtsSchon.contains(messageHandler::toString(myIndex,10,2)+",")) {
      myIndex = rand()%48;
    }
    gibtsSchon += messageHandler::toString(myIndex,10,2)+",";
#else
    myIndex = loop;
#endif
    if (!previewActive) {
      // w/o preview, this loop will only be executed once...
      myIndex = activeCoreIndex;
    }

    // Read frame...
    blockSif(true);
    for (uInt32 line = 0; line < frameHeight; line++) {
      scanLine = reinterpret_cast<uchar *>(image->scanLine(line));
      if (frameColorMode != 32) {
        sccAccess->read32(frameRoute[myIndex], frameDestId[myIndex], frameAddress[myIndex]+line*frameWidth*frameColorMode/8, (char*)scanLine, frameWidth*frameColorMode/8, SIF_RC_PORT, false);
      } else {
        sccAccess->read32(frameRoute[myIndex], frameDestId[myIndex], frameAddress[myIndex]+line*frameWidth*frameColorMode/8, (char*)tmp, frameWidth*frameColorMode/8, SIF_RC_PORT, false);
        for (unsigned int col=0; col<frameWidth;col++) {
          *scanLine++ = tmp[ col*4+1];
          *scanLine++ = tmp[ col*4+2];
          *scanLine++ = tmp[ col*4+3];
          *scanLine++ = 0xFF-tmp[ col*4+0]; // Alpha blending
        }
      }
    }
    blockSif(false);
    // Display frame...
    if (previewActive) {
      imagePreview->setPixmap(image, pid[myIndex]);
    } else {
      // Update imageLabel
      if (imageLabel->isFullScreen()) {
        imageLabel->setPixmap(QPixmap::fromImage(*image).scaled(QApplication::desktop()->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
      } else {
        imageLabel->setPixmap(QPixmap::fromImage(*image));
      }
      // w/o preview, this loop will only be executed once...
      myIndex = coreSelection->count();
    }

    if (!previewActive) {
      // Find out fps
      gettimeofday(&currentTimeval, NULL);
      if (((currentTimeval.tv_sec == fpsTimeval.tv_sec) && (currentTimeval.tv_usec >= fpsTimeval.tv_usec)) || (currentTimeval.tv_sec > fpsTimeval.tv_sec)) {
        setFramerate(fpsValue);
        // Now poll performance values, if requested before re-starting fps timer...
        displayApp->pollPerfmeterWidget();
        // Restart fps timer
        gettimeofday(&fpsTimeval, NULL);
        fpsTimeval.tv_sec += 1;
        fpsValue = 0;
      } else {
        fpsValue++;
      }

    } else {
      displayApp->pollPerfmeterWidget();
    }

    if (requestSwitch) myIndex = coreSelection->count();
  }

  if (!previewActive && (activeCoreIndex >= 0) && hidTcpSocket[activeCoreIndex] && hidTcpSocket[activeCoreIndex]->bytesAvailable()) {
    int in_len = hidTcpSocket[activeCoreIndex]->bytesAvailable();
    char rxbuf[in_len];
    QClipboard *clip = QApplication::clipboard();

    //printfInfo("RX data available! len %d", in_len);
    hidTcpSocket[activeCoreIndex]->read(rxbuf, in_len);
    //printfInfo("RX data: %s", rxbuf + 1);
    for (int t = 0; t < in_len - 1; t++) {
      //printfInfo("%2.2x ", rxbuf[t + 1]);
      if (rxbuf[t + 1] == 0x0d) {
        rxbuf[t + 1] = '\n';
      }
    }

    clip->setText(rxbuf + 1);
  }

  viewerWidget->update();
  qApp->processEvents(); // allow GUI to update

  pollServerRunning = false;
#if RANDOM_PREVIEW == 1
  if (pollTimer) pollTimer->start(POLLING_INT);
# else
  if (pollTimer) pollTimer->start(previewActive?1500:POLLING_INT);
#endif

}

void sccDisplayWidget::stopPolling() {
  if (!pollTimer) return; // We're done... ;-)

  pollTimer->stop();
  while (pollServerRunning) {
    qApp->processEvents(); // allow GUI and SCEMI loop to update
  }
  blockSif(false);
  disconnect(pollTimer, 0, this, 0);
  delete pollTimer;
  pollTimer = NULL;
}

void sccDisplayWidget::selectCore(int index) {
  char txData[5];
  // Enable broadcasting for now...
  bool wasBroadcastingEnabled = broadcastingEnabled;
  broadcastingEnabled = false;
  // Disable Sound of current core if a new core was selected...
  if (activeCoreIndex != index) {
    txData[0] = EVENT_SOUND_ENABLE;
    txData[1] = 0;
    writeSocket((char*)&txData, 5);
  }
  // Activate new core...
  activeCoreIndex = index;
  // Enable sound of "new" core...
  txData[0] = EVENT_SOUND_ENABLE;
  txData[1] = 1;
  writeSocket((char*)&txData, 5);
  // Restore broadcasting setting...
  broadcastingEnabled=wasBroadcastingEnabled;
}

void sccDisplayWidget::selectCore(int x, int y, int core) {
  if (coreAvailable(x,y,core)) {
    selectCore(coreIndex(PID(x,y,core)));
  }
}

bool sccDisplayWidget::coreAvailable(int x, int y, int core) {
  for (int loop = 0; loop < coreSelection->count(); loop++) {
    if (pid[loop] == PID(x,y,core)) {
      return true;
    }
  }
  return false;
}

int sccDisplayWidget::coreIndex(int pid) {
  for (int loop = 0; loop < coreSelection->count(); loop++) {
    if (this->pid[loop] == pid) {
      return loop;
    }
  }
  return -1;
}

int sccDisplayWidget::currentCorePid() {
  return pid[activeCoreIndex];
}

int sccDisplayWidget::nextCorePid(int currentPid) {
  int index;
  index = coreIndex(currentPid) + 1;
  if (index >= coreSelection->count()) {
    index = 0;
  }
  return pid[index];
}

int sccDisplayWidget::prevCorePid(int currentPid) {
  int index;
  index = coreIndex(currentPid) - 1;
  if (index < 0) {
    index = coreSelection->count()-1;
  }
  return pid[index];
}

void sccDisplayWidget::writeSocket(char* txData, int numBytes) {
  if (!broadcastingEnabled) {
    if (socketMode==HID_TCP) {
      if (hidTcpSocket[activeCoreIndex]) {
        hidTcpSocket[activeCoreIndex]->write(txData, numBytes);
      }
    } else {
      if (hidUdpSocket[activeCoreIndex]) {
        hidUdpSocket[activeCoreIndex]->write(txData, numBytes);
      }
    }
  } else {
    // Connect to each channel and write data...
    for (int loop=0; loop < coreSelection->count(); loop++) {
      if (socketMode==HID_TCP) {
        if (hidTcpSocket[loop]) {
          hidTcpSocket[loop]->write(txData, numBytes);
        }
      } else {
        if (hidUdpSocket[loop]) {
          hidUdpSocket[loop]->write(txData, numBytes);
        }
      }
    }
  }
}

void sccDisplayWidget::setBroadcasting(bool enabled) {
  if (coreSelection->count()==1) {
    broadCastingStatus->setText("<font color=\"black\">Broadcasting: n.a.</font>");
    return;
  }
  broadcastingEnabled = enabled;
  if (enabled) {
    broadCastingStatus->setText("<font color=\"green\">Broadcasting: on</font>");
  } else {
    broadCastingStatus->setText("<font color=\"black\">Broadcasting: off</font>");
  }
}

bool sccDisplayWidget::getBroadcasting() {
  return broadcastingEnabled;
}

void sccDisplayWidget::setFramerate(int framerate) {
  fpsStatus->setText("<font color=\"black\">"+(QString)((framerate)?messageHandler::toString(framerate,10,3):"---")+"fps</font>");
}

bool showDisplayIsFullscreen;
int showDisplayX;
int showDisplayY;
int showDisplayZ;
void sccDisplayWidget::showDisplay(bool isFullscreen, int x, int y, int core) {
  requestSwitch = true;
  if (pollServerRunning) {
    if (!showDisplayRetry) {
      showDisplayRetry = new QTimer(this);
      showDisplayRetry->setSingleShot(true);
      connect(showDisplayRetry, SIGNAL(timeout()), this, SLOT(showDisplay()));
    }
    showDisplayIsFullscreen = isFullscreen;
    showDisplayX = x;
    showDisplayY = y;
    showDisplayZ = core;
    if (showDisplayRetry) showDisplayRetry->start(POLLING_INT);
    return;
  }
  imageLabel = new sccVirtualDisplay(coreSelection, this);
  imageLabel->setGeometry(0,0,frameWidth,frameHeight);
  scrollArea->setWidget(imageLabel);
  imageLabel->setFocus();
  picFormat->setText(QString::number(frameWidth)+"x"+QString::number(frameHeight)+" - "+QString::number(frameColorMode)+" bit");
  if (isFullscreen && showPreviewWasFullscreen) imageLabel->toggleFullscreen();
  coreSelection->setEnabled(true);
  bool noChange = (PID(x,y,core) == pid[activeCoreIndex]);
  coreSelection->setCurrentIndex(coreIndex(PID(x,y,core)));
  if (noChange) {
    bool wasBroadcastingEnabled = broadcastingEnabled;
    broadcastingEnabled = false;
    char txData[5];
    txData[0] = EVENT_SOUND_ENABLE;
    txData[1] = 1;
    writeSocket((char*)&txData, 5);
    broadcastingEnabled = wasBroadcastingEnabled;
  }
  previewActive = false;
  pollTimer->stop();
  requestSwitch = false;
  gettimeofday(&fpsTimeval, NULL);
  fpsTimeval.tv_sec += 1;
  setFramerate(0);
  fpsValue=0;
  setBroadcasting(broadcastingEnabled);
  pollTimer->start(0);
}

// Retry-Slot for sccDisplayWidget::showDisplay(bool isFullscreen, int x, int y, int core)
void sccDisplayWidget::showDisplay() {
  showDisplay(showDisplayIsFullscreen, showDisplayX, showDisplayY, showDisplayZ);
}

void sccDisplayWidget::showPreview(bool isFullscreen, bool wasFullscreen) {
  requestSwitch = true;
  showPreviewWasFullscreen = wasFullscreen;
  if (pollServerRunning) {
    if (!showPreviewRetry) {
      showPreviewRetry = new QTimer(this);
      showPreviewRetry->setSingleShot(true);
      connect(showPreviewRetry, SIGNAL(timeout()), this, SLOT(showPreview()));
    }
    showPreviewIsFullscreen = isFullscreen;
    if (showPreviewRetry) showPreviewRetry->start(POLLING_INT);
    return;
  }
  // Disable Sound of current core...
  bool wasBroadcastingEnabled = broadcastingEnabled;
  broadcastingEnabled = false;
  char txData[5];
  txData[0] = EVENT_SOUND_ENABLE;
  txData[1] = 0;
  writeSocket((char*)&txData, 5);
  broadcastingEnabled = wasBroadcastingEnabled;
  imagePreview = new sccTilePreview(this);
  imagePreview->setGeometry(0,0,frameWidth,frameHeight);
  imagePreview->refresh();
  scrollArea->setWidget(imagePreview);
  imagePreview->setFocus();
  coreSelection->setEnabled(false);
  broadCastingStatus->setText("<font color=\"black\">Tile</font>");
  fpsStatus->setText("...");
  picFormat->setText("preview");
  if (isFullscreen) imagePreview->toggleFullscreen();
  imagePreview->highlightTile(pid[activeCoreIndex]);
  previewActive = true;
  requestSwitch = false;
}

// Retry-Slot for sccDisplayWidget::showPreview(bool isFullscreen)
void sccDisplayWidget::showPreview() {
  showPreview(showPreviewIsFullscreen, showPreviewWasFullscreen);
}

void sccDisplayWidget::setWasFullscreen(bool wasFullscreen) {
  showPreviewWasFullscreen = wasFullscreen;
}

void sccDisplayWidget::togglePerfmeter() {
  displayApp->togglePerfmeter();
}

void sccDisplayWidget::close() {
  // Disable Sound of current core...
  bool wasBroadcastingEnabled = broadcastingEnabled;
  broadcastingEnabled = false;
  char txData[5];
  txData[0] = EVENT_SOUND_ENABLE;
  txData[1] = 0;
  writeSocket((char*)&txData, 5);
  broadcastingEnabled = wasBroadcastingEnabled;
  // Exit gracefully
  exit(0);
}

