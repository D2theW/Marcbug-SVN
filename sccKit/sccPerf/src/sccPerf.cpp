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

#include "sccPerf.h"

// The LOG_DEBUG enables extensive debug messages if set to "1" (should be 0 for releases)
#define LOG_DEBUG 0

// The USE_DRIVER define can be set to "0" to debug the GUI w/o the need to load a driver (should be 1 for releases)...
#define USE_DRIVER 1

sccPerf::sccPerf(QWidget *parent) : QMainWindow(parent) {
  // Initialization

  // Load settings
  settings = new QSettings("sccKit", "sccGui");

  // Create UI and message handler
  QFont font;
  log = new messageHandler(true, "", NULL, NULL);
  #if LOG_DEBUG == 1
    log->setLogLevel(log_debug);
  #endif
  CONNECT_MESSAGE_HANDLER;

  // Invoke sccApi and prepare widgets as well as semaphores...
  sccAccess = new sccExtApi(log);
  sccAccess->openBMCConnection();

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
  printInfo("Didn't load PCIe driver! This is a binary for the power management demo...");
#endif

  // Don't open GUI if initialization failed. Otherwise open it...
  if (!initFailed) {
    printInfo("Welcome to sccPerf ", VERSION_TAG, (QString)((BETA_FEATURES!=0)?"beta":""), " (build date ", __DATE__, " - ", __TIME__, ")...");
    // Create PerfmeterWidget...
    PerfmeterWidget = new perfmeterWidget(NUM_COLS, NUM_ROWS, NUM_CORES, sccAccess);
    perfPollingTimer = NULL;
    perfRoute = settings->value("perfMeter/Route", 0x00).toInt();
    perfDestid = settings->value("perfMeter/Destid", 0).toInt();
    perfSIFPort = settings->value("perfMeter/SIFPort", 0).toInt();
    perfApertureStart = settings->value("perfMeter/ApertureStart", 0).toULongLong();
    PerfmeterWidget->show();
    PerfmeterWidget->raise();
    PerfmeterWidget->activateWindow();

    // Only start polling when widget is open...
    if (!PerfmeterWidget->isVisible()) return;

    // Start polling
    for (int loop = 0; loop < 64; loop++) perfStatus[loop] = 0xff;
  #if USE_DRIVER == 1
    if (perfApertureStart) sccAccess->write32(perfRoute, perfDestid, perfApertureStart, perfStatus, 64, perfSIFPort, false);
  #endif
    perfPollingTimer = new QTimer();
    connect(perfPollingTimer, SIGNAL(timeout()), this, SLOT(pollPerfmeterWidget()));
    perfPollingTimer->setSingleShot(true); // The slot routine will re-start the timer after execution of polling...
  #if USE_DRIVER == 1
    perfPollingTimer->start(PERF_POLL_INTERVALL);
  #else
    perfPollingTimer->start(10000);
  #endif
  } else {
    printError("Unable to connect to PCIe driver...\n       Make sure that PCIe driver is installed and\n       not in use by someone else... Aborting!");
    exit(-1);
  }
}

sccPerf::~sccPerf() {
  closeEvent(NULL);
  delete settings;
  if (sccAccess) delete sccAccess;
  printInfo("Closing sccPerf application! Bye...\n");
  delete log;
}

// Make sure to close widgets when user requests to close main application window...
void sccPerf::closeEvent(QCloseEvent *event) {
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
  return;
  // Will never be reached: Fake code to prevent compiler to prevent "unused argument" warning...
  if (event) return;
}

void sccPerf::pollPerfmeterWidget() {
  // Find out current power consumption...
  double power = sccAccess->getCurrentPowerconsumption();
  if (power) PerfmeterWidget->updatePower(power);

  // Poll perf values
#if USE_DRIVER == 1
  if (perfApertureStart) sccAccess->read32(perfRoute, perfDestid, perfApertureStart, perfStatus, 64, perfSIFPort, false);
#endif

  for (int loop = 0; loop < 48; loop++) {
    if (PerfmeterWidget) {
      PerfmeterWidget->setValue(X_PID(loop), Y_PID(loop), Z_PID(loop), perfStatus[loop]);
    }
  }

  // We use a single-shot timer, so that polling requests can't overtake themselves...
  // Restart timer when we're done with a complete polling interval:
#if USE_DRIVER == 1
  if (perfPollingTimer) perfPollingTimer->start(PERF_POLL_INTERVALL);
#else
  if (perfPollingTimer) perfPollingTimer->start(10000);
#endif
}
