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

// This is the main window widget. It parses the command line arguments and then
// invokes sccDisplayWidget with the actual visual content. The widget also contains
// a help message that is displayed when no core is reachable or the user invokes
// the application with --help...

#include "sccDisplay.h"

// The LOG_DEBUG enables extensive debug messages if set to "1" (should be 0 for releases)
#define LOG_DEBUG 0

// The USE_DRIVER define can be set to "0" to debug the GUI w/o the need to load a driver (should be 1 for releases)...
#define USE_DRIVER 1

powerThread::powerThread(char* status) {
  this->status = status;
  log = new messageHandler(true);
  PerfmeterWidget = new perfmeterWidget(NUM_COLS, NUM_ROWS, NUM_CORES, NULL);
  serverRunning = false;
}

powerThread::~powerThread() {
  delete PerfmeterWidget;
}

void powerThread::run() {
  int loop;
  double power;
  if (serverRunning) {
    return;
  }
  serverRunning = true;
  serverOn = true;
  sccAccess = new sccExtApi(log);
  sccAccess->openBMCConnection();
  connect(this, SIGNAL(signalValue(int, int, int, int)), PerfmeterWidget, SLOT(setValue(int, int, int, int)));
  do {
    if (perfmeterVisible()) {
      power = sccAccess->getCurrentPowerconsumption();
      if (power) PerfmeterWidget->updatePower(power);
      for (loop = 0; loop < 48; loop++) {
        emit signalValue(X_PID(loop), Y_PID(loop), Z_PID(loop), status[loop]);
      }
    }
    usleep(1000000); // Let other threads do things... Only update once per second!
  } while (serverOn);
  delete sccAccess;
  serverRunning = false;
}

void powerThread::stop() {
  serverOn = false;
  while (serverRunning) {
    usleep(1);
  }
}

void powerThread::showPerfmeter(bool show) {
  if (show) {
    PerfmeterWidget->show();
    if (!PerfmeterWidget->isVisible()) { // Worked?
      printf("Warning: Unable to open performance meter...\n"); fflush(0);
    }
  } else {
    PerfmeterWidget->hide();
  }
}

void powerThread::togglePerfmeter() {
  showPerfmeter(!PerfmeterWidget->isVisible());
}

bool powerThread::perfmeterVisible() {
  return PerfmeterWidget->isVisible();
}

