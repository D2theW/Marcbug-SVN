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

#include "sccBmc.h"

// The LOG_DEBUG enables extensive debug messages if set to "1" (should be 0 for releases)
#define LOG_DEBUG 0

QString init = "";
QString file = "";
QString cmd = "";
QString host = "";
bool fastRpc = false;
sccBmc::sccBmc() {
  // Load settings
  settings = new QSettings("sccKit", "sccGui");
  noCheck = false;

  // Create UI and message handler
  QFont font;
  log = new messageHandler(true, "", NULL, NULL);
  CONNECT_MESSAGE_HANDLER;
  #if LOG_DEBUG == 1
    log->setLogLevel(log_debug);
  #endif

  // Parse commandline...
  if (QCoreApplication::arguments().count() < 2) {
    printError("Please provide valid parameters...");
    printUsage(-1);
  } else if ((QCoreApplication::arguments().at(1) == "-h") || (QCoreApplication::arguments().at(1) == "--help")) {
    printUsage(0);
} else if ((QCoreApplication::arguments().at(1) == "-i") || (QCoreApplication::arguments().at(1) == "--init") ||
           (QCoreApplication::arguments().at(1) == "-p") || (QCoreApplication::arguments().at(1) == "--preset") ||
           (QCoreApplication::arguments().at(1) == "-r") || (QCoreApplication::arguments().at(1) == "--rpcFast")) {
    if ((QCoreApplication::arguments().at(1) == "-p") || (QCoreApplication::arguments().at(1) == "--preset")) {
      noCheck = true;
    }
    if ((QCoreApplication::arguments().at(1) == "-r") || (QCoreApplication::arguments().at(1) == "--rpcFast")) {
      fastRpc = true;
    }
    if (QCoreApplication::arguments().count() > 3) {
      printError("Too many arguments specified for -i option...");
      printUsage(-1);
    } else if (QCoreApplication::arguments().count() == 3) {
      init = QCoreApplication::arguments().at(2);
    } else {
      init = "select";
    }
  } else if ((QCoreApplication::arguments().at(1) == "-l") || (QCoreApplication::arguments().at(1) == "--list")) {
    if (QCoreApplication::arguments().count() > 2) {
      printError("Too many arguments specified for -l option...");
      printUsage(-1);
    }
    init = "list";
  } else if ((QCoreApplication::arguments().at(1) == "-f") || (QCoreApplication::arguments().at(1) == "--file")) {
    if (QCoreApplication::arguments().count() < 3) {
      printError("No file specfied...");
      printUsage(-1);
    } else if (QCoreApplication::arguments().count() == 4) {
      host = QCoreApplication::arguments().at(3);
    } else if (QCoreApplication::arguments().count() > 4) {
      printError("Too many arguments for -f option...");
      printUsage(-1);
    }
    file = QCoreApplication::arguments().at(2);
  } else if ((QCoreApplication::arguments().at(1) == "-c") || (QCoreApplication::arguments().at(1) == "--command")) {
    if (QCoreApplication::arguments().count() < 3) {
      printError("No file specfied...");
      printUsage(-1);
    } else if (QCoreApplication::arguments().count() == 4) {
      host = QCoreApplication::arguments().at(3);
    } else if (QCoreApplication::arguments().count() > 4) {
      printError("Too many arguments for -c option...");
      printUsage(-1);
    }
    cmd = QCoreApplication::arguments().at(2);
  } else {
    printError("Please provide valid argument...");
    printUsage(-1);
  }

  // Invoke sccExtApi and prepare widgets as well as semaphores...
  sccAccess = new sccExtApi(log);
  if (host != "") {
    sccAccess->setBMCServer(host);
  }
  sccAccess->openBMCConnection();

  // Open driver if necessary...
  bool initFailed = false;
  if ((init != "") && (init != "list")) {
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
  }

  // Don't open GUI if initialization failed. Otherwise open it...
  if (!initFailed) {
    printInfo("Welcome to sccBmc ", VERSION_TAG, (QString)((BETA_FEATURES!=0)?"beta":""), " (build date ", __DATE__, " - ", __TIME__, ")...");
  } else {
    printError("Unable to connect to PCIe driver...\n       Make sure that PCIe driver is installed and\n       not in use by someone else... Aborting!");
    exit(-1);
  }
}

sccBmc::~sccBmc() {
  delete settings;
  if (sccAccess) delete sccAccess;
  delete log;
}

