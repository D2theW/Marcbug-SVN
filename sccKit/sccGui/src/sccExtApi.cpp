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

#include "sccExtApi.h"

// Enable/Disable debugging information in this "module"
#define DEBUG_MESSAGES 0

sccExtApi::sccExtApi(messageHandler *log) : sccApi(log) {
  readEmacConfig();
}

sccExtApi::~sccExtApi() {
}

// =============================================================================================
// SCC DMA access methods and members...
// =============================================================================================
int sccExtApi::loadOBJ(QString objFile, sccProgressBar *pd, int sif_port) {
  QString tmpName=objFile;
  QFileInfo tmpInfo;

  // Sanity check... Make sure that driver is connected...
  if (!fd)  {
    printError("loadOBJ: System is not initialized... Ignoring request!");
    return ERROR;
  }

  // There are up to four object files (one for each MCH)... Search them and upload them, if available.
  bool foundObjectWithCoordinates = false;
  for (int cur_y=0; cur_y < NUM_ROWS; cur_y++) {
    for (int cur_x=0; cur_x < NUM_COLS; cur_x++) {
      tmpName.replace(QRegExp("_\\d_\\d"), ("_"+QString::number(cur_x,16)+"_"+QString::number(cur_y,16)));
      tmpInfo = tmpName;
      if (tmpInfo.isReadable()) {
        foundObjectWithCoordinates = true;
        printInfo("Found object for MC x=", QString::number(cur_x,16), ", y=", QString::number(cur_y,16), ": \"", tmpName, "\"...");
        int retVal;
        if ((retVal=writeMemFromOBJ(TID(cur_x, cur_y), (cur_x==0)?PERIW:(cur_x==5)?PERIE:(cur_y==0)?PERIS:PERIN, tmpName, pd, ((cur_x==3) && (cur_y==0))?SIF_HOST_PORT:sif_port)) != SUCCESS) {
          if (retVal == ERROR) printWarning("File ", tmpName, " could not be loaded. Aborting...");
          return retVal;
        } else {
          printInfo("Successfully (re-)loaded object file \"", tmpName, "\"...");
        }
      }
    }
  }
  // If there's no object file with coordinates, just load the specified object to DDR3 memory 0x00-West (default)...
  if (!foundObjectWithCoordinates) {
    if (!writeMemFromOBJ(0x00, PERIW, objFile, pd, sif_port)) {
      printWarning("File ", objFile, " could not be loaded. Aborting...");
      return ERROR;
    } else {
      printInfo("Successfully (re-)loaded object file \"", objFile, "\"...");
    }
  }
  return SUCCESS;
}