sccDisplay::sccDisplay(QWidget *parent) : QMainWindow(parent) {
  // Initialization
  mW = this;
  bool noPing = false;
  uInt32 socketMode = DEFAULT_HID_SOCKET;

  // Create UI and message handler
  log = new messageHandler(true, "", NULL, NULL);
  #if LOG_DEBUG == 1
    log->setLogLevel(log_debug);
  #endif
  CONNECT_MESSAGE_HANDLER;

  // Evaluate commandline args
  int pid, rangeStart, rangeEnd;
  bool okay;
  bool abort = false;
  displayMask.disableAll();
  for (int loop = 1; loop < QCoreApplication::arguments().count(); loop++) {
    bool validArgument = false;
    if (QCoreApplication::arguments().at(loop).contains(QRegExp("[.]+"))) {
      QStringList list =  QCoreApplication::arguments().at(loop).split(QRegExp("[.]+"));
      if (list.count() == 2) {
        rangeStart = list.at(0).toInt(&okay, 10);
        if (!okay) {
          rangeStart = list.at(0).toInt(&okay, 16);
        }
        if (okay) {
          rangeEnd = list.at(1).toInt(&okay, 10);
          if (!okay) {
            rangeEnd = list.at(1).toInt(&okay, 16);
          }
          if (okay) {
            if (rangeEnd<rangeStart) {
              pid = rangeEnd;
              rangeEnd = rangeStart;
              rangeStart = pid;
            }
            if ((rangeStart>=0) && (rangeEnd<NUM_ROWS*NUM_COLS*NUM_CORES)) {
              validArgument = true;
              for (pid = rangeStart; pid <= rangeEnd; pid++) {
                printDebug("Found PID (range) ", messageHandler::toString(pid, 10, 2));
                displayMask.setEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid), true);
              }
            }
          }
        }
      }
      if (!validArgument) {
        printWarning("The provided PID range range \"", QCoreApplication::arguments().at(loop), "\" is invalid...");
        abort = true;
      }
      validArgument = true;
    } else {
      pid = QCoreApplication::arguments().at(loop).toInt(&okay, 10);
      if (!okay) {
        pid = QCoreApplication::arguments().at(loop).toInt(&okay, 16);
      }
      if (okay) {
        if ((pid>=0) && (pid<NUM_ROWS*NUM_COLS*NUM_CORES)) {
          validArgument = true;
          printDebug("Found PID ", messageHandler::toString(pid, 10, 2));
          displayMask.setEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid), true);
        }
      }
    }
    // No PID... Other Argument?
    if (!validArgument) {
      if ((QCoreApplication::arguments().at(loop).toLower() == "-h") || (QCoreApplication::arguments().at(loop).toLower() == "--help")) {
        printUsage(0);
      } else if ((QCoreApplication::arguments().at(loop).toLower() == "-t") || (QCoreApplication::arguments().at(loop).toLower() == "--tcp")) {
        socketMode=HID_TCP;
      } else if ((QCoreApplication::arguments().at(loop).toLower() == "-u") || (QCoreApplication::arguments().at(loop).toLower() == "--udp")) {
        socketMode=HID_UDP;
      } else if (QCoreApplication::arguments().at(loop).toLower() == "--noping") {
        // This is an "I know what I'm doing" undocumented debug option that should be followed by a selection of cores!
        // It disables the ping check, so that it's possible to watch the core booting. Example: "sccDisplay --noping 0"
        // In case of TCP connection, the connection itself might fail...
        noPing = true;
      } else {
        printWarning("The provided argument \"", QCoreApplication::arguments().at(loop), "\" is invalid...");
        abort = true;
      }
    }
  }

  // Invalid args -> Quit
  if (abort) {
    printUsage(-1);
  }

  // No args -> Use all cores
  if (!displayMask.getNumCoresEnabled()) {
    displayMask.enableAll();
  }

  // Invoke sccApi and prepare widgets as well as semaphores...
  sccAccess = new sccExtApi(log);
  // sccAccess->openBMCConnection(); Not needed

  // Check if selected cores are available...
  if (!noPing) {
    coreMask pingMask;
    pingMask.pingCores(sccAccess->getSccIp(0));
    notAllSelectedCoresAvailable = false;
    for (pid = 0; pid < NUM_ROWS*NUM_COLS*NUM_CORES; pid++) {
      if (displayMask.getEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid)) && !pingMask.getEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid))) {
        printDebug("Selected core with PID ", messageHandler::toString(pid, 10, 2), " is not reachable via ping. Ignoring selection...");
        displayMask.setEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid), false);
        notAllSelectedCoresAvailable = true;
      }
    }
    if (!displayMask.getNumCoresEnabled()) {
      printError ("Can't open sccDisplay as none of the selected cores is reachable via\n",
                  "       ping. Please check if all cores are ready with booting.\n"
                  "       sccBoot -s gives you the current status...");
      printUsage(-1);
    }
  }

  // Load settings
  settings = new QSettings("sccKit", "sccGui");

  // Open driver...
  bool initFailed = false;
