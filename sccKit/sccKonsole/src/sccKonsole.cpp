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

#include "sccKonsole.h"

// The LOG_DEBUG enables extensive debug messages if set to "1" (should be 0 for releases)
#define LOG_DEBUG 0

sccKonsole::sccKonsole() {
  // Open message handler
  log = new messageHandler(true, "");
  CONNECT_MESSAGE_HANDLER;
  konsoleProcess = NULL;

  // Open API (to find out IP range of SCC)
  sccAccess = new sccExtApi(log);

  // Evaluate commandline args
  int pid, rangeStart, rangeEnd;
  bool okay;
  bool abort = false;
  konsoleMask.disableAll();
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
                konsoleMask.setEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid), true);
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
          konsoleMask.setEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid), true);
        }
      }
    }
    // No PID... Other Argument?
    if (!validArgument) {
      if ((QCoreApplication::arguments().at(loop).toLower() == "-h") || (QCoreApplication::arguments().at(loop).toLower() == "--help")) {
        printUsage(0);
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
  if (!konsoleMask.getNumCoresEnabled()) {
    konsoleMask.enableAll();
  }

  // Check if selected cores are available...
  coreMask pingMask;
  pingMask.pingCores(sccAccess->getSccIp(0));
  notAllSelectedCoresAvailable = false;
  for (pid = 0; pid < NUM_ROWS*NUM_COLS*NUM_CORES; pid++) {
    if (konsoleMask.getEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid)) && !pingMask.getEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid))) {
      printDebug("Selected core with PID ", messageHandler::toString(pid, 10, 2), " is not reachable via ping. Ignoring selection...");
      konsoleMask.setEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid), false);
      notAllSelectedCoresAvailable = true;
    }
  }
  if (!konsoleMask.getNumCoresEnabled()) {
    printError ("Can't open sccKonsole as none of the selected cores is reachable via\n",
                "       ping. Please check if all cores are ready with booting.\n"
                "       sccBoot -s gives you the current status...");
    printUsage(-1);
  }

  // Load settings
  settings = new QSettings("sccKit", "sccGui");

  // Tell user if we succeeded...
  printInfo("Welcome to sccKonsole ", VERSION_TAG, (QString)((BETA_FEATURES!=0)?"beta":""), " (build date ", __DATE__, " - ", __TIME__, ")...");
}

sccKonsole::~sccKonsole() {
  delete settings;
  delete log;
}

#define WAIT_TIMEOUT	-1

