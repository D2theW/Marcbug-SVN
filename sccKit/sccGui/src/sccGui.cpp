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

#include "sccGui.h"

// The LOG_DEBUG enables extensive debug messages if set to "1" (should be 0 for releases)
#define LOG_DEBUG 0
#define SOFTRAM_DEBUG 0

// The USE_DRIVER define can be set to "0" to debug the GUI w/o the need to load a driver (should be 1 for releases)...
#define USE_DRIVER 1

sccGui::sccGui(QWidget *parent) : QMainWindow(parent) {
  // Initialization
  mW = this;
  linuxMask.disableAll();
  linuxMask.setZeroSelectionPolicy(false);
  clearMPBMask.setZeroSelectionPolicy(false);
  setWindowIcon(QIcon(":/resources/guiPixmaps/sccGuiIcon.png"));
  konsoleProcess = NULL;
  cSoftram = NULL;
  uartWidget = NULL;

  // Load settings
  settings = new QSettings("sccKit", "sccGui");

  // Create UI and message handler
  QFont font;
  ui.setupUi(this);
  ui.textEdit->setFont(getAppFont(1));
  ui.actionSplashscreenEnabled->setChecked(settings->value("GUI/Splashscreen", true).toBool());
  log = new messageHandler(false, "sccGui.log", ui.textEdit, ui.statusBar);
  #if LOG_DEBUG == 1
    log->setLogLevel(log_debug);
  #endif
  CONNECT_MESSAGE_HANDLER;

  // Define toolbars...
  ui.fileToolBar->addAction(ui.actionClearLogWindow);
  ui.bmcToolBar->addAction(ui.actionInitializePlatform);
  ui.toolsToolBar->addAction(ui.actionWischer);
  ui.toolsToolBar->addAction(ui.actionBootLinux);
  ui.toolsToolBar->addAction(ui.actionTestMemory);
  ui.widgetToolBar->addAction(ui.actionFlitWidget);
  ui.widgetToolBar->addAction(ui.actionPacketWidget);
  ui.widgetToolBar->addAction(ui.actionReadMemWidget);
  ui.widgetToolBar->addAction(ui.actionPerfmeterWidget);
  ui.widgetToolBar->addAction(ui.actionStartkScreen);

  // Invoke sccApi and prepare widgets as well as semaphores...
  sccAccess = new sccExtApi(log);
  sccAccess->openBMCConnection();

  // Show splash screen, if configured...
  QSplashScreen splash(QPixmap(":/resources/guiPixmaps/sccGuiSplashscreen.png"));
  if (ui.actionSplashscreenEnabled->isChecked()) {
    splash.showMessage("Connecting PCIe device driver...", Qt::AlignCenter, Qt::blue);
    splash.show();
    splash.raise();
    splash.activateWindow();
    qApp->processEvents();
  }

  // Open driver...
  bool initFailed = false;
#if USE_DRIVER == 1
  if (!sccAccess->connectSysIF()) {
    // As we won't open the GUI, print error message to shell instead...
    delete log;
    log = new messageHandler(true);
    CONNECT_MESSAGE_HANDLER;
    initFailed = true;
  } else {
#ifndef BMC_IS_HOST
    printInfo("Successfully connected to PCIe driver...");
#else
    printInfo("Successfully connected to System Interface FPGA BMC-Interface...");
#endif
  }
#else
  printInfo("Didn't load PCIe driver! This is a binary for debugging purposes...");
#endif

  if (ui.actionSplashscreenEnabled->isChecked()) {
    sleep(1);
    if (!initFailed) {
#ifndef BMC_IS_HOST
      splash.showMessage("Successfully connected to PCIe driver!", Qt::AlignCenter, Qt::green);
#else
      splash.showMessage("Successfully connected to System Interface FPGA BMC-Interface...", Qt::AlignCenter, Qt::green);
#endif
      qApp->processEvents();
      sleep(2);
    } else {
      splash.showMessage("Error: Failed to connect to PCIe driver! Aborting...", Qt::AlignCenter, Qt::red);
      qApp->processEvents();
      sleep(5);
    }
    splash.close();
  } else if (initFailed) {
    QMessageBox::critical(this, "Unable to connect driver", "Unable to connect to PCIe driver...\nMake sure that PCIe driver is installed and\nnot in use by someone else... Aborting!", QMessageBox::Cancel);
  }

  // Update reset mask...
  if (!initFailed) {
    bool tmp;
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          if (!sccAccess->getReset(TID(loop_x, loop_y), loop_core, &tmp)) {
            printError("Unable to read reset state of SCC... Aborting reset operation...");
            initFailed = true;
            loop_x = NUM_COLS;
            loop_y = NUM_ROWS;
            loop_core = NUM_CORES;
          }
          changeResetMask.setEnabled(loop_x, loop_y, loop_core, tmp);
        }
      }
    }
  }

  // Connect slots...
  connect(ui.actionExit, SIGNAL(triggered()), this, SLOT(close()));
  connect(ui.actionAbout, SIGNAL(triggered()), this, SLOT(slotAboutDialog()));
  connect(ui.actionClearLogWindow, SIGNAL(triggered()), this, SLOT(slotClearLogWindow()));
  connect(ui.actionSplashscreenEnabled, SIGNAL(triggered(bool)), this, SLOT(slotSplashscreenEnabled(bool)));
  connect(ui.actionSaveGeometry, SIGNAL(triggered()), this, SLOT(slotSaveGeometry()));
  connect(ui.actionRestoreGeometry, SIGNAL(triggered()), this, SLOT(slotRestoreGeometry()));
  connect(ui.actionFlitWidget, SIGNAL(triggered()), this, SLOT(slotFlitWidget()));
  connect(ui.actionPacketWidget, SIGNAL(triggered()), this, SLOT(slotPacketWidget()));
  connect(ui.actionReadMemWidget, SIGNAL(triggered()), this, SLOT(slotReadMemWidget()));
  connect(ui.actionStartkScreen, SIGNAL(triggered()), this, SLOT(slotStartkScreen()));
  connect(ui.actionPerfmeterWidget, SIGNAL(triggered()), this, SLOT(slotPerfmeterWidget()));
  connect(ui.actionSetPacketTraceFile, SIGNAL(triggered()), this, SLOT(slotSetPacketTraceFile()));
  connect(ui.actionSetPacketTraceMode, SIGNAL(triggered()), this, SLOT(slotSetPacketTraceMode()));
  connect(ui.actionEnableSoftwareRamAndUart, SIGNAL(triggered(bool)), this, SLOT(slotEnableSoftwareRamAndUart(bool)));
  connect(ui.actionLinuxEnableL2, SIGNAL(triggered(bool)), this, SLOT(slotLinuxEnableL2(bool)));
  connect(ui.actionLinuxClearMPBs, SIGNAL(triggered(bool)), this, SLOT(slotLinuxClearMPBs(bool)));
  connect(ui.actionLinuxSelectImage, SIGNAL(triggered()), this, SLOT(slotLinuxSelectImage()));
  connect(ui.actionLinuxRestoreImage, SIGNAL(triggered()), this, SLOT(slotLinuxRestoreImage()));
  connect(ui.actionLinuxBootAll, SIGNAL(triggered(bool)), this, SLOT(slotLinuxBootAll(bool)));
  connect(ui.actionPopshmSettings, SIGNAL(triggered()), this, SLOT(slotPopshmSettings()));
  connect(ui.actionCmdLineArgs, SIGNAL(triggered()), this, SLOT(slotCmdLineArgs()));
  connect(ui.actionWischer, SIGNAL(triggered()), this, SLOT(slotWischer()));
  connect(ui.actionBootLinux, SIGNAL(triggered()), this, SLOT(slotBootLinux()));
  connect(ui.actionPreloadObject, SIGNAL(triggered()), this, SLOT(slotPreloadObject()));
  connect(ui.actionPreloadLut, SIGNAL(triggered()), this, SLOT(slotPreloadLut()));
  connect(ui.actionClearMPBs, SIGNAL(triggered()), this, SLOT(slotClearMPBs()));
  connect(ui.actionChangeL2Enables, SIGNAL(triggered()), this, SLOT(slotChangeL2Enables()));
  connect(ui.actionChangeResets, SIGNAL(triggered()), this, SLOT(slotChangeResets()));
  connect(ui.actionGetBoardStatus, SIGNAL(triggered()), this, SLOT(slotGetBoardStatus()));
  connect(ui.actionResetPlatform, SIGNAL(triggered()), this, SLOT(slotResetPlatform()));
  connect(ui.actionCustomBmcCmd, SIGNAL(triggered()), this, SLOT(slotCustomBmcCmd()));
  connect(ui.actionInitializePlatform, SIGNAL(triggered(bool)), this, SLOT(slotInitializePlatform(bool)));
  connect(ui.actionExecuteRccFile, SIGNAL(triggered()), this, SLOT(slotExecuteRccFile()));
  connect(ui.actionTrainSystemInterface, SIGNAL(triggered()), this, SLOT(slotTrainSystemInterface()));
  connect(ui.actionTestMemory, SIGNAL(triggered()), this, SLOT(slotTestMemory()));
  connect(ui.actionSetLinuxGeometry, SIGNAL(triggered()), this, SLOT(slotSetLinuxGeometry()));

  // Re-Try of booting linux in case of busy sccApi...
  bootRetryTimer.setSingleShot(true);
  connect(&bootRetryTimer, SIGNAL(timeout()), this, SLOT(slotBootLinux()));

  // Handle geometry...
  bool ok;
  setGeometry(settings->value("GUI/startupPositionX", (int)(QApplication::desktop()->width() - INITIAL_WIDTH) / 2).toInt(&ok),
              settings->value("GUI/startupPositionY", (int)(QApplication::desktop()->height() - INITIAL_HEIGHT) / 2).toInt(&ok),
              settings->value("GUI/startupWidth", INITIAL_WIDTH).toInt(&ok),
              settings->value("GUI/startupHeight", INITIAL_HEIGHT).toInt(&ok));

  // Widgets
  startFlitWidget();
  startPacketWidget();
  startReadMemWidget();
  startPerfmeterWidget(NUM_COLS, NUM_ROWS, NUM_CORES);

  // Load further settings
  if (settings->value("debugging/EnableSoftwareRamAndUart", false).toBool()) {
    printWarning("Software RAM and software UART are still enabled. The CRBIF will not be usable!\n         Please ignore this warning if the use of SoftRAM and/or SoftUART is intended!");
  }
  enableSoftwareRamAndUart(settings->value("debugging/EnableSoftwareRamAndUart", false).toBool());
  ui.actionLinuxEnableL2->setChecked(settings->value("bootLinux/EnableL2", true).toBool());
  ui.actionLinuxClearMPBs->setChecked(settings->value("bootLinux/ClearMPBs", false).toBool());
  ui.actionLinuxBootAll->setChecked(settings->value("bootLinux/bootAll", false).toBool());
  ui.actionEnableSoftwareRamAndUart->setChecked(settings->value("debugging/EnableSoftwareRamAndUart", false).toBool());
  linuxImage = settings->value("bootLinux/linuxImage", getSccKitPath() + "resources/linux.obj").toString();
  genericObject = settings->value("genericBoot/genericObject", "").toString();
  genericLut = settings->value("genericBoot/genericLut", "").toString();
  initDDR3 = settings->value("GUI/initDDR3", false).toBool(); // Debug feature...

  // Initialize semaphore...
  protectApi.release(1);

  // Don't open GUI if initialization failed. Otherwise open it...
  if (!initFailed) {
    // Show GUI
    show();
    raise();
    activateWindow();
    printInfo("Configuration of this system:\n- Management console PC (MCPC): ", QHostInfo::localHostName(), "\n- Attached platform: ", sccAccess->getPlatform(), " (", QString::number(sccAccess->getSizeOfMC()), " GB per MC -> ", QString::number(sccAccess->getSizeOfMC()*4), " GB total)\n- BMC:  ", sccAccess->getBMCServer());
    printInfo("Welcome to sccGui ", VERSION_TAG, (QString)((BETA_FEATURES!=0)?"beta":""), " (build date ", __DATE__, " - ", __TIME__, ")...");
  } else {
    exit(-1);
  }
}

sccGui::~sccGui() {
  closeEvent(NULL);
  delete settings;
  delete sccAccess;
  softramRelease();
  printInfo("Closing sccGui application! Bye...\n");
  delete log;
}

// Make sure to close widgets when user requests to close main application window...
void sccGui::closeEvent(QCloseEvent *event) {
  if (ui.actionInitializePlatform->isChecked()) {
    event->ignore();
    printWarning("Please end SIF training before closing GUI! Ignoring request...");
    return;
  }
  enableSoftwareRamAndUart(false);
  linuxMask.abortDialog();
  clearMPBMask.abortDialog();
  changeL2Mask.abortDialog();
  changeResetMask.abortDialog();
  stopFlitWidget();
  stopPacketWidget();
  stopReadMemWidget();
  stopPerfmeterWidget();
  if (konsoleProcess) slotStartkScreen(); // Actually closes the konsole, when it's open...
  return;
  // Will never be reached: Fake code to prevent compiler to prevent "unused argument" warning...
  if (event) return;
}

// System Interface protection methods...
bool sccGui::acquireApi() {
  while (!protectApi.tryAcquire(1)) {
    qApp->processEvents(); // allow others to continue...
    #if DEBUG_SIF_SEMA == 1
      printf("."); fflush(0);
    #endif
    if (releaseSemaphores) return false;
  }
  #if DEBUG_SIF_SEMA == 1
    printf("Acquired Api (semaphore value is %0d)...\n", protectApi.available()); fflush(0);
  #endif
  return true;
}