int sccExtApi::writeMemFromOBJ(int routeid, int destid, QString objFile, sccProgressBar *pd, int sif_port, int commandPrefix) {
  uInt64 origin=0, dataAvail = 0, dataIdx = 0, wraddr=0; //dataBytes = 0;
  int i, j, newOrigin = 0, flushData = 0, valid = 0, eof = 0, input_handled = 0;
  uInt32 indata[8];
  char input[128];
  FILE* objTemp;

  // Sanity check... Make sure that driver is connected...
  if (!fd)  {
    printError("writeMemFromOBJ: System is not initialized... Ignoring request!");
    return ERROR;
  }

  objTemp = fopen(objFile.toAscii(), "r");
  if (!objTemp) {
    printError("File ", objFile, " not found...");
    return ERROR;
  }

  fseek (objTemp, 0, SEEK_END);
  uInt32 fileSize=ftell (objTemp);
  fseek (objTemp, 0, SEEK_SET);

  // Define progress bar...
  QString infotext = "Loading content of file \""+objFile+"\" to DestID "+decodeDestID(destid,false)+" of Tile 0x"+QString::number(routeid,16)+" ("+decodePort(sif_port,false)+" port)...";
  bool dispBar=((fileSize>=1<<14) && pd);
  if (dispBar) {
    pd->setLabelText(infotext);
    if (commandPrefix == BCMC) {
      pd->setWindowTitle("Progress of broadcasting to all MCs...");
    } else {
      pd->setWindowTitle("Progress of upload to MC (x=" + QString::number(X_TID(routeid),10) + ", y=" + QString::number(Y_TID(routeid),10) + ")...");
    }
    pd->setMinimum(0);
    pd->setMaximum((int)(fileSize>>10)-1);
    pd->setCancelButtonText("Abort");
    pd->setModal(true);
    pd->show();
  }

  for (i = 0; i < 8; i++) {
    indata[i] = 0;
  }

  // Disable packet-tracing (will be restored after pre-loading obj file!)...
  int backupDebugPacket = debugPacket;
  debugPacket = NO_DEBUG_PACKETS;

  i = fscanf(objTemp, "%s", input);
  while (i > 0) {
    input_handled = 0;

    // Read origin command and address from input file
    if (!strncmp(input, "/origin", 7)) {
      j = fscanf(objTemp, "%s", input);
      if (j != 1) {
        if (dispBar) pd->hide();
        printError("writeMemFromOBJ(...): 'fscanf(objTemp, \"%s\", input)' failed");
        fclose(objTemp);
        debugPacket=backupDebugPacket;
        dmaAbort();
        return ERROR;
      }
      j = sscanf(input, "%llx", &origin);
      if (j != 1) {
        if (dispBar) pd->hide();
        printError("writeMemFromOBJ(...): 'sscanf(input, \"%lx\", &origin' failed");
        fclose(objTemp);
        debugPacket=backupDebugPacket;
        dmaAbort();
        return ERROR;
      }
      input_handled = 1;
      newOrigin = 1;
      if (valid) {
        if (((origin << 2) & (~0x1f)) != wraddr) {
          flushData = 1;
        }
      }
      if(origin > 0x3FFFFFFFFull) {
        if (dispBar) pd->hide();
        printError("writeMemFromOBJ(...): Failed to write at application memory address 0x", QString::number(origin, 16));
        printInfo("Max application memory width is 16GByte (0x3FFFFFFFF), please check OBJ File");
        fclose(objTemp);
        debugPacket=backupDebugPacket;
        dmaAbort();
        return ERROR;
      } else {
        printDebug("New origin: 0x", QString::number(origin, 16), " (ByteAddr: 0x", QString::number(origin << 2, 16), ", 32B aligned 0x", QString::number((origin << 2) & (~0x1f), 16), ")" );
      }
    }

    // Read eof command from input file
    if (!strncmp(input, "/eof", 4)) {
      input_handled = 1;
      flushData = 1;
      eof = 1;
    }

    // Read data from input file, check if data buffer is full, set flushData in that case
    // to dump out to memory
    if ((!input_handled) && valid){
      j = sscanf(input, "%x", &indata[dataIdx]);
      if(j!=1) {
        if (dispBar) pd->hide();
        printError("writeMemFromOBJ(): 'sscanf(input, \"%lx\", &indata[dataIdx])' failed");
        fclose(objTemp);
        debugPacket=backupDebugPacket;
        dmaAbort();
        return ERROR;
      }
      dataIdx ++;
      input_handled = 1;
      dataAvail = 1;
      if (dataIdx > 7)
        flushData = 1;
    }

    // flush data currently in buffer to memory. Needs to take care of 32byte alignment
    if (flushData == 1) {
      if ((valid == 1) && (dataAvail == 1)) {
        // Really write the memory here
        if (sif_port == SIF_HOST_PORT) {
          transferPacket(routeid, destid, wraddr, commandPrefix+WBI, 0xff, (uInt64*)&indata, sif_port);
        } else {
          dmaTransfer((uInt64*)&indata);
        }
        if (dispBar && !txTransid) {
          pd->setValue((int)(ftell(objTemp)>>10));
          qApp->processEvents();
          if (pd->wasCanceled()) {
            if (dispBar) pd->hide();
            printInfo("Action canceled by user... Aborting!");
            fclose(objTemp);
            debugPacket=backupDebugPacket;
            dmaAbort();
            return ABORT;
          }
        }
      }

      if (eof)
        valid = 0;
      if (valid) {
        wraddr += 32;
        dataIdx = 0;
        dataAvail = 0;
      }
      flushData = 0;
    }

    // handle new origin - set data buffer pointer etc.
    if (newOrigin == 1) {
      wraddr = (origin << 2) & (~0x1f);
      dataIdx = origin - (wraddr >> 2);
      for (i = 0; i < 8; i++) {
        indata[i] = 0;
      }
      newOrigin = 0;
      valid = 1;
      if (sif_port != SIF_HOST_PORT) {
        if (DMATransferInProgress) dmaComplete(); // Transfer old chunk before initiating new one...
        dmaPrepare(routeid, destid, wraddr, commandPrefix+WBI, 0xff, sif_port);
      }
    }

    i = fscanf(objTemp, "%s", input);
    if ((i == 1) && (eof == 1)) {
      if (dispBar) pd->hide();
      printError("writeMemFromOBJ(...): Error reading file. Got eof, but data still available");
    }
  }

  if (dispBar) pd->hide();
  printInfo("writeMemFromOBJ(...): Configuration of memory done!");

  fclose(objTemp);

  if (sif_port != SIF_HOST_PORT) dmaComplete();

  // Restore debugPacket
  debugPacket=backupDebugPacket;
  if (debugPacket!=NO_DEBUG_PACKETS) PRINT_PACKET_LOG("TX %s\n", infotext.toStdString().c_str());

  return SUCCESS;
}

