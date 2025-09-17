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

#include "sccBoot.h"

// The LOG_DEBUG enables extensive debug messages if set to "1" (should be 0 for releases)
#define LOG_DEBUG 0

sccBoot::sccBoot() {
  // Open message handler
  log = new messageHandler(true, "");
  CONNECT_MESSAGE_HANDLER;

  // Evaluate commandline args
  linuxMask.disableAll();
  int pid, rangeStart, rangeEnd;
  bool okay;
  argMemory = false;
  argLinux = false;
  argGeneric = "";
  userImage = "";
  showStatus = false;
  if (QCoreApplication::arguments().count()<2) {
    printWarning("Please provide valid arguments...");
    printUsage(1);
  }
  for (int loop = 1; loop < QCoreApplication::arguments().count(); loop++) {
    if ((QCoreApplication::arguments().at(loop).toLower() == "-h") || (QCoreApplication::arguments().at(loop).toLower() == "--help")) {
      printUsage(0);
    } else if ((QCoreApplication::arguments().at(loop).toLower() == "-s") || (QCoreApplication::arguments().at(loop).toLower() == "--status")) {
      if (argLinux || argGeneric != "" || argMemory) {
        printError("Please don't mix -l, -s, -g and -m options... Aborting!");
        printUsage(1);
      }
      showStatus = true;
    } else if ((QCoreApplication::arguments().at(loop).toLower() == "-m") || (QCoreApplication::arguments().at(loop).toLower() == "--memory")) {
      if (showStatus || argGeneric != "" || argLinux) {
        printError("Please don't mix -l, -s, -g and -m options... Aborting!");
        printUsage(1);
      }
      argMemory = true;
    } else if ((QCoreApplication::arguments().at(loop).toLower() == "-l") || (QCoreApplication::arguments().at(loop).toLower() == "--linux")) {
      if (showStatus || argGeneric != "" || argMemory) {
        printError("Please don't mix -l, -s, -g and -m options... Aborting!");
        printUsage(1);
      }
      argLinux = true;
    } else if ((QCoreApplication::arguments().at(loop).toLower() == "-g") || (QCoreApplication::arguments().at(loop).toLower() == "--generic")) {
      if (showStatus || argLinux || argMemory) {
        printError("Please don't mix -l, -s, -g and -m options... Aborting!");
        printUsage(1);
      }
      if (QCoreApplication::arguments().count()<=(loop+1)) {
        printWarning("Please provide valid obj-path...");
        printUsage(1);
      }
      // Possible object names: mch_0_0.32.obj  mch_0_2.32.obj  mch_5_0.32.obj  mch_5_2.32.obj
      argGeneric = QCoreApplication::arguments().at(loop+1);
      QFileInfo tmpInfo = argGeneric+"/mch_0_0.32.obj";
      if (!tmpInfo.isReadable()) {
        tmpInfo = argGeneric+"/mch_0_2.32.obj";
        if (!tmpInfo.isReadable()) {
          tmpInfo = argGeneric+"/mch_5_0.32.obj";
          if (!tmpInfo.isReadable()) {
            tmpInfo = argGeneric+"/mch_5_2.32.obj";
            if (!tmpInfo.isReadable()) {
              printWarning("Please provide valid obj-path (given path doesn't contain memory image)...");
              printUsage(1);
            }
          }
        }
      }
      tmpInfo = argGeneric+"/lut_init.dat";
      if (!tmpInfo.isReadable()) {
        printWarning("Please provide valid obj-path (given path doesn't contain LUT file)...");
        printUsage(1);
      }
      loop++;
    } else if (argLinux) {
      bool validArgument = false;
      QFileInfo tmpInfo = QCoreApplication::arguments().at(loop);
      if (QCoreApplication::arguments().at(loop).contains(QRegExp("[.]obj$")) && tmpInfo.isReadable()) {
        // It's a file with ending ".obj"... So we'll assume that the user passed a linux image and use it...
        userImage = QCoreApplication::arguments().at(loop);
        validArgument = true;
      } else if (QCoreApplication::arguments().at(loop).contains(QRegExp("[.]+"))) {
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
                  linuxMask.setEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid), true);
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
            linuxMask.setEnabled(X_PID(pid), Y_PID(pid), Z_PID(pid), true);
          }
        }
      }
    } else {
      printWarning("The provided argument \"", QCoreApplication::arguments().at(loop), "\" is invalid...");
      printUsage(-1);
    }
  }

  // No args -> Use all cores
  if (!linuxMask.getNumCoresEnabled()) {
    linuxMask.enableAll();
  }

  // Invoke sccApi
  bool initFailed = false;
  if (!showStatus) { // Don't open driver to show status...
    log->setShell(false); // Only write these details to logfile...
    sccAccess = new sccExtApi(log);
    sccAccess->openBMCConnection();

    // Open driver...
    if (sccAccess->connectSysIF()) {
#ifndef BMC_IS_HOST
      printInfo("Successfully connected to PCIe driver...");
#else
      printInfo("Successfully connected to System Interface FPGA BMC-Interface...");
#endif
    } else {
      initFailed = true;
    }
  } else {
    // For the status we only need to create the object w/o connecting the SIF.
    sccAccess = new sccExtApi(log);
  }

  // Load settings
  settings = new QSettings("sccKit", "sccGui");

  // Tell user if we succeeded...
  log->setShell(true); // Re-enable printing to the screen...
  if (!initFailed) {
    printInfo("Welcome to sccBoot ", VERSION_TAG, (QString)((BETA_FEATURES!=0)?"beta":""), " (build date ", __DATE__, " - ", __TIME__, ")...");
  } else {
    printError("Unable to connect to PCIe driver...\n       Make sure that PCIe driver is installed and\n       not in use by someone else... Aborting!");
    qApp->processEvents(); // Update logfile
    exit(-1);
  }
}