bool sccGui::tryAcquireApi() {
  bool result = protectApi.tryAcquire(1);
  #if DEBUG_SIF_SEMA == 1
    printf("Tryed to aquire Api (semaphore value is %0d)...\n", result); fflush(0);
  #endif
  if (result) {
  }
  return result;
}

bool sccGui::availableApi() {
  bool result = protectApi.available();
  #if DEBUG_SIF_SEMA == 1
    printf("Probed Api availability (semaphore value is %0d)...\n", protectApi.available()); fflush(0);
  #endif
  return result;
}

void sccGui::releaseApi() {
  while(protectApi.tryAcquire(1));
  protectApi.release(1);
  #if DEBUG_SIF_SEMA == 1
    printf("Released Api (semaphore value is %0d)...\n", protectApi.available()); fflush(0);
  #endif
}

void sccGui::slotAboutDialog() {
  QMessageBox msgBox;
  msgBox.setWindowTitle(tr("About sccGui"));
  msgBox.setIconPixmap ( QPixmap(":/resources/guiPixmaps/sccGuiIcon.png") );
  QString message = (QString)"<b>sccGui</b> - SCC configuration software" +
  "<br><br>Configuration of this system:" +
  "<br>- Management console PC (MCPC): " + QHostInfo::localHostName() +
  "<br>- Attached platform: " + sccAccess->getPlatform() + " (" + QString::number(sccAccess->getSizeOfMC()) + " GB per MC -> " + QString::number(sccAccess->getSizeOfMC()*4) + " GB total)" +
  "<br>- BMC:  " + sccAccess->getBMCServer() +
  "<br><br>For comments, bug reports or questions<br>please use <a href=\"http://communities.intel.com/community/marc\">MARC forum</a>!" +
  "<br><br><b>Release Info: " + VERSION_TAG + (QString)((BETA_FEATURES!=0)?"beta":"") + "</b> (build date " + __DATE__ + " - " + __TIME__ + ")" +
  "<br><br>Copyright(C) 2009-2010 by Intel Corporation" +
  "<br>Intel(R) Labs Braunschweig. All rights reserved.<br>";
  msgBox.setText(message);
  msgBox.setStandardButtons(QMessageBox::Ok);
  msgBox.setDefaultButton(QMessageBox::Ok);
  msgBox.exec();
}

void sccGui::slotSplashscreenEnabled(bool checked) {
  settings->setValue("GUI/Splashscreen", checked);
}

void sccGui::slotSaveGeometry() {
  settings->setValue("GUI/startupPositionX", x());
  settings->setValue("GUI/startupPositionY", y());
  settings->setValue("GUI/startupWidth", width());
  settings->setValue("GUI/startupHeight", height());
}

void sccGui::slotRestoreGeometry() {
  settings->remove("GUI/startupPositionX");
  settings->remove("GUI/startupPositionY");
  settings->remove("GUI/startupWidth");
  settings->remove("GUI/startupHeight");
  setGeometry((int)(QApplication::desktop()->width() - INITIAL_WIDTH) / 2,
              (int)(QApplication::desktop()->height() - INITIAL_HEIGHT) / 2,
              INITIAL_WIDTH, INITIAL_HEIGHT);
}

void sccGui::slotClearLogWindow() {
  ui.actionClearLogWindow->setIcon(QPixmap(":/resources/menuIcons/garbageFull.png"));
  log->clear();
}

void sccGui::slotSetPacketTraceFile() {
  QString tmpName;
  QFileInfo tmpInfo;

  tmpName = QFileDialog::getSaveFileName(
             mW,
             "Choose location of trace logfile...",
             sccAccess->getPacketLog(),
             "Trace logfile (*.log)" );

  // Cancel in case of user abort
  if (tmpName == "") {
    printInfo("Selection cancelled... Keeping current logfile \"",sccAccess->getPacketLog(),"\"...");
    return;
  }
  sccAccess->setPacketLog(tmpName);
}

void sccGui::slotSetPacketTraceMode() {
  bool okay;
  QString value;
  int index = 0;
  if (sccAccess->getDebugPacket()==NO_DEBUG_PACKETS)
    index = 0;
  else if (sccAccess->getDebugPacket()==RAW_DEBUG_PACKETS)
    index = 1;
  else if (sccAccess->getDebugPacket()==TEXT_DEBUG_PACKETS)
    index = 2;
  value = QInputDialog::getItem(mW, "Packet monitor", "Select packet monitoring mode:", QStringList() << "Disable" << ((QString)"Trace RAW packet transfers to \""+sccAccess->getPacketLog()+"\"") << ((QString)"Trace text description of packet transfers to \""+sccAccess->getPacketLog()+"\""), index, false, &okay);
  if (okay) {
    sccAccess->setDebugPacket((value=="Disable")?NO_DEBUG_PACKETS:(value==((QString)"Trace RAW packet transfers to \""+sccAccess->getPacketLog()+"\""))?RAW_DEBUG_PACKETS:TEXT_DEBUG_PACKETS);
  }
}

void sccGui::slotEnableSoftwareRamAndUart(bool checked) {
  settings->setValue("debugging/EnableSoftwareRamAndUart", checked);
  enableSoftwareRamAndUart(checked);
}

void sccGui::enableSoftwareRamAndUart(bool enable) {
  if (enable) {
    if (cSoftram) return; // The stuff is already enabled...
    // Prepare IO debugging
    disconnect(sccAccess, SIGNAL(ioRequest(uInt32*)), 0, 0);
    connect(sccAccess, SIGNAL(ioRequest(uInt32*)), this, SLOT(slotSoftUartRequest(uInt32*)));
    // Prepare Soft-RAM
    disconnect(sccAccess, SIGNAL(softramRequest(uInt32*)), 0, 0);
    connect(sccAccess, SIGNAL(softramRequest(uInt32*)), this, SLOT(slotSoftramRequest(uInt32*)));
    softramInit(1<<30);
    sccAccess->softramSlotConnected(true);
    QFont font("Monospace");
    QFontMetrics fm(font);
    uartWidget = new QTabWidget();
    uartWidget->setWindowTitle("Software UART emulation...");
    uartWidget->setTabPosition(QTabWidget::South);
    uartWidget->resize(fm.width(QChar('X'))*80,fm.height()*52+30);
    for (int loop = 0; loop < (NUM_ROWS*NUM_COLS*NUM_CORES); loop++) {
      if (uartTrace[loop]) {
        uartWidget->removeTab(loop);
        delete uartTrace[loop];
      }
      uartTrace[loop] = new QTextEdit;
      uartTrace[loop]->setReadOnly(true);
      uartTrace[loop]->setWindowTitle("Core"+messageHandler::toString(Z_PID(loop),10,1)+" of tile x="+messageHandler::toString(X_PID(loop),10,1)+", y="+messageHandler::toString(Y_PID(loop),10,1)+"...");
      uartTrace[loop]->setLineWrapMode(QTextEdit::NoWrap);
      uartTrace[loop]->setBaseSize(320,240);
      uartTrace[loop]->setCurrentFont(font);
      uartTrace[loop]->resize(fm.width(QChar('X'))*80,fm.height()*52);
      uartWidget->insertTab(loop, uartTrace[loop], "rck"+messageHandler::toString(loop,10,2));
    }
    // Disable automatic generation of responses.
    uInt32 regValue;
    sccAccess->readFpgaGrb(SIFCFG_CONFIG, &regValue);
    if (regValue != (regValue & 0xfffffffd)) {
      regValue = regValue & 0xfffffffd;
      sccAccess->writeFpgaGrb(SIFCFG_CONFIG, regValue);
      printInfo( "CRBIF network driver is now deactivated (automatic write responses disabled)...");
    }
    // Enable Software RAM in memory reader...
    ReadMemWidget->ui.cB_SubDest->insertItem(ReadMemWidget->ui.cB_SubDest->count(), "SoftRAM");
    printInfo("Software RAM and software UART are enabled...");
  } else {
    if (!cSoftram) return; // The stuff is already disabled...
    // End Soft-RAM
    sccAccess->softramSlotConnected(false);
    disconnect(sccAccess, SIGNAL(softramRequest(uInt32*)), 0, 0);
    connect(sccAccess, SIGNAL(softramRequest(uInt32*)), sccAccess, SLOT(slotSoftramRequest(uInt32*)));
    softramRelease();
    // End IO debugging
    disconnect(sccAccess, SIGNAL(ioRequest(uInt32*)), 0, 0);
    connect(sccAccess, SIGNAL(ioRequest(uInt32*)), sccAccess, SLOT(slotIoRequest(uInt32*)));
    for (int loop = 0; loop < (NUM_ROWS*NUM_COLS*NUM_CORES); loop++) {
      if (uartTrace[loop]) {
        delete uartTrace[loop];
      }
      uartTrace[loop] = NULL;
    }
    delete uartWidget;
    uartWidget = NULL;
    // (Re-)Enable write response generation...
    uInt32 regValue;
    sccAccess->readFpgaGrb(SIFCFG_CONFIG, &regValue);
    if (regValue != (regValue | 0x02)) {
      regValue = regValue | 0x02;
      sccAccess->writeFpgaGrb(SIFCFG_CONFIG, regValue);
      printInfo( "CRBIF network driver is now re-activated (automatic write responses enabled)...");
    }
    // Disable Software RAM in memory reader...
    if (ReadMemWidget->ui.cB_SubDest->itemText(ReadMemWidget->ui.cB_SubDest->count()-1) == "SoftRAM") {
      ReadMemWidget->ui.cB_SubDest->removeItem(ReadMemWidget->ui.cB_SubDest->count()-1);
    }
    printInfo("Software RAM and software UART are disabled...");
  }
}

// Process tracing requests...
uInt32 trcMsgArray[12];
uInt32* trcMsgPointer = (uInt32*)trcMsgArray;
void sccGui::slotSoftUartRequest(uInt32 * message) {
  int tmp;
  mem32Byte mem;
  int sif_port_recipient;
  int sif_port_sender;
  int transid;
  int byteenable;
  int destid;
  int routeid;
  int cmd;

  tmp = message[8]; // contains byteenable, transid, srcid, destid
  sif_port_sender = (tmp >> 16) & 0x0ff;
  transid = (tmp >> 8) & 0x0ff;
  byteenable = tmp & 0x0ff;
  mem.addr = message[9];  //  32b of the 34bit physical target adress
  tmp = message[10];  // contains 2b of physical target address, 12b command type; 8b sccroute (x,y); 3b src/destid
  mem.addr += ((uInt64) (tmp & 0x03)) << 32;
  destid = tmp >> 22 & 0x07;
  routeid = tmp >> 14 & 0x0ff;
  cmd = tmp >> 2 & 0x01ff;
  sif_port_recipient = tmp >> 11 & 0x03;
  mem.data[3] = ((uInt64) message[7] << 32) + (uInt64) message[6];
  mem.data[2] = ((uInt64) message[5] << 32) + (uInt64) message[4];
  mem.data[1] = ((uInt64) message[3] << 32) + (uInt64) message[2];
  mem.data[0] = ((uInt64) message[1] << 32) + (uInt64) message[0];

  // Print message...
  if (((mem.addr == 0x2f8)||(mem.addr == 0x3f8)) && (byteenable == 0x01) && (cmd == NCIOWR)) {
    int pid = PID(X_TID(routeid), Y_TID(routeid), destid);
    if (uartTrace[pid]) {
      QChar tmp = (char)(mem.data[0] & 0x0ff);
      if (tmp != "\r"[0]) { // Filter \r as QTextEdit interprets it as CR...
        uartTrace[pid]->insertPlainText(tmp);
        if (tmp == "\n"[0]) {
          QTextCursor c = uartTrace[pid]->textCursor();
          c.movePosition(QTextCursor::End);
          uartTrace[pid]->setTextCursor(c);
        }
        if (!uartWidget->isVisible()) {
          uartWidget->show();
        }
        // printf("ASCII character: %c (0x%02x / %03dd)\n",(int)(mem.data[0] & 0x0ff), (int)(mem.data[0] & 0x0ff), (int)(mem.data[0] & 0x0ff));
        // fflush(0);
      }
    } else {
      printf("%c",(int)(mem.data[0] & 0x0ff));
      fflush(0);
    }
  } else {
    // This is an IO Access to the host. Currently this is not supported, so print a warning...
    // In future this section may contain IO signals for IORD and IOWR that can be connected by user scenarios...
    printWarning("Received unexpected IO-Packet (tracing only works on address 0x2f8 & 0x3f8):");
    sccAccess->printPacket(message, "Unexpected IO packet", false, false);
  }

  // handshake signal with SIF interface that we're done with the message array - don't touch again.
  message[10] = 0;

  // Prepare SCEMI message with response...
  trcMsgPointer[10]  = (uInt32)destid<<22;
  trcMsgPointer[10] += (uInt32)routeid<<14;
  trcMsgPointer[10] += (uInt32)MEMCMP<<2;
  trcMsgPointer[10] += (uInt32)((uInt64)(mem.addr)>>32);
  trcMsgPointer[9]  = (uInt32)(mem.addr & 0x0ffffffff);
  trcMsgPointer[8]  = (uInt32)sif_port_sender<<24;
  trcMsgPointer[8] += (uInt32)sif_port_recipient<<16;
  trcMsgPointer[8] += (uInt32)transid<<8;
  trcMsgPointer[8] += (uInt32)byteenable;
  memcpy((void*) trcMsgPointer, (void*) mem.data, 8*sizeof(uInt32));
  if (sif_port_sender == SIF_HOST_PORT) {
    // If request comes from host, answer directly (faster)...
    if (sccAccess->debugPacket==RAW_DEBUG_PACKETS) sccAccess->printPacket(trcMsgPointer, "RX", true, true);
    sccAccess->rxMsg = (uInt32*) malloc(12*sizeof(uInt32));
    memcpy((void*) sccAccess->rxMsg, (void*) trcMsgPointer, 12*sizeof(uInt32));
    sccAccess->rxMsgAvail = true;
  } else {
    // Send SCEMI packet otherwise...
    sccAccess->sendMessage(trcMsgPointer);
    // Print packet log directly as we bypass transfer_packet to prevent deadlock...
    if (sccAccess->debugPacket==RAW_DEBUG_PACKETS) {
      if (sccAccess->packetLog.isOpen()) {
        char msg[255];
        sprintf(msg, "TX %03d from %s to %s -> transferPacket(0x%02x, %s, 0x%01x_%08x, %s, 0x%02x, 0x%016llx_%016llx_%016llx_%016llx);\n", transid, sccAccess->decodePort(sif_port_recipient).toStdString().c_str(), sccAccess->decodePort(sif_port_sender).toStdString().c_str(), routeid, sccAccess->decodeDestID(destid).toStdString().c_str(), (unsigned int)(mem.addr>>32), (unsigned int)(mem.addr & 0x0ffffffff), sccAccess->decodeCommand(MEMCMP).toStdString().c_str(), byteenable, mem.data[3], mem.data[2], mem.data[1], mem.data[0] );
        sccAccess->packetLog.write(msg);
        sccAccess->packetLog.flush();
      }
    }
  }
  // GUI stuff
  if (!transid) qApp->processEvents(); // allow GUI to update from time to time...

  return;
}