uInt64 file_data[4]={0,0,0,0};
bool sccExtApi::WriteFlitsFromFile(QString FileName, QString ContentType, sccProgressBar *pd, int sif_port, bool groupingEnabled) {
  bool ok;
  int retval=1;
  uInt64 int_arg[4];
  QFile file(FileName);
  QString line;
  QStringList lines;
  QStringList arglist;
  int bufferFill=0;
  uInt64 bufferRoute=0;
  uInt64 bufferDestID=0;
  uInt64 bufferStartaddr=0;
  QString infotext = "Configuring "+ContentType+" with "+(groupingEnabled?"write-combined ":"")+"content of file \""+FileName+"\"...";

  // Sanity checks... Make sure that driver is connected and file is readable...
  if (!fd)  {
    printError("WriteFlitsFromFile: System is not initialized... Ignoring request!");
    return 0;
  }
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    printError("WriteFlitsFromFile: Failed to open data file \"", FileName, "\"... File doesn't exist or is not readable. Aborting!");
    return 0;
  }

  // Disable packet-tracing (will be restored after loading dat file!)...
  int backupDebugPacket = debugPacket;
  debugPacket = NO_DEBUG_PACKETS;

  // Read, split and send lines of file
  printInfo(infotext);
  int linecount=0;
  qint64 charcount=0;
  // Find out number of digits
  while (!file.atEnd()) {
    file.readLine();
    linecount++;
  }
  file.reset();
  // Progress bar?
  bool dispBar=((linecount > 2500) && pd);
  if (dispBar) {
    pd->setLabelText(infotext);
    pd->setWindowTitle("Progress");
    pd->setMinimum(0);
    pd->setMaximum(linecount);
    pd->setCancelButtonText("Abort");
    pd->setModal(true);
    pd->show();
  }
  // Read file
  int currentLine = 0;
  while (!file.atEnd()) {
    currentLine++;
    line = file.readLine();
    charcount+=line.size();
    line = line.trimmed();
    arglist = line.split(QRegExp("\\s+"));
    if (arglist.size() != 4) {
      if (dispBar) pd->hide();
      printError("Wrong data format in line ",QString::number(currentLine,10)," of file \"",FileName,"\"... Aborting!");
      debugPacket=backupDebugPacket;
      return 0;
    }
    // Convert strings to integer values
    for (int loop_j=0; loop_j<4; loop_j++) {
      int_arg[loop_j] = (arglist[loop_j]).toULongLong(&ok, 16);
      if (!ok) {
        printError("Wrong data format in column ",QString::number(loop_j+1,10)," (\"",arglist[loop_j],"\") of line ",QString::number(currentLine,10)," of file \"",FileName,"\"... Aborting!");
      }
    }
    // Flit buffer (chance to optimize 4 consecutive NCWR commands to one WBI
    if (groupingEnabled && ((int_arg[2]%32) == 0) && !file.atEnd()) {
      if (bufferFill) {
        // No burst possible! Flush buffer...
        if (DEBUG_MESSAGES) printf("Flushing buffer at burst address (%0d entries)...\n", bufferFill );
        for (int loop_j=0; loop_j<bufferFill; loop_j++) {
          if (!writeFlit(bufferRoute, bufferDestID, bufferStartaddr+(8*loop_j), file_data[loop_j], sif_port)) {
            if (dispBar) pd->hide();
            debugPacket=backupDebugPacket;
            return 0;
          }
        }
      }
      bufferFill = 0;
      if (DEBUG_MESSAGES) printf("First buffer entry (0)...\n" );
      bufferRoute = int_arg[0];
      bufferDestID = int_arg[1];
      bufferStartaddr = int_arg[2];
      file_data[bufferFill++] = int_arg[3];
    } else if (groupingEnabled && (bufferRoute==int_arg[0]) && (bufferDestID==int_arg[1]) && ((bufferStartaddr+(8*bufferFill))==int_arg[2]) && (!file.atEnd()||bufferFill==3)) {
      if (DEBUG_MESSAGES) printf("Appending buffer entry (%0d)...\n", bufferFill );
      file_data[bufferFill++] = int_arg[3];
      if (bufferFill==4) {
        // Burst transfer of four 64 bit values...
        transferPacket(bufferRoute, bufferDestID, bufferStartaddr, WBI, 0xff, (uInt64*)&file_data, sif_port);
        bufferFill = 0;
      }
    } else {
      if (bufferFill) {
        // No burst possible! Flush buffer...
        if (DEBUG_MESSAGES) printf("Flushing buffer (%0d entries)...\n", bufferFill );
        for (int loop_j=0; loop_j<bufferFill; loop_j++) {
          if (!writeFlit(bufferRoute, bufferDestID, bufferStartaddr+(8*loop_j), file_data[loop_j], sif_port)) {
            if (dispBar) pd->hide();
            debugPacket=backupDebugPacket;
            return 0;
          }
        }
        bufferFill = 0;
      }
      // Don't try to group this...
      if (!writeFlit(int_arg[0], (int)int_arg[1], int_arg[2], int_arg[3], sif_port)) {
        if (dispBar) pd->hide();
        debugPacket=backupDebugPacket;
        return 0;
      }
    }

    if (dispBar) {
      pd->setValue(currentLine);
      if (pd->wasCanceled()) {
        retval = 0;
        break;
      }
    }
  }

  if (dispBar) {
    pd->hide();
  }

  if (dispBar && pd->wasCanceled()) {
    printInfo("-> Configuration of ",ContentType," has been aborted!");
  } else {
    printInfo("-> Configuration of ",ContentType," done!");
  }

  // Restore debugPacket
  debugPacket=backupDebugPacket;
  if (debugPacket!=NO_DEBUG_PACKETS) PRINT_PACKET_LOG("TX %s\n", infotext.toStdString().c_str());

  return retval;
}

