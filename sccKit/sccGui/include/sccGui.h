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

#ifndef SCCGUI_H
#define SCCGUI_H

#include <QApplication>
#include <QtGui>
#include <QMessageBox>
#include <QSplashScreen>
#include <QSettings>
#include <QHostInfo>
#include "ui_sccGui.h"
#include "sccExtApi.h"
#include "flitWidget.h"
#include "packetWidget.h"
#include "readMemWidget.h"
#include "perfmeterWidget.h"
#include "coreMaskDialog.h"
#include "sccKitVersion.h"

// Define initial windowsize:
#define INITIAL_WIDTH 900
#define INITIAL_HEIGHT 650

class sccGui : public QMainWindow
{
  Q_OBJECT

public:
  sccGui(QWidget *parent = 0);
  ~sccGui();
  void closeEvent(QCloseEvent *event);

// =============================================================================================
// Protection members & methods...
// =============================================================================================
protected:
  QSemaphore protectApi;
  bool       releaseSemaphores;
  QString    linuxImage;
  QTimer bootRetryTimer;
  coreMaskDialog   linuxMask;
  coreMaskDialog   clearMPBMask;
  coreMaskDialog   changeL2Mask;
  coreMaskDialog   changeResetMask;
  QString    genericObject;
  QString    genericLut;
  bool initDDR3;

public:
  void bootLinux();
  bool acquireApi();
  bool tryAcquireApi();
  bool availableApi();
  void releaseApi();

  // Menu slots...
public slots:
  // Most important dialog: ;-)
  void slotAboutDialog();
  // Settings menu actions
  void slotSplashscreenEnabled(bool checked);
  void slotSaveGeometry();
  void slotRestoreGeometry();
  void slotClearLogWindow();
  void slotSetPacketTraceFile();
  void slotSetPacketTraceMode();
  void slotEnableSoftwareRamAndUart(bool checked);
  void slotSoftUartRequest(uInt32 * message);
  void slotLinuxEnableL2(bool checked);
  void slotLinuxClearMPBs(bool checked);
  void slotLinuxSelectImage();
  void slotLinuxRestoreImage();
  void slotLinuxBootAll(bool checked);
  void slotPopshmSettings();
  void slotCmdLineArgs();
  void slotWischer();
  // slotBootLinux(): This Routine runs an MMIO based Linux build on selected cores (dialog). No
  // checks are performed. The Linux images are distributed on all RCK memory Controllers!
  void slotBootLinux();
  void slotPreloadObject();
  void slotPreloadLut();
  void slotClearMPBs();
  void slotChangeL2Enables();
  void slotChangeResets();
  void slotTestMemory();
  void slotSetLinuxGeometry();
  void slotGetBoardStatus();
  void slotResetPlatform();
  void slotCustomBmcCmd();
  void slotInitializePlatform(bool checked);
  void slotExecuteRccFile();
  void slotTrainSystemInterface();

public slots:
  // Widget slots
  void slotPacketWidget();
  void slotFlitWidget();
  void slotReadMemWidget();
  void slotStartkScreen();
  void slotClosekScreen(int exitCode);
  void slotPerfmeterWidget();
  void pollPerfmeterWidget();

private:
  // Widget methods
  void startPacketWidget();
  void stopPacketWidget();
  void startFlitWidget();
  void stopFlitWidget();
  void startReadMemWidget();
  void stopReadMemWidget();
  void startPerfmeterWidget(int mesh_x, int mesh_y, int tile_cores);
  void stopPerfmeterWidget();
  void startPerfmeterPolling(bool openWidget=false);
  void stopPerfmeterPolling(bool closeWidget=false);
  void updatePerfmeterWidget(int x, int y, int core, int percent, double power);

public:
  // Main window including log text pane...
  QMainWindow *mW;
  Ui::sccGuiClass ui;
  messageHandler *log;
  // Widgets
  flitWidget *FlitWidget;
  packetWidget *PacketWidget;
  readMemWidget *ReadMemWidget;
  perfmeterWidget *PerfmeterWidget;
  QTimer *perfPollingTimer;
  QProcess* konsoleProcess;
  char perfStatus[64];
  int perfRoute, perfDestid, perfSIFPort;
  uInt64 perfApertureStart;
  sccProgressBar InitializePlatformPd;
  bool userAbort;
  QTabWidget* uartWidget;
  QTextEdit* uartTrace[NUM_ROWS*NUM_COLS*NUM_CORES];

public:
  void uiEnable(bool menubar, bool fileToolBar, bool bmcToolBar, bool toolsToolBar, bool widgetToolBar);
  void enableSoftwareRamAndUart(bool enable);

protected:
  char formatMsg[8192];
signals:
  void logMessageSignal(LOGLEVEL level, const char *logmsg);
  void logMessageSignal(LOGLEVEL level, const QString a, const QString b = "", const QString c = "", const QString d = "", const QString e = "", const QString f = "", const QString g = "", const QString h = "", const QString i = "", const QString j = "", const QString k = "", const QString l = "", const QString m = "", const QString n = "", const QString o = "", const QString p = "", const QString q = "", const QString r = "", const QString s = "", const QString t = "", const QString u = "" );

// =============================================================================================
// Soft-RAM emulation
// =============================================================================================
private:
  /*! Provides array to simulate RAM from software, softram is used for
   *  debug purpose of Application Memmory only. */
  uInt64 *cSoftram;

  /*! Softram access mask, initialized by softramInit(), according to softram size
   *  (cSoftramMask = (size - 1) >> 3) \n
   *  (initial value = 0) */
  uInt32 cSoftramMask;

  /*! Mallocs softram array according to size, if cSoftramInit == false, if
   *  cSoftramInit == true softram array will be freed.
   *  \param  size Size of softram array to be emulated, depends on application to be debugged
   *  Returns status of softram initialization:
   *  \retval true If softram successfully allocated
   *  \retval false If softram is successfully freed, or if malloc failed  */
  void softramInit(uInt32 size);

  /* Free memory... */
  void softramRelease();

  /*! Write access function for softram emulation
   *  \param  *mem mem32Byte typedef with address and 32 byte data array */
  void set32ByteSR(mem32Byte *mem);

  /*! Read access function for softram emulation
   *  \param  *mem mem32Byte typedef with address and 32 byte data array */
  void get32ByteSR(mem32Byte *mem);

private slots:
  void slotSoftramRequest(uInt32* message);

private:
  // Helper functions
  QFont getAppFont(uInt32 type, uInt32 size = 12);
  QString getSccKitPath();

private:
  // Config values...
  QSettings *settings;

private:
  // SCC API object
  sccExtApi *sccAccess;
};

#endif // SCCGUI_H