void sccGui::slotLinuxEnableL2(bool checked) {
  settings->setValue("bootLinux/EnableL2", checked);
}

void sccGui::slotLinuxClearMPBs(bool checked) {
  settings->setValue("bootLinux/ClearMPBs", checked);
}

void sccGui::slotLinuxSelectImage() {
  QString tmpName;
  QFileInfo tmpInfo;

  tmpName = QFileDialog::getOpenFileName(
             mW,
             "Choose a new object...",
             linuxImage,
             "Objects (*.obj)" );

  // Cancel in case of user abort
  if (tmpName == "") return;

  // Check file and update settings
  tmpInfo = tmpName;
  if (tmpInfo.isReadable()) {
    linuxImage = tmpName;
    settings->setValue("bootLinux/linuxImage", linuxImage);
    printInfo("Standard Linux image is from now on \"", linuxImage, "\"...");
  } else {
    printWarning("Failed to use object name \"", tmpName, "\" (File doesn't exist or is not readable)...");
    return;
  }
}

void sccGui::slotLinuxRestoreImage() {
  settings->remove("bootLinux/linuxImage");
  linuxImage = getSccKitPath() + "resources/linux.obj";
  printInfo("Using default sccKit Linux image\"", linuxImage, "\" from now on...");
}

void sccGui::slotLinuxBootAll(bool checked) {
  settings->setValue("bootLinux/bootAll", checked);
}

void sccGui::slotPopshmSettings() {
  bool okay;
  QString message = (QString)"\
This dialog allows you to select the amount of consecutive\n\
memory in MB that each Linux instance dedicates for POP-SHM\n\
use... If you don't want to use POP-SHM (or don't even know\n\
what that is): Select 0 MB to maximize the private memory size.";
  QStringList selections;
  selections << " 0 MB per Core (POP-SHM disabled)"            << "16 MB per Core (Total of  768 MB @ 48 Cores)"
             << "32 MB per Core (Total of 1536 MB @ 48 Cores)" << "48 MB per Core (Total of 2304 MB @ 48 Cores)"
             << "64 MB per Core (Total of 3072 MB @ 48 Cores)" << "80 MB per Core (Total of 3840 MB @ 48 Cores)";
  int currentSetting = settings->value("bootLinux/popShmPages", 0).toInt();
  QString selection = QInputDialog::getItem(mW, "Set POP-SHM size...", message, selections, currentSetting, false, &okay);
  if (okay) {
    QString tmp = selection;
    int newSetting = tmp.replace(QRegExp(" MB.*"), QString("")).toInt(&okay) / 16;
    settings->setValue("bootLinux/popShmPages", newSetting);
    if (newSetting) {
      printfInfo("New POP-SHM setting: %2d MB per Core (Total of %4d MB @ 48 Cores)...", 16*newSetting, 16*48*newSetting);
    } else {
      printInfo( "New POP-SHM setting: 0 MB per Core (POP-SHM disabled)...");
    }
  }
}

void sccGui::slotCmdLineArgs() {
  bool ok;
  QString cmdLine = QInputDialog::getText(this, "Custom kernel command line",
                                            "BE WARNED! This is an expert setting that should only\nbe used by kernel and/or driver programmers!\n\nThat said... Please specify a custom kernel command line:", QLineEdit::Normal,
                                            settings->value("bootLinux/cmdLine", "").toString(), &ok);
  if (ok) {
    printInfo("Custom kernel command line is \"", cmdLine,"\"...");
    settings->setValue("bootLinux/cmdLine", cmdLine);
  }
}

void sccGui::slotWischer() {
  // Don't allow others events to be triggered...
  uiEnable(false, false, false, false, false);
  printInfo("Applying global software reset to SCC (cores & CRB registers)...");
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core, true)) {
          printError("Unable to apply reset... Please perform platform (re-)initialization!");
          uiEnable(true, true, true, true, true);
          return;
        }
        changeResetMask.setEnabled(loop_x, loop_y, loop_core, true);
      }
    }
  }

  // Initialize SHM-COM pointer area with all zeros...
  uInt64 nullData[4]={0,0,0,0};
  perfApertureStart = settings->value("perfMeter/ApertureStart", 0).toULongLong();
  for (uInt64 loopAddr=perfApertureStart-PERF_SHM_OFFSET; loopAddr < (perfApertureStart-PERF_SHM_OFFSET+0x1000); loopAddr+=0x20) {
    sccAccess->transferPacket(0x00, PERIW, loopAddr, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT, BCMC);
  }

  // Initialize Tile registers with defaults...!
  sccAccess->initCrbRegs();

  // Initialize FPGA registers...
  sccAccess->programAllGrbRegisters();

  // Re-Init GUI...
  ui.actionBootLinux->setIcon(QPixmap(":/resources/menuIcons/linuxBlue.png"));
  ui.actionBootLinux->setToolTip("Linux not booted! Click icon to start booting...");
  slotClosekScreen(0);
  stopPerfmeterPolling(true);
  uiEnable(true, true, true, true, true);

  // Trace stuff...
  if (settings->value("debugging/EnableSoftwareRamAndUart", false).toBool()) {
    // Clear content of trace window (if one exists...)
    for (int loop = 0; loop < (NUM_ROWS*NUM_COLS*NUM_CORES); loop++) {
      if (uartTrace[loop]) {
        uartTrace[loop]->clear();
      }
    }
  }
}

// slotBootLinux(): This Routine runs an MMIO based Linux build on selected cores (dialog). No
// checks are performed. The Linux images are distributed on all RCK memory Controllers!
void sccGui::slotBootLinux() {
  // Don't start with polling before current block transfers are done...
  if (!tryAcquireApi()) {
    bootRetryTimer.start(1); // Very fast retry...
    return;
  }

  // boot Linux
  bootLinux();

  // Finally release API...
  releaseApi();
}