// ================================================================ //
//  ____  __  __  ____   ___       _             __                 //
// | __ )|  \/  |/ ___| |_ _|_ __ | |_ ___ _ __ / _| __ _  ___ ___  //
// |  _ \| |\/| | |      | || '_ \| __/ _ \ '__| |_ / _` |/ __/ _ \ //
// | |_) | |  | | |___   | || | | | ||  __/ |  |  _| (_| | (_|  __/ //
// |____/|_|  |_|\____| |___|_| |_|\__\___|_|  |_|  \__,_|\___\___| //
//                                                                  //
//                                                                  //
// ================================================================ //
double sccExtApi::getCurrentPowerconsumption() {
  // Sanity check
  if (!tcpSocket) {
    return 0.000;
  }

  QString tmpString = executeBMCCommand("status");
  QString Volt;
  QString Amp;
  double Watts;
  bool okay;

  // CRB format ->Rail 3V3RCK:  3.296 V, 16.733 A<-
  // RLB format ->  3V3SCC:    3.296 V   31.386 A<-

  tmpString.replace(QRegExp(".*3V3(RCK|SCC):"), QString("3V3"));
  tmpString.replace(QRegExp("A.*"), QString("A"));

  Volt = Amp = tmpString;
  Volt.replace(QRegExp("3V3\\s*([0-9.]+)\\s*V,*\\s*[0-9.]+\\s*A"), QString("\\1"));
  Amp.replace(QRegExp("3V3\\s*[0-9.]+\\s*V,*\\s*([0-9.]+)\\s*A"), QString("\\1"));
  Watts = (Volt.toDouble(&okay)*Amp.toDouble(&okay));
  if (!okay) Watts = 0;

  return Watts;
}