// startKonsole(): This Routine starts the Konsole...
void sccKonsole::startKonsole() {
  QProcess versionProcess;
  versionProcess.start("konsole -v");
  versionProcess.waitForFinished(3000);
  QString versionString = versionProcess.readAllStandardOutput();
  if ( versionString.contains(QRegExp("KDE[:]*\\s+3")) ) {
    konsoleProcess = new QProcess();
    konsoleProcess->start("konsole --script");
    printInfo("Opening konsole (DCOP: konsole-", QString::number(konsoleProcess->pid()), ") for all cores that are reachable via ping: ", konsoleMask.getMaskString());
    qApp->processEvents();
    QString dcopString = "konsole-" + QString::number(konsoleProcess->pid());

    // DCOP communication
    QProcess* dcopProcess = new QProcess();
    QString dcopOutput;
    dcopProcess->start("dcop");
    dcopProcess->waitForFinished(WAIT_TIMEOUT);
    dcopOutput = dcopProcess->readAllStandardOutput();
    while (!dcopOutput.contains(dcopString)) {
      sleep(1);
      dcopProcess->start("dcop");
      dcopProcess->waitForFinished(WAIT_TIMEOUT);
      dcopOutput = dcopProcess->readAllStandardOutput();
    }
    // Create sessions
    for (int loop = 1; loop < konsoleMask.getNumCoresEnabled(); loop++) {
      dcopProcess->start("dcop " + dcopString + " konsole newSession");
      dcopProcess->waitForFinished(WAIT_TIMEOUT);
    }
    dcopProcess->start("dcop " + dcopString + " konsole nextSession");
    dcopProcess->waitForFinished(WAIT_TIMEOUT);
    int nr = 1;
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          if (konsoleMask.getEnabled(loop_x, loop_y, loop_core)) {
            dcopProcess->start("dcop " + dcopString + " session-" + QString::number(nr) + " renameSession rck" + messageHandler::toString(PID(loop_x, loop_y, loop_core), 10, 2));
            dcopProcess->waitForFinished(WAIT_TIMEOUT);
            dcopProcess->start("dcop " + dcopString + " session-" + QString::number(nr) + " sendSession \"ssh -F /opt/sccKit/.ssh2/ssh2_config root@" + sccAccess->getSccIp(PID(loop_x, loop_y, loop_core)));
            dcopProcess->waitForFinished(WAIT_TIMEOUT);
            dcopProcess->start("dcop " + dcopString + " session-" + QString::number(nr) + " sendSession clear");
            dcopProcess->waitForFinished(WAIT_TIMEOUT);
            nr++;
          }
        }
      }
    }
    delete dcopProcess;
  } else if ( versionString.contains(QRegExp("KDE[:]*\\s+4")) ) {
    // qdbus communication
    QString qdbusString = "org.kde.konsole";
    QProcess* qdbusProcess = new QProcess();
    QString qdbusOutput;
    qdbusProcess->start("qdbus");
    qdbusProcess->waitForFinished(WAIT_TIMEOUT);
    qdbusOutput = qdbusProcess->readAllStandardOutput();
    if (qdbusOutput.contains(qdbusString)) {
      printInfo("Closing and re-opening konsole (QDBUS: ", qdbusString, ") for all cores that are reachable via ping: ", konsoleMask.getMaskString());
      qdbusProcess->start("qdbus "+qdbusString+" /MainApplication quit");
      qdbusProcess->waitForFinished(WAIT_TIMEOUT);
      sleep(3);
      konsoleProcess = new QProcess();
      konsoleProcess->start("konsole");
    } else {
      printInfo("Opening konsole (QDBUS: ", qdbusString, ") for all cores that are reachable via ping: ", konsoleMask.getMaskString());
      konsoleProcess = new QProcess();
      konsoleProcess->start("konsole --nofork");
    }
    qApp->processEvents();

    // qdbus communication
    qdbusProcess->start("qdbus");
    qdbusProcess->waitForFinished(WAIT_TIMEOUT);
    qdbusOutput = qdbusProcess->readAllStandardOutput();
    while (!qdbusOutput.contains(qdbusString)) {
      sleep(1);
      qdbusProcess->start("qdbus");
      qdbusProcess->waitForFinished(WAIT_TIMEOUT);
      qdbusOutput = qdbusProcess->readAllStandardOutput();
    }
    // Create sessions
    for (int loop = 1; loop < konsoleMask.getNumCoresEnabled(); loop++) {
      qdbusProcess->start("qdbus " + qdbusString + " /Konsole newSession");
      qdbusProcess->waitForFinished(WAIT_TIMEOUT);
    }
    qdbusProcess->start("qdbus " + qdbusString + " /Konsole nextSession");
    qdbusProcess->waitForFinished(WAIT_TIMEOUT);
    int nr = 1;
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          if (konsoleMask.getEnabled(loop_x, loop_y, loop_core)) {
            qdbusProcess->start("qdbus " + qdbusString + " /Sessions/" + QString::number(nr) + " org.kde.konsole.Session.setTitle 1 rck" + messageHandler::toString(PID(loop_x, loop_y, loop_core), 10, 2));
            qdbusProcess->waitForFinished(WAIT_TIMEOUT);
            qdbusProcess->start("qdbus " + qdbusString + " /Sessions/" + QString::number(nr) + " sendText \"ssh -F /opt/sccKit/.ssh2/openssh_config root@" + sccAccess->getSccIp(PID(loop_x, loop_y, loop_core)) + "\r\"");
            qdbusProcess->waitForFinished(WAIT_TIMEOUT);
            qdbusProcess->start("qdbus " + qdbusString + " /Sessions/" + QString::number(nr) + " sendText \"clear\r\"");
            qdbusProcess->waitForFinished(WAIT_TIMEOUT);
            nr++;
          }
        }
      }
    }
    delete qdbusProcess;
  } else {
    printWarning("Unable to find out KDE version... Check if \"konsole\" is available on your system (requires KDE3 or KDE4)");
  }
  konsoleProcess->waitForFinished(-1);
  delete konsoleProcess;
  return;
}

void sccKonsole::printUsage(int exitCode) {
  QString usage=(QString)((exitCode)?"\n":"") + "\
Usage: " + QCoreApplication::arguments().at(0) + " [option] [<PID or PID range> .. <PID or PID range>]\n\
\n\
Options:\n\
  -h or --help: Print this message and exit...\n\
\n\
PID or PID range: \n\
  Specifies a processor ID (PID). e.g. \"0\" or a range of PIDs separated\n\
  with dots. e.g. \"0..47\"\n\
\n\
This application will start a konsole with one tab for each available\n\
core (reachable via ping)... If PIDs are specified, only the specified\n\
cores (if reachable via ping) are connected... If no PIDs are specified\n\
sccKonsole connects to all cores that are reachable via ping...";
  log->setMsgPrefix(log_info, "");
  printInfo(usage);
  log->setMsgPrefix(log_info, "INFO");
  exit(exitCode);
}
