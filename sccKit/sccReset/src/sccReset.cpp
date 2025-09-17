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

#include "sccReset.h"

// The LOG_DEBUG enables extensive debug messages if set to "1" (should be 0 for releases)
#define LOG_DEBUG 0

sccReset::sccReset() {
  // Open message handler
  log = new messageHandler(true, "");
  CONNECT_MESSAGE_HANDLER;

  // Evaluate commandline args
  bool resetOperation = false;
  bool globalReset = false;
  pullResets = true; // default
  showStatus = false; // default
  resetMask.disableAll();
  int pid, rangeStart, rangeEnd;
  bool okay;
  for (int loop = 1; loop < QCoreApplication::arguments().count(); loop++) {
    bool validArgument = false;
    if ((QCoreApplication::arguments().at(loop).toLower() == "-h") || (QCoreApplication::arguments().at(loop).toLower() == "--help")) {
      printUsage(0);
    } else if ((QCoreApplication::arguments().at(loop).toLower() == "-p") || (QCoreApplication::arguments().at(loop).toLower() == "--pull")) {
      if (!showStatus) {
        pullResets = true;
        resetOperation = true;
      }
    } else if ((QCoreApplication::arguments().at(loop).toLower() == "-g") || (QCoreApplication::arguments().at(loop).toLower() == "--global")) {
      globalReset = true;
    } else if ((QCoreApplication::arguments().at(loop).toLower() == "-r") || (QCoreApplication::arguments().at(loop).toLower() == "--release")) {
      if (!showStatus) {
        pullResets = false;
        resetOperation = true;
      }
    } else if ((QCoreApplication::arguments().at(loop).toLower() == "-s") || (QCoreApplication::arguments().at(loop).toLower() == "--status")) {
      if (pullResets) showStatus = true;
    } else if (resetOperation) {
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
                  resetMask.setEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid), true);
                }
              }
            }
          }
        }
        if (!validArgument) {
          printWarning("The provided PID range range \"", QCoreApplication::arguments().at(loop), "\" is invalid...");
          printUsage(-1);
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
            resetMask.setEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid), true);
          }
        }
      }
    } else {
      printWarning("The provided argument \"", QCoreApplication::arguments().at(loop), "\" is invalid...");
      printUsage(-1);
    }
  }

  // Sanity check...
  if (!showStatus && !resetOperation && !globalReset) {
    printWarning("Please provide proper arguments...");
    printUsage(-1);
  }

  // No args -> Use all cores
  if (resetOperation && !resetMask.getNumCoresEnabled()) {
    resetMask.enableAll();
  }

  // Invoke sccApi
  log->setShell(false); // Only write these details to logfile...
  sccAccess = new sccExtApi(log);
  if (globalReset) sccAccess->openBMCConnection(); // Needed for GRB register initialization...

  // Open driver...
  bool initFailed = false;
  if (sccAccess->connectSysIF()) {
#ifndef BMC_IS_HOST
    printInfo("Successfully connected to PCIe driver...");
#else
    printInfo("Successfully connected to System Interface FPGA BMC-Interface...");
#endif
  } else {
    initFailed = true;
  }

  // Load settings
  settings = new QSettings("sccKit", "sccGui");

  // Tell user if we succeeded...
  log->setShell(true); // Re-enable printing to the screen...
  if (!initFailed) {
    printInfo("Welcome to sccReset ", VERSION_TAG, (QString)((BETA_FEATURES!=0)?"beta":""), " (build date ", __DATE__, " - ", __TIME__, ")...");
  } else {
    printError("Unable to connect to PCIe driver...\n       Make sure that PCIe driver is installed and\n       not in use by someone else... Aborting!");
    qApp->processEvents(); // Update logfile
    exit(-1);
  }
  if (globalReset) {
    theWischer();
  } else {
    reset();
  }
}

sccReset::~sccReset() {
  delete settings;
  if (sccAccess) delete sccAccess;
  delete log;
}

void sccReset::theWischer() {
  uInt64 perfApertureStart;
  printInfo("Applying global software reset to SCC (cores & CRB registers)...");
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core, true)) {
          printError("Unable to apply reset... Please perform platform (re-)initialization!");
          return;
        }
        resetMask.setEnabled(loop_x, loop_y, loop_core, true);
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
}

// reset(): This Routine simply resets all selected cores...
void sccReset::reset() {
  // Variables & initializations...

  // Only show status?
  if (showStatus) {
    bool tmp;
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          if (!sccAccess->getReset(TID(loop_x, loop_y), loop_core, &tmp)) {
            printError("Unable to read reset state of SCC... Aborting operation...");
            return;
          }
          resetMask.setEnabled(loop_x, loop_y, loop_core, tmp);
        }
      }
    }
    log->setMsgPrefix(log_info, "Status");
    printInfo("The following resets are active (pulled): ", resetMask.getMaskString());
    return;
  }

  // Release resets...
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (resetMask.getEnabled(loop_x,loop_y,loop_core)) {
          if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,pullResets)) return;
        }
      }
    }
  }
  // That's it...
  printInfo("Resets have been ", (QString)((pullResets)?"pulled":"released"), ": ", resetMask.getMaskString());
  return;
}

void sccReset::printUsage(int exitCode) {
  QString usage=(QString)((exitCode)?"\n":"") + "\
Usage: " + QCoreApplication::arguments().at(0) + " [option]\n\
\n\
Options:\n\
  -h or --help: Print this message and exit...\n\
  -g or --global: Resets all cores as well as CRB registers.\n\
  -p or --pull [<PID or PID range> .. <PID or PID range>]:\n\
    Pull resets of specified cores... If no cores are specified\n\
    all cores' reset will be pulled!\n\
  -r or --release [<PID or PID range> .. <PID or PID range>]:\n\
    Release the resets of specified cores... If no cores are specified\n\
    all cores' reset will be released!\n\
  -s or --status: Only print which cores are in reset and exit...\n\
\n\
PID or PID range: \n\
  Specifies a processor ID (PID). e.g. \"0\" or a range of PIDs separated\n\
  with dots. e.g. \"0..47\"\n\
\n\
This application will simply pull the resets of the selected cores... The\n\
-r option allows to release the resets of the selected cores instead...";
  log->setMsgPrefix(log_info, "");
  printInfo(usage);
  log->setMsgPrefix(log_info, "INFO");
  exit(exitCode);
}