// Find out what fast clock we're using...
uInt32 sccExtApi::getFastClock() {
  QString sclsb, scmsb, jtagValue;
  int m, n, div;
  bool okay;
  // Read reference clock
  sclsb = executeBMCCommand("cpld SCLSB");
  sclsb.replace(QRegExp("SCLSB = (0x.*)"), QString("\\1"));
  scmsb = executeBMCCommand("cpld SCMSB");
  scmsb.replace(QRegExp("SCMSB = (0x.*)"), QString("\\1"));
  m = 256*(scmsb.toInt(&okay, 16) & 0x01);
  if (!okay) return 0;
  m += sclsb.toInt(&okay, 16);
  if (!okay) return 0;
  // printfInfo("m = %0d...", m);
  n = (scmsb.toInt(&okay, 16) / 2) & 0x07;
  if (!okay) return 0;
  switch(n) {
    case 0: n=2; break;
    case 1: n=4; break;
    case 2: n=8; break;
    case 3: n=16; break;
    case 4: n=1; break;
    case 5: n=2; break;
    case 6: n=4; break;
    case 7: n=8; break;
    default: return 0;
  }
  // printfInfo("n = %0d...", n);
  // Read fastclock divider (Bits 35:31 of JTAG value "left pll")
  jtagValue = executeBMCCommand("jtag read left pll");
  jtagValue.replace(QRegExp("Read Chain:\\s*(\\S+).*"),QString("\\1"));
  jtagValue=jtagValue.right(9);
  jtagValue=jtagValue.left(2);
  div = jtagValue.toInt(&okay,16)/8;
  // printfInfo("div = %0d...", div);
  if (!okay) return 0;
  // Return product of reference clock and fastclock divider...
  // printfInfo("Fastclock = %0d...", ((2*m)/n)*div);
  return ((2*m)/n)*div;
}

// =============================================================================================
// SCC IP address-handling...
// =============================================================================================
QString sccExtApi::getSccIp(int pid) {
  QStringList ipList = sccFirstIp.split(".");
  bool ok;
  int hostIdentifier = ipList[3].toInt(&ok) + pid;
  return ipList[0]+"."+ipList[1]+"."+ipList[2]+"."+QString::number(hostIdentifier);
}

uInt32  sccExtApi::getFirstSccIp() {
  uInt32 result = 0;
  bool ok;
  QStringList ipList = sccFirstIp.split(".");
  for (int loop = 0; loop < 4; loop++) {
    result += ipList[3-loop].toInt(&ok) << (loop*8);
  }
  return result;
}

uInt32 sccExtApi::getHostIp() {
  uInt32 result = 0;
  bool ok;
  QStringList ipList = sccHostIp.split(".");
  for (int loop = 0; loop < 4; loop++) {
    result += ipList[3-loop].toInt(&ok) << (loop*8);
  }
  return result;
}

uInt32 sccExtApi::getGateway() {
  uInt32 result = 0;
  bool ok;
  QStringList ipList = sccGateway.split(".");
  for (int loop = 0; loop < 4; loop++) {
    result += ipList[3-loop].toInt(&ok) << (loop*8);
  }
  return result;
}

uInt64  sccExtApi::getFirstSccMac() {
  uInt64 result = 0;
  bool ok;
  QStringList macList = sccFirstMac.split(":");
  for (int loop = 0; loop < 6; loop++) {
    result += ((uInt64)macList[5-loop].toInt(&ok,16)) << (loop*8);
  }
  return result;
}