sccBoot::~sccBoot() {
  delete settings;
  if (sccAccess) delete sccAccess;
  delete log;
}

void sccBoot::go() {
  if (argLinux) {
      bootLinux();
  } else if (argGeneric!="") {
    loadGeneric(argGeneric);
  } else if (showStatus) {
    linuxMask.pingCores(sccAccess->getSccIp(0));
    log->setMsgPrefix(log_info, "Status");
    printInfo("The following cores can be reached with ping (booted): ", linuxMask.getMaskString());
    return;
  } else if (argMemory) {
    testMemory();
    return;
  }
}

// bootLinux(): This Routine runs an MMIO based Linux build on selected cores (dialog). No
// checks are performed. The Linux images are distributed on all RCK memory Controllers!
void sccBoot::bootLinux() {
  // Variables & initializations...
  int shmSIFPort = SIF_RC_PORT;
  QByteArray errMsg;

  // Start booting...
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
  QString cmdLine = settings->value("bootLinux/cmdLine", "").toString();
  if (cmdLine!="") printInfo("Custom kernel parameters: \""+cmdLine+"\"...");
  cmdLine = "priv_core_space=" + QString::number(sccAccess->getSizeOfMC()*80 - videoBufferSize) +
            " video=rckgfx:base:" + QString::number(sccAccess->getSizeOfMC()*80 - videoBufferSize) + ",size:" + QString::number(videoBufferSize) + ",x:" + QString::number(sccAccess->getDisplayX()) + ",y:" + QString::number(sccAccess->getDisplayY()) + ",bpp:" + QString::number(sccAccess->getDisplayDepth()) +
            " popshmpages=" + QString::number(popShmPages) +
            (QString)((cmdLine != "")?" "+cmdLine:"");

  // Do we have a pre-merged image and want to start on 48 cores?
  QString linuxImage;
  if (userImage != "") {
    linuxImage = userImage;
    if (!linuxImage.contains("/")) linuxImage = "./"+linuxImage; // Makes sure that the following regular expressions work...
    printInfo("Using linux image \"", linuxImage, "\" (provided by user)...");
  } else {
    linuxImage = settings->value("bootLinux/linuxImage", getSccKitPath() + "resources/linux.obj").toString();
    printInfo("Using linux image \"", linuxImage, "\" (default image as defined by sccGui \"Settings->Linux boot settings\")...");
  }
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
          if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,true)) return;
        }
      }
    }

    // Preload objects & LUT
    printInfo("Preloading Memory with pre-merged linux object file \"", linuxPremerged, "\"...");
    bool retVal;
    sccProgressBar pd(false);
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
    retVal = sccAccess->writeMemFromOBJ(0x00, PERIW, copycatImage, &pd, SIF_RC_PORT, BCMC);
    printInfo("Preloading copycat LUTs...");
    if (!sccAccess->WriteFlitsFromFile(copycatDat, "LUTs", &pd)) return;

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
          if (!sccAccess->setL2(TID(loop_x, loop_y),loop_core,settings->value("bootLinux/EnableL2", true).toBool())) return;
          if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,false)) return;
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
        return;
      }
    }

    // Pull all resets
    printInfo("Duplication of linux images completed successfully!");
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,true)) return;
        }
      }
    }

    printInfo("Patching LUTs for boot of Linux...");
    if (!sccAccess->WriteFlitsFromFile(linuxDat, "LUTs")) return;

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
    settings->setValue("perfMeter/Route", (tmp&0x01fe000)>>13);
    settings->setValue("perfMeter/Destid", (tmp&0x01c00)>>10);
    settings->setValue("perfMeter/SIFPort", shmSIFPort);
    uInt64 perfApertureStart = ((tmp&0x3ff)<<24)+PERF_SHM_OFFSET;
    if ((shmSIFPort==SIF_MC_PORT) && (perfApertureStart >= 0x180000000ull)) perfApertureStart -= 0x180000000ull;
    settings->setValue("perfMeter/ApertureStart", perfApertureStart);

    // Initialize SHM-COM pointer area with all zeros...
    for (uInt64 loopAddr=perfApertureStart-PERF_SHM_OFFSET; loopAddr < (perfApertureStart-PERF_SHM_OFFSET+0x1000); loopAddr+=0x20) {
      sccAccess->transferPacket(0x00, PERIW, loopAddr, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT, BCMC);
    }

    // (Re-)Configure driver
    sccAccess->setTxBaseAddr(perfApertureStart-PERF_SHM_OFFSET+0x200000); // Substract Performance meter offset and add SHM Host driver offset (0x200000)
    sccAccess->setSubnetSize(1);

    // Re-programm GRB registers, just in case that someone modified the correspondingm settings...
    sccAccess->programAllGrbRegisters();

    // Release resets...
    printInfo("Releasing resets...");
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,false)) return;
        }
      }
    }

    // That's it...
    printInfo("Linux has been started successfully. Cores should be reachable via TCP/IP shortly...");
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
    return;
  }
  if ((errMsg = sccMerge->readAllStandardError()) != "") {
    printError("Failed to process mt file \"",sccAccess->getTempPath(),"linux.mt\"... Aborting!");
    printError("sccMerge error message:\n", errMsg);
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
  printInfo("Pulling resets and ", settings->value("bootLinux/EnableL2", true).toBool()?"en":"dis", "abling L2 caches: ", linuxMask.getMaskString());
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (linuxMask.getEnabled(loop_x,loop_y,loop_core)) {
          if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,true)) return;
          if (!sccAccess->setL2(TID(loop_x, loop_y),loop_core,settings->value("bootLinux/EnableL2", true).toBool())) return;
        }
      }
    }
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

  // Load objects
  printInfo("Preloading Memory with object file...");
  qApp->processEvents(); // Update log
  bool retVal;
  sccProgressBar pd(false);
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
  pd.hide();

  if (retVal != SUCCESS) {
    if (retVal == ERROR) printWarning("bootLinux(): Unable to preload memory... Aborting!");
    return;
  }

  // Load LUTs...
  printInfo("Preloading LUTs...");
  if (!sccAccess->WriteFlitsFromFile((sccAccess->getTempPath()+"obj/lut_init.dat"), "LUTs", &pd)) return;

  // Find out SHM location
  uInt64 tmp = linuxMask.getFirstActiveCore();
  tmp = sccAccess->readFlit(TID_PID(tmp), CRB, MC_SHMBASE_MC0+((Z_PID(tmp))?LUT1:LUT0));
  settings->setValue("perfMeter/Route", (tmp&0x01fe000)>>13);
  settings->setValue("perfMeter/Destid", (tmp&0x01c00)>>10);
  settings->setValue("perfMeter/SIFPort", shmSIFPort);
  uInt64 perfApertureStart = ((tmp&0x3ff)<<24)+PERF_SHM_OFFSET;
  if ((shmSIFPort==SIF_MC_PORT) && (perfApertureStart >= 0x180000000ull)) perfApertureStart -= 0x180000000ull;
  settings->setValue("perfMeter/ApertureStart", perfApertureStart);
  sccAccess->writeFpgaGrb(SIFGRB_PMEMSLOT, 0); // TODO: Replace PMEMSLOT with non-default value

  // Initialize SHM-COM pointer area with all zeros...
  for (uInt64 loopAddr=perfApertureStart-PERF_SHM_OFFSET; loopAddr < (perfApertureStart-PERF_SHM_OFFSET+0x1000); loopAddr+=0x20) {
    sccAccess->transferPacket(0x00, PERIW, loopAddr, WBI, 0xff, (uInt64*)&nullData, SIF_RC_PORT, BCMC);
  }

  // (Re-)Configure driver
  sccAccess->setTxBaseAddr(perfApertureStart-PERF_SHM_OFFSET+0x200000); // Substract Performance meter offset and add SHM Host driver offset (0x200000)
  sccAccess->setSubnetSize(1);

  // Re-programm GRB registers, just in case that someone modified the correspondingm settings...
  sccAccess->programAllGrbRegisters();

  // Release resets...
  printInfo("Releasing resets...");
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (linuxMask.getEnabled(loop_x,loop_y,loop_core)) {
          if (!sccAccess->toggleReset(TID(loop_x, loop_y),loop_core)) return;
        }
      }
    }
  }

  // Update attributes for use with performance meter in sccGui...

  // That's it...
  printInfo("Linux has been started successfully. Cores should be reachable via TCP/IP shortly...");
  return;
}

