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

#include "sccWrite.h"

// The LOG_DEBUG enables extensive debug messages if set to "1" (should be 0 for releases)
#define LOG_DEBUG 0

sccWrite::sccWrite() {
  bool isDDR = false;
  bool isMpb = false;
  bool isCrb = false;
  bool isSif = false;
  int Route = -1;
  int subDestID = 0;
  uInt64 startAddress = 0;
  uInt64 value = 0;
  QString addressAlias = "";

  // Open message handler
  log = new messageHandler(true, "");
  CONNECT_MESSAGE_HANDLER;

  // Evaluate commandline args
  for (int loop = 0; loop < QCoreApplication::arguments().count()-1; loop++) {
    if ((QCoreApplication::arguments().at(loop+1) == "-h") || (QCoreApplication::arguments().at(loop+1) == "--help")) {
      printUsage(0);
    } else if (((QCoreApplication::arguments().at(loop+1) == "-d") || (QCoreApplication::arguments().at(loop+1) == "--ddr3")) ||
               ((QCoreApplication::arguments().at(loop+1) == "-m") || (QCoreApplication::arguments().at(loop+1) == "--mpb")) ||
               ((QCoreApplication::arguments().at(loop+1) == "-c") || (QCoreApplication::arguments().at(loop+1) == "--crb"))) {
      if (isDDR || isMpb || isCrb || isSif) {
        printError("Options -s, -d, -m and -c are mutually exclusive...");
        printUsage(-1);
      }
      isDDR = ((QCoreApplication::arguments().at(loop+1) == "-d") || (QCoreApplication::arguments().at(loop+1) == "--ddr3"));
      isMpb = ((QCoreApplication::arguments().at(loop+1) == "-m") || (QCoreApplication::arguments().at(loop+1) == "--mpb"));
      isCrb = ((QCoreApplication::arguments().at(loop+1) == "-c") || (QCoreApplication::arguments().at(loop+1) == "--crb"));
      if (QCoreApplication::arguments().count() < loop+5) {
        if (isDDR) {
          printError("Please provide 3 parameters to -d (--ddr3) option: <route> <address> <value>");
        } else if (isMpb) {
          printError("Please provide 3 parameters to -m (--mpb) option: <route> <address> <value>");
        } else {
          printError("Please provide 3 parameters to -c (--crb) option: <route> <address> <value>");
        }
        printUsage(-1);
      }
      bool okay;
      // Fetch route...
      Route = QCoreApplication::arguments().at(loop+2).toInt(&okay, 10);
      if (!okay) {
        Route = QCoreApplication::arguments().at(loop+2).toInt(&okay, 16);
      }
      if (!okay) {
        printError("Provide valid route...");
        printUsage(-1);
      }
      if (isDDR) {
        if ((Route == 0x00) || (Route == 0x20)) {
          subDestID = PERIW;
        } else if ((Route == 0x05) || (Route == 0x25)) {
          subDestID = PERIE;
        } else {
          printError("The given route 0x", QString::number(Route, 16), " is not valid...\nOnly 0x00, 0x20, 0x05 and 0x25 are valid MC routes!");
          printUsage(-1);
        }
      } else if (isMpb) {
        subDestID = MPB;
        if (((Route & 0x0f) > 5) || ((Route & 0xf0) > 0x30)) {
          printError("The given route 0x", QString::number(Route, 16), " is not valid...\nOnly 0xYX with X={0..5} and Y={0..3} are valid MPB routes!");
          printUsage(-1);
        }
      } else {
        subDestID = CRB;
        if (((Route & 0x0f) > 5) || ((Route & 0xf0) > 0x30)) {
          printError("The given route 0x", QString::number(Route, 16), " is not valid...\nOnly 0xYX with X={0..5} and Y={0..3} are valid CRB routes!");
          printUsage(-1);
        }
      }

      // Fetch address...
      startAddress = QCoreApplication::arguments().at(loop+3).toULongLong(&okay, 10);
      if (!okay) {
        startAddress = QCoreApplication::arguments().at(loop+3).toULongLong(&okay, 16);
      } else {
        addressAlias = messageHandler::toString(startAddress, 16,9);
      }
      if (!okay) {
        if (isCrb) {
          addressAlias = QCoreApplication::arguments().at(loop+3).toUpper();
          if (QCoreApplication::arguments().at(loop+3).toUpper() == "GLCFG0") {
            startAddress = GLCFG0;
          } else if (QCoreApplication::arguments().at(loop+3).toUpper() == "GLCFG1") {
            startAddress = GLCFG1;
          } else if (QCoreApplication::arguments().at(loop+3).toUpper() == "L2CFG0") {
            startAddress = L2CFG0;
          } else if (QCoreApplication::arguments().at(loop+3).toUpper() == "L2CFG1") {
            startAddress = L2CFG1;
          } else if (QCoreApplication::arguments().at(loop+3).toUpper() == "SENSOR") {
            startAddress = SENSOR;
          } else if (QCoreApplication::arguments().at(loop+3).toUpper() == "GCBCFG") {
            startAddress = GCBCFG;
          } else if (QCoreApplication::arguments().at(loop+3).toUpper() == "MYTILEID") {
            startAddress = MYTILEID;
          } else if (QCoreApplication::arguments().at(loop+3).toUpper() == "LOCK0") {
            startAddress = LOCK0;
          } else if (QCoreApplication::arguments().at(loop+3).toUpper() == "LOCK1") {
            startAddress = LOCK1;
          } else {
            printError("Provide valid start address or register symbol (e.g. GCBCFG)...");
            printUsage(-1);
          }
        } else {
          printError("Provide valid start address...");
          printUsage(-1);
        }
      } else {
        addressAlias = messageHandler::toString(startAddress, 16,9);
      }
      // Sanity checks...
      if (isCrb && startAddress > 0x1800) {
        printError("Maximum valid CRB address is 0x17f8, but you provided ",messageHandler::toString(startAddress, 16, 8),"...");
        printUsage(-1);
      }
      if (isDDR || isMpb) {
        if (startAddress % 0x10ul) {
          printError("The given address 0x", QString::number(startAddress, 16), " is not 64 bit aligned!\nTry again with 64 bit aligned address (e.g. 0x", QString::number(startAddress-(startAddress%0x10), 16), ")!");
          printUsage(-1);
        }
      } else {
        if (startAddress % 0x08) {
          printError("The given address 0x", QString::number(startAddress, 16), " is not 32 bit aligned!\nTry again with 32 bit aligned address (e.g. 0x", QString::number(startAddress-(startAddress%0x08), 16), ")!");
          printUsage(-1);
        }
      }
      // Fetch value...
      value = QCoreApplication::arguments().at(loop+4).toULongLong(&okay, 10);
      if (!okay) {
        value = QCoreApplication::arguments().at(loop+4).toULongLong(&okay, 16);
      }
      if (!okay) {
        printError("Provide valid value...");
        printUsage(-1);
      }
      if (isCrb && (value >= 0x100000000ull)) {
        printError("Value exceeds 32 bits...");
        printUsage(-1);
      }
      loop+=3;
    } else if ((QCoreApplication::arguments().at(loop+1) == "-s") || (QCoreApplication::arguments().at(loop+1) == "--sif")) {
      if (isDDR || isMpb || isCrb || isSif) {
        printError("Options -s, -d, -m and -c are mutually exclusive...");
        printUsage(-1);
      }
      isSif = true;
      if (QCoreApplication::arguments().count() < loop+4) {
        printError("Please provide one parameter to -s (--sif) option: <address> <value>");
        printUsage(-1);
      }
      bool okay;

      // Fetch address...
      startAddress = QCoreApplication::arguments().at(loop+2).toULongLong(&okay, 10);
      if (!okay) {
        startAddress = QCoreApplication::arguments().at(loop+2).toULongLong(&okay, 16);
      }
      if (!okay) {
        printError("Provide valid start address...");
        printUsage(-1);
      }
      if (startAddress % 0x08) {
        printError("The given address 0x", QString::number(startAddress, 16), " is not 32 bit aligned!\nTry again with 32 bit aligned address (e.g. 0x", QString::number(startAddress-(startAddress%0x08), 16), ")!");
        printUsage(-1);
      }
      // Fetch value...
      value = QCoreApplication::arguments().at(loop+3).toULongLong(&okay, 10);
      if (!okay) {
        value = QCoreApplication::arguments().at(loop+3).toULongLong(&okay, 16);
      }
      if (!okay) {
        printError("Provide valid value...");
        printUsage(-1);
      }
      if (value >= 0x100000000ull) {
        printError("Value exceeds 32 bits...");
        printUsage(-1);
      }
      loop+=2;
    }
  }

  if (!isSif && !isDDR && !isMpb && !isCrb) {
    printError("Please provide some arguments...");
    printUsage(-1);
  }

  // Invoke sccApi
  log->setShell(false); // Only write these details to logfile...
  sccAccess = new sccExtApi(log);
  // sccAccess->openBMCConnection(); // Not needed!?

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

  // Tell user if we succeeded...
  log->setShell(true); // Re-enable printing to the screen...
  if (!initFailed) {
    printInfo("Welcome to sccWrite ", VERSION_TAG, (QString)((BETA_FEATURES!=0)?"beta":""), " (build date ", __DATE__, " - ", __TIME__, ")...");
  } else {
    printError("Unable to connect to PCIe driver...\n       Make sure that PCIe driver is installed and\n       not in use by someone else... Aborting!");
    qApp->processEvents(); // Update logfile
    exit(-1);
  }

  // Action...
  if (isDDR || isMpb) {
    uInt64 write_data[4]={0,0,0,0};
    write_data[0]=value;
    if (sccAccess->transferPacket(Route, subDestID, startAddress, NCWR, 0xff, (uInt64*)&write_data, SIF_RC_PORT)) {
      if (isDDR) {
        printInfo("Wrote ", messageHandler::toString(value, 16,16), " to address ", addressAlias, " in DDR3 memory (MC of Tile ",messageHandler::toString(Route, 16,2),")...");
      } else if (isMpb) {
        printInfo("Wrote ", messageHandler::toString(value, 16,16), " to address ", addressAlias, " in MPB of Tile ",messageHandler::toString(Route, 16,2),"...");
      }
    } else {
      printError("Failed to write to requested destination...");
    }
  } else if (isCrb) {
    if (sccAccess->writeFlit(Route, subDestID, startAddress, value, SIF_RC_PORT)) {
      printInfo("Wrote ", messageHandler::toString(value, 16,8), " to address ", addressAlias, " in CRB of Tile ",messageHandler::toString(Route, 16,2),"...");
    } else {
      printError("Failed to write to requested destination...");
    }
  } else {
    if (sccAccess->writeFpgaGrb(startAddress, value)) {
      printInfo("Wrote ",messageHandler::toString(value, 16,8)," to SIF-GRB address ", messageHandler::toString(startAddress, 16,9), "...");
    } else {
      printError("Failed to write to SIF-GRB register...");
    }
  }
}