void sccGui::bootLinux() {
  // Variables & initializations...
  sccProgressBar pd;
  int shmSIFPort = SIF_RC_PORT;
  QByteArray errMsg;

  // Don't allow others events to be triggered...
  uiEnable(false, false, false, false, false);

  // Find out which cores to use
  if (ui.actionLinuxBootAll->isChecked()) {
    linuxMask.enableAll();
  } else {
    linuxMask.pingCores();
    linuxMask.invertMask(); // Offer "unbooted" cores for (re-)boot...
    if (!linuxMask.selectionDialog("Boot Linux on selected cores...", "Checked box means that Linux OS will be booted on that core!")) {
      printInfo("Operation cancelled by user...");
      qApp->processEvents();
      uiEnable(true, true, true, true, true);
      return;
    }
  }

  // Update status
  ui.actionBootLinux->setIcon(QPixmap(":/resources/menuIcons/linuxOrange.png"));
  ui.actionBootLinux->setToolTip("Booting Linux...");
  qApp->processEvents();
  printInfo("Starting to boot Linux: ", linuxMask.getMaskString());

  // Find out video buffer size ((2*x*y*(bpp/8) + 0x1000 ) / 0x100000...
  //                              ^          ^       ^          ^_ MBytes instead of Bytes
  //                              |          |       |____________ GFX Header...
  //                              |          |____________________ Bytes instead of bits
  //                              |_______________________________ Allow double buffering (double capacity)
  uInt32 videoBufferSize = ((2 * sccAccess->getDisplayX() * sccAccess->getDisplayY() * (sccAccess->getDisplayDepth() / 8)) + 0x1000) / 0x100000;
  if (((2 * sccAccess->getDisplayX() * sccAccess->getDisplayY() * (sccAccess->getDisplayDepth() / 8)) + 0x1000) % 0x100000) videoBufferSize++;

  // Find out POP-SHM settings
  int popShmPages = settings->value("bootLinux/popShmPages", 0).toInt();

  // Define command-line for kernel...
  printfInfo("Over-all private memory:\t  %3d MB per Core", sccAccess->getSizeOfMC()*80);
  printfInfo("Video buffer size:\t- %3d MB per Core", videoBufferSize);
  printfInfo("POP-SHM buffer size:\t- %3d MB per Core%s", 16*popShmPages, (popShmPages)?"":" (disabled)");
  printfInfo("Usable private memory:\t= %3d MB per Core", sccAccess->getSizeOfMC()*80-videoBufferSize-16*popShmPages);
  printfInfo("Video resolution is %0dx%0d @%0d bit...", sccAccess->getDisplayX(), sccAccess->getDisplayY(), sccAccess->getDisplayDepth());
  if (settings->value("debugging/EnableSoftwareRamAndUart", false).toBool()) printInfo("Early kernel printing is enabled (implicitly by enabling Software UART)...");
  QString cmdLine = settings->value("bootLinux/cmdLine", "").toString();
  if (cmdLine!="") printInfo("Custom kernel parameters: \""+cmdLine+"\"...");
  cmdLine = "priv_core_space=" + QString::number(sccAccess->getSizeOfMC()*80 - videoBufferSize) +
            " video=rckgfx:base:" + QString::number(sccAccess->getSizeOfMC()*80 - videoBufferSize) + ",size:" + QString::number(videoBufferSize) + ",x:" + QString::number(sccAccess->getDisplayX()) + ",y:" + QString::number(sccAccess->getDisplayY()) + ",bpp:" + QString::number(sccAccess->getDisplayDepth()) +
            " popshmpages=" + QString::number(popShmPages) +
            (QString)((settings->value("debugging/EnableSoftwareRamAndUart", false).toBool())?" earlyprintk=scc,keep":"") +
            (QString)((cmdLine != "")?" "+cmdLine:"");

  // Do we have a pre-merged image and want to start on 48 cores?
  QString linuxPremerged = linuxImage; linuxPremerged.replace(QRegExp("/[^/]+$"), "/premerge_image_0_0.32.obj");
  QString linuxDat = linuxImage; linuxDat.replace(QRegExp("/[^/]+$"), "/patch.dat");
  QString copycatImage = linuxImage; copycatImage.replace(QRegExp("/[^/]+$"), "/copycat_0_0.32.obj");
  QString copycatDat = linuxImage; copycatDat.replace(QRegExp("/[^/]+$"), "/copycat.dat");;
  bool usePreMerged = false;
  QFileInfo tmpFileInfo(linuxPremerged);
  if ((linuxMask.getNumCoresEnabled()==NUM_ROWS*NUM_COLS*NUM_CORES) && (sccAccess->getSizeOfMC()==8)) {
    if (tmpFileInfo.exists()) {
      tmpFileInfo.setFile(linuxDat);
      if (tmpFileInfo.exists()) {
        tmpFileInfo.setFile(copycatImage);
        if (tmpFileInfo.exists()) {
          tmpFileInfo.setFile(copycatDat);
          if (tmpFileInfo.exists()) {
            usePreMerged = true;
          }
        }
      }
    }
  }

  // -> If so, use pre-merged image...
  if (usePreMerged) {
    // Pull all resets
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,true)) {
            uiEnable(true, true, true, true, true);
            return;
          }
        }
      }
    }

    // Preload objects
    printInfo("Preloading Memory with pre-merged linux object file \"", linuxPremerged, "\"...");
    bool retVal;
    retVal = sccAccess->writeMemFromOBJ(0x00, PERIW, linuxPremerged, &pd, SIF_RC_PORT, BCMC);
    // Patch command line if requested...
    if (cmdLine != "") {
      QProcess* sccCmdline = new QProcess();
      sccCmdline->setWorkingDirectory(sccAccess->getTempPath());
      qApp->processEvents(); // Update log
      sccCmdline->start(getSccKitPath() + "bin/sccCmdline -p \"" + cmdLine + "\"\n");
      if (!sccCmdline->waitForFinished(30000)) {
        printError("sccCmdline timed out (more than 30 seconds)... Aborting!");
        sccCmdline->terminate();
        if (!sccCmdline->waitForFinished(1000)) {
          sccCmdline->kill();
        }
        return;
      }
      if ((errMsg = sccCmdline->readAllStandardError()) != "") {
        printError("Failed to process kernel command-line parameters... Aborting!");
        printError("sccCmdline error message:\n", errMsg);
        return;
      }
      delete sccCmdline;
      QFileInfo tmpInfo = sccAccess->getTempPath()+"cmdline.obj";
      if(tmpInfo.isReadable()) {
        printInfo("Patching kernel command-line...");
        retVal = sccAccess->writeMemFromOBJ(0x00, PERIW, sccAccess->getTempPath()+"cmdline.obj", &pd, SIF_RC_PORT, BCMC);
      }
    }
    // Duplicate image and update LUTs
    printInfo("Preloading Memory with copycat object file \"", copycatImage, "\"...");
    retVal = sccAccess->writeMemFromOBJ(0x00, PERIW, copycatImage, NULL, SIF_RC_PORT, BCMC);
    printInfo("Preloading copycat LUTs...");
    if (!sccAccess->WriteFlitsFromFile(copycatDat, "LUTs")) {
      uiEnable(true, true, true, true, true);
      return;
    }

    // Find out how many slots we need to copy
    int max_code_slot = (int) sccAccess->readFlit(0x00, CRB, LUT0+0x7f*0x08);

    // Init MPB "done flags"...
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_COLS; loop_core++) {
          sccAccess->writeFlit(TID(loop_x, loop_y), MPB, loop_core*0x2000+0x08*(max_code_slot+1), 0);
        }
      }
    }

    // Release all resets and en-/ or disable L2s!
    printInfo("Beginning to duplicate linux image (",QString::number(max_code_slot+1)," slots) to all cores and ", settings->value("bootLinux/EnableL2", true).toBool()?"en":"dis", "abling L2 caches...");
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          if (!sccAccess->setL2(TID(loop_x, loop_y),loop_core,settings->value("bootLinux/EnableL2", true).toBool())) {
            uiEnable(true, true, true, true, true);
            return;
          }
          if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,false)) {
            uiEnable(true, true, true, true, true);
            return;
          }
        }
      }
    }

    // Check MPB "done flags"...
    struct timeval timeout, current;
    bool copyIsReady = false;

    // Get time and prepare timeout counter
    gettimeofday(&timeout, NULL);
    timeout.tv_sec += (max_code_slot+1)*10;
    // Wait until all cores copied the image...
    while(!copyIsReady) {
      copyIsReady = true;
      for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
        for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
          for (int loop_core=0; loop_core < NUM_COLS; loop_core++) {
            if (!sccAccess->readFlit(TID(loop_x, loop_y), MPB, loop_core*0x2000+0x08*(max_code_slot+1))) copyIsReady = false;
          }
        }
      }
      // Allow polling...
      qApp->processEvents();
      // Check if timout timer expired
      gettimeofday(&current, NULL);
      if (((current.tv_sec == timeout.tv_sec) && (current.tv_usec >= timeout.tv_usec)) || (current.tv_sec > timeout.tv_sec)) {
        printError("Failed to copy Linux image to all cores...");
        uiEnable(true, true, true, true, true);
        return;
      }
    }

    // Pull all resets
    printInfo("Duplication of linux images completed successfully!");
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,true)) {
            uiEnable(true, true, true, true, true);
            return;
          }
        }
      }
    }

    printInfo("Patching LUTs for boot of Linux...");
    if (!sccAccess->WriteFlitsFromFile(linuxDat, "LUTs")) {
      uiEnable(true, true, true, true, true);
      return;
    }

    // Clear MPBs
    uInt64 nullData[4]={0,0,0,0};
    if (settings->value("bootLinux/ClearMPBs", false).toBool()) {
      printInfo("Clearing MPBs: ", linuxMask.getMaskString());
      if (linuxMask.getNumCoresEnabled() == NUM_ROWS*NUM_COLS*NUM_CORES) {
        for (uInt64 loop_i=0; loop_i<(1<<14); loop_i+=0x20) {
          sccAccess->transferPacket(0x00, SUB_DESTID_MPB, loop_i, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT, BCTILE);
        }
      } else {
        for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
          for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
            for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
              if (linuxMask.getEnabled(loop_x,loop_y,loop_core)) {
                for (uInt64 loop_i=((loop_core)?(1<<13):0); loop_i<((loop_core)?(1<<14):(1<<13)); loop_i+=0x20) {
                  sccAccess->transferPacket(TID(loop_x, loop_y), SUB_DESTID_MPB, loop_i, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT);
                }
              }
            }
          }
        }
      }
    }

    // Find out SHM location
    uInt64 tmp = linuxMask.getFirstActiveCore();
    tmp = sccAccess->readFlit(TID_PID(tmp), CRB, MC_SHMBASE_MC0+((Z_PID(tmp))?LUT1:LUT0));
    settings->setValue("perfMeter/Route", perfRoute = (tmp&0x01fe000)>>13);
    settings->setValue("perfMeter/Destid", perfDestid = (tmp&0x01c00)>>10);
    settings->setValue("perfMeter/SIFPort", perfSIFPort = shmSIFPort);
    uInt64 oldApertureStart = settings->value("perfMeter/ApertureStart", 0).toULongLong();
    perfApertureStart = ((tmp&0x3ff)<<24)+PERF_SHM_OFFSET;
    if ((perfSIFPort==SIF_MC_PORT) && (perfApertureStart >= 0x180000000ull)) perfApertureStart -= 0x180000000ull;
    settings->setValue("perfMeter/ApertureStart", perfApertureStart);
    sccAccess->writeFpgaGrb(SIFGRB_PMEMSLOT, 0);

    // In case of new SHM-COM pointer area, re-initialize it...
    if (oldApertureStart != perfApertureStart) {
      // Initialize SHM-COM pointer area with all zeros...
      uInt64 nullData[4]={0,0,0,0};
      for (uInt64 loopAddr=perfApertureStart-PERF_SHM_OFFSET; loopAddr < (perfApertureStart-PERF_SHM_OFFSET+0x1000); loopAddr+=0x20) {
        sccAccess->transferPacket(0x00, PERIW, loopAddr, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT, BCMC);
      }
    }

    // (Re-)Configure driver
    sccAccess->setTxBaseAddr(perfApertureStart-PERF_SHM_OFFSET+0x200000); // Substract Performance meter offset and add SHM Host driver offset (0x200000)
    sccAccess->setSubnetSize(1);

    // Re-programm GRB registers, just in case that someone modified the correspondingm settings...
    sccAccess->programAllGrbRegisters();

    // Re-Init perfmeter...
    for (int loop = 0; loop < 64; loop++) perfStatus[loop] = 0xff;
    if (perfApertureStart) sccAccess->write32(perfRoute, perfDestid, perfApertureStart, perfStatus, 64, perfSIFPort, false);

    // Release resets...
    printInfo("Releasing resets...");
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,false)) {
            uiEnable(true, true, true, true, true);
            return;
          }
        }
      }
    }

    // Update buttons
    ui.actionBootLinux->setIcon(QPixmap(":/resources/menuIcons/linuxGreen.png"));
    ui.actionBootLinux->setToolTip("Linux is booted... Click this button to restart!");

    // That's it...
    printInfo("Linux has been started successfully. Cores should be reachable via TCP/IP shortly...");
    uiEnable(true, true, true, true, true);
    return;
  }

  // In case that we need to preload images for more than 12 cores, we simply preload all
  // memory locations (using the broadcasting feature) to limit the effort to 12 images...
  bool useBroadcasting = (linuxMask.getNumCoresEnabled()>12);

  // Update shm Routes and destinations...
  QMap<int,int> shmRoute;
  QMap<int,int> shmDestID;
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      if (shmSIFPort == SIF_RC_PORT) {
        shmRoute[TID(loop_x, loop_y)] = MCHTID_XY(loop_x,loop_y);
        shmDestID[TID(loop_x, loop_y)] = MCHPORT_XY(loop_x,loop_y);
      } else if ((shmSIFPort == SIF_MC_PORT) || (shmSIFPort == SIF_HOST_PORT)) {
        shmRoute[TID(loop_x, loop_y)] = 0x30;
        shmDestID[TID(loop_x, loop_y)] = PERIS;
      }
    }
  }

  // Create .mt file for the currently configured number of cores...
  printInfo("Creating .mt file \"",sccAccess->getTempPath(),"linux.mt\"...");
  QMap<int,int> MCSlotMap;
  MCSlotMap[0x00]=MCSlotMap[0x05]=MCSlotMap[0x20]=MCSlotMap[0x25]=MCSlotMap[0x30]=0;
  QFile file((sccAccess->getTempPath()+"linux.mt"));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    printError("Failed to open mt file \"",sccAccess->getTempPath(),"linux.mt\"... File is not writable. Aborting!");
    uiEnable(true, true, true, true, true);
    return;
  }
  QTextStream out(&file);
  out << "# pid mch-route mch-dest-id mch-offset-base testcase" << endl;
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (useBroadcasting || linuxMask.getEnabled(loop_x, loop_y, loop_core)) {
          out << messageHandler::toString(PID(loop_x, loop_y, loop_core),16,2) << " " << messageHandler::toString(shmRoute[TID(loop_x, loop_y)],16,2) << " " << messageHandler::toString(shmDestID[TID(loop_x, loop_y)],10,1) << " " << messageHandler::toString(MCSlotMap[shmRoute[TID(loop_x, loop_y)]]++,16,2) << " " << sccAccess->getTempPath()+"linux.obj" << endl;
        } else {
          MCSlotMap[shmRoute[TID(loop_x, loop_y)]]++; // Make sure that the cores are always at the same location...
        }
      }
    }
  }
  file.close();

  // Copy object to temp dir (faster local disc) if it is not yet there...
  QString dest = sccAccess->getTempPath() + "linux.obj";
  // Remove obj folder if image changed!
  QProcess* shell_cmd = new QProcess();
  shell_cmd->start("diff -q " + linuxImage + " " + dest);
  shell_cmd->waitForFinished(300000);
  // Copy if files are different
  if (!shell_cmd->exitCode()) {
    printInfo("Nothing to copy as ", linuxImage, " and ", dest, " don't differ...");
  } else {
    // Remove obj folder to make sure that the merged object will be re-built...
    shell_cmd->start("rm -rf " + sccAccess->getTempPath() + "obj");
    shell_cmd->waitForFinished(300000);
    // Finally copy object...
    shell_cmd->start("cp -f " + linuxImage + " " + dest);
    shell_cmd->waitForFinished(300000);
    if ((errMsg = shell_cmd->readAllStandardError()) != "") {
      printError("Failed to copy object to temp dir... Aborting!");
      printError("Copy command error message:\n", errMsg);
      delete shell_cmd;
      uiEnable(true, true, true, true, true);
      return;
    }
    printInfo("Copied object " , linuxImage, " to faster local disc (",sccAccess->getTempPath(),")...");
  }
  delete shell_cmd;

  // Merge objects
  printInfo("Merging objects with sccMerge:");
  QString memorySettings  = (QString)"-m " + QString::number(sccAccess->getSizeOfMC()) + " -n 12 ";
  QProcess* sccMerge = new QProcess();
  sccMerge->setWorkingDirectory(sccAccess->getTempPath());
  printInfo("-> sccMerge " + (QString)((useBroadcasting)?"-broadcast ":"") + memorySettings + "-noimage linux.mt");
  qApp->processEvents(); // Update log
  sccMerge->start(getSccKitPath() + "bin/sccMerge " + (QString)((useBroadcasting)?"-broadcast ":"") + memorySettings + "-noimage linux.mt");
  if (!sccMerge->waitForFinished(300000)) {
    printError("sccMerge timed out (more than five minutes)... Aborting!");
    sccMerge->terminate();
    if (!sccMerge->waitForFinished(1000)) {
      sccMerge->kill();
    }
    delete sccMerge;
    uiEnable(true, true, true, true, true);
    return;
  }
  if ((errMsg = sccMerge->readAllStandardError()) != "") {
    printError("Failed to process mt file \"",sccAccess->getTempPath(),"linux.mt\"... Aborting!");
    printError("sccMerge error message:\n", errMsg);
    delete sccMerge;
    uiEnable(true, true, true, true, true);
    return;
  }
  delete sccMerge;

  // Merge command-line patches
  if (cmdLine != "") {
    printInfo("Merging command-line patches...");
    QProcess* sccCmdline = new QProcess();
    sccCmdline->setWorkingDirectory(sccAccess->getTempPath());
    sccCmdline->start(getSccKitPath() + "bin/sccCmdline -m . \"" + cmdLine + "\"\n");
    if (!sccCmdline->waitForFinished(30000)) {
      printError("sccCmdline timed out (more than 30 seconds)... Aborting!");
      sccCmdline->terminate();
      if (!sccCmdline->waitForFinished(1000)) {
        sccCmdline->kill();
      }
      return;
    }
    if ((errMsg = sccCmdline->readAllStandardError()) != "") {
      printError("Failed to process kernel command-line parameters... Aborting!");
      printError("sccCmdline error message:\n", errMsg);
      return;
    }
    delete sccCmdline;
  }

  // Pull all resets and en-/ or disable L2s!
  printInfo("Pulling resets and ", ui.actionLinuxEnableL2->isChecked()?"en":"dis", "abling L2 caches: ", linuxMask.getMaskString());
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (linuxMask.getEnabled(loop_x,loop_y,loop_core)) {
          if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,true)) {
            uiEnable(true, true, true, true, true);
            return;
          }
          changeResetMask.setEnabled(loop_x, loop_y, loop_core, true);
          if (!sccAccess->setL2(TID(loop_x, loop_y),loop_core,ui.actionLinuxEnableL2->isChecked())) {
            uiEnable(true, true, true, true, true);
            return;
          }
          changeL2Mask.setEnabled(loop_x, loop_y, loop_core, ui.actionLinuxEnableL2->isChecked());
        }
      }
    }
  }
  qApp->processEvents(); // Update reset and L2 mask dialogs...

  // Clear MPBs
  if (ui.actionLinuxClearMPBs->isChecked()) {
    uInt64 nullData[4]={0,0,0,0};
    printInfo("Clearing MPBs: ", linuxMask.getMaskString());
    if (linuxMask.getNumCoresEnabled() == NUM_ROWS*NUM_COLS*NUM_CORES) {
      for (uInt64 loop_i=0; loop_i<(1<<14); loop_i+=0x20) {
        sccAccess->transferPacket(0x00, SUB_DESTID_MPB, loop_i, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT, BCTILE);
      }
    } else {
      for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
        for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
          for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
            if (linuxMask.getEnabled(loop_x,loop_y,loop_core)) {
              for (uInt64 loop_i=((loop_core)?(1<<13):0); loop_i<((loop_core)?(1<<14):(1<<13)); loop_i+=0x20) {
                sccAccess->transferPacket(TID(loop_x, loop_y), SUB_DESTID_MPB, loop_i, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT);
              }
            }
          }
        }
      }
    }
  }

  // Load objects
  printInfo("Preloading Memory with object file...");
  qApp->processEvents(); // Update log
  int retVal;
  if (useBroadcasting) {
    retVal = sccAccess->writeMemFromOBJ(0x00, PERIW, sccAccess->getTempPath()+"obj/mch_0_0.32.obj", &pd, shmSIFPort, BCMC);
    if (cmdLine != "") {
      QFileInfo tmpInfo = sccAccess->getTempPath()+"cmdline/mch_0_0.32.obj";
      if(tmpInfo.isReadable()) {
        printInfo("Patching kernel command-line...");
        retVal += sccAccess->writeMemFromOBJ(0x00, PERIW, sccAccess->getTempPath()+"cmdline/mch_0_0.32.obj", &pd, shmSIFPort, BCMC);
      }
    }
  } else {
    retVal = sccAccess->loadOBJ(sccAccess->getTempPath()+"obj/mch_0_0.32.obj", &pd, shmSIFPort);
    if (cmdLine != "") {
      QFileInfo tmpInfo = sccAccess->getTempPath()+"cmdline/mch_0_0.32.obj";
      if(tmpInfo.isReadable()) {
        printInfo("Patching kernel command-line...");
        retVal += sccAccess->loadOBJ(sccAccess->getTempPath()+"cmdline/mch_0_0.32.obj", &pd, shmSIFPort);
      }
    }
  }

  if (retVal != SUCCESS) {
    if (retVal == ERROR) printWarning("bootLinux(): Unable to preload memory... Aborting!");
    ui.actionBootLinux->setIcon(QPixmap(":/resources/menuIcons/linuxBlue.png"));
    ui.actionBootLinux->setToolTip("Linux not booted! Click icon to start booting...");
    qApp->processEvents();
    uiEnable(true, true, true, true, true);
    return;
  }

  // Load LUTs...
  printInfo("Preloading LUTs...");
  if (!sccAccess->WriteFlitsFromFile((sccAccess->getTempPath()+"obj/lut_init.dat"), "LUTs")) {
    uiEnable(true, true, true, true, true);
    return;
  }

  // Find out SHM location
  uInt64 tmp = linuxMask.getFirstActiveCore();
  tmp = sccAccess->readFlit(TID_PID(tmp), CRB, MC_SHMBASE_MC0+((Z_PID(tmp))?LUT1:LUT0));
  settings->setValue("perfMeter/Route", perfRoute = (tmp&0x01fe000)>>13);
  settings->setValue("perfMeter/Destid", perfDestid = (tmp&0x01c00)>>10);
  settings->setValue("perfMeter/SIFPort", perfSIFPort = shmSIFPort);
  uInt64 oldApertureStart = settings->value("perfMeter/ApertureStart", 0).toULongLong();
  perfApertureStart = ((tmp&0x3ff)<<24)+PERF_SHM_OFFSET;
  if ((perfSIFPort==SIF_MC_PORT) && (perfApertureStart >= 0x180000000ull)) perfApertureStart -= 0x180000000ull;
  settings->setValue("perfMeter/ApertureStart", perfApertureStart);

  // In case of new SHM-COM pointer area, re-initialize it...
  if (oldApertureStart != perfApertureStart) {
    // Initialize SHM-COM pointer area with all zeros...
    uInt64 nullData[4]={0,0,0,0};
    for (uInt64 loopAddr=perfApertureStart-PERF_SHM_OFFSET; loopAddr < (perfApertureStart-PERF_SHM_OFFSET+0x1000); loopAddr+=0x20) {
      sccAccess->transferPacket(0x00, PERIW, loopAddr, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT, BCMC);
    }
  }

  // (Re-)Configure driver
  sccAccess->setTxBaseAddr(perfApertureStart-PERF_SHM_OFFSET+0x200000); // Substract Performance meter offset and add SHM Host driver offset (0x200000)
  sccAccess->setSubnetSize(1);

  // Re-programm GRB registers, just in case that someone modified the correspondingm settings...
  sccAccess->programAllGrbRegisters();

  // Re-Init perfmeter...
  for (int loop = 0; loop < 64; loop++) perfStatus[loop] = 0xff;
  if (perfApertureStart) sccAccess->write32(perfRoute, perfDestid, perfApertureStart, perfStatus, 64, perfSIFPort, false);

  // Release resets...
  printInfo("Releasing resets...");
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (linuxMask.getEnabled(loop_x,loop_y,loop_core)) {
          if (!sccAccess->toggleReset(TID(loop_x, loop_y),loop_core)) {
            uiEnable(true, true, true, true, true);
            return;
          }
          changeResetMask.setEnabled(loop_x, loop_y, loop_core, false);
        }
      }
    }
  }
  qApp->processEvents(); // Update reset dialog...

  // Update buttons
  ui.actionBootLinux->setIcon(QPixmap(":/resources/menuIcons/linuxGreen.png"));
  ui.actionBootLinux->setToolTip("Linux is booted... Click this button to restart!");

  // Set attributes that will be used by performance meter (if invoked by user)...
  startPerfmeterPolling(false); // Only starts polling when window is open...

  // That's it...
  printInfo("Linux has been started successfully. Cores should be reachable via TCP/IP shortly...");
  uiEnable(true, true, true, true, true);
  return;
}

