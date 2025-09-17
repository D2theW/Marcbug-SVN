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

#include "coreMask.h"

// ================================================================================
// Class coreMask
// ================================================================================

coreMask::coreMask() {
  // Initialize mask...
  enableAll();
}

coreMask::~coreMask() {
}

void coreMask::enableAll() {
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        setEnabled(loop_x, loop_y, loop_core, true);
      }
    }
  }
}

void coreMask::disableAll() {
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        setEnabled(loop_x, loop_y, loop_core, false);
      }
    }
  }
}

void coreMask::setEnabled(int x, int y, int core, bool enabled) {
  currentMask[PID(x, y, core)]=enabled;
}

bool coreMask::getEnabled(int x, int y, int core) {
  return currentMask.value(PID(x, y, core), false);
}

void coreMask::invertMask() {
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        setEnabled(loop_x, loop_y, loop_core, !getEnabled(loop_x, loop_y, loop_core));
      }
    }
  }
}

QString coreMask::getMaskString() {
  QString result = "";
  QString divider = "";
  int counter = 0;

  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (currentMask[PID(loop_x, loop_y, loop_core)]) {
          result += divider + messageHandler::toString(PID(loop_x, loop_y, loop_core), 16, 2);
          divider = ", ";
          counter++;
        }
      }
    }
  }

  if (counter == 0) {
    result = "No cores!";
  } else if (counter == (NUM_ROWS * NUM_COLS * NUM_CORES)) {
    result = "All cores!";
  } else if (counter == 1) {
    result = QString::number(counter) + " core (PID = " + result + ")...";
  } else {
    result = QString::number(counter) + " cores (PIDs = " + result + ")...";
    result.replace(QRegExp(", (?!.*, .*)"), " and ");
  }

  return result;
}

int coreMask::getNumCoresEnabled() {
  int counter = 0;

  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (currentMask[PID(loop_x, loop_y, loop_core)]) {
          counter++;
        }
      }
    }
  }

  return counter;
}

int coreMask::getFirstActiveCore() {
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (currentMask[PID(loop_x, loop_y, loop_core)]) {
          return PID(loop_x, loop_y, loop_core);
        }
      }
    }
  }
  return -1;
}

void coreMask::pingCores(QString startIp, int timeout) {
  // Ping all cores
  QProcess* pingProcess[NUM_ROWS*NUM_COLS*NUM_CORES];
  QString host;
  QString hostPrefix = "rck";
  int hostIdentifier = 0;
  disableAll();
  if (startIp.contains(QRegExp("^[0-9]+[.][0-9]+[.][0-9]+[.][0-9]+$"))) {
    QStringList ipList = startIp.split(".");
    bool ok;
    hostIdentifier = ipList[3].toInt(&ok);
    if (hostIdentifier + 47 > 254) {
      return;
    }
    hostPrefix = ipList[0]+"."+ipList[1]+"."+ipList[2]+".";
  }
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        pingProcess[PID(loop_x, loop_y, loop_core)] = new QProcess();
        if (hostPrefix == "rck") {
          host = hostPrefix+messageHandler::toString(PID(loop_x, loop_y, loop_core), 10, 2);
        } else {
          host = hostPrefix + QString::number(hostIdentifier+PID(loop_x, loop_y, loop_core));
        }
#ifndef BMC_IS_HOST
        pingProcess[PID(loop_x, loop_y, loop_core)]->start("ping -q -c 3 -W "+QString::number(timeout)+" "+host);
#else
        pingProcess[PID(loop_x, loop_y, loop_core)]->start("ping -q -c 3 "+host);
#endif
      }
    }
  }
  // Wait till all ping processes are ready...
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (!pingProcess[PID(loop_x, loop_y, loop_core)]->waitForFinished(-1)) {
          pingProcess[PID(loop_x, loop_y, loop_core)]->terminate();
          if (!pingProcess[PID(loop_x, loop_y, loop_core)]->waitForFinished(1000)) {
            pingProcess[PID(loop_x, loop_y, loop_core)]->kill();
          }
          return;
        }
      }
    }
  }
  // Evaluate results
  QString result;
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        result = pingProcess[PID(loop_x, loop_y, loop_core)]->readAllStandardOutput();
        // 4 Debugging: printf("%s",result.toStdString().c_str());
        setEnabled(loop_x, loop_y, loop_core, result.contains(" 0% packet loss"));
        delete pingProcess[PID(loop_x, loop_y, loop_core)];
      }
    }
  }
#ifdef BMC_IS_HOST
  if (timeout) return; // Redundant code to prevent "unused variable" message of compiler
#endif
}
