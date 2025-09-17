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

#ifndef sccDisplayWidget_H
#define sccDisplayWidget_H
#include <QPixmap>
#include <QWidget>
#include <QVector>
#include <QColor>
#include <sys/time.h>
#include <QtGui>
#include <QUdpSocket>
#include <QTcpSocket>
#include "coreMask.h"
#include "sccExtApi.h"
#include "sccDisplay.h"
#include "sccVirtualDisplay.h"
#include "sccTilePreview.h"

// Time between read of two frames...
#define POLLING_INT 0
#define FB_OFFSET 4096
#define RANDOM_PREVIEW 0

// Pre-declaration
class sccDisplay;
class sccVirtualDisplay;
class sccTilePreview;

class sccDisplayWidget : public QWidget {
  Q_OBJECT

public:
  sccDisplayWidget(sccExtApi *sccAccess, messageHandler *log, QStatusBar* statusBar, uInt64 pFrameOffset, coreMask* coresEnabled, uInt32 socketMode, sccDisplay* displayApp);
  ~sccDisplayWidget();

public:
  void setHorizontalScrollValue(uInt32 value);
  uInt32 getHorizontalScrollValue();
  void setVerticalScrollValue(uInt32 value);
  uInt32 getVerticalScrollValue();
  void writeSocket(char* txData, int numBytes);
  void setBroadcasting(bool enabled);
  bool getBroadcasting();
  void setFramerate(int framerate);
  void showDisplay(bool isFullscreen, int x=-1, int y=-1, int core=-1);
  void showPreview(bool isFullscreen, bool wasFullscreen);
  void setWasFullscreen(bool wasFullscreen);
  void togglePerfmeter();
  void close();

private slots:
  void showDisplay();
  void showPreview();

private:
  void stopPolling();

private:
  sccExtApi* sccAccess;
  sccDisplay* displayApp;
  messageHandler *log;
  QStatusBar* statusBar;
  QLabel* fpsStatus;
  QLabel* picFormat;
  bool broadcastingEnabled;
  bool previewActive;
  QLabel* broadCastingStatus;
  coreMask* coresEnabled;
  QTabBar* coreSelection;
  uInt64 pFrameOffset;
  uInt32 frameWidth;
  uInt32 frameHeight;
  uInt32 frameColorMode;
  uInt32 fpsValue;
  struct timeval fpsTimeval;
  struct timeval currentTimeval;
  QString widgettitle;
  QWidget* viewerWidget;
  sccVirtualDisplay* imageLabel;
  sccTilePreview* imagePreview;
  QVBoxLayout *viewerLayout;
  QScrollArea *scrollArea;
  bool pollServerRunning;
  QTimer* pollTimer;
  QImage *image;
  QVector <QRgb> colormap;
  QVector <QRgb> greymap;
  bool abort;
  bool aborted;
  QString coreHostName[NUM_ROWS*NUM_COLS*NUM_CORES];
  uInt32 socketMode;
  QTcpSocket* hidTcpSocket[NUM_ROWS*NUM_COLS*NUM_CORES];
  QUdpSocket* hidUdpSocket[NUM_ROWS*NUM_COLS*NUM_CORES];
  uInt64 frameAddress[NUM_ROWS*NUM_COLS*NUM_CORES];
  uInt32 frameRoute[NUM_ROWS*NUM_COLS*NUM_CORES];
  uInt32 frameDestId[NUM_ROWS*NUM_COLS*NUM_CORES];
  int pid[NUM_ROWS*NUM_COLS*NUM_CORES];
  int activeCoreIndex;
  uchar *scanLine;
  bool showDynamicContent;
  bool sifInUse;
  bool requestSwitch;
  QTimer* showDisplayRetry;
  bool showDisplayIsFullscreen;
  int showDisplayX;
  int showDisplayY;
  int showDisplayZ;
  bool showPreviewIsFullscreen;
  bool showPreviewWasFullscreen;
  QTimer* showPreviewRetry;

private slots:
  void poll();
  void selectCore(int index);

public slots:
  void selectCore(int x, int y, int core);
  bool coreAvailable(int x, int y, int core);
  int coreIndex(int pid);
  int currentCorePid();
  int nextCorePid(int currentPid);
  int prevCorePid(int currentPid);
  void blockSif(bool block);
  bool isSifBlocked();

protected:
 char formatMsg[8192];
signals:
 void logMessageSignal(LOGLEVEL level, const char *logmsg);
 void logMessageSignal(LOGLEVEL level, const QString a, const QString b = "", const QString c = "", const QString d = "", const QString e = "", const QString f = "", const QString g = "", const QString h = "", const QString i = "", const QString j = "", const QString k = "", const QString l = "", const QString m = "", const QString n = "", const QString o = "", const QString p = "", const QString q = "", const QString r = "", const QString s = "", const QString t = "", const QString u = "" );
};

// --------------------------------------------------------------------------
#endif
// !!! do not add anything behind the #endif line above !!!