void sccGui::slotPreloadObject() {
  QString tmpName;
  QFileInfo tmpInfo;

  tmpName = QFileDialog::getOpenFileName(
             mW,
             "Choose a new object...",
             genericObject,
             "Objects (*.obj)" );

  // Cancel in case of user abort
  if (tmpName == "") return;

  // Check file and update settings
  tmpInfo = tmpName;
  if (tmpInfo.isReadable()) {
    genericObject = tmpName;
    settings->setValue("genericBoot/genericObject", genericObject);
    printInfo("Loading object \"", genericObject, "\"...");
    sccAccess->loadOBJ(genericObject);
  } else {
    printWarning("Failed to use object name \"", tmpName, "\" (File doesn't exist or is not readable)...");
    return;
  }
}

void sccGui::slotPreloadLut() {
  QString tmpName;
  QFileInfo tmpInfo;

  tmpName = QFileDialog::getOpenFileName(
             mW,
             "Choose a new LUT data file...",
             genericLut,
             "LUT data files (*.dat)" );

  // Cancel in case of user abort
  if (tmpName == "") return;

  // Check file and update settings
  tmpInfo = tmpName;
  if (tmpInfo.isReadable()) {
    genericLut = tmpName;
    settings->setValue("genericBoot/genericLut", genericLut);
    sccAccess->WriteFlitsFromFile(genericLut, "Flits", NULL, SIF_RC_PORT, false);
  } else {
    printWarning("Failed to use LUT name \"", tmpName, "\" (File doesn't exist or is not readable)...");
    return;
  }
}

void sccGui::slotClearMPBs() {
  uInt64 nullData[4]={0,0,0,0};
  if (!clearMPBMask.selectionDialog("Clear message passing buffers...", "Checked box means that MPB of that core will be cleaned!")) return;
  printInfo("Clearing MPBs: ", clearMPBMask.getMaskString());
  if (clearMPBMask.getNumCoresEnabled() == NUM_ROWS*NUM_COLS*NUM_CORES) {
    qApp->processEvents();
    for (uInt64 loop_i=0; loop_i<(1<<14); loop_i+=0x20) {
      sccAccess->transferPacket(0x00, SUB_DESTID_MPB, loop_i, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT, BCTILE);
    }
  } else {
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          if (clearMPBMask.getEnabled(loop_x,loop_y,loop_core)) {
            for (uInt64 loop_i=((loop_core)?(1<<13):0); loop_i<((loop_core)?(1<<14):(1<<13)); loop_i+=0x20) {
              sccAccess->transferPacket(TID(loop_x, loop_y), SUB_DESTID_MPB, loop_i, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT);
            }
          }
        }
      }
    }
  }
}

void sccGui::slotChangeL2Enables() {
  bool tmp;
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (!sccAccess->getL2(TID(loop_x, loop_y), loop_core, &tmp)) {
          printError("Unable to read L2 state of SCC... Aborting L2 operation...");
          return;
        }
        changeL2Mask.setEnabled(loop_x, loop_y, loop_core, tmp);
      }
    }
  }
  if (!changeL2Mask.selectionDialog("Change selected L2 caches...", "Checked box means that the L2 of that core will be enabled!")) return;
  printInfo("Activating L2 caches: ", changeL2Mask.getMaskString());
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (!sccAccess->setL2(TID(loop_x, loop_y),loop_core, changeL2Mask.getEnabled(loop_x,loop_y,loop_core))) return;
      }
    }
  }
}

void sccGui::slotChangeResets() {
  bool tmp;
  char currPerfStatus[64];
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (!sccAccess->getReset(TID(loop_x, loop_y), loop_core, &tmp)) {
          printError("Unable to read reset state of SCC... Aborting reset operation...");
          return;
        }
        changeResetMask.setEnabled(loop_x, loop_y, loop_core, tmp);
      }
    }
  }
  if (!changeResetMask.selectionDialog("Change selected resets...", "Checked box means that the reset of that core active!")) return;
  // Update perfmeter, if polling is enabled...
  if (perfPollingTimer && perfApertureStart) {
    sccAccess->read32(perfRoute, perfDestid, perfApertureStart, currPerfStatus, 64, perfSIFPort, false);
  }
  printInfo("Pulling reset signal of cores: ", changeResetMask.getMaskString());
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core, changeResetMask.getEnabled(loop_x,loop_y,loop_core))) return;
        if (changeResetMask.getEnabled(loop_x,loop_y,loop_core)) currPerfStatus[PID(loop_x,loop_y,loop_core)] = 0xff; // gray perf value
      }
    }
  }
  // Update perfmeter, if polling is enabled...
  if (perfPollingTimer && perfApertureStart) {
    sccAccess->write32(perfRoute, perfDestid, perfApertureStart, currPerfStatus, 64, perfSIFPort, false);
  }
}