void sccBmc::startInitialization() {
  if (init != "") {
    int defaultItem = 0;
    QFileInfoList settingFiles;
    QStringList settingNames;
    QMap <int, int> settingFrequency;
    QString sccKitPath = QCoreApplication::applicationDirPath();
    sccKitPath.replace(QRegExp("bin$"), "");
    QDir dir(sccKitPath + "settings");
    if (dir.exists()) {
      settingFiles = dir.entryInfoList((QStringList)("*setting."+sccAccess->platformSetupSuffix()));
      for (int i = 0; i < settingFiles.length(); i++) {
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
    if (!settingNames.length()) {
      printError("Didn't find settings in folder \"", dir.path(), "\"... Aborting!");
      return;
    }
    int selection;
    if (init == "select") {
      QString message = (QString)"\
This tool allows you to (re-)initialize the SCC platform.\n\
This means that the reset of the actual SCC device is triggered and\n\
that all clock settings are programmed from scratch. The (re-)\n\
programming of the clock settings also implies a training of the\n\
physical System Interface!\n\n\
Short said: The whole procedure takes a while and you should only do\n\
it when necessary! This step is NOT always required after starting\n\
the GUI. You would normally invoke it when the system reset executes\n\
with errors or when the board has just been powered up...\n\n\
Please select from the following possibilities:";
      printInfo(message);
      for (int i = 0; (i < settingFiles.length()) && (i < 10); i++) {
        printInfo("(", QString::number(i), ") ", settingNames[i]);
      }
      printInfo("(others) Abort!");
      printf("Make your selection: ");
      selection = getchar() - 48;
    } else { // Init = user setting or "list"
      selection = -1;
      if (init != "list") {
        for (int i = 0; (i < settingFiles.length()) && (i < 10); i++) {
          if (settingNames[i].toLower() == init.toLower()) {
            selection = i;
          }
        }
      }
      if (selection == -1) {
        if (init != "list") {
          printWarning("The selection that you made (",init,") is not valid...");
        }
        printInfo("Valid initialization settings are:");
        for (int i = 0; (i < settingFiles.length()) && (i < 10); i++) {
          printInfo("=> ", settingNames[i]);
        }
        selection = settingFiles.length() + 1;
      }
    }
    if (selection < settingFiles.length()) {
      printInfo("Starting system initialization (with setting ", settingNames[selection], ")...");
      sccProgressBar pd(false);
      sccAccess->processRCCFile(settingFiles[selection].absoluteFilePath().replace("_setting.", "_preset."), false, ECHO_OFF_RW, &pd);
      if (!sccAccess->trainSIF(noCheck?PRESET_NO_CHECK:AUTO_MODE, settingFrequency[selection])) {
        printInfo("Configuring memory controllers:");
        QString retval;
        pd.reset();
        retval = sccAccess->processRCCFile(settingFiles[selection].absoluteFilePath(), sccAccess->getPlatform() != "Copperridge", ECHO_OFF_RW, (sccAccess->getPlatform() != "Copperridge")?NULL:&pd);
        if (retval.contains("MEMRD ERROR")) {
          printInfo("Repeating configuration of memory controllers...");
          pd.reset();
          retval = sccAccess->processRCCFile(settingFiles[selection].absoluteFilePath(), sccAccess->getPlatform() != "Copperridge", ECHO_OFF_RW, (sccAccess->getPlatform() != "Copperridge")?NULL:&pd);
          if (retval.contains("MEMRD ERROR")) {
            printError("Configuration of memory controllers failed twice... Please refer to messages above for details...");
          }
        }
      }
      // Initialize SHM-COM pointer area with all zeros...
      uInt64 nullData[4]={0,0,0,0};
      perfApertureStart = settings->value("perfMeter/ApertureStart", 0).toULongLong();
      for (uInt64 loopAddr=perfApertureStart-PERF_SHM_OFFSET; loopAddr < (perfApertureStart-PERF_SHM_OFFSET+0x1000); loopAddr+=0x20) {
        sccAccess->transferPacket(0x00, PERIW, loopAddr, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT, BCMC);
      }
      // Initialize GCB registers corresonding to JTAG content...
      sccAccess->initCrbGcbcfg();
      // Re-programm GRB registers, just in case that someone modified the correspondingm settings...
      sccAccess->programAllGrbRegisters();
      // Enable fast RPC, if requested...
      if (fastRpc) {
        sccAccess->enableFastRpc();
        printInfo("Enabled fast RPC mode (10MHz instead of 0.65MHz)...");
      }
    }
  } else if (file != "") {
    printInfo("Executing command-file \"", file,"\":");
    if (sccAccess->processRCCFile(file, true) == "") {
      printUsage(-1);
    }
  } else if (cmd != "") {
    QString tmpString = sccAccess->executeBMCCommand(cmd);
    if (tmpString != "") {
      printInfo("Result of BMC command \"", cmd,"\":\n", tmpString);
    }
  }
}

void sccBmc::printUsage(int exitCode) {
  QString usage=(QString)((exitCode)?"\n":"") + "\
Usage: " + QCoreApplication::arguments().at(0) + " [option]\n\
\n\
Options:\n\
  -h or --help: Print this message and exit...\n\
  -l or --list: Show list of available settings.\n\
  -f or --file <rcc script> [<bmc>]: Execute BMC batch file <rcc script>...\n\
        The optional parameter <bmc> allows you to remote control another BMC with\n\
        hostname or IP address  <bmc> (e.g. 123.123.123.123:5010)... By default the\n\
        BMC specified in \"/opt/sccKit/systemSettings.ini\" will be used...\n\
  -c or --command \"<cmd>\" [<bmc>]: Execute BMC command <cmd>... \n\
        Please use quotation marks if command contains spaces...\n\
        e.g.: " + QCoreApplication::arguments().at(0) + " -c \"help status\"\n\
        The optional parameter <bmc> allows you to remote control another BMC with\n\
        hostname or IP address  <bmc> (e.g. 123.123.123.123:5010)... By default the\n\
        BMC specified in \"/opt/sccKit/systemSettings.ini\" will be used...\n\
  -i or --init [<setting>]: \n\
        Initialize attached SCC Platform. If <setting> is\n\
        missing, a selection dialog will show up.\n\
        e.g.: " + QCoreApplication::arguments().at(0) + " -i Tile533_Mesh800_DDR800\n\
  -p or --preload [<setting>]: Exactly like \"-i\" option, but does not perform checks\n\
        and it will only work after a hardware power-cycle. Usually only used for \n\
        platform selfboot via board management controller...\n\
  -r or --rpcFast [<setting>]: Exactly like \"-i\" option, but enabling the fast RPC \n\
        clock. This option should only be used when you plan to use DVFS features...\n\
\n\
This application will either initialize the attached SCC Platform with one of\n\
default settings or execute one or more dedicated BMC commands (e.g. \"status\")...\n\
BMC stands for \"board management controller\"...";
  log->setMsgPrefix(log_info, "");
  printInfo(usage);
  log->setMsgPrefix(log_info, "INFO");
  exit(exitCode);
}