void sccExtApi::getMacEnableConfig(bool *a, bool *b, bool *c, bool *d) {
  QString macEnable = sysSettings->value("sccMacEnable", "").toString();
  *a = macEnable.contains("a", Qt::CaseInsensitive);
  *b = macEnable.contains("b", Qt::CaseInsensitive);
  *c = macEnable.contains("c", Qt::CaseInsensitive);
  *d = macEnable.contains("d", Qt::CaseInsensitive);
}

void sccExtApi::setMacEnable(bool a, bool b, bool c, bool d) {
  // Find out which eMACs to enable...
  uInt32 fpgaIpConfig;
  readFpgaGrb(SIFGRB_FPGAIPSTAT, &fpgaIpConfig); // [14:0] 2bit SATA, 4 bit eMAC, 9 bit SCEMI Size - RO
  // printfInfo("fpgaipstat = 0x%08x", fpgaIpConfig);
  fpgaIpConfig = (fpgaIpConfig & 0x07fff) << 16; // [30:16] 2bit SATA, 4 bit eMAC, 9 bit SCEMI Size - RW
  if (!(a && (fpgaIpConfig&0x02000000))) fpgaIpConfig &= ~0x02000000;
  if (!(b && (fpgaIpConfig&0x04000000))) fpgaIpConfig &= ~0x04000000;
  if (!(c && (fpgaIpConfig&0x08000000))) fpgaIpConfig &= ~0x08000000;
  if (!(d && (fpgaIpConfig&0x10000000))) fpgaIpConfig &= ~0x10000000;
  // printfInfo("fpgaipconfig = 0x%08x", fpgaIpConfig);
  writeFpgaGrb(SIFGRB_FPGAIPCONFIG, fpgaIpConfig);
}

void sccExtApi::setMacEnable() {
  bool a, b, c, d;
  getMacEnableConfig(&a, &b, &c, &d);
  setMacEnable(a, b, c, d);
}

// =============================================================================================
// SCC virtual display related...
// =============================================================================================
void sccExtApi::setDisplayGeometry(uInt32 x, uInt32 y, uInt32 depth) {
  userSettings->setValue("virtualDisplay/resX", x);
  userSettings->setValue("virtualDisplay/resY", y);
  userSettings->setValue("virtualDisplay/depth", depth);
  writeFpgaGrb(SIFGRB_RESX, x);
  writeFpgaGrb(SIFGRB_RESY, y);
  writeFpgaGrb(SIFGRB_RESDEPTH, depth);
}

uInt32 sccExtApi::getDisplayX() {
  uInt32 result;
  bool okay;
  result = userSettings->value("virtualDisplay/resX",640).toInt(&okay);
  if (!okay) return 640;
  return result;
}

uInt32 sccExtApi::getDisplayY() {
  uInt32 result;
  bool okay;
  result = userSettings->value("virtualDisplay/resY",480).toInt(&okay);
  if (!okay) return 480;
  return result;
}

uInt32 sccExtApi::getDisplayDepth() {
  uInt32 result;
  bool okay;
  result = userSettings->value("virtualDisplay/depth",16).toInt(&okay);
  if (!okay) return 16;
  return result;
}