sccWrite::~sccWrite() {
  if (sccAccess) delete sccAccess;
  delete log;
}

void sccWrite::printUsage(int exitCode) {
  QString usage=(QString)((exitCode)?"\n":"") + "\
Usage: " + QCoreApplication::arguments().at(0) + " [option]\n\
\n\
Options:\n\
  -h or --help: Print this message and exit...\n\
  -d or --ddr3 <route> <64 bit aligned address> <value>: Write 64 bit to memory controller...\n\
  -m or --mpb <route> <64 bit aligned address> <value>: Write 64 bit to message passing buffer...\n\
  -c or --crb <route> <32 bit aligned address> <value>: Write 32 bit configuration register or LUT entry of tile <route>...\n\
  -s or --sif <32 bit aligned address> <value>: Write 32 bit system interface FPGA register at address <address> (e.g. 0x8010)...\n\
\n\
This application will write memory content (or memory mapped registers) to the SCC\n\
Platform... It's possible to address the MCs, the MPBs or the CRBs...\n\
Access is always 32 bit. Addresses also need to be 32-bit aligned and will be truncated otherwise... If you need\n\
to write smaller chunks, you need to perform read-modify-write. This limitation may disappear in future versions...";

  log->setMsgPrefix(log_info, "");
  printInfo(usage);
  log->setMsgPrefix(log_info, "INFO");
  exit(exitCode);
}