#if USE_DRIVER == 1
  if (!sccAccess->connectSysIF()) {
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

  // Create PerfmeterWidget
  fetchPower = new powerThread(perfStatus);
  fetchPower->start();
  perfRoute = settings->value("perfMeter/Route", 0x00).toInt();
  perfDestid = settings->value("perfMeter/Destid", 0).toInt();
  perfSIFPort = settings->value("perfMeter/SIFPort", 0).toInt();
  perfApertureStart = settings->value("perfMeter/ApertureStart", 0).toULongLong();

  // Start polling
  for (int loop = 0; loop < 64; loop++) {
    perfStatus[loop] = 0xff;
  }
  if (perfApertureStart) sccAccess->write32(perfRoute, perfDestid, perfApertureStart, perfStatus, 64, perfSIFPort, false);

  connect(QApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(clipboardDataChanged()));

  // Display Widget...
  display = new sccDisplayWidget(sccAccess, log, statusBar(), settings->value("perfMeter/ApertureStart", 0).toULongLong()+0x40, &displayMask, socketMode, this);
  setCentralWidget(display);
  setWindowFlags( Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint );

  // Don't open GUI if initialization failed. Otherwise open it...
  if (!initFailed) {
    printInfo("Welcome to sccDisplay ", VERSION_TAG, (QString)((BETA_FEATURES!=0)?"beta":""), " (build date ", __DATE__, " - ", __TIME__, ")...");
    printInfo("Opening display for all selected cores", noPing?"":" that are reachable via ping", ": ", displayMask.getMaskString());
    qApp->processEvents(); // allow GUI to update
    show();
  } else {
    printError("Unable to connect to PCIe driver...\n       Make sure that PCIe driver is installed and\n       not in use by someone else... Aborting!");
    exit(-1);
  }
}

sccDisplay::~sccDisplay() {
  disconnect( QApplication::clipboard(), SIGNAL( dataChanged() ), this, SLOT( clipboardDataChanged() ) );
  delete display;
  delete settings;
  delete fetchPower;
  if (sccAccess) delete sccAccess;
  delete log;
}

void sccDisplay::clipboardDataChanged() {
  QClipboard *clip = QApplication::clipboard();
  QString text;

  text = clip->text();
  if (text != NULL) {
    char txdata[255 + 1];
    int transfer_len = 0;

    memset(txdata, 0, sizeof(txdata));
    txdata[0] = EVENT_SET_CLIPBOARD;
    transfer_len = (unsigned int)text.length() > (sizeof(txdata) - 1) ? (sizeof(txdata) - 1) : text.length();
    strncpy(txdata + 1, text.toAscii(), transfer_len);
    //printInfo("Clipboard entry: ", text);
    display->writeSocket((char*)&txdata, transfer_len + 1);
  } else {
    printfInfo("Clipboard entry is NULL");
  }
}

// Make sure to close widgets when user requests to close main application window...
void sccDisplay::closeEvent(QCloseEvent *event) {
  if (display) display->close();
  event->accept();
}

// Will be invoked by sccDisplayWidget...
void sccDisplay::pollPerfmeterWidget() {
  // Only poll if we need to...
  if (fetchPower->perfmeterVisible() && perfApertureStart) {
    sccAccess->read32(perfRoute, perfDestid, perfApertureStart, perfStatus, 64, perfSIFPort, false);
  }
}

void sccDisplay::showPerfmeter(bool show) {
  fetchPower->showPerfmeter(show);
}

void sccDisplay::togglePerfmeter() {
  fetchPower->togglePerfmeter();
}

void sccDisplay::printUsage(int exitCode) {
  QString usage=(QString)((exitCode)?"\n":"") + "\
Usage: " + QCoreApplication::arguments().at(0) + " [option] [<PID or PID range> .. <PID or PID range>]\n\
\n\
Options:\n\
  -t or --tcp:  Use TCP/IP connection for remote HID connection" + (QString)((DEFAULT_HID_SOCKET==HID_TCP)?" (default)":"") + "...\n\
  -u or --udp:  Use UDP connection for remote HID connection" + (QString)((DEFAULT_HID_SOCKET==HID_UDP)?" (default)":"") + "...\n\
  -h or --help: Print this message and exit...\n\
\n\
PID or PID range: \n\
  Specifies a processor ID (PID). e.g. \"0\" or a range of PIDs separated\n\
  with dots. e.g. \"0..47\"\n\
\n\
This application will start a virtual display with one tab for each available\n\
core (reachable via ping)... If PIDs are specified, only the specified\n\
cores (if reachable via ping) are connected... If no PIDs are specified\n\
sccDisplay connects to all cores that are reachable via ping...";

  log->setMsgPrefix(log_info, "");
  printInfo(usage);
  log->setMsgPrefix(log_info, "INFO");
  exit(exitCode);
}