// =============================================================================================
// (Re-) configuration of GRB registers...
// =============================================================================================
void sccExtApi::readEmacConfig() {
  // Get sccFirstSccIp and perform short sanity check...
  sccFirstIp = sysSettings->value("sccFirstIp","192.168.0.1").toString();
  QStringList ipList = sccFirstIp.split(".");
  if (ipList.size() != 4) {
    printWarning("Wrong data format of sccFirstIp setting (", sccFirstIp, ") in \"/opt/sccKit/systemSettings.ini\"... Using default start IP 192.168.0.1!");
    sccFirstIp = "192.168.0.1";
    ipList = sccFirstIp.split(".");
  }
  bool ok;
  int tmp;
  for (int loop = 0; loop < 4; loop++) {
    tmp = ipList[loop].toInt(&ok);
    if (!ok) {
      printError("Wrong data format of sccFirstIp setting (", sccFirstIp, ") in \"/opt/sccKit/systemSettings.ini\"... Using default start IP 192.168.0.1!");
      sccFirstIp = "192.168.0.1";
      ipList = sccFirstIp.split(".");
    }
  }
  if (tmp + 47 > 254) { // tmp == ipList[3] == host identifier...
    printError("Host identifier of sccFirstIp setting (", sccFirstIp, ") in \"/opt/sccKit/systemSettings.ini\" is to high (max 207)... Using default start IP 192.168.0.1!");
    sccFirstIp = "192.168.0.1";
    ipList = sccFirstIp.split(".");
  }

  // Get sccHostIp and perform short sanity check...
  sccHostIp = sysSettings->value("sccHostIp","192.168.1.254").toString();
  ipList = sccHostIp.split(".");
  if (ipList.size() != 4) {
    printWarning("Wrong data format of sccHostIp setting (", sccHostIp, ") in \"/opt/sccKit/systemSettings.ini\"... Using default start IP 192.168.1.254!");
    sccHostIp = "192.168.1.254";
    ipList = sccHostIp.split(".");
  }
  for (int loop = 0; loop < 4; loop++) {
    tmp = ipList[loop].toInt(&ok);
    if (!ok) {
      printError("Wrong data format of sccHostIp setting (", sccHostIp, ") in \"/opt/sccKit/systemSettings.ini\"... Using default start IP 192.168.1.254!");
      sccHostIp = "192.168.0.1";
      ipList = sccHostIp.split(".");
    }
  }
  if (tmp > 254) { // tmp == ipList[3] == host identifier...
    printError("Host identifier of sccHostIp setting (", sccHostIp, ") in \"/opt/sccKit/systemSettings.ini\" is to high (max 254)... Using default start IP 192.168.1.254!");
    sccHostIp = "192.168.0.1";
    ipList = sccHostIp.split(".");
  }

  // Get sccGateway and perform short sanity check...
  sccGateway = sysSettings->value("sccGateway","192.168.1.254").toString();
  ipList = sccGateway.split(".");
  if (ipList.size() != 4) {
    printWarning("Wrong data format of sccGateway setting (", sccGateway, ") in \"/opt/sccKit/systemSettings.ini\"... Using default start IP 192.168.1.254!");
    sccGateway = "192.168.1.254";
    ipList = sccGateway.split(".");
  }
  for (int loop = 0; loop < 4; loop++) {
    tmp = ipList[loop].toInt(&ok);
    if (!ok) {
      printError("Wrong data format of sccGateway setting (", sccGateway, ") in \"/opt/sccKit/systemSettings.ini\"... Using default start IP 192.168.1.254!");
      sccGateway = "192.168.0.1";
      ipList = sccGateway.split(".");
    }
  }
  if (tmp > 254) { // tmp == ipList[3] == host identifier...
    printError("Host identifier of sccGateway setting (", sccGateway, ") in \"/opt/sccKit/systemSettings.ini\" is to high (max 254)... Using default start IP 192.168.1.254!");
    sccGateway = "192.168.0.1";
    ipList = sccGateway.split(".");
  }

  // Get sccFirstMac and perform short sanity check...
  sccFirstMac = sysSettings->value("sccFirstMac","00:45:4D:41:44:31").toString();
  QStringList macList = sccFirstMac.split(":");
  if (macList.size() != 6) {
    printWarning("Wrong data format of sccFirstMac setting (", sccFirstMac, ") in \"/opt/sccKit/systemSettings.ini\"... Using default sccFirstMac 00:45:4D:41:44:31!");
    sccFirstMac = "00:45:4D:41:44:31";
    macList = sccFirstMac.split(":");
  }
  for (int loop = 0; loop < 6; loop++) {
    tmp = macList[loop].toInt(&ok,16);
    if (!ok) {
      printError("Wrong data format of sccFirstMac setting (", sccFirstMac, ") in \"/opt/sccKit/systemSettings.ini\"... Using default sccFirstMac 00:45:4D:41:44:31!");
      sccFirstMac = "00:45:4D:41:44:31";
      macList = sccFirstMac.split(":");
    }
  }
}

