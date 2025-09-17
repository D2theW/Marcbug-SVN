#include "sccDump.h"

// The LOG_DEBUG enables extensive debug messages if set to "1" (should be 0 for releases)
#define LOG_DEBUG 0

// Initialize noMem...
QByteArray sccDump::noMem=0;

int init = -1;
QString file = "";
QString cmd = "";
sccDump::sccDump() {
  bool isDDR = false;
  bool isMpb = false;
  bool isCrb = false;
  bool isSif = false;

  // Load settings
  settings = new QSettings("sccKit", "sccGui");
  fileName = "";
  Route = -1;

  // Create UI and message handler
  QFont font;
  log = new messageHandler(true, "", NULL, NULL);
  #if LOG_DEBUG == 1
    log->setLogLevel(log_debug);
  #endif
  CONNECT_MESSAGE_HANDLER;

  // Parse commandline...
  if (QCoreApplication::arguments().count() < 2) {
    printError("Please provide valid parameters...");
    printUsage(-1);
  }

  for (int loop = 0; loop < QCoreApplication::arguments().count()-1; loop++) {
    if ((QCoreApplication::arguments().at(loop+1) == "-h") || (QCoreApplication::arguments().at(loop+1) == "--help")) {
      printUsage(0);
    } else if (((QCoreApplication::arguments().at(loop+1) == "-d") || (QCoreApplication::arguments().at(loop+1) == "--ddr3")) ||
               ((QCoreApplication::arguments().at(loop+1) == "-m") || (QCoreApplication::arguments().at(loop+1) == "--mpb")) ||
               ((QCoreApplication::arguments().at(loop+1) == "-c") || (QCoreApplication::arguments().at(loop+1) == "--crb"))) {
      int args = 1;
      if (isDDR || isMpb || isCrb || isSif) {
        printError("Options -s, -d, -m and -crb are mutually exclusive...");
        printUsage(-1);
      }
      isDDR = ((QCoreApplication::arguments().at(loop+1) == "-d") || (QCoreApplication::arguments().at(loop+1) == "--ddr3"));
      isMpb = ((QCoreApplication::arguments().at(loop+1) == "-m") || (QCoreApplication::arguments().at(loop+1) == "--mpb"));
      isCrb = ((QCoreApplication::arguments().at(loop+1) == "-c") || (QCoreApplication::arguments().at(loop+1) == "--crb"));
      if (QCoreApplication::arguments().count() <= loop+4) {
        if (isDDR) {
          printError("Please provide 3-4 parameters to -d (--ddr3) option: <route> <address> <bytecount> [<alignment>]");
          printUsage(-1);
        } else if (isMpb) {
          printError("Please provide 3-4 parameters to -m (--mpb) option: <route> <address> <bytecount> [<alignment>]");
          printUsage(-1);
        } else if (QCoreApplication::arguments().count() <= loop+2) {
          printError("Please provide 1 or 2 parameters to -c (--crb) option: <route> [<address>]");
          printUsage(-1);
        }
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
      args++;
      // Fetch start address...
      requestedStartAddress = (uInt64)-1;
      if ((QCoreApplication::arguments().count() > loop+3)) {
        startAddress = QCoreApplication::arguments().at(loop+3).toULongLong(&okay, 10);
        if (!okay) {
          startAddress = QCoreApplication::arguments().at(loop+3).toULongLong(&okay, 16);
          if (!okay) {
            if (isCrb) {
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
          }
        }
        requestedStartAddress=startAddress;
        if (isCrb && requestedStartAddress > 0x1800) {
          printError("Maximum valid CRB address is 0x17f8, but you provided ",messageHandler::toString(requestedStartAddress, 16, 8),"...");
          printUsage(-1);
        }
        args++;
      }
      // Fetch number of bytes to read...
      if ((QCoreApplication::arguments().count() > loop+4) && !isCrb) {
        numBytes = QCoreApplication::arguments().at(loop+4).toInt(&okay, 10);
        if (!okay) {
          numBytes = QCoreApplication::arguments().at(loop+4).toInt(&okay, 16);
        }
        if (!okay) {
          printError("Provide number of bytes...");
          printUsage(-1);
        }
        // validating number of bytes. Qutting if 0.
        if(!numBytes) {
          printInfo("Requested 0 bytes. We're done...");
          return;
        }
        args++;
      }
      // Fetch format...
      if (QCoreApplication::arguments().count() > loop+5 && !isCrb) {
        if ((QCoreApplication::arguments().at(loop+5) == "1") || (QCoreApplication::arguments().at(loop+5) == "2") ||
           (QCoreApplication::arguments().at(loop+5) == "4") || (QCoreApplication::arguments().at(loop+5) == "8")) {
          addrFact = QCoreApplication::arguments().at(loop+5).toInt(&okay, 10);
          if (!okay) {
            addrFact = QCoreApplication::arguments().at(loop+5).toInt(&okay, 16);
          }
          if (okay && (addrFact>0)) {
            if ((addrFact == FACT08BIT)||(addrFact == FACT16BIT)||(addrFact == FACT32BIT)||(addrFact == FACT64BIT)) {
              loop++;
            } else {
              printError("The given <alignment> argument is invalid. Please choose from {1, 2, 4, 8}...");
              printUsage(-1);
            }
          }
        } else {
          okay = false;
        }
        args++;
      } else {
        okay = false;
      }
      if (!okay) {
        addrFact = FACT08BIT;
      }
      loop += args;
    } else if ((QCoreApplication::arguments().at(loop+1) == "-s") || (QCoreApplication::arguments().at(loop+1) == "--sif")) {
      if (isDDR || isMpb || isCrb || isSif) {
        printError("Options -s, -d, -m and -crb are mutually exclusive...");
        printUsage(-1);
      }
      isSif = true;
      if (QCoreApplication::arguments().count() < loop+3) {
        printError("Please provide one parameter to -s (--sif) option: <address>");
        printUsage(-1);
      }
      bool okay;
      // Fetch start address...
      startAddress = QCoreApplication::arguments().at(loop+2).toULongLong(&okay, 10);
      if (!okay) {
        startAddress = QCoreApplication::arguments().at(loop+2).toULongLong(&okay, 16);
      }
      if (!okay) {
        printError("Provide valid start address...");
        printUsage(-1);
      }
      Route=0x03;
      loop++;
    } else if ((QCoreApplication::arguments().at(loop+1) == "-f") || (QCoreApplication::arguments().at(loop+1) == "--file")) {
      if (fileName != "") {
        printError("Only specify one filename for dumping...");
        printUsage(-1);
      }
      // Fetch filename...
      if (QCoreApplication::arguments().count() > loop+2) {
        fileName = QCoreApplication::arguments().at(loop+2);
      } else {
        printError("Please provide <filename> for -f || --file option...");
        printUsage(-1);
      }
      loop++;
    } else {
      printError("Please provide valid argument...");
      printUsage(-1);
    }
  }

  if (Route == -1) {
    printError("Please specify memory location to dump...");
    printUsage(-1);
  }

  // Invoke sccApi and prepare widgets as well as semaphores...
  sccAccess = new sccApi(log);
  // sccAccess->openBMCConnection(); -> Not needed for sccDump...

  // Open driver...
  bool initFailed = false;
  if (!sccAccess->connectSysIF()) {
    initFailed = true;
  } else {
    // Finally print success message!
#ifndef BMC_IS_HOST
    printInfo("Successfully connected to PCIe driver...");
#else
    printInfo("Successfully connected to System Interface FPGA BMC-Interface...");
#endif
    // Now that the driver is loaded, check FIFO size...
    if (numBytes > SIZE_1MB) {
      // Only show up to 1MB of memory...
      printInfo("Limit exceeded: Only showing first megabyte of requested data...");
      numBytes = SIZE_1MB;
    }
    numBytesRequested = numBytes;
  }

  // Don't open GUI if initialization failed. Otherwise open it...
  if (!initFailed) {
    printInfo("Welcome to sccDump ", VERSION_TAG, (QString)((BETA_FEATURES!=0)?"beta":""), " (build date ", __DATE__, " - ", __TIME__, ")...");
  } else {
    printError("Unable to connect to PCIe driver...\n       Make sure that PCIe driver is installed and\n       not in use by someone else... Aborting!");
    exit(-1);
  }
  // Read memory...
  if (subDestID == CRB) {
    QString lenmess;
    if (requestedStartAddress != (uInt64)-1) {
      lenmess = "CRB register at address " + messageHandler::toString(requestedStartAddress, 16,4) + " of Tile " + messageHandler::toString(Route, 16,2) + " = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, requestedStartAddress), 16, 8);
    } else {
      int lutEntry;
      uInt64 lock0 = 0, lock1 = 0;
      lenmess = "=============================================================";
      lenmess += "\nDumping CRB registers of Tile " + messageHandler::toString(Route, 16, 2);
      lenmess += "\n=============================================================";
      lenmess += "\nGLCFG0   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x010), 16, 8);
      lenmess += "\nGLCFG1   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x018), 16, 8);
      lenmess += "\nL2CFG0   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x020), 16, 8);
      lenmess += "\nL2CFG1   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x028), 16, 8);
      lenmess += "\nSENSOR   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x040), 16, 8);
      lenmess += "\nGCBCFG   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x080), 16, 8);
      lenmess += "\nMYTILEID = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x0100), 16, 8);
      lenmess += "\nLOCK0    = " + messageHandler::toString(lock0=(int)sccAccess->readFlit(Route, CRB, 0x0200), 16, 8);
      lenmess += "\nLOCK1    = " + messageHandler::toString(lock1=(int)sccAccess->readFlit(Route, CRB, 0x0400), 16, 8);
      if (lock0 || lock1) {
        lenmess += "\n-------------------------------------------------------------";
        lenmess += "\nRestoring lock";
        lenmess += (lock0&&lock1)?"s: LOCK0 and LOCK1":(lock0)?": LOCK0":": LOCK1";
        if (lock0) sccAccess->writeFlit(Route, CRB, 0x200, 0x00000000);
        if (lock1) sccAccess->writeFlit(Route, CRB, 0x400, 0x00000000);
      }
      lenmess += "\n=============================================================";
      lenmess += "\nDumping LUTs of Tile " + messageHandler::toString(Route, 16, 2);
      lenmess += "\nFormat: Bypass(bin)_Route(hex)_subDestId(dec)_AddrDomain(hex)";
      lenmess += "\n=============================================================";
      for (int loopCore = 0; loopCore <2; loopCore++) {
        for (int loopLutIndex = 0; loopLutIndex < 0x100; loopLutIndex += 1) {
          lutEntry = sccAccess->readFlit(Route, CRB, 0x800 + loopCore*0x800 + 8*loopLutIndex);
          lenmess += "\nLUT" + QString::number(loopCore) + ", Entry " + messageHandler::toString(loopLutIndex, 16, 2) + " (CRB addr = " + messageHandler::toString(0x800 + loopCore*0x800 + 8*loopLutIndex, 16, 4) + "): "
                             + messageHandler::toString((lutEntry>>21)&0x01, 2, 1) + "_"
                             + messageHandler::toString((lutEntry>>13)&0x0ff, 16, 2) + "_"
                             + messageHandler::toString((lutEntry>>10)&0x07, 10, 1) + "(" + sccAccess->decodeDestID((lutEntry>>10)&0x07, true).replace(QRegExp("(\\S+)\\s+"), "-\\1-") + ")_"
                             + messageHandler::toString(lutEntry & 0x3ff, 16, 3);
        }
      }
      lenmess += "\n=============================================================";
    }
    if (fileName != "") {
      printInfo("Dumping CRB (registers and LUTs) to file \"", fileName,"\"...");
      QFile file(fileName);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        printError("Failed to open output file \"", fileName, "\"... File is not writable.\nPrinting to screen instead!");
        fileName = "";
      } else {
        QTextStream out(&file);
        out << lenmess << endl;
      }
      file.close();
    }
    // Filename not defined (or overwritten with "" because it wasn't writable...
    if (fileName == "") {
      printf( "%s\n", lenmess.toStdString().c_str() );
    }
    return;
  } else if (Route == 0x03) { // FPGA register...
    uInt32 data;
    sccAccess->readFpgaGrb(startAddress, &data);
    printInfo("FPGA register at address ",messageHandler::toString(startAddress,16,8)," (",QString::number(startAddress),"d) contains ",messageHandler::toString(data,16,8)," (",QString::number(data),"d)...");
  } else {
    memLimit = (subDestID==MPB)?0x04000ull:sccAccess->getSizeOfMC()*SIZE_1GB;
    if (startAddress % 0x20) {
      numBytes += (startAddress % 0x20); // Read more...
      startAddress -= (startAddress % 0x20); // ... and start erlier so that the bytes of interest are contained in the dump!
    }
    // Read memory...
    readMemSlot(fileName);
  }
}

sccDump::~sccDump() {
  delete settings;
  if (sccAccess) delete sccAccess;
  delete log;
}

void sccDump::displayAligned() {
  //This function monitors memory at system level.
  uInt32 length=mem.size();
  if(!length) {
    return;
  }

  QString lenmess;

  uInt64 word;
  char _Byte[addrFact];

  uInt32 x = 0;
  QString res = "";
  signed int y = 0;


  QString printAddr="";
  QString contAscii="";
  QString winMem="";

  printInfo( "--------------------------------------------------");
  lenmess= "** " + QString::number( (uInt32) 8*addrFact, 10) + " bits alignment";
  printInfo(lenmess);
  printInfo( "--------------------------------------------------");
  x = 0;
  y = 0;

  int idx=0;

  for (uInt64 i = 0; i < (length/addrFact); i++) {
    word=0;
    idx=x;
    for (int j=addrFact-1; j>=0; j--) {
      _Byte[j] =  (unsigned char) mem.at(x++) & 0xFF; //(*mem)[x++] & 0xFF;
      //    }
      //for (int j=0; j<addrFact; j++) {
      word <<= 8;
      word |= (unsigned char)  mem.at(idx+j) & 0xFF; //_Byte[j] & 0xFF;
      //substituting codes below 0x20 by "." for ASCII printing
      _Byte[j]=(_Byte[j]<0x20)?0x2E:_Byte[j];
    }
    res += QString("%1").arg(QString::number( (uInt64) word, 16), 2*addrFact, QLatin1Char(0x30));
    res += " ";

    contAscii += QString::fromLatin1 (_Byte, addrFact );


    if(++y == 8/addrFact) {
      printAddr = QString("%1").arg(QString::number( (uInt64)startAddress + x - 8, 16), 16, QLatin1Char(0x30)) + " | ";
      {
  res += " | " + contAscii;
  //printInfo( printAddr + res);
  winMem+=printAddr + res + "\n";
      }
      contAscii="";
      res = "";
      y = 0;
    }
  }
  printf( "%s", winMem.toStdString().c_str());
}

uInt32 sccDump::getDispBytes (uInt32 numBytes) {
  uInt32 rndFact= (numBytes % 8)?(8 - numBytes % 8):0;
  uInt32 dispBytes=numBytes + rndFact; // bytes to be read
  return dispBytes;
}

uInt32 sccDump::getNumBytesAlign (uInt32 numBytes,  uInt64 startAddress, uInt64 lastAddress){
  uInt32 numBytesA=numBytes;
  numBytesA+=(numBytes % 32)?(32 - numBytes % 32):0; // rounding to multiples of 32, since it is the minimal amount to be read
  while ((uInt64) (32*((uInt64)(startAddress / 32)) + numBytesA) < lastAddress) {
    numBytesA+=32; // to ensure the desired address is displayed
  }

  numBytesA=(numBytesA % 32)?(32*(1 + int(numBytesA/32))):(32*int(numBytesA/32));
  return numBytesA;
}

uInt64 sccDump::getStartAddrAlign (uInt64 startAddress) {
  //rounding addresses and num of bytes acordingly for readMem function
  uInt64 startAddressA= 32*((uInt64)(startAddress / 32));
  return startAddressA;
}

uInt32 sccDump::readMem(uInt64 startAddress, uInt32 numBytes, QByteArray *qmem, QString file ) {
  bool ofileOK = false;
  FILE* ofile=NULL;

  if (file != "") {
    if(!(ofile = fopen (file.toStdString().c_str(), "w"))) {
      printError("Could not open selected output file for writing... Dumping to screen instead!");
    } else {
      ofileOK = true;
    }
  }

  //proceeding to align the address and the length of the block  to be read
  //32 bits
  uInt32 dispBytes=getDispBytes(numBytes);
  uInt64 lastAddress=startAddress+dispBytes; // last address to be read

  uInt32 numBytesA=getNumBytesAlign(numBytes,startAddress,lastAddress);

  //rounding addresses and num of bytes acordingly for transfer packet function
  uInt64 startAddressA = getStartAddrAlign(startAddress);

  //validating is start address is in range. Qutting if not.
  if(startAddressA >= memLimit) {
    printInfo("Memory access out of range. Please reduce start address (aligned startaddress is 0x"+QString::number(startAddressA,16)+" but limit is 0x"+QString::number(memLimit,16)+").");
    return 0;
  }

  //Ensuring memory limit is not exceeded
  if(startAddressA < memLimit && lastAddress > memLimit) {
      lastAddress=memLimit-1;
      numBytes=memLimit-startAddress;
      numBytesA=getNumBytesAlign(numBytes,startAddressA,lastAddress);
      dispBytes=numBytes;
  }
  printInfo(" Contacting SystemIF to read memory. Please be patient ...");

  char mem[numBytesA];
  dispBytes = sccAccess->read32(Route, subDestID, startAddressA, (char*)&mem, numBytesA, SIF_RC_PORT, NULL);
  uInt32 idx=startAddress-startAddressA;
  for (uInt32 i = idx; i < (idx+dispBytes); i++) {
    if (qmem != &noMem) qmem->append((unsigned char) mem[i]);
    if (ofileOK) putc(mem[i],ofile);
  }

  printInfo(" done System Read.");

  if (ofileOK)
    fclose(ofile);

  return (ofileOK)?0:1;
}

void sccDump::readMemSlot(QString dumpFile) {
  //This function monitors memory at system level.
  //Every system memory address range has to be converted(or decoded) to
  // nodeID, memory address range

  QString lenmess;
  // Now performing a primitive node decoding. This should be done according memory managing.
  uInt64 dispAddress= startAddress; // address to be displayed, sinc startAddress will be rounded later on
  uInt32 dispBytes=getDispBytes(numBytes); //numBytes + rndFact; // bytes to be displayed
  uInt64 lastAddress=dispAddress+dispBytes-1; // last address to be displayed

  //validating is start address is in range. Qutting if not.
  if(startAddress >= memLimit) {
    lenmess="--------------------------------------------------\n";
    lenmess+="\nMemory access out of range. Limit = "+QString::number( (uInt64 ) memLimit, 16)+" hex. Please reduce start address.";
    lenmess+="\n--------------------------------------------------";
    printInfo(lenmess);
    return;
  }

  //At this point each system address range has to be decoded in to node ID and node address range
  //since now only one node is implemented, only type conversion is required
  uInt64 nodeStartAddr=(uInt64) startAddress;
  uInt32 nodeNumBytes=(uInt32) numBytes;

  if(startAddress < memLimit && lastAddress >= memLimit) {
      lastAddress=memLimit-1;
      //nodeNumBytes=64;
  }

  mem.clear();

  if (readMem(nodeStartAddr,nodeNumBytes,&mem,dumpFile)) {

    if(!mem.length()) {
      printError("Error while trying to read memory.");
      return;
    }

    lenmess ="--------------------------------------------------\n";
    if (startAddress != requestedStartAddress) {
      lenmess += "Start address is not 256-bit aligned... Using 0x"+ QString::number(startAddress, 16) +" instead!\n";
    }
    log->setMsgPrefix(log_info, "");
    lenmess += "  Reading on " + sccAccess->decodeDestID(subDestID) + " of Tile x=" + QString::number(Route&0x0f) + ", y=" + QString::number(Route>>4);
    lenmess += "\n  startAddress = " + QString::number( (uInt64) startAddress, 16) + " hex";
    lenmess += " (" + QString::number((uInt64)nodeStartAddr, 10) + " dec) ";
    lenmess += "\n  stopAddress  = " + QString::number( (uInt64) lastAddress, 16) + " hex";
    lenmess += "\n  Requested/Displayed length: " + QString::number(numBytesRequested, 10) + "/" + QString::number( (uInt32) mem.length(), 10) + " bytes";
    printInfo(lenmess);

    displayAligned();

    if (dispAddress + dispBytes -1 >= memLimit) {
      lenmess="--------------------------------------------------";
      lenmess+= "\nMemory limit " + QString::number( (uInt64) memLimit, 16) + " hex reached";
      lenmess+="\n--------------------------------------------------";
      printInfo( lenmess );
    }
  } else {
    printInfo("Successfully dumped memory content to file \"", dumpFile, "\"...");
  }
  log->setMsgPrefix(log_info, "Info");

}

void sccDump::printUsage(int exitCode) {
  QString usage=(QString)((exitCode)?"\n":"") + "\
Usage: " + QCoreApplication::arguments().at(0) + " [option]\n\
\n\
Options:\n\
  -h or --help: Print this message and exit...\n\
  -d or --ddr3 <route> <address> <bytecount> [<alignment>]: Read from memory controller...\n\
  -m or --mpb <route> <address> <bytecount> [<alignment>]: Read from message passing buffer...\n\
  -c or --crb <route> [<address>]: Dump all configuration registers & LUTs of tile <route> or a\n\
                                   specific CRB address <address>...\n\
  -s or --sif <address>: Dump system interface FPGA register at address <address> (e.g. 0x8010)...\n\
  -f or --file <file>: Write result to the specified file... In case of -m or -f the resulting\n\
                       file will be in binary format. -c option produces ASCII output.\n\
\n\
This application will read memory content (or memory mapped registers) from the SCC\n\
Platform... It's possible to address the MCs, the MPBs or the CRBs... With \"Grouping\"\n\
we mean the byte alignment of the memory content (Chunks of 1 <default>, 2, 4 or 8 bytes)...\n\
Addresses need to be 256-bit aligned as this software reads whole cachelines... The number\n\
of requested bytes may not exceed 1MB.";

  log->setMsgPrefix(log_info, "");
  printInfo(usage);
  log->setMsgPrefix(log_info, "INFO");
  exit(exitCode);
}