// #define PATTERNSIZE 0x08000ul
#define PATTERNSIZE 2097152
unsigned char *testPattern = NULL;
unsigned char *nullPattern = NULL;
unsigned char *compPattern = NULL;
void sccGui::slotTestMemory() {
  // Allocate memory, if not yet done...
  if (!testPattern) {
    testPattern = (unsigned char *) malloc(PATTERNSIZE);
    nullPattern = (unsigned char *) malloc(PATTERNSIZE);
    compPattern = (unsigned char *) malloc(PATTERNSIZE);
    if (!testPattern || !nullPattern || !compPattern) {
      printError("Failed to allocate memory for DDR3 memory test...");
      return;
    }
  }
  // Initialize random test-pattern...
  struct timeval current;
  gettimeofday(&current, NULL);
  srand(current.tv_sec);
  for (uInt32 addr=0; addr < PATTERNSIZE; addr+=1) {
    testPattern[addr] = rand()%256;
    nullPattern[addr] = 0x00;
  }
  // Make sure that all cores are in reset!
  slotWischer();
  // Now start testing...
  printfInfo("Sweeping through memory controllers (%0dGB per MC):", sccAccess->getSizeOfMC());
  int mcRoute[4] = {0x00, 0x05, 0x20, 0x25};
  int mcDest[4] = {PERIW, PERIE, PERIW, PERIE};
  bool abort = false;
  int errCnt = 0;
  for (int loopMC=0; (loopMC < 4) && !abort; loopMC++) {
    QString msg = "Checking MC " + messageHandler::toString(mcRoute[loopMC], 16, 2) + "-" + sccAccess->decodeDestID(mcDest[loopMC],false) + "...";
    sccProgressBar *pd = new sccProgressBar();
    uInt64 progress = 0;
    pd->setLabelText(msg);
    pd->setWindowTitle("Progress");
    pd->setMinimum(0);
    pd->setMaximum(((sccAccess->getSizeOfMC()*SIZE_1GB)-PATTERNSIZE)/4);
    pd->setCancelButtonText("Abort");
    pd->setModal(true);
    pd->show();
    // Write pattern
    printInfo(msg.replace("Checking", "Preparing").replace("...", " by writing random pattern..."));
    qApp->processEvents();
    for (uInt64 loopAddr=0; (loopAddr < sccAccess->getSizeOfMC()*SIZE_1GB) && !abort; loopAddr+=PATTERNSIZE) {
      if (sccAccess->write32(mcRoute[loopMC], mcDest[loopMC], loopAddr, (char*)testPattern, PATTERNSIZE, SIF_RC_PORT) != PATTERNSIZE) {
        printError("Aborting because of error during DMA pattern write access (refer to log messages above for details)!" );
        abort = true;
      }
      // Update progress bar
      if (!(loopAddr % 3*PATTERNSIZE)) {
        pd->setValue((progress+=PATTERNSIZE)/4);
      }
      qApp->processEvents();
      if (pd->wasCanceled()) {
        printInfo(msg = "Action canceled by user... Stopping DDR3 memory check!");
        abort = true;
      }
    }
    // Read pattern and initialize with 0x00...
    if (!abort) {
      printInfo(msg.replace("Preparing", "Checking patterns and finally initializing").replace(" by writing random pattern...", " with all-zero..."));
      qApp->processEvents();
      for (uInt64 loopAddr=0; (loopAddr < sccAccess->getSizeOfMC()*SIZE_1GB) && !abort; loopAddr+=PATTERNSIZE) {
        if (sccAccess->read32(mcRoute[loopMC], mcDest[loopMC], loopAddr, (char*)compPattern, PATTERNSIZE, SIF_RC_PORT) != PATTERNSIZE) {
          printError("Aborting because of error during DMA read access (refer to log messages above for details)!" );
          abort = true;
        } else if (sccAccess->write32(mcRoute[loopMC], mcDest[loopMC], loopAddr, (char*)nullPattern, PATTERNSIZE, SIF_RC_PORT) != PATTERNSIZE) {
          printError("Aborting because of error during DMA zero write access (refer to log messages above for details)!" );
          abort = true;
        }
        // Check...
        for (uInt32 addr=0; (addr < PATTERNSIZE) && !abort; addr+=1) {
          if (testPattern[addr] != compPattern[addr]) {
            // Find out on wich DDR3 DIMM slot the error occured...
            // 0x00      0xxxxxxxxxx   J5
            // 0x00      1xxxxxxxxxx   J4
            // 0x05      0xxxxxxxxxx   J9
            // 0x05      1xxxxxxxxxx   J8
            // 0x20      0xxxxxxxxxx   J3
            // 0x20      1xxxxxxxxxx   J2
            // 0x25      0xxxxxxxxxx   J7
            // 0x25      1xxxxxxxxxx   J6
            char dimSlot = ((mcRoute[loopMC]==0x00)?4:(mcRoute[loopMC]==0x05)?8:(mcRoute[loopMC]==0x20)?2:6) + (((loopAddr+addr) < ((sccAccess->getSizeOfMC()*SIZE_1GB)/2))?1:0);
            printError("Found error on DIMM J", QString::number(dimSlot), " (address ", messageHandler::toString(loopAddr+addr, 16, 9), " on MC ", messageHandler::toString(mcRoute[loopMC], 16, 2), "-", sccAccess->decodeDestID(mcDest[loopMC],false), ") -> Received ", messageHandler::toString(compPattern[addr],16,2), " but expected ", messageHandler::toString(testPattern[addr],16,2), "!" );
            errCnt++;
          }
          if (errCnt >= 100) {
            printError("Found more than 100 errors! Aborting..." );
            abort = true;
          }
        }
        // Update progress bar
        if (!(loopAddr % 3*PATTERNSIZE)) {
          pd->setValue((progress+=2*PATTERNSIZE)/4);
        }
        qApp->processEvents();
        if (pd->wasCanceled()) {
          printInfo(msg = "Action canceled by user... Stopping DDR3 memory check!");
          abort = true;
        }
      }
    }
    pd->hide();
    delete pd;
  }
  // Success message
  if (!errCnt && !abort) {
    printInfo("Success! All DDR3 memories have been verified...");
    printInfo("The whole address range is initialized with \"all zero\"!");
  }
}

void sccGui::slotSetLinuxGeometry() {
  bool okay;
  QString message = (QString)"\
This dialog allows you to select the screen resolution\n\
of the Linux image. It will be used by the virtual display\n\
driver. The default resolution is 640x480 - 16 bit!";
  QStringList settings;
  settings << "640x480 - 16bit" << "640x480 - 32bit"
           << "800x600 - 16bit" << "800x600 - 32bit"
           << "1024x768 - 16bit" << "1024x768 - 32bit"
           << "1280x1024 - 16bit" << "1280x1024 - 32bit";
  int currentSetting = 0;

  // Sanity check...
  if (changeResetMask.getNumCoresEnabled() != TOTAL_CORES) {
    printWarning("Sorry, the resolution can only be changed while all cores are in reset.\n         Please apply Software SCC reset in Tools menu and try again...");
    return;
  }

  // Dialog...
  for (int i = 0; i < settings.size(); i++) {
    if (settings[i] == QString::number(sccAccess->getDisplayX())+"x"+QString::number(sccAccess->getDisplayY())+" - "+QString::number(sccAccess->getDisplayDepth())+"bit") {
      currentSetting = i;
    }
  }
  QString selection = QInputDialog::getItem(mW, "Set Linux screen resolution...", message, settings, currentSetting, false, &okay);
  if (okay) {
    if (selection == QString::number(sccAccess->getDisplayX())+"x"+QString::number(sccAccess->getDisplayY())+" - "+QString::number(sccAccess->getDisplayDepth())+"bit") {
      printInfo("Keeping current resolution (", selection, ")...");
      return;
    }
    QString tmp = selection;
    int x = tmp.replace(QRegExp("([0-9]+)x[0-9]+ - [0-9]+bit"), QString("\\1")).toInt(&okay);
    tmp = selection;
    int y = tmp.replace(QRegExp("[0-9]+x([0-9]+) - [0-9]+bit"), QString("\\1")).toInt(&okay);
    tmp = selection;
    int depth =  tmp.replace(QRegExp("[0-9]+x[0-9]+ - ([0-9]+)bit"), QString("\\1")).toInt(&okay);
    sccAccess->setDisplayGeometry(x,y,depth);
    printInfo("New resolution is ", selection, "!");
  }
}

void sccGui::slotGetBoardStatus() {
  QString tmpString = sccAccess->executeBMCCommand("status");
  if (tmpString != "") {
    printInfo("Status result of BMC server ", sccAccess->getBMCServer() , ":\n", tmpString);
  }
}

void sccGui::slotResetPlatform() {
  if (sccAccess->getPlatform() == "Copperridge") {
    printInfo("Resetting Copperridge Board: ", sccAccess->executeBMCCommand("board_reset"));
  } else {
    printInfo("Resetting Rocky Lake SCC device: ", sccAccess->executeBMCCommand("reset scc"));
    printInfo("Resetting Rocky Lake FPGA: ", sccAccess->executeBMCCommand("reset fpga"));
  }
  printInfo("Re-Initializing PCIe driver..."); sccAccess->reInitDriver();
}

void sccGui::slotCustomBmcCmd() {
  bool ok;
  QString command = QInputDialog::getText(this, tr("Custom BMC command"),
                                            "Please specify BMC command to be executed on "+sccAccess->getBMCServer()+":", QLineEdit::Normal,
                                            "status", &ok);
  if (ok) {
    if (command != "") {
      printInfo("Executing custom BMC command \"", command,"\":\n", sccAccess->executeBMCCommand(command));
    } else {
      printWarning("No custom BMC command specified (\"\")... Ignoring request.");
    }
  }
}

void sccGui::slotInitializePlatform(bool checked) {
  // User abort:
  if (!checked) {
    userAbort = true;
    printInfo("Initiating user abort... Please be patient!");
    sccAccess->abortTraining();
    if (InitializePlatformPd.isVisible()) {
      InitializePlatformPd.cancel();
    }
    return;
  }
  // Disable other UI elements...
  uiEnable(false, false, true, false, false);
  // Training request:
  bool okay;
  int defaultItem = 0;
  QFileInfoList settingFiles;
  QStringList settingNames;
  QMap <int, int> settingFrequency;
  QDir dir(getSccKitPath() + "settings");
  if (dir.exists()) {
    settingFiles = dir.entryInfoList((QStringList)("*setting."+sccAccess->platformSetupSuffix()));
    for (int i = 0; i < settingFiles.size(); i++) {
      if (QFile::exists(settingFiles[i].absoluteFilePath().replace("_setting.", "_preset."))) {
        settingNames << settingFiles[i].fileName().replace(QRegExp("_setting[.]..."), "");
        if (settingNames[i].toLower() == "norm") {
          settingFrequency[i] = 50;
        } else if (settingNames[i].toLower() == "norm_plus") {
          settingFrequency[i] = 266;
          defaultItem = i; // Use NORM_plus as default setting...
        } else if (settingNames[i].toLower() == "demo") {
          settingFrequency[i] = 266;
        } else {
          settingFrequency[i] = 0; // Unknown...
        }
      }
    }
  }
  if (!settingNames.size()) {
    printError("Didn't find settings in folder \"", dir.path(), "\"... Aborting!");
    ui.actionInitializePlatform->setChecked(false);
    uiEnable(true, true, true, true, true);
    return;
  }
  QString message = (QString)"\
This dialog allows you to (re-)initialize the SCC platform.\n\
This means that the reset of the actual SCC device is triggered and\n\
that all clock settings are programmed from scratch. The (re-)\n\
programming of the clock settings also implies a training of the\n\
physical System Interface!\n\n\
Short said: The whole procedure takes a while and you should only do\n\
it when necessary! This step is NOT always required after starting\n\
the GUI. You would normally invoke it when the system reset executes\n\
with errors or when the board has just been powered up...\n\n\
Please select from the following possibilities:";
  QString tmpSelection = QInputDialog::getItem(mW, "(Re-)initialize platform...", message, settingNames, defaultItem, false, &okay);
  int selection = 0;
  for (int i = 0; i < settingFiles.size(); i++) {
    if (settingNames[i] == tmpSelection) {
      selection = i;
    }
  }
  if (okay) {
    userAbort = false;
    // Close konsole & perfmeter (if open)
    ui.actionBootLinux->setIcon(QPixmap(":/resources/menuIcons/linuxBlue.png"));
    ui.actionBootLinux->setToolTip("Linux not booted! Click icon to start booting...");
    slotClosekScreen(0);
    stopPerfmeterPolling(true);
    printInfo("=====================================================");
    printInfo("Starting system initialization (with setting ", settingNames[selection], ")...");
    printInfo("Training may be aborted by pressing the SIF training button again during training.");
    InitializePlatformPd.reset();
    sccAccess->processRCCFile(settingFiles[selection].absoluteFilePath().replace("_setting.", "_preset."), true, ECHO_OFF_RW, &InitializePlatformPd);
    if (userAbort || InitializePlatformPd.wasCanceled()) {
      printWarning("Training aborted by user! SIF is NOT trained...");
      ui.actionInitializePlatform->setChecked(false);
      uiEnable(true, true, true, true, true);
      return;
    }
    printInfo("Training System Interface:");
    if (!sccAccess->trainSIF(AUTO_MODE, settingFrequency[selection])) {
      if (userAbort) {
        printWarning("Training aborted by user! SIF is NOT trained...");
        ui.actionInitializePlatform->setChecked(false);
        uiEnable(true, true, true, true, true);
        return;
      }
      printInfo("Configuring memory controllers:");
      InitializePlatformPd.reset();
      QString retval = sccAccess->processRCCFile(settingFiles[selection].absoluteFilePath(), true, ECHO_OFF_RW, &InitializePlatformPd);
      if (retval.contains("MEMRD ERROR")) {
        printWarning("Configuration of memory controllers failed... Trying again.");
        InitializePlatformPd.reset();
        retval = sccAccess->processRCCFile(settingFiles[selection].absoluteFilePath(), true, ECHO_OFF_RW, &InitializePlatformPd);
        if (retval.contains("MEMRD ERROR")) {
          printError("Configuration of memory controllers failed twice... Please refer to messages above for details...");
        }
      }
      if (retval.contains("User pressed \"Cancel\"... Aborting!")) {
        printWarning("Training aborted by user! DDR3 memory is NOT trained...");
        ui.actionInitializePlatform->setChecked(false);
        uiEnable(true, true, true, true, true);
        return;
      }
    } else {
      ui.actionInitializePlatform->setChecked(false);
      uiEnable(true, true, true, true, true);
      return;
    }
    printInfo("System initialization done.");
    printInfo("=====================================================");
    // Re-programm GRB registers, just in case that someone modified the correspondingm settings...
    sccAccess->programAllGrbRegisters();
    if (initDDR3) {
      printInfo("Initializing DDR3 memories with all zero...");
      // Initialize SHM-COM pointer area with all zeros...
      uInt64 nullData[4]={0,0,0,0};
      InitializePlatformPd.reset();
      InitializePlatformPd.setLabelText("Initializing DDR3 memory with all zero...");
      InitializePlatformPd.setWindowTitle("Progress");
      InitializePlatformPd.setMinimum(0);
      InitializePlatformPd.setMaximum( (SIZE_1GB/0x2000)*sccAccess->getSizeOfMC());
      InitializePlatformPd.setCancelButtonText("Abort");
      InitializePlatformPd.show();
      perfApertureStart = settings->value("perfMeter/ApertureStart", 0).toULongLong();
      for (uInt64 loopAddr=0; loopAddr < SIZE_1GB*sccAccess->getSizeOfMC(); loopAddr+=0x20) {
        sccAccess->transferPacket(0x00, PERIW, loopAddr, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT, BCMC);
        if (!(loopAddr%0x2000)) {
          InitializePlatformPd.setValue(loopAddr/0x2000);
          if (InitializePlatformPd.wasCanceled()) {
            printWarning("DDR3 initialization has been aborted!");
            InitializePlatformPd.reset();
            InitializePlatformPd.hide();
            printInfo("=====================================================");
            ui.actionInitializePlatform->setChecked(false);
            uiEnable(true, true, true, true, true);
            return;
          }
        }
      }
      printInfo("=====================================================");
    } else {
      // Initialize SHM-COM pointer area with all zeros...
      uInt64 nullData[4]={0,0,0,0};
      perfApertureStart = settings->value("perfMeter/ApertureStart", 0).toULongLong();
      for (uInt64 loopAddr=perfApertureStart-PERF_SHM_OFFSET; loopAddr < (perfApertureStart-PERF_SHM_OFFSET+0x1000); loopAddr+=0x20) {
        sccAccess->transferPacket(0x00, PERIW, loopAddr, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT, BCMC);
      }
    }
  }
  ui.actionInitializePlatform->setChecked(false);
  uiEnable(true, true, true, true, true);
}