void sccExtApi::programGrbEmacRegisters() {
  readEmacConfig(); // Re-read... Just in case someone modified the values...
  uInt64 firstMac = getFirstSccMac();
  writeFpgaGrb(SIFGRB_EMACBASEADDRH, (uInt32)(firstMac >> 32));
  writeFpgaGrb(SIFGRB_EMACBASEADDRL, (uInt32)firstMac);
  writeFpgaGrb(SIFGRB_EMACCOIPADDR, getFirstSccIp());
  writeFpgaGrb(SIFGRB_EMACHTIPADDR, getHostIp());
  writeFpgaGrb(SIFGRB_EMACGATEADDR, getGateway());
  writeFpgaGrb(SIFGRB_FASTCLOCK, getFastClock());
  setMacEnable();
}

void sccExtApi::programGrbVgaRegisters() {
  writeFpgaGrb(SIFGRB_RESX, getDisplayX());
  writeFpgaGrb(SIFGRB_RESY, getDisplayY());
  writeFpgaGrb(SIFGRB_RESDEPTH, getDisplayDepth());
}

void sccExtApi::programAllGrbRegisters() {
  printInfo("(Re-)configuring GRB registers...");
  programGrbEmacRegisters();
  programGrbVgaRegisters();
}

void sccExtApi::initCrbGcbcfg() {
  bool ok;
  QString routerChain;
  QString bmcCommand = "jtag read TILE00 ROUTER";
  uInt32 rxbRatio, tileRatio, tileDiv;
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      bmcCommand.replace(QRegExp("(.*TILE)[0-5][0-3](.*)"), QString("\\1")+QString::number(loop_x)+QString::number(loop_y)+QString("\\2"));
      routerChain = executeBMCCommand(bmcCommand);
      routerChain.replace(QRegExp("\nEND"), "");
      routerChain = routerChain.right(15);
      routerChain = routerChain.left(11);
      rxbRatio = (routerChain.toLongLong(&ok, 16)>>20) & 0x07f;
      if (!ok) rxbRatio = 0;
      tileRatio = (routerChain.toLongLong(&ok, 16)>>7) & 0x07f;
      if (!ok) tileRatio = 0;
      tileDiv = (routerChain.toLongLong(&ok, 16)>>2) & 0x0f;
      if (!ok) tileDiv = 0;
      // printfInfo("x=%0d, y=%0d: rxbRatio=%0d, tileRatio=%0d, tileDiv=%0d...", loop_x, loop_y, rxbRatio, tileRatio, tileDiv);
      writeFlit(TID(loop_x, loop_y), CRB, GCBCFG, (rxbRatio<<19)+(tileRatio<<12)+(tileDiv<<8)+0x0ff);
    }
  }
}

void sccExtApi::initCrbRegs() {
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      writeFlit(TID(loop_x, loop_y), CRB, GLCFG0, 0x00348df8);
      writeFlit(TID(loop_x, loop_y), CRB, GLCFG1, 0x00348df8);
      writeFlit(TID(loop_x, loop_y), CRB, L2CFG0, 0x000006cf);
      writeFlit(TID(loop_x, loop_y), CRB, L2CFG1, 0x000006cf);
      writeFlit(TID(loop_x, loop_y), CRB, SENSOR, 0x00000000);
      writeFlit(TID(loop_x, loop_y), CRB, LOCK0, 0x00000000);
      writeFlit(TID(loop_x, loop_y), CRB, LOCK1, 0x00000000);
    }
  }
  initCrbGcbcfg();
}

// Enable fast RPC (10 MHz instad of 0.65MHz)...
void sccExtApi::enableFastRpc() {
  bool ok;
  QString routerChain, modChain;
  uInt64 modChainVal;
  routerChain = executeBMCCommand("jtag read left rpc");
  routerChain.replace(QRegExp("\nEND"), "");
  routerChain = routerChain.right(25);
  modChainVal = routerChain.mid(8,2).toInt(&ok, 16);
  if (!ok) modChainVal = 0;
  modChainVal = 0x046004f8 + (modChainVal & 0x03);
  modChain = messageHandler::toString(modChainVal,16,8).replace("0x", "");
  modChain = routerChain.left(2) + modChain + routerChain.right(15);
  executeBMCCommand("jtag write left rpc 0"+modChain);
}