// loadGeneric(): Preloads MCs and LUTs with generic content (pre-merged with sccMerge)
void sccBoot::loadGeneric(QString objPath) {
  // Variables & initializations...
  int shmSIFPort = SIF_RC_PORT;
  QByteArray errMsg;

  // Pull all resets!
  printInfo("Pulling reset of all cores...");
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,true)) return;
      }
    }
  }

  // Load objects
  printInfo("Preloading Memory with object file...");
  qApp->processEvents(); // Update log
  sccAccess->loadOBJ(objPath+"/mch_0_0.32.obj", NULL, shmSIFPort);

  // Load LUTs...
  printInfo("Preloading LUTs...");
  if (!sccAccess->WriteFlitsFromFile((objPath+"/lut_init.dat"), "LUTs")) return;

  // That's it...
  printInfo("Image is now pre-loaded. Release resets to start individual cores...");
  return;
}

// #define PATTERNSIZE 0x08000ul
#define PATTERNSIZE 2097152
unsigned char *testPattern = NULL;
unsigned char *nullPattern = NULL;
unsigned char *compPattern = NULL;
void sccBoot::testMemory() {
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
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (!sccAccess->setReset(TID(loop_x, loop_y),loop_core,true)) return;
      }
    }
  }

  // Now start testing...
  printfInfo("Sweeping through memory controllers (%0dGB per MC):", sccAccess->getSizeOfMC());
  int mcRoute[4] = {0x00, 0x05, 0x20, 0x25};
  int mcDest[4] = {PERIW, PERIE, PERIW, PERIE};
  bool abort = false;
  int errCnt = 0;
  QString msg;
  sccProgressBar *pd = new sccProgressBar(false);
  uInt64 progress;
  pd->setMinimum(0);
  pd->setMaximum(((sccAccess->getSizeOfMC()*SIZE_1GB)-PATTERNSIZE)/4);
  for (int loopMC=0; (loopMC < 4) && !abort; loopMC++) {
    progress = 0;
    msg = "Checking MC " + messageHandler::toString(mcRoute[loopMC], 16, 2) + "-" + sccAccess->decodeDestID(mcDest[loopMC],false) + "...";
    // Write pattern
    printInfo(msg);
    printInfo(msg.replace("Checking", "Preparing").replace("...", " by writing random pattern..."));
    pd->show();
    for (uInt64 loopAddr=0; (loopAddr < sccAccess->getSizeOfMC()*SIZE_1GB) && !abort; loopAddr+=PATTERNSIZE) {
      if (sccAccess->write32(mcRoute[loopMC], mcDest[loopMC], loopAddr, (char*)testPattern, PATTERNSIZE, SIF_RC_PORT) != PATTERNSIZE) {
        pd->hide();
        printError("Aborting because of error during DMA pattern write access (refer to log messages above for details)!" );
        abort = true;
      }
      // Update progress bar
      if (!(loopAddr % 3*PATTERNSIZE)) {
        pd->setValue((progress+=PATTERNSIZE)/4);
      }
      qApp->processEvents();
      if (pd->wasCanceled()) {
        pd->hide();
        printInfo(msg = "Action canceled by user... Stopping DDR3 memory check!");
        abort = true;
      }
    }
    // Read pattern and initialize with 0x00...
    if (!abort) {
      pd->hide();
      printInfo(msg.replace("Preparing", "Checking patterns and finally initializing").replace(" by writing random pattern...", " with all-zero..."));
      pd->show();
      qApp->processEvents();
      for (uInt64 loopAddr=0; (loopAddr < sccAccess->getSizeOfMC()*SIZE_1GB) && !abort; loopAddr+=PATTERNSIZE) {
        if (sccAccess->read32(mcRoute[loopMC], mcDest[loopMC], loopAddr, (char*)compPattern, PATTERNSIZE, SIF_RC_PORT) != PATTERNSIZE) {
          pd->hide();
          printError("Aborting because of error during DMA read access (refer to log messages above for details)!" );
          abort = true;
        } else if (sccAccess->write32(mcRoute[loopMC], mcDest[loopMC], loopAddr, (char*)nullPattern, PATTERNSIZE, SIF_RC_PORT) != PATTERNSIZE) {
          pd->hide();
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
            pd->hide();
            printError("Found error on DIMM J", QString::number(dimSlot), " (address ", messageHandler::toString(loopAddr+addr, 16, 9), " on MC ", messageHandler::toString(mcRoute[loopMC], 16, 2), "-", sccAccess->decodeDestID(mcDest[loopMC],false), ") -> Received ", messageHandler::toString(compPattern[addr],16,2), " but expected ", messageHandler::toString(testPattern[addr],16,2), "!" );
            pd->show();
            errCnt++;
          }
          if (errCnt >= 100) {
            pd->hide();
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
          pd->hide();
          printInfo(msg = "Action canceled by user... Stopping DDR3 memory check!");
          abort = true;
        }
      }
    }
    pd->hide();
  }
  delete pd;
  // Success message
  if (!errCnt && !abort) {
    printInfo("Success! All DDR3 memories have been verified...");
    printInfo("The whole address range is initialized with \"all zero\"!");
    exit(0);
  } else {
    exit(-1);
  }
}