void sccGui::slotExecuteRccFile() {
  QString tmpName = getSccKitPath() + "resources/status.rcc";
  QFileInfo tmpInfo;

  tmpName = QFileDialog::getOpenFileName(
             mW,
             "Choose a new BMC configuration file...",
             tmpName,
             "BMC configuration files (*.rcc *." + sccAccess->platformSetupSuffix() + ");;Generic BMC configuration file (*.rcc);;Platform specific BMC configuration file (*." + sccAccess->platformSetupSuffix() + ")" );

  // Cancel in case of user abort
  if (tmpName == "") return;

  // Check file and update settings
  tmpInfo = tmpName;
  if (tmpInfo.isReadable()) {
    sccAccess->processRCCFile(tmpName, true);
  } else {
    printWarning("Failed to execute BMC configuration file \"", tmpName, "\" (File doesn't exist or is not readable)...");
    return;
  }
}

void sccGui::slotTrainSystemInterface() {
  sccAccess->trainSIF(AUTO_MODE, 000);
}

//This method is called from constructor
void  sccGui::startFlitWidget() {
  FlitWidget= new flitWidget(sccAccess);
  FlitWidget->Radix = 16;

  // React when the widget gets closed by user...
  connect(FlitWidget, SIGNAL(widgetClosing()), this, SLOT(slotFlitWidget()));

  // Connect everything but doneButton manually
  connect(FlitWidget->ui.subDestIDBox, SIGNAL(currentIndexChanged(const QString &)), FlitWidget, SLOT(subDestIDBoxChanged( const QString &)));
  connect(FlitWidget->ui.addressBox, SIGNAL(currentIndexChanged(const QString &)), FlitWidget, SLOT(addressBoxChanged( const QString &)));
  connect(FlitWidget->ui.addressEdit, SIGNAL(returnPressed()), FlitWidget, SLOT(read()));
  connect(FlitWidget->ui.dataEdit, SIGNAL(returnPressed()), FlitWidget, SLOT(read()));
  connect(FlitWidget->ui.radioButtonHex, SIGNAL(clicked()), FlitWidget, SLOT(HexClicked()));
  connect(FlitWidget->ui.radioButtonBin, SIGNAL(clicked()), FlitWidget, SLOT(BinClicked()));
  connect(FlitWidget->ui.readButton, SIGNAL(clicked()), FlitWidget, SLOT(read()));
  connect(FlitWidget->ui.writeButton, SIGNAL(clicked()), FlitWidget, SLOT(write()));

  // Toggle ui.addressBox entry to update tooltip
  FlitWidget->ui.addressBox->setCurrentIndex(9);
  FlitWidget->ui.addressBox->setCurrentIndex(0);
}

//This method is called from destructor
void  sccGui::stopFlitWidget() {
  if (FlitWidget) {
    delete FlitWidget;
    FlitWidget = NULL;
  }
}

// This slot will be called by widget action...
void sccGui::slotFlitWidget() {
  if (FlitWidget->isVisible()) {
    FlitWidget->hide();
    FlitWidget->setGeometry(FlitWidget->geometry()); // Save position for user convenience...
    ui.actionFlitWidget->setToolTip("Open flit widget...");
    ui.actionFlitWidget->setChecked(false);
  } else {
    FlitWidget->show();
    FlitWidget->raise();
    FlitWidget->activateWindow();
    ui.actionFlitWidget->setToolTip("Close flit widget...");
    ui.actionFlitWidget->setChecked(true);
  }
}

//This method is called from constructor
void sccGui::startPacketWidget() {
  PacketWidget= new packetWidget(sccAccess);
  PacketWidget->Radix = 16;

  // React when the widget gets closed by user...
  connect(PacketWidget, SIGNAL(widgetClosing()), this, SLOT(slotPacketWidget()));

  // Connect everything but doneButton manually
  connect(PacketWidget->ui.subDestIDBox, SIGNAL(currentIndexChanged(const QString &)), PacketWidget, SLOT(subDestIDBoxChanged( const QString &)));
  connect(PacketWidget->ui.addressBox, SIGNAL(currentIndexChanged(const QString &)), PacketWidget, SLOT(addressBoxChanged( const QString &)));
  connect(PacketWidget->ui.addressEdit, SIGNAL(returnPressed()), PacketWidget, SLOT(transfer()));
  connect(PacketWidget->ui.dataEdit1, SIGNAL(returnPressed()), PacketWidget, SLOT(transfer()));
  connect(PacketWidget->ui.dataEdit2, SIGNAL(returnPressed()), PacketWidget, SLOT(transfer()));
  connect(PacketWidget->ui.dataEdit3, SIGNAL(returnPressed()), PacketWidget, SLOT(transfer()));
  connect(PacketWidget->ui.dataEdit4, SIGNAL(returnPressed()), PacketWidget, SLOT(transfer()));
  connect(PacketWidget->ui.byteEnEdit, SIGNAL(returnPressed()), PacketWidget, SLOT(transfer()));
  connect(PacketWidget->ui.SIFSubIDBox, SIGNAL(currentIndexChanged(const QString &)), PacketWidget, SLOT(SIFSubIDBoxChanged( const QString &)));
  connect(PacketWidget->ui.radioButtonHex, SIGNAL(clicked()), PacketWidget, SLOT(HexClicked()));
  connect(PacketWidget->ui.radioButtonBin, SIGNAL(clicked()), PacketWidget, SLOT(BinClicked()));
  connect(PacketWidget->ui.txButton, SIGNAL(clicked()), PacketWidget, SLOT(transfer()));

  // Use RC_PORT as default port
  if (sccAccess->getPlatform() == "Copperridge") {
    PacketWidget->ui.SIFSubIDBox->insertItems(0, QStringList() << "RC_PORT" << "HOST_PORT" << "GRB_PORT" << "MC_PORT"); // Invokes subDestChanged implicitly
  } else {
    PacketWidget->ui.SIFSubIDBox->insertItems(0, QStringList() << "RC_PORT" << "HOST_PORT" << "GRB_PORT" << "SATA_PORT"); // Invokes subDestChanged implicitly
  }
}

//This method is called from destructor
void  sccGui::stopPacketWidget() {
  if (PacketWidget) {
    delete PacketWidget;
    PacketWidget = NULL;
  }
}

// This slot will be called by widget action...
void sccGui::slotPacketWidget() {
  if (PacketWidget->isVisible()) {
    PacketWidget->hide();
    PacketWidget->setGeometry(PacketWidget->geometry()); // Save position for user convenience...
    ui.actionPacketWidget->setToolTip("Open packet widget...");
    ui.actionPacketWidget->setChecked(false);
  } else {
    PacketWidget->show();
    PacketWidget->raise();
    PacketWidget->activateWindow();
    ui.actionPacketWidget->setToolTip("Close packet widget...");
    ui.actionPacketWidget->setChecked(true);
  }
}

//This method is called from constructor
void  sccGui::startReadMemWidget() {
  ReadMemWidget= new readMemWidget(sccAccess);
  ReadMemWidget->ui.rB_32bit->setChecked(true);

  // React when the widget gets closed by user...
  connect(ReadMemWidget, SIGNAL(widgetClosing()), this, SLOT(slotReadMemWidget()));

  ReadMemWidget->ui.memWindow->setFont( getAppFont(1, 14) );
  connect(ReadMemWidget->ui.pB_Close, SIGNAL(clicked()), ReadMemWidget, SLOT(closeWidget()));
  connect(ReadMemWidget->ui.pB_Reload, SIGNAL(clicked()), ReadMemWidget, SLOT(readMemSlot()));
  connect(ReadMemWidget->ui.pB_SaveAs, SIGNAL(clicked()), ReadMemWidget, SLOT(readAndSaveMemSlot()));
  connect(ReadMemWidget->ui.lE_StartAddr, SIGNAL(returnPressed()), ReadMemWidget, SLOT(readMemSlot()));
  connect(ReadMemWidget->ui.lE_numBytes, SIGNAL(returnPressed()), ReadMemWidget, SLOT(readMemSlot()));
  connect(ReadMemWidget->ui.rB_8bit, SIGNAL(clicked()), ReadMemWidget, SLOT(displayAligned()));
  connect(ReadMemWidget->ui.rB_16bit, SIGNAL(clicked()), ReadMemWidget, SLOT(displayAligned()));
  connect(ReadMemWidget->ui.rB_64bit, SIGNAL(clicked()), ReadMemWidget, SLOT(displayAligned()));
  connect(ReadMemWidget->ui.rB_32bit, SIGNAL(clicked()), ReadMemWidget, SLOT(displayAligned()));
  connect(ReadMemWidget->ui.cB_TileID, SIGNAL(currentIndexChanged(const QString &)), ReadMemWidget, SLOT(TileIDChanged( const QString &)));
  connect(ReadMemWidget->ui.cB_SubDest, SIGNAL(currentIndexChanged(const QString &)), ReadMemWidget, SLOT(subDestChanged( const QString &)));

  // GUI setup
  if (sccAccess->getPlatform() == "Copperridge") {
    ReadMemWidget->ui.cB_SubDest->insertItems(0, QStringList() << "MC" << "MPB" << "CRB" << "DDR2"); // Invokes subDestChanged implicitly
  } else {
    ReadMemWidget->ui.cB_SubDest->insertItems(0, QStringList() << "MC" << "MPB" << "CRB"); // Invokes subDestChanged implicitly
  }
  ReadMemWidget->ui.lE_numBytes->setText("256");
  ReadMemWidget->ui.lE_StartAddr->setText("00000000");
  ReadMemWidget->ui.lE_StartAddr->setFocus();
  ReadMemWidget->ui.memWindow->append("\nPlease enter the physical memory address ");
}

//This method is called from destructor
void  sccGui::stopReadMemWidget() {
  if (ReadMemWidget) {
    delete ReadMemWidget;
    ReadMemWidget = NULL;
  }
}

// This slot will be called by widget action...
void sccGui::slotReadMemWidget() {
  if (ReadMemWidget->isVisible()) {
    ReadMemWidget->hide();
    ReadMemWidget->setGeometry(ReadMemWidget->geometry()); // Save position for user convenience...
    ui.actionReadMemWidget->setToolTip("Open memory reader widget...");
    ui.actionReadMemWidget->setChecked(false);
  } else {
    ReadMemWidget->show();
    ReadMemWidget->raise();
    ReadMemWidget->activateWindow();
    ui.actionReadMemWidget->setToolTip("Close memory reader widget...");
    ui.actionReadMemWidget->setChecked(true);
  }
}

//This method is called from constructor
void sccGui::startPerfmeterWidget(int mesh_x, int mesh_y, int tile_cores) {
  PerfmeterWidget = new perfmeterWidget(mesh_x, mesh_y, tile_cores, sccAccess);
  perfPollingTimer = NULL;
  perfRoute = settings->value("perfMeter/Route", 0x00).toInt();
  perfDestid = settings->value("perfMeter/Destid", 0).toInt();
  perfSIFPort = settings->value("perfMeter/SIFPort", 0).toInt();
  perfApertureStart = settings->value("perfMeter/ApertureStart", 0).toULongLong();

  // React when the widget gets closed by user...
  connect(PerfmeterWidget, SIGNAL(widgetClosing()), this, SLOT(slotPerfmeterWidget()));
}

// This method will be connected to X Button
void  sccGui::stopPerfmeterWidget() {
  if (perfPollingTimer) {
    disconnect(perfPollingTimer, 0, this, 0);
    perfPollingTimer->stop();
    delete perfPollingTimer;
    perfPollingTimer = NULL;
  }
  if (PerfmeterWidget) {
    delete PerfmeterWidget;
    PerfmeterWidget = NULL;
  }
}

// This slot will be called by widget action...
void sccGui::slotPerfmeterWidget() {
  if (PerfmeterWidget->isVisible()) {
    stopPerfmeterPolling(true);
  } else {
    startPerfmeterPolling(true);
  }
}

void sccGui::startPerfmeterPolling(bool openWidget) {
  // Open widget, if requested...
  if (PerfmeterWidget) {
    if (openWidget && !PerfmeterWidget->isVisible()) {
      PerfmeterWidget->show();
      PerfmeterWidget->raise();
      PerfmeterWidget->activateWindow();
      ui.actionPerfmeterWidget->setToolTip("Close performance meter widget...");
      ui.actionPerfmeterWidget->setChecked(true);
    }
  }
  // Only start polling when widget is open...
  if (!PerfmeterWidget->isVisible()) return;

  // Start polling
  for (int loop = 0; loop < 64; loop++) perfStatus[loop] = 0xff;
  if (perfApertureStart) sccAccess->write32(perfRoute, perfDestid, perfApertureStart, perfStatus, 64, perfSIFPort, false);
  perfPollingTimer = new QTimer();
  connect(perfPollingTimer, SIGNAL(timeout()), this, SLOT(pollPerfmeterWidget()));
  perfPollingTimer->setSingleShot(true); // The slot routine will re-start the timer after execution of polling...
  perfPollingTimer->start(PERF_POLL_INTERVALL);
}