void sccBoot::printUsage(int exitCode) {
  QString usage=(QString)((exitCode)?"\n":"") + "\
Usage: " + QCoreApplication::arguments().at(0) + " [option] \n\
\n\
Options:\n\
  -h or --help: Print this message and exit...\n\
  -s or --status: Pings all cores and shows which ones are reachable...\n\
     This only works when Linux (including network support) is booted!\n\
  -l or --linux [<linux image>] [<PID or PID range> .. <PID or PID range>]:\n\
     Boots Linux on all selected cores. PID or PID range: \n\
     Specifies a processor ID (PID). e.g. \"0\" or a range of PIDs separated\n\
     with dots. e.g. \"0..47\". If no PID or PID range is selected, Linux will\n\
     will be booted on all cores! If <linux image> is not provided, the current\n\
     setting of sccGui \"Settings->Linux boot settings\" will be used.\n\
  -g or --generic <obj path>: Specifies the path to a merged object (created\n\
     with sccMerge). This folder is normally nabmed \"obj\"... If the obj path\n\
     has been specified, the object file as well as the LUTs get preloaded.\n\
     Resets need to be changed with sccReset...\n\
  -m or --memory: Tests the complete DDR3 memory. This option takes a while and\n\
     usually only makes sense for production testing... After execution the whole\n\
     DDR3 system memory (of all four MCs) will be initialized with all zero.\n\
\n\
When booting Linux on selected PIDs, the Linux image to use as well as the L2\n\
settings are loaded from the sccGui configuration file. Thus, to select\n\
another image, enable/diable L2s or change the resolution of the virtual display\n\
please open sccGui and change the corresponding settings.";
  log->setMsgPrefix(log_info, "");
  printInfo(usage);
  log->setMsgPrefix(log_info, "INFO");
  exit(exitCode);
}

QString sccBoot::getSccKitPath() {
  QString result = QCoreApplication::applicationDirPath();
  result.replace(QRegExp("bin$"), "");
  return result;
}