void sccGui::stopPerfmeterPolling(bool closeWidget) {
  // Stop polling...
  if (perfPollingTimer) {
    disconnect(perfPollingTimer, 0, this, 0);
    perfPollingTimer->stop();
    delete perfPollingTimer;
    perfPollingTimer = NULL;
  }
  // Close widget, if requested...
  if (PerfmeterWidget) {
    for (int loop = 0; loop < 48; loop++) {
      updatePerfmeterWidget(X_PID(loop), Y_PID(loop), Z_PID(loop), 0xff, 0);
    }
    if (closeWidget && PerfmeterWidget->isVisible()) {
      PerfmeterWidget->hide();
      PerfmeterWidget->setGeometry(PerfmeterWidget->geometry()); // Save position for user convenience...
      ui.actionPerfmeterWidget->setToolTip("Open performance meter widget...");
      ui.actionPerfmeterWidget->setChecked(false);
    }
  }
}

void sccGui::pollPerfmeterWidget() {
  // Don't start with polling before current block transfers are done...
  if (!tryAcquireApi()) {
    if (perfPollingTimer) perfPollingTimer->start(250); // Fast retry...
    return;
  }

  // Find out current power consumption...
  double power = sccAccess->getCurrentPowerconsumption();
  if (power) PerfmeterWidget->updatePower(power);

  // Poll perf values
  if (perfApertureStart) sccAccess->read32(perfRoute, perfDestid, perfApertureStart, perfStatus, 64, perfSIFPort, false);
  for (int loop = 0; loop < 48; loop++) {
    if (PerfmeterWidget) {
      PerfmeterWidget->setValue(X_PID(loop), Y_PID(loop), Z_PID(loop), perfStatus[loop]);
    }
  }
  releaseApi();

  // We use a single-shot timer, so that polling requests can't overtake themselves...
  // Restart timer when we're done with a complete polling interval:
  if (perfPollingTimer) perfPollingTimer->start(PERF_POLL_INTERVALL);
}

void sccGui::updatePerfmeterWidget(int x, int y, int core, int percent, double power) {
  if (PerfmeterWidget) {
    PerfmeterWidget->setValue(x, y, core, percent);
    PerfmeterWidget->updatePower(power);
  }
}

void sccGui::uiEnable(bool menubar, bool fileToolBar, bool bmcToolBar, bool toolsToolBar, bool widgetToolBar) {
  ui.menubar->setEnabled(menubar);
  ui.fileToolBar->setEnabled(fileToolBar);
  ui.bmcToolBar->setEnabled(bmcToolBar);
  ui.toolsToolBar->setEnabled(toolsToolBar);
  ui.widgetToolBar->setEnabled(widgetToolBar);
}

void sccGui::slotStartkScreen() {
  if (!konsoleProcess) {
    konsoleProcess = new QProcess();
    connect (konsoleProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotClosekScreen(int)));
    konsoleProcess->start("sccKonsole");
    printInfo("Launching program sccKonsole (also available as commandline tool)...");
  } else {
    slotClosekScreen(0);
  }
}

void sccGui::slotClosekScreen(int exitCode) {
  // Close process...
  if (konsoleProcess != NULL) {
    disconnect(konsoleProcess, 0, this, 0);
    konsoleProcess->terminate();
    if (!konsoleProcess->waitForFinished(3000)) {
      konsoleProcess->kill();
    }
    delete konsoleProcess;
    konsoleProcess = NULL;
  }
  // Disable checkbox but prevent "restart"...
  disconnect(ui.actionStartkScreen, 0, this, 0);
  ui.actionStartkScreen->setChecked(false);
  connect(ui.actionStartkScreen, SIGNAL(triggered()), this, SLOT(slotStartkScreen()));
  return;
  printInfo("Exit code is ", QString::number(exitCode)); // Will never be reached. Just to prevent compiler warning...
}

// --------------------------------------------------------------------------
// ---------- Soft-RAM functions --------------------------------------------
// --------------------------------------------------------------------------
void sccGui::set32ByteSR(mem32Byte* mem) {

  for (int i=0; i<4; i++) {
    if (SOFTRAM_DEBUG) printf("Setting softram at Byte addr 0x%08llx (index 0x%08llx masked 0x%08llx) to 0x%016llx\n", mem->addr + i*  8, ((mem->addr)>>3)+i, (((mem->addr)>>3)+i) & cSoftramMask, mem->data[i]);
    cSoftram[(((mem->addr)>>3) + i) & cSoftramMask] = mem->data[i];
  }
}

void sccGui::get32ByteSR(mem32Byte* mem) {

  for (int i=0; i<4; i++) {
    mem->data[i] = cSoftram[(((mem->addr)>>3) + i) & cSoftramMask];
    if (SOFTRAM_DEBUG){
      printf("Data @ Byte addr 0x%08llx (index 0x%08llx masked 0x%08llx): 0x%016llx\n", mem->addr + i*  8, ((mem->addr)>>3)+i, (((mem->addr)>>3)+i) & cSoftramMask, mem->data[i]);
    }
  }
}

void sccGui::softramInit(uInt32 size) {
  free(cSoftram);
  cSoftram = (uInt64*) malloc(size);
  if (cSoftram == NULL) {
    printError("softramInit(): Allocation of ", QString::number(size>>20), " MB Softram failed.");
  } else {
    cSoftramMask = (size - 1) >> 3;
    printInfo("softramInit(): Allocation of ", QString::number(size>>20), " MB Softram succeeded.");
    printDebug( "Bytes: 0x", QString::number(size, 16), " Index: 0x", QString::number(size >> 3, 16), " Mask: 0x", QString::number(cSoftramMask, 16));
  }
  return;
}

void sccGui::softramRelease() {
  if (cSoftram) {
    free(cSoftram);
    cSoftram = NULL;
    printInfo("softramRelease(): Softram freed...");
  }
}

// Process softram requests...
uInt32 msgArray[12];
uInt32* msgPointer = (uInt32*)msgArray;
void sccGui::slotSoftramRequest(uInt32* message) {
  int tmp;
  mem32Byte mem, rmw_mem;
  int sif_port_recipient;
  int sif_port_sender;
  int transid;
  int byteenable;
  int destid;
  int routeid;
  int cmd;
  int answerType=0;
  tmp=message[8];
  sif_port_sender = (tmp >> 16) & 0x0ff;
  transid = (tmp >> 8) & 0x0ff;
  byteenable = tmp & 0x0ff;
  mem.addr = message[9];
  tmp=message[10];
  mem.addr += ((uInt64)(tmp & 0x03))<<32;
  destid = tmp>>22 & 0x07;
  routeid = tmp>>14 & 0x0ff;
  cmd = tmp>>2 & 0x01ff;
  sif_port_recipient = tmp>>11 & 0x03;
  mem.data[3] = ((uInt64)message[7]<<32)+(uInt64)message[6];
  mem.data[2] = ((uInt64)message[5]<<32)+(uInt64)message[4];
  mem.data[1] = ((uInt64)message[3]<<32)+(uInt64)message[2];
  mem.data[0] = ((uInt64)message[1]<<32)+(uInt64)message[0];
  message[10]=0; // Signal that we're done...
                 // Now, never use message again in this slot....

  if (sif_port_recipient != SIF_HOST_PORT) return;

  if (cmd == WBI) {
    // write memory to softram
    set32ByteSR(&mem);
    answerType = MEMCMP;
    if (SOFTRAM_DEBUG) printInfo("Processed write request (WBI) to addr: 0x", QString::number(mem.addr, 16));
  } else if ((cmd == WCWR) || (cmd == NCWR)) {
    // Read (from softram)
    rmw_mem.addr = mem.addr;
    get32ByteSR(&rmw_mem);
    // Modify (Only overwrite data if byteenable is active)
    uInt64 tmp_data = rmw_mem.data[0];
    rmw_mem.data[0]=0;
    for (int loop=0; loop<8; loop++) {
      if (byteenable & (1<<loop)) {
        rmw_mem.data[0] += (uInt64)(mem.data[0]&((uInt64)0x0ff<<(8*loop)));
      } else {
        rmw_mem.data[0] += (uInt64)(tmp_data&((uInt64)0x0ff<<(8*loop)));
      }
    }
    // Write (to softram)
    set32ByteSR(&rmw_mem);
    answerType = MEMCMP;
    if (SOFTRAM_DEBUG) printInfo("Processed write request (", sccAccess->decodeCommand(cmd,false),") to addr: 0x", QString::number(mem.addr, 16), ", byteenable: 0x", QString::number(byteenable, 16));
  } else if ((cmd == RDO) || (cmd == NCRD)) {
    // Read (from softram)
    get32ByteSR(&mem);
    if (cmd == NCRD) {
      // In case of NCRD, take care of byte enable...
      uInt64 tmp_data = mem.data[0];
      mem.data[0] = mem.data[1] = mem.data[2] = mem.data[3] = 0;
      for (int loop=0; loop<8; loop++) {
        if (byteenable & (1<<loop)) {
          mem.data[0] += (uInt64)(tmp_data&((uInt64)0x0ff<<(8*loop)));
        }
      }
    }
    answerType = DATACMP;
    if (SOFTRAM_DEBUG) printInfo("Processed read request (", sccAccess->decodeCommand(cmd,false),") to addr: 0x", QString::number(mem.addr, 16), ", byteenable: 0x", QString::number(byteenable, 16));
  }

  // Prepare SCEMI message with response...
  msgPointer[10]  = (uInt32)destid<<22;
  msgPointer[10] += (uInt32)routeid<<14;
  msgPointer[10] += (uInt32)answerType<<2;
  msgPointer[10] += (uInt32)((uInt64)(mem.addr)>>32);
  msgPointer[9]  = (uInt32)(mem.addr & 0x0ffffffff);
  msgPointer[8]  = (uInt32)sif_port_sender<<24;
  msgPointer[8] += (uInt32)sif_port_recipient<<16;
  msgPointer[8] += (uInt32)transid<<8;
  msgPointer[8] += (uInt32)byteenable;
  memcpy((void*) msgPointer, (void*) mem.data, 8*sizeof(uInt32));
  if (sif_port_sender == SIF_HOST_PORT) {
    // If request comes from host, answer directly (faster)...
    if (sccAccess->debugPacket==RAW_DEBUG_PACKETS) sccAccess->printPacket(msgPointer, "RX", true, true);
    sccAccess->rxMsg = (uInt32*) malloc(12*sizeof(uInt32));
    memcpy((void*) sccAccess->rxMsg, (void*) msgPointer, 12*sizeof(uInt32));
    sccAccess->rxMsgAvail = true;
  } else {
    // Send SCEMI packet otherwise...
    sccAccess->sendMessage(msgPointer);
    // Print packet log directly as we bypass transfer_packet to prevent deadlock...
    if (sccAccess->debugPacket==RAW_DEBUG_PACKETS) {
      if (sccAccess->packetLog.isOpen()) {
        char msg[255];
        sprintf(msg, "TX %03d from %s to %s -> transferPacket(0x%02x, %s, 0x%01x_%08x, %s, 0x%02x, 0x%016llx_%016llx_%016llx_%016llx);\n", transid, sccAccess->decodePort(sif_port_recipient).toStdString().c_str(), sccAccess->decodePort(sif_port_sender).toStdString().c_str(), routeid, sccAccess->decodeDestID(destid).toStdString().c_str(), (unsigned int)(mem.addr>>32), (unsigned int)(mem.addr & 0x0ffffffff), sccAccess->decodeCommand(answerType).toStdString().c_str(), byteenable, mem.data[3], mem.data[2], mem.data[1], mem.data[0] );
        sccAccess->packetLog.write(msg);
        sccAccess->packetLog.flush();
      }
    }
  }
  // GUI stuff
  if (!transid) qApp->processEvents(); // allow GUI to update from time to time...
}

// Helper functions
QFont sccGui::getAppFont(uInt32 type, uInt32 size) {
  QStringList defaultFontFamily = (QStringList() << "Lucida Sans" << "B&H Lucida");
  QStringList defaultMonoFontFamily = (QStringList() << "Lucida Sans Typewriter" << "B&H LucidaTypewriter" << "Efont Fixed" << "Monospace");

  QString requestedFont;
  const QString defaultFontStyle = "Regular";
  QFont guiFont;
  QFontDatabase guiFDB;

  int i = 0;
  bool found = false;
  if (type == 0) {
    while (i < defaultFontFamily.size() && !found) {
      requestedFont = defaultFontFamily[i];
      guiFont = guiFDB.font(requestedFont, defaultFontStyle, size);
      if (guiFont.family() == requestedFont) {
        found = true;
      }
      ++i;
    }
  } else {
    while (i < defaultMonoFontFamily.size() && !found) {
      requestedFont = defaultMonoFontFamily[i];
      guiFont = guiFDB.font(requestedFont, defaultFontStyle, size);
      if (guiFont.family() == requestedFont) {
        found = true;
      }
      ++i;
    }
  }

  return guiFont;
} // getAppFont

QString sccGui::getSccKitPath() {
  QString result = QCoreApplication::applicationDirPath();
  result.replace(QRegExp("bin$"), "");
  return result;
}
