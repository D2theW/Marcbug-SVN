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

#include "sccApi.h"

// Enable/Disable debugging information in this "module"
#define DEBUG_MESSAGES 0
#define DMA_DEBUG 0
#define RX_POLLDELAY 0
#define SIFTRAINING_DEBUG 0
#define SIFTRAINING_JTAG_DEBUG 0
#define SIFTRAIN_RXDATA_DELAY 0
#define DEBUG_SIF_SEMA 0

// Timeouts (seconds)...
#define READ_PACKET_TIMEOUT 10
#define BMC_TIMEOUT 10

// DMA Setup & debug section (The following setting requres FPGA release 091001-0 or newer)
#define DISABLE_WRITERESPONSES 1
#define USE_BLOCKTRANSFER 1
#define FPGAMEMCMP 1

// FPGA GRB access
#define USE_LEGACY_GRB_ACCESS_FORMAT 0
#define DEBUG_GRB_ACCESS 0

// Support for BMC (board management controller) as host...
#ifdef BMC_IS_HOST
    #include <string>
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <iostream>
    #define GPIO_BASE     0x40000000
    #define FPGA_BUS_BASE 0x60000000
    #define CPLD_FPGASTATUS         (*(gpioBase + 0x1A))  // Status lines from FPGA to CPLD
    #define CPLD_CPLDSTATUS         (*(gpioBase + 0x1B))  // Status lines from CPLD to FPGA
    #define CPLD_GPIOPR0            (*(gpioBase + 0x1C))  // GPIO Pin Register 0
    volatile unsigned char *gpioBase;
    volatile unsigned char *fpgaBusBase;
#endif

// =============================================================================================
// sccApi
// =============================================================================================
sccApi::sccApi(messageHandler *log) {
  // Initialize some members...
  this->log = log;
  CONNECT_MESSAGE_HANDLER;
  tcpSocket = NULL;
  fd = -1;
  txMsg = NULL;
  rxMsg = (uInt32*) malloc(12*sizeof(uInt32));
  rxMsgAvail = false;
  rxServer = NULL;
  txServer = NULL;
  DMATransferInProgress=false;
  isActiveSoftram = false;
  for (int loop=0; loop < 4; loop++) {
    readFpgaGrbData[loop] = 0;
    writeFpgaGrbData[loop] = 0;
  }
  crbFifoDepth = 0;

  // Open user userSettings...
  userSettings = new QSettings("sccKit", "sccApi");

  // Open system Settings (read only)...
  QString sysSettingFile = QCoreApplication::applicationDirPath();
  sysSettingFile.replace(QRegExp("bin$"), "../systemSettings.ini");
  sysSettings = new QSettings(sysSettingFile, QSettings::IniFormat);
  CRBServer = sysSettings->value("CRBServer", "none").toString();
  memorySize = sysSettings->value("memorySize", 4).toInt();
  platform = sysSettings->value("platform", "Copperridge").toString();
  maxTransId = sysSettings->value("maxTransId", DEFAULT_MAX_TRANS_ID).toInt();

  // Open SIF preset file...
  QString sifSettingFile = QCoreApplication::applicationDirPath();
  sifSettingFile.replace(QRegExp("bin$"), "../sifPresets.ini");
  sifSettings = new QSettings(sifSettingFile, QSettings::IniFormat);

  // Connect default hooks for Soft-RAM and IO
  connect(this, SIGNAL(ioRequest(uInt32* )), this, SLOT(slotIoRequest(uInt32*)));

  // Initialization stuff
  initTempPath();
}

sccApi::~sccApi() {
  delete sysSettings;
  delete userSettings;
  delete sifSettings;
  userSettings = NULL;
  delete rxServer;
  delete txServer;
  free(rxMsg);
  free(txMsg);
}

// =============================================================================================
// Protection members & methods...
// =============================================================================================

bool sccApi::acquireSysIF() {
  while (!protectSysIF.tryAcquire(1)) {
    qApp->processEvents(); // allow others to continue...
#if DEBUG_SIF_SEMA == 1
    printf("."); fflush(0);
#endif
    if (releaseSemaphores) return false;
  }
#if DEBUG_SIF_SEMA == 1
  printf("Acquired SysIF (semaphore value is %0d)...\n", protectSysIF.available()); fflush(0);
#endif
  return true;
}

bool sccApi::tryAcquireSysIF() {
  bool result = protectSysIF.tryAcquire(1);
#if DEBUG_SIF_SEMA == 1
  printf("Tryed to aquire SysIF (semaphore value is %0d)...\n", result); fflush(0);
#endif
  if (result) {
  }
  return result;
}

bool sccApi::availableSysIF() {
  bool result = protectSysIF.available();
#if DEBUG_SIF_SEMA == 1
  printf("Probed SysIF availability (semaphore value is %0d)...\n", protectSysIF.available()); fflush(0);
#endif
  return result;
}

void sccApi::releaseSysIF() {
  while(protectSysIF.tryAcquire(1));
  protectSysIF.release(1);
#if DEBUG_SIF_SEMA == 1
  printf("Released SysIF (semaphore value is %0d)...\n", protectSysIF.available()); fflush(0);
#endif
}

// =============================================================================================
//  SCC Initialization methods...
// =============================================================================================
void sccApi::initTempPath() {
  // Default temp dir is "/tmp/$USER_tmp/"
  QStringList environment = QProcess::systemEnvironment();
  QString UserID = environment.value(environment.indexOf(QRegExp("^USER=.*")),"USER="+QUuid::createUuid().toString()); // Find out username. Use unique ID if not defined...
  UserID = UserID.replace(QRegExp("^USER="), "");
#ifdef BMC_IS_HOST
  QString defaultPath = "/media/usb/tmp/sccKit_"+UserID+"/"; // Default to be used
#else
  QString defaultPath = QDir::tempPath()+"/sccKit_"+UserID+"/"; // Default to be used
#endif
  setTempPath(defaultPath);
}

void sccApi::setTempPath(QString path) {
  tmpPath.setPath(path);
  if (!tmpPath.mkpath(tmpPath.absolutePath())) {
    printError("The selected temp directory \"", tmpPath.absolutePath(), "\" cannot be accessed... Please create it!");
    exit(-1);
  }
}

QString sccApi::getTempPath() {
  return (tmpPath.absolutePath()+"/");
}

void sccApi::setSubnetSize(uInt32 size) {
#ifdef BMC_IS_HOST
  if (size) return; // Redundant code to prevent "unused parameter" warning of compiler...
#else
  if (fd < 0) {
    printError("setSubnetSize(...): Driver device is not open... Ignoring request!");
    return;
  }
  ioctl( fd, CRBIF_SET_SUBNET_SIZE, size );
#endif
}

void sccApi::setTxBaseAddr(uInt64 addr) {
#ifdef BMC_IS_HOST
  if (addr) return; // Redundant code to prevent "unused parameter" warning of compiler...
#else
  // As we can only transfer 32 bit values we shift the 34 bit address by 2 (assuming that the 2 LSBs are always zero).
  // The driver will finally shift the data back...
  if (addr & 0x03ull) {
    // Should never happen. Just to be sure that our assumption is correct...
    printError("setTxBaseAddr(): The two LSBs of the new TX base address are not zero! Ignoring request...\n");
    return;
  }
  if (fd < 0) {
    printError("setTxBaseAddr(...): Driver device is not open... Ignoring request!");
    return;
  }
  ioctl( fd, CRBIF_SET_TX_BASE_ADDRESS, (uInt32)(addr>>2) );
#endif
}

void sccApi::reInitDriver() {
#ifdef BMC_IS_HOST
  return;
#else
  if (fd < 0) {
    printError("reInitDriver(...): Driver device is not open... Ignoring request!");
    return;
  }
  ioctl( fd, CRBIF_RESET, 0 );
#endif
}

uInt32 sccApi::getCrbFifoDepth() {
#ifdef BMC_IS_HOST
  return 0x10000ull;
#else
  // Do we already know the FIFO depth?
  if (crbFifoDepth) {
    return crbFifoDepth;
  }
  // Not? Ask driver otherwise... Fifo size won't change during runtime!
  if (fd < 0) {
    printError("getCrbFifoDepth(...): Driver device is not open... Ignoring request!");
    return 0;
  }
  uInt64 depth;
  ioctl( fd, CRBIF_GET_FIFO_DEPTH, &depth );
  depth &= 0x0ffffffff;
  if ((depth != 0x10000ull) && (depth != 0x40000ull)) {
    // We expect 64kB or 256kB... If we receive something else, we might have the wrong
    // driver... In that case we'll assume 64kB!
    depth = 0x10000ull;
  }
  crbFifoDepth = (uInt32)depth;
  return crbFifoDepth;
#endif
}

void sccApi::setMaxTid(uInt32 maxTid) {
#ifdef BMC_IS_HOST
  if (maxTid) return; // Redundant code to prevent "unused parameter" warning of compiler...
#else
  if (fd < 0) {
    printError("setMaxTid(...): Driver device is not open... Ignoring request!");
    return;
  }
  if ((maxTid == 0x40) || (maxTid == 0x100)) {
    ioctl( fd, CRBIF_SET_MAX_TID, maxTid );
  } else {
    ioctl( fd, CRBIF_SET_MAX_TID, DEFAULT_MAX_TRANS_ID );
  }
#endif
}

// =============================================================================================
// PCIe access members
// =============================================================================================
bool sccApi::connectSysIF() {
  initPacketLogging();
#ifdef BMC_IS_HOST
  // On BMC we don't open a driver. We use the 16 bit IF to the FPGA instead (mmap).
  // Open mem device for read/write
  fd = open ("/dev/mem", O_RDWR | O_SYNC);
  if (fd < 0) {
    printError("Open of /dev/mem failed!\n");
    exit(-1);
  }

  // Finally get pointer to GPIO_BASE...
  gpioBase = (volatile unsigned char*)mmap (NULL, 4096UL, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, GPIO_BASE);
  if ((gpioBase == MAP_FAILED) || (gpioBase == NULL)) {
    printError("Mmap of /dev/mem failed!\n");
    exit(-1);
  }

  // ... and to FPGA_BUS_BASE!
  fpgaBusBase = (volatile unsigned char*)mmap (NULL, 4096UL, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, FPGA_BUS_BASE);
  if ((fpgaBusBase == MAP_FAILED) || (fpgaBusBase == NULL)) {
    printError("Mmap of /dev/mem failed!\n");
    exit(-1);
  }

  // Then close device!
  if (close (fd) != 0)  {
    printError ("Close (fd) of /dev/mem failed!\n");
  }
#else
  bool driverLoaded = false;
  bool driverAvailable = false;
  QFileInfo tmpInfo;
  QStringList devices;

  printInfo( "Initializing System Interface (SCEMI setup)....");
  QString device = "/dev/crbif0rb0";
  printDebug("Trying to open device ", device, "...");
  tmpInfo = device;
  if(tmpInfo.isReadable()) {
    driverAvailable = true;
    if (fd>= 0) {
      printWarning("Device ", device, " is already open....");
    }
    fd = open(device.toStdString().c_str(), O_RDWR);
    if (fd < 0) {
      printError("Couldn't open device ", device, "...");
    } else {
      printDebug("Opening device ", device, " succeeded...");
      driverLoaded = true;
    }
    if (!driverLoaded) {
      printDebug("Not possible to open device ", device, "...");
    }
  }

  // Failure? :-(
  if (!driverLoaded) {
    if (driverAvailable) {
      printError("initializeSysIF(): Failed to open PCIe driver device. Driver in use or PCIe link down...");
    } else {
      printError("initializeSysIF(): Failed to open PCIe driver device. Driver devices not installed/available...");
    }
    return false;
  }

  // Define max TID
  setMaxTid(maxTransId);

#endif

  // Create txMsg...
  free(txMsg);
  txMsg = (uInt32*) malloc(12*sizeof(uInt32));

  // Initialize SysIF semaphores...
  releaseSysIF();
  sendAbort = false;

  // Reset transaction IDs
  txIntTransid = 0;
  txTransid = 0;
  rxTransid = -1;

  // Start rx & tx servers
  rxServer = new rxThread(this, log, fd);
  rxServer->start();
  txServer = new txThread(this, log, fd);
  txServer->start();
  return true;
}

/************************************************************
 sendMessage(): Writes packet to the SIF.
 *************************************************************/
void sccApi::sendMessage(uInt32* message) {
  txServer->sendMessage(message);
}

/**************************************************************************
 receiveMessage(): Handles new packets from the System interface...
                   This method will be invoked by the rxServer (Thread)...
**************************************************************************/
void sccApi::receiveMessage(uInt32* messagePtr) {
  // Print debug stuff, if requested...
  if (debugPacket==RAW_DEBUG_PACKETS) printPacket(messagePtr, "RX", true, true);

  // Update rxTransid
  rxTransid=(messagePtr[8]>>8)&0x0ff;

  // Find out who cares for this packet...
  rxCmd = (messagePtr[10]>>2)&0x0fff;
  if ((rxCmd==DATACMP) || (rxCmd==NCDATACMP) || (rxCmd==MEMCMP) || (rxCmd==ABORT_MESH)) {
    // Deal with responses...
    if (DMATransferInProgress) {
      // Forward dma response...
      dmaReceive(messagePtr);
    } else if (rxTransid == txTransid) {
      // This is a PIO read or write response to the host... Update rxMsg which will be evaluated by transferPacket(...).
      memcpy((void*) rxMsg, (void*) messagePtr, 12*sizeof(uInt32));
      rxMsgAvail = true;
      if (DEBUG_MESSAGES && packetLog.isOpen()) { char msg[255]; sprintf(msg, "Created rxPacket for TID %03d...\n", rxTransid ); packetLog.write(msg); packetLog.flush(); };
    } else {
      // This response packet has not been expected at all...
      printWarning("Received unexpected response:");
      printPacket(messagePtr, "Unexpected response", false, false);
    }
  } else if ((rxCmd==NCIORD) || (rxCmd==NCIOWR)) {
    emit ioRequest(messagePtr);
    while (messagePtr[10]) {
      qApp->processEvents(); // ioSlot needs to overwrite cmd with zero in order to continue...
    }
  } else if ((rxCmd==RDO) || (rxCmd==NCRD) || (rxCmd==WCWR) || (rxCmd==WBI) || (rxCmd==NCWR)) {
    // This is a memory Access to the host.
    if (isActiveSoftram) {
      // As the Softram is active, we assume this access goes to the Softram...
      emit softramRequest(messagePtr);
      while (messagePtr[10]) qApp->processEvents(); // softram slot needs to overwrite cmd with zero in order to continue...
    } else {
      // As the Softram is inactive, this access doesn't make sense!
      printWarning("Received unexpected Data-Packet. If the usage of the Softram was intended");
      printWarning("make sure to activate the Softram and try again...");
      printPacket(messagePtr, "Unexpected DATA packet", false, false);
    }
  } else {
    // This packet has not been expected at all...
    printWarning("Received unexpected Packet:");
    printPacket(messagePtr, "Unexpected packet", false, false);
  }

  return;
}

// =============================================================================================
// SCC PIO access methods...
// =============================================================================================

/************************************************************
Task : readFlit
Description : Perform NCRD to specific location
 *************************************************************/
uInt64 read_data[4]={0,0,0,0};
uInt64 sccApi::readFlit(int Route, int SubdestID, uInt64 Address, int sif_port) {
  transferPacket(Route, SubdestID, Address, NCRD, 0xff, (uInt64*)&read_data, sif_port);
  return read_data[0];
}

bool sccApi::readFlit(int Route, int SubdestID, uInt64 Address, uInt64 *Data, int sif_port) {
  bool retval = transferPacket(Route, SubdestID, Address, NCRD, 0xff, (uInt64*)&read_data, sif_port);
  *Data = read_data[0];
  return retval;
}

/************************************************************
Task : writeFlit
Description : Perform NCWR to specific location
 *************************************************************/
uInt64 write_data[4]={0,0,0,0};
bool sccApi::writeFlit(int Route, int SubdestID, uInt64 Address, uInt64 Data, int sif_port) {
  write_data[0]=Data;
  return transferPacket(Route, SubdestID, Address, NCWR, 0xff, (uInt64*)&write_data, sif_port);
}

/************************************************************
Task : transferPacket
Description : Write to or read from certain System address
 *************************************************************/
bool sccApi::transferPacket(int routeid, int destid, uInt64 address, int command, unsigned char byteenable, uInt64* data, int sif_port, int commandPrefix) {

  // Sanity check... Make sure that driver is connected...
  if (!fd)  {
    printError("transferPacket: System is not initialized... Ingnoring request!");
    data[0]=data[1]=data[2]=data[3]=0xdeadbeefull;
    return false;
  }

  // Acquire SIF before we modify internal data...
  if (!acquireSysIF()) {
    return false;
  }

  // Initialize data with zeros as the content is don't care in case of a read request...
  if ((commandPrefix == NOGEN) && ((command==NCRD) || (command==RDO) || (command==NCIORD))) {
    data[0]=data[1]=data[2]=data[3]=0x0;
  }

  // Debug output
  if (debugPacket==RAW_DEBUG_PACKETS) PRINT_PACKET_LOG("TX %03d from HOST to %s -> transferPacket(0x%02x, %s, 0x%01x_%08x, %s, 0x%02x, 0x%016llx_%016llx_%016llx_%016llx);\n", txTransid, decodePort(sif_port).toStdString().c_str(), routeid, decodeDestID(destid).toStdString().c_str(), (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), decodeCommand(command).toStdString().c_str(), byteenable, data[3], data[2], data[1], data[0]);

  // Create TX packet
  uInt32 tmp;
  tmp  = (uInt32)sif_port<<24;
  tmp += (uInt32)SIF_HOST_PORT<<16;
  tmp += (uInt32)txTransid<<8;
  tmp += (uInt32)byteenable;
  txMsg[8] = tmp;
  tmp  = (uInt32)address&0x0ffffffff;
  txMsg[9] = tmp;
  tmp  = (uInt32)destid<<22;
  tmp += (uInt32)routeid<<14;
  tmp += (uInt32)(commandPrefix + command)<<2;
  tmp += (uInt32)((uInt64)address>>32);
  txMsg[10] = tmp;
  txMsg[11] = 0;
  txMsg[7] = (uInt32)(((uInt64)data[3]) >> 32);
  txMsg[6] = (uInt32)(((uInt64)data[3]) & 0x0ffffffff);
  txMsg[5] = (uInt32)(((uInt64)data[2]) >> 32);
  txMsg[4] = (uInt32)(((uInt64)data[2]) & 0x0ffffffff);
  txMsg[3] = (uInt32)(((uInt64)data[1]) >> 32);
  txMsg[2] = (uInt32)(((uInt64)data[1]) & 0x0ffffffff);
  txMsg[1] = (uInt32)(((uInt64)data[0]) >> 32);
  txMsg[0] = (uInt32)(((uInt64)data[0]) & 0x0ffffffff);

  // If we addressed the software memory take a shortcut
  if (sif_port==SIF_HOST_PORT && (command==RDO || command==NCRD || command==WCWR || command==WBI || command==NCWR)) {
    emit softramRequest(txMsg);
  } else {
    // Send regular packet otherwise...
    sendMessage(txMsg);
  }
  if (DEBUG_MESSAGES && (debugPacket==RAW_DEBUG_PACKETS)) PRINT_PACKET_LOG("transferPacket(): Sent packet with TID %0d via PIO mode...\n",txTransid );

  // Evaluate RX packet (read request of any kind...) -> Blocking!
  if ((command==NCRD) || (command==RDO) || (command==NCIORD)) {
    struct timeval timeout, current;

    // Get time and prepare timeout counter
    gettimeofday(&timeout, NULL);
    timeout.tv_sec += READ_PACKET_TIMEOUT;
    // Wait until we receive the correct rx transid...
    while(!rxMsgAvail) {
      // Allow polling...
      qApp->processEvents();
      // Check if timout timer expired
      gettimeofday(&current, NULL);
      if (((current.tv_sec == timeout.tv_sec) && (current.tv_usec >= timeout.tv_usec)) || (current.tv_sec > timeout.tv_sec)) {
        printError("Timeout while waiting for Read request answer (CMD=0x",QString::number(command,16),") with TID ",QString::number(txTransid,10),"! Cancelling request...");
        data[0]=data[1]=data[2]=data[3]=0xdeadbeefull;
        txIntTransid++; txTransid = (unsigned char)(txIntTransid%maxTransId);
        rxMsgAvail=false;
        qApp->processEvents();
        releaseSysIF();
        return false;
      }
    }
    if (DEBUG_MESSAGES) PRINT_PACKET_LOG("Received result %03d...\n", rxTransid);
    if (((rxMsg[10]>>2)&0x0fff) == ABORT_MESH) {
      printError("Received ABORT_MESH packet during read operation (TID = ",QString::number(txTransid,10),", CMD=", decodeCommand(command,false),")!");
      if (debugPacket==TEXT_DEBUG_PACKETS) PRINT_PACKET_LOG("Read transfer %03d from HOST to address %01x_%08x of tile %01x-%s (%s) returned ABORT_MESH!\n", txTransid, (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), routeid, decodeDestID(destid,false).toStdString().c_str(), decodePort(sif_port,false).toStdString().c_str());
      data[0]=data[1]=data[2]=data[3]=0xdeadbeefull;
      txIntTransid++; txTransid = (unsigned char)(txIntTransid%maxTransId);
      rxMsgAvail=false;
      releaseSysIF();
      return false;
    }
    if  (command==RDO) {
      data[0] = ((uInt64)rxMsg[1]<<32)+(uInt64)rxMsg[0];
      data[1] = ((uInt64)rxMsg[3]<<32)+(uInt64)rxMsg[2];
      data[2] = ((uInt64)rxMsg[5]<<32)+(uInt64)rxMsg[4];
      data[3] = ((uInt64)rxMsg[7]<<32)+(uInt64)rxMsg[6];
      // Debug messages...
      if (debugPacket==TEXT_DEBUG_PACKETS) PRINT_PACKET_LOG("Cacheline-read (RDO) transfer %03d from HOST to address %01x_%08x of tile %01x-%s (sif_port=%s) returned 0x%016llx_%016llx_%016llx_%016llx.\n", txTransid, (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), routeid, decodeDestID(destid,false).toStdString().c_str(), decodePort(sif_port,false).toStdString().c_str(), data[3], data[2], data[1], data[0]);
    } else {
      data[0] = data[1] = data[2] = data[3] = 0;
      for (int loop=0; loop<4; loop++) if (byteenable & (1<<loop)) data[0] += (uInt64)(rxMsg[0]&(0x0ff<<(8*loop)));
      for (int loop=4; loop<8; loop++) if (byteenable & (1<<loop)) data[0] += ((uInt64)rxMsg[1]<<32)&((uInt64)0x0ff<<(8*loop));
      // Debug messages...
      if (debugPacket==TEXT_DEBUG_PACKETS) PRINT_PACKET_LOG("%s transfer %03d from HOST to address %01x_%08x of tile 0x%01x-%s (%s) returned 0x%016llx (byte enable 0x%02x).\n", (command==NCRD)?"Non-cacheable read (NCRD)":(command==NCIORD)?"I/O Read (NCIORD)":"Read", txTransid, (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), routeid, decodeDestID(destid,false).toStdString().c_str(), decodePort(sif_port,false).toStdString().c_str(), data[0], byteenable);
    }
  } else if ((command==NCWR) || (command==WCWR) || (command==WBI) || (command==NCIOWR)) {
    struct timeval timeout, current;

    // Debug output (if requested)
    if (debugPacket==TEXT_DEBUG_PACKETS) {
      if (command==WBI) {
        PRINT_PACKET_LOG("Invoked cacheline-write (WBI) transfer %03d from HOST to address %01x_%08x of tile 0x%01x-%s (%s) with data 0x%016llx_%016llx_%016llx_%016llx.\n", txTransid, (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), routeid, decodeDestID(destid,false).toStdString().c_str(), decodePort(sif_port,false).toStdString().c_str(), data[3], data[2], data[1], data[0]);
      } else {
        PRINT_PACKET_LOG("Invoked %s transfer %03d from HOST to address %01x_%08x of tile 0x%01x-%s (%s) with data 0x%016llx (byte enable  0x%02x).\n", (command==WCWR)?"memory write (WCWR)":(command==NCWR)?"non-cacheable write (NCWR)":(command==NCIOWR)?"I/O Write (NCIOWR)":"write" , txTransid, (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), routeid, decodeDestID(destid,false).toStdString().c_str(), decodePort(sif_port,false).toStdString().c_str(), data[0], byteenable);
      }
    }

    // Get time and prepare timeout counter
    gettimeofday(&timeout, NULL);
    timeout.tv_sec += READ_PACKET_TIMEOUT;

#if DISABLE_WRITERESPONSES == 0
    // Wait until we receive the correct rx transid...
    while(!rxMsgAvail) {
#else
    // Wait for completion of transmission...
    while (!txServer->isEmpty()) {
#endif
      // Allow polling...
      qApp->processEvents();
      // Check if timout timer expired
      gettimeofday(&current, NULL);
      if (((current.tv_sec == timeout.tv_sec) && (current.tv_usec >= timeout.tv_usec)) || (current.tv_sec > timeout.tv_sec)) {
        printError("Timeout while waiting for Write response (CMD=0x",QString::number(command,16),") with TID ",QString::number(txTransid,10),"! Cancelling request...");
        data[0]=data[1]=data[2]=data[3]=0xdeadbeefull;
        txIntTransid++; txTransid = (unsigned char)(txIntTransid%maxTransId);
        rxMsgAvail=false;
        qApp->processEvents();
        releaseSysIF();
        return false;
      }
    }
#if DISABLE_WRITERESPONSES == 0
    if (((rxMsg[10)>>2)&0x0fff) == ABORT_MESH) {
      printError("Received ABORT_MESH packet during write operation (TID = ",QString::number(txTransid,10),", CMD=", decodeCommand(command,false),")!");
      if (debugPacket==TEXT_DEBUG_PACKETS) PRINT_PACKET_LOG("Write transfer %03d from HOST to address %01x_%08x of tile %01x-%s (%s) returned ABORT_MESH!\n", txTransid, (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), routeid, decodeDestID(destid,false).toStdString().c_str(), decodePort(sif_port,false).toStdString().c_str());
      data[0]=data[1]=data[2]=data[3]=0xdeadbeefull;
      txIntTransid++; txTransid = (unsigned char)(txIntTransid%maxTransId);
      rxMsgAvail=false;
      releaseSysIF();
      return false;
    }
#endif
  } else if (debugPacket==TEXT_DEBUG_PACKETS) {
    PRINT_PACKET_LOG("Invoked %s transfer %03d from HOST to address %01x_%08x of tile 0x%01x-%s (%s) with data 0x%016llx_%016llx_%016llx_%016llx.\n", decodeCommand(command,false).toStdString().c_str(), txTransid, (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), routeid, decodeDestID(destid,false).toStdString().c_str(), decodePort(sif_port,false).toStdString().c_str(), data[3], data[2], data[1], data[0]);
  }

  if (!txTransid) qApp->processEvents(); // allow GUI to update from time to time. Do this after(!) actual transfer...
  txIntTransid++; txTransid = (unsigned char)(txIntTransid%maxTransId);
  rxMsgAvail = false;
  releaseSysIF();

  return true;
}

// =============================================================================================
// SCC FPGA register access...
// =============================================================================================

// readFpgaGrb: Read 32 bit register from GRB configuration space of FPGA
bool sccApi::readFpgaGrb(uInt32 address, uInt32 *value) {
#if USE_LEGACY_GRB_ACCESS_FORMAT == 1
  readFpgaGrbAddr = (uInt64)(address/4);
  readFpgaGrbByteEn = 0xff;
#else
  readFpgaGrbAddr = (uInt64)(address & ~0x07);
  readFpgaGrbByteEn = (address & 0x04)?0xf0:0x0f;
  // Sanity check
  if (address & 0x03) {
    printWarning("Ignoring address bits 1:0 of FPGA register address (",messageHandler::toString(address,16,8),")... Please double-check your code!");
  }
#endif
  // Access
  if(!transferPacket(SIFROUTE, SIFDESTID, readFpgaGrbAddr, NCRD, readFpgaGrbByteEn, (uInt64*)&(readFpgaGrbData), SIF_GRB_PORT)) {
    *value = 0xdeadbeef;
#if DEBUG_GRB_ACCESS == 1
    printfError("readFpgaGrb(address=0x%08x, value = 0xdeadbeef) -> grbAddr = 0x%08x, grbByteEn = 0x%02x...", address, (uInt32)readFpgaGrbAddr, readFpgaGrbByteEn);
#endif
    return false;
  }
#if USE_LEGACY_GRB_ACCESS_FORMAT == 1
  *value = (uInt32)readFpgaGrbData[0];
#else
  *value = (address & 0x04)?(uInt32)(readFpgaGrbData[0]>>32):(uInt32)readFpgaGrbData[0];
#endif
#if DEBUG_GRB_ACCESS == 1
  printfInfo("readFpgaGrb(address=0x%08x, value = 0x%08x) -> grbAddr = 0x%08x, grbByteEn = 0x%02x...", address, *value, (uInt32)readFpgaGrbAddr, readFpgaGrbByteEn);
  qApp->processEvents(); // allow others to continue...
#endif
  return true;
}

// writeFpgaGrb: Write 32 bit register to GRB configuration space of FPGA
bool sccApi::writeFpgaGrb(uInt32 address, uInt32 value) {
#if USE_LEGACY_GRB_ACCESS_FORMAT == 1
  writeFpgaGrbAddr = (uInt64)(address/4);
  writeFpgaGrbByteEn = 0xff;
#else
  writeFpgaGrbAddr = (uInt64)(address & ~0x07);
  writeFpgaGrbByteEn = (address & 0x04)?0xf0:0x0f;
  // Sanity check
  if (address & 0x03) {
    printWarning("Ignoring address bits 1:0 of FPGA register address (",messageHandler::toString(address,16,8),")... Please double-check your code!");
  }
#endif
#if DEBUG_GRB_ACCESS == 1
  printfInfo("writeFpgaGrb(address=0x%08x, value = 0x%08x) -> grbAddr = 0x%08x, grbByteEn = 0x%02x...", address, value, (uInt32)writeFpgaGrbAddr, writeFpgaGrbByteEn);
  qApp->processEvents(); // allow others to continue...
#endif
#if USE_LEGACY_GRB_ACCESS_FORMAT == 1
  writeFpgaGrbData[0] = (uInt64)value;
#else
  writeFpgaGrbData[0] = ((uInt64)value)<<32;
  writeFpgaGrbData[0] += value;
#endif
  // Access and wait for completion...
  return transferPacket(SIFROUTE, SIFDESTID, writeFpgaGrbAddr, NCWR, writeFpgaGrbByteEn, (uInt64*)&(writeFpgaGrbData), SIF_GRB_PORT);
}

// =============================================================================================
// SCC DMA access methods and members...
// =============================================================================================

/************************************************************
Task : read32
Description : Reads numBytes from memory address startAddress.
              Complete cachelines only... Returns number of
              bytes read (less than numBytes in case of error
              or user abort)...
 *************************************************************/
uInt32 sccApi::read32(int routeid, int destid, uInt64 startAddress, char memBlock[], uInt32 numBytes, int sif_port, sccProgressBar *pd) {
  uInt32 bytesRead = 0;

#if USE_BLOCKTRANSFER == 1
  // If read chunk-size is higher than FIFO depth, split access to smaller chunks. We use recursion for this.
  // This addresses a limitation of the DMA read access...
  if (numBytes > getCrbFifoDepth()) {
    uInt32 size;
    while (bytesRead < numBytes) {
      size = ((numBytes-bytesRead)>=getCrbFifoDepth())?getCrbFifoDepth():(numBytes-bytesRead);
      bytesRead+=read32(routeid, destid, startAddress+bytesRead, (char*)&memBlock[bytesRead], size, sif_port, NULL); // TODO: Enable progress bar for larger chunks as well...
    }
    return bytesRead;
  }
#endif

  // Sanity check... Make sure that driver is connected...
  if (!fd)  {
    printError("read32: System is not initialized... Ignoring request!");
    return bytesRead;
  }

  // Alignment check...
  if(!( ((numBytes%32) == 0) &&  ((startAddress%32) == 0))) {
    printError("read32(): RAW memory length and startAddress not 32Byte aligned, please check.");
    return bytesRead;
  }

  mem32Byte mem32;
  mem32.addr = startAddress;
  uInt32 index = numBytes / 32;

  if (pd) {
    pd->setLabelText("Reading Memory in progress.");
    pd->setWindowTitle("Progress");
    pd->setMinimum(0);
    pd->setMaximum(index-1);
    pd->setCancelButtonText("Abort");
    pd->setModal(true);
    pd->show();
  }

  if (sif_port != SIF_HOST_PORT) {
#if USE_BLOCKTRANSFER != 1
    dmaPrepare(routeid, destid, mem32.addr, RDO, 0xff, sif_port);
    DMATransferID+=index;
#else
    dmaPrepare(routeid, destid, mem32.addr, RDO, 0xff, sif_port, BLOCK, index);
    DMAData = (uInt64*)memBlock;
    DMACurrentTID = txTransid;
    txIntTransid+=index; txTransid = (unsigned char)(txIntTransid%maxTransId);
    DMATransferID+=index;
    // Send request packet
    sendMessage(txMsg);
#endif
  }

  for(uInt32 k=0; k < index; k++) {
    if (sif_port == SIF_HOST_PORT) {
      transferPacket(routeid, destid, mem32.addr, RDO, 0xff, (uInt64*)(memBlock+32*k), sif_port);
      mem32.addr += 32;
    } else {
#if USE_BLOCKTRANSFER != 1
      dmaTransfer((uInt64*)(memBlock+32*k));
#endif
    }
    bytesRead += 32;
    if (pd) {
      pd->setValue(k);
      if (pd->wasCanceled()) {
        pd->reset();
        break;
      }
    }
  }

  if (sif_port != SIF_HOST_PORT) if (!dmaComplete()) bytesRead = 0;

  return bytesRead;
}

/************************************************************
Task : write32
Description : Writes complete cachelines to memory address
              startAddress. Returns number of
              bytes written (less than numBytes in case of
              error or user abort)...
 *************************************************************/
uInt32 sccApi::write32(int routeid, int destid, uInt64 startAddress, char memBlock[], uInt32 numBytes, int sif_port, sccProgressBar *pd) {
  uInt32 bytesWritten = 0;

  // Sanity check... Make sure that driver is connected...
  if (!fd)  {
    printError("write32: System is not initialized... Ingnoring request!");
    return bytesWritten;
  }

  // Alignment check...
  if(!( ((numBytes%32) == 0) &&  ((startAddress%32) == 0))) {
    printError("write32(): RAW memory length and startAddress not 32Byte aligned, please check.");
    return bytesWritten;
  }

  uInt32 index = numBytes / 32;
  mem32Byte mem32;
  mem32.addr = startAddress;

  if (pd) {
    pd->setLabelText("Writing Memory in progress.");
    pd->setWindowTitle("Progress");
    pd->setMinimum(0);
    pd->setMaximum(index-1);
    pd->setCancelButtonText("Abort");
    pd->setModal(true);
    pd->show();
  }

  if (sif_port != SIF_HOST_PORT) dmaPrepare(routeid, destid, mem32.addr, WBI, 0xff, sif_port);

  for(uInt32 k=0; k<index; k++){
    if (sif_port == SIF_HOST_PORT) {
      transferPacket(routeid, destid, mem32.addr, WBI, 0xff, (uInt64*)&(memBlock[k*32]), sif_port);
    } else {
      dmaTransfer((uInt64*)&(memBlock[k*32]));
    }
    mem32.addr += 32;
    bytesWritten += 32;
    if (pd) {
      pd->setValue(k);
      if (pd->wasCanceled()) {
        pd->reset();
        break;
      }
    }
  }

  if (sif_port != SIF_HOST_PORT) if (!dmaComplete()) bytesWritten = 0;

  return bytesWritten;
}

/************************************************************
Task : dmaPrepare
Description : Write to or read from certain System address
              This routine only initiates the transfer...
              The answer needs to be evaluated seperatly.
 *************************************************************/
void sccApi::dmaPrepare(int routeid, int destid, uInt64 address, int command, unsigned char byteenable, int sif_port, int commandPrefix, int count) {
#if  DMA_DEBUG==1
  printf("dmaPrepare... "); fflush(0);
#endif

  // Acquire SIF before we modify internal data...
  if (!acquireSysIF()) {
    return;
  }

  DMACommand = command;
  DMASifPort = sif_port;
  DMAAddress = address;
  DMAData = NULL;

#if USE_BLOCKTRANSFER == 1
  if ((count > (int)getCrbFifoDepth()) &&  ((DMACommand==NCRD) || (DMACommand==RDO) || (DMACommand==NCIORD))) {
    printWarning("DMA read: Internal assumption violation. Requested number of bytes in block transfer exceeds FIFO\nsize (", QString::number(getCrbFifoDepth()/1024), " kB).This assumption violation may cause trouble... Please review sccKit code!");
  }
#endif

  if (debugPacket==RAW_DEBUG_PACKETS) {
    if ( commandPrefix != NOGEN) {
      PRINT_PACKET_LOG("TD %03d from HOST to %s -> dmaPrepare(0x%02x, %s, 0x%01x_%08x, %s, 0x%02x, %s[count=%0d]);\n", txTransid, decodePort(sif_port, false).toStdString().c_str(), routeid, decodeDestID(destid, false).toStdString().c_str(), (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), decodeCommand(command, false).toStdString().c_str(), byteenable, decodeCommandPrefix(command, false).toStdString().c_str(), count);
    } else {
      PRINT_PACKET_LOG("TD %03d from HOST to %s -> dmaPrepare(0x%02x, %s, 0x%01x_%08x, %s, 0x%02x);\n", txTransid, decodePort(sif_port, false).toStdString().c_str(), routeid, decodeDestID(destid, false).toStdString().c_str(), (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), decodeCommand(command, false).toStdString().c_str(), byteenable);
    }
  }

  // Create TX packet
  uInt32 tmp;
  tmp  = (uInt32)sif_port<<24;
  tmp += (uInt32)SIF_HOST_PORT<<16;
  tmp += (uInt32)txTransid<<8;
  tmp += (uInt32)byteenable;
  txMsg[8]=tmp;
  tmp  = (uInt32)address&0x0ffffffff;
  txMsg[9]=tmp;
  tmp  = (uInt32)destid<<22;
  tmp += (uInt32)routeid<<14;
  tmp += (uInt32)(commandPrefix+command)<<2;
  tmp += (uInt32)((uInt64)address>>32);
  txMsg[10]=tmp;
  txMsg[11]=0;
  if ((DMACommand==NCRD) || (DMACommand==RDO) || (DMACommand==NCIORD)) {
    // Initialize data with zeros as the content is don't care in case of a read request...
    for (int loop = 0; loop < 8; loop++) {
      txMsg[loop]=0;
    }
  }
  // Init DMA variables...
  if ( commandPrefix != NOGEN) txMsg[2]=count;
  DMATransferID = 0;
  DMATransferInProgress = true;
#if  DMA_DEBUG==1
  printf("Done.\n"); fflush(0);
#endif
  return;
}

/************************************************************
Task : dmaTransfer
Description : Write to or read from certain System address
              This routine only initiates the transfer...
              The answer needs to be evaluated seperatly.
              The transfer does automatically transfer 32
              bytes (RCK cacheline)...
 *************************************************************/
void sccApi::dmaTransfer(uInt64* data) {
#if  DMA_DEBUG==1
  printf("dmaTransfer entered %04d. time (tx tid = %0d)... ", DMATransferID+1, txTransid); fflush(0);
#endif

  // Debug output
  if (debugPacket==RAW_DEBUG_PACKETS) PRINT_PACKET_LOG("TD %03d to auto incremented address 0x%01x_%08x -> dmaTransfer(0x%016llx_%016llx_%016llx_%016llx);\n", txTransid, (unsigned int)(DMAAddress>>32), (unsigned int)(DMAAddress & 0x0ffffffff), data[3], data[2], data[1], data[0]);

  // Create TX packet (Header is set up already...)
  if ((DMACommand!=NCRD) && (DMACommand!=RDO) && (DMACommand!=NCIORD)) {
    memcpy((void*) txMsg, (void*) data, 8*sizeof(uInt32));
  } else {
    if (!DMAData) {
      DMAData = data;
      DMACurrentTID = txTransid;
    }
  }

  // Send request packet
  sendMessage(txMsg);

  // Autoincrement txTransid
  txIntTransid++; txTransid = (unsigned char)(txIntTransid%maxTransId);
  uInt32 tmp=txMsg[8];
  tmp &= 0x0ffff00ff;
  tmp += txTransid<<8;
  txMsg[8]=tmp;

  // Autoincrement address (only change MSBs if they actually changed)...
  int MSBs = (int)(DMAAddress >> 32);
  DMAAddress += 0x20;
  txMsg[9]=(uInt32)DMAAddress & 0x0ffffffff;
  if ((int)(DMAAddress >> 32) != MSBs) {
    tmp=txMsg[10];
    tmp &= 0x0fffffffc;
    tmp += (int)(DMAAddress >> 32);
    txMsg[10]=tmp;
  }

#if  DMA_DEBUG==1
  printf("Done...\n"); fflush(0);
#endif
  return;
}

/************************************************************
Task : dmaReceive
Description : This method collects responses to DMA requests.
 *************************************************************/
void sccApi::dmaReceive(const uInt32* message) {
#if  DMA_DEBUG==1
  printf("dmaReceive entered (rx tid = %0d)..", rxTransid); fflush(0);
#endif
  if (!DMAData) {
    if ((DMACommand==NCRD) || (DMACommand==RDO) || (DMACommand==NCIORD)) {
      printWarning("Unable to map received packet to Read request...");
      printPacket(message, "Unexpected DMA response", false, false);
    } else {
      DMATransferID--;
#if  DMA_DEBUG==1
      printf("MEMCMP!\n"); fflush(0);
#endif
    }
  } else {
    if (DMACurrentTID != rxTransid) {
      // TODO: By evaluating the TIDs we could also adjust the DMAData address automatically instead of issuing an error message!
      printError("The In-Order assumption has been violated (Expected ", messageHandler::toString(DMACurrentTID, 16, 2)," but received ", messageHandler::toString(rxTransid, 16, 2) ,"! Data will be stale!");
    }
    DMACurrentTID = (DMACurrentTID+1)%maxTransId;
    memcpy((void*) DMAData, (void*) message, 8*sizeof(uInt32));
    DMAData += 0x4; // Four long integers (32 byte)
    DMATransferID--;
#if  DMA_DEBUG==1
    printf("DATACMP!\n"); fflush(0);
#endif
  }
#if  DMA_DEBUG==1
  printf("Done...\n"); fflush(0);
#endif
  return;
}

/************************************************************
Task : dmaAbort
Description : Routine to release System Interface in case that
              we abort the DMA transfer for some reason.
 *************************************************************/
void sccApi::dmaAbort() {
  if (debugPacket==RAW_DEBUG_PACKETS) {
    PRINT_PACKET_LOG("Aborted DMA transfer from HOST to %s...\n", decodePort(DMASifPort, false).toStdString().c_str());
  }
  DMATransferID = 0;
  DMATransferInProgress = false;
  releaseSysIF();
}

/************************************************************
Task : dmaComplete
Description : Routine to check if DMA transfer has been
              completed
 *************************************************************/
bool sccApi::dmaComplete() {
  struct timeval timeout, current;
  bool retval = true;
  int oldDMATransferID = DMATransferID;

#if  DMA_DEBUG==1
  printf("dmaComplete entered... Waiting for completion!\n"); fflush(0);
#endif

  // Get time and prepare timeout counter
  gettimeofday(&timeout, NULL);
  timeout.tv_sec += READ_PACKET_TIMEOUT;

#if DISABLE_WRITERESPONSES == 1
  if ((DMACommand==NCRD) || (DMACommand==RDO) || (DMACommand==NCIORD)) { // Only wait for responses when reading...
#endif
    while (DMATransferID) {
      // Allow polling...
      qApp->processEvents();
      // Restart timer if TransferID changed...
      if (oldDMATransferID != DMATransferID) {
        gettimeofday(&timeout, NULL);
        timeout.tv_sec += READ_PACKET_TIMEOUT;
        oldDMATransferID = DMATransferID;
      }
      // Check for timeout
      gettimeofday(&current, NULL);
      if (((current.tv_sec == timeout.tv_sec) && (current.tv_usec >= timeout.tv_usec)) || (current.tv_sec > timeout.tv_sec)) {
        printError("Timeout while waiting for DMA transfer to complete! Cancelling request...");
        // Only show message once. Then abort transfer. "Late" packets will be shown as unexpected packets...
        DMATransferID = 0;
        retval = false;
      }
    }
#if DISABLE_WRITERESPONSES == 1
  } else {
    while (!txServer->isEmpty()) {
      qApp->processEvents();
    }
    DMATransferID = 0;
  }
#endif

  if (debugPacket==RAW_DEBUG_PACKETS) {
    PRINT_PACKET_LOG("End of DMA transfer from HOST to %s...\n", decodePort(DMASifPort, false).toStdString().c_str());
  }

#if  DMA_DEBUG==1
  printf("dmaComplete: Done.\n"); fflush(0);
#endif

  // Restart PIO infrastracture...
  DMATransferInProgress = false;
  releaseSysIF();
  return retval;
}

// =============================================================================================
// SCC methods
// =============================================================================================

// Get L2 mode of selected core
bool sccApi::getL2(int route, int core, bool *l2Enabled) {
  uInt64 l2cfgValue;
  // L2CFG_WAYDISABLE_BIT     04
  if(!readFlit(route, SUB_DESTID_CRB, core?L2CFG1:L2CFG0, &l2cfgValue)) return 0;
  *l2Enabled = !((l2cfgValue & 0x010) != 0);
  return 1;
}

// Set L2 mode of selected core
bool sccApi::setL2(int route, int core, bool l2Enabled) {
  uInt64 l2cfgValue;
  // L2CFG_WAYDISABLE_BIT     04
  if(!readFlit(route, SUB_DESTID_CRB, core?L2CFG1:L2CFG0, &l2cfgValue)) return 0;
  if (!l2Enabled) {
    // Set WAYDISABLE bit...
    l2cfgValue = l2cfgValue | 0x010;
  } else {
    // Reset WAYDISABLE bit...
    l2cfgValue = l2cfgValue & ~(0x010);
  }
  if (!writeFlit(route, SUB_DESTID_CRB, core?L2CFG1:L2CFG0, l2cfgValue)) return 0;
  return 1;
}

// Get reset value of selected core
bool sccApi::getReset(int route, int core, bool *resetEnabled) {
  uInt64 gcbcfgValue;
  // GCBCFG_L2_1_RESET_BIT        03
  // GCBCFG_L2_0_RESET_BIT        02
  // GCBCFG_CORE1_RESET_BIT       01
  // GCBCFG_CORE0_RESET_BIT       00
  // Prepare RMW cycle...
  if (!readFlit(route, SUB_DESTID_CRB, GCBCFG, &gcbcfgValue)) return 0;
  *resetEnabled = ((gcbcfgValue & (core?0x00a:0x005)) != 0);
  return 1;
}

// Reset selected core
bool sccApi::setReset(int route, int core, bool resetEnabled) {
  uInt64 gcbcfgValue;
  // GCBCFG_L2_1_RESET_BIT        03
  // GCBCFG_L2_0_RESET_BIT        02
  // GCBCFG_CORE1_RESET_BIT       01
  // GCBCFG_CORE0_RESET_BIT       00
  // Prepare RMW cycle...
  if (!readFlit(route, SUB_DESTID_CRB, GCBCFG, &gcbcfgValue)) return 0;
  // Set or reset the reset bits
  if (resetEnabled) {
    gcbcfgValue = gcbcfgValue | (core?0x00a:0x005);
    if (!writeFlit(route, SUB_DESTID_CRB, GCBCFG, gcbcfgValue)) return 0;
    // Initialize IRQ mask in GRB
    writeFpgaGrb(SIFGRB_FIRST_IRQ_MASK+PID(X_TID(route),Y_TID(route),core)*0x08, 0x0ffffffff);
    writeFpgaGrb(SIFGRB_FIRST_IRQ_MASK+PID(X_TID(route),Y_TID(route),core)*0x08+0x04, 0x0ffffffff);
    // Disable EMAC ports to prevent FPGA from modifying DDR3 memory...
    writeFpgaGrb(SIFGRB_RX_NETWORK_PORT_ENABLE_A+PID(X_TID(route),Y_TID(route),core)*0x04, 0);
    writeFpgaGrb(SIFGRB_RX_NETWORK_PORT_ENABLE_B+PID(X_TID(route),Y_TID(route),core)*0x04, 0);
    writeFpgaGrb(SIFGRB_RX_NETWORK_PORT_ENABLE_C+PID(X_TID(route),Y_TID(route),core)*0x04, 0);
    writeFpgaGrb(SIFGRB_RX_NETWORK_PORT_ENABLE_D+PID(X_TID(route),Y_TID(route),core)*0x04, 0);
  } else {
    gcbcfgValue = gcbcfgValue & ~(core?0x00a:0x005);
    if (!writeFlit(route, SUB_DESTID_CRB, GCBCFG, gcbcfgValue)) return 0;
  }
  return 1;
}


bool sccApi::toggleReset(int route, int core, bool performBist) {
  uInt64 gcbcfgValue;
  // GCBCFG_L2_1_RESET_BIT        03
  // GCBCFG_L2_0_RESET_BIT        02
  // GCBCFG_CORE1_RESET_BIT       01
  // GCBCFG_CORE0_RESET_BIT       00
  // Prepare RMW cycle...
  if (!readFlit(route, SUB_DESTID_CRB, GCBCFG, &gcbcfgValue)) return 0;
  // Set reset bits...
  gcbcfgValue = gcbcfgValue | (core?0x00a:0x005);
  if (!writeFlit(route, SUB_DESTID_CRB, GCBCFG, gcbcfgValue)) return 0;
  // Set INIT value (if BIST requested)
  int glcfgValue = 0;
  if (performBist) {
    // RMW acces to bit 2: GLCFG_XINIT_BIT 02
    glcfgValue = readFlit(route, SUB_DESTID_CRB, (core)?GLCFG1:GLCFG0);
    glcfgValue = glcfgValue | 0x04;
    if (!writeFlit(route, SUB_DESTID_CRB, (core)?GLCFG1:GLCFG0, glcfgValue)) return 0;
  }
  // Initialize IRQ mask in GRB
  writeFpgaGrb(SIFGRB_FIRST_IRQ_MASK+PID(X_TID(route),Y_TID(route),core)*0x08, 0x0ffffffff);
  writeFpgaGrb(SIFGRB_FIRST_IRQ_MASK+PID(X_TID(route),Y_TID(route),core)*0x08+0x04, 0x0ffffffff);
  // Release reset bits
  gcbcfgValue = gcbcfgValue & ~(core?0x00a:0x005);
  if (!writeFlit(route, SUB_DESTID_CRB, GCBCFG, gcbcfgValue)) return 0;
  // ... and release INIT (if BIST requested)
  if (performBist) {
    // RMW access to bit 2: GLCFG_XINIT_BIT 02
    glcfgValue = glcfgValue & ~(0x04);
    if (!writeFlit(route, SUB_DESTID_CRB, (core)?GLCFG1:GLCFG0, glcfgValue)) return 0;
  }
  return 1;
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

bool sccApi::openBMCConnection() {
  bool returnValue=false; // Returns true if connection has been opened successfully
  QString noParticipantString = "";
  char result[1024];

  QString newServer = CRBServer; // Server format: "<Hostname or IP>:<port>"

  // Sanitiy check...
  if (newServer=="none") {
    printInfo("openBMCConnection(): No server configured! Skipping open request...");
    return 0;
  }

  // Extract host and port number
  bool ok;
  QStringList tmpList = newServer.split(":");
  if (tmpList.size() != 2) {
    printError("openBMCConnection(): \"",newServer,"\" is no valid server name... Valid format is <Hostname or IP>:<port>!");
    return 0;
  }
  QString host = tmpList.at(0);
  host = host.trimmed();
  int port = tmpList.at(1).toInt(&ok);
  if (!ok) {
    printError("openBMCConnection(): \"",tmpList.at(1),"\" is no valid port... The port needs to be a decimal number!");
    return 0;
  }

  // Open connection
  tcpSocket = new QTcpSocket(this);
  tcpSocket->connectToHost(host,port);
  bool weAreDone = false;
  if (tcpSocket->waitForConnected(BMC_TIMEOUT*1000)) {
    int retval;
    // Read and ignore welcome message...
    while (tcpSocket->waitForReadyRead(50) || !weAreDone) {
      while ((retval=tcpSocket->readLine((char*)&result, 1024))>0) {
        if (QString(result).contains("You are participant")) {
          noParticipantString = result;
          // Remove Prompt and leading/training whitespaces
          noParticipantString.replace(QRegExp("\\]\\>"), "");
          noParticipantString = noParticipantString.trimmed();
          // Print number of participants
          printInfo("openBMCConnection(", host, ":",QString::number(port), "): ", noParticipantString);
          returnValue = true;
        }
        if (QString(result).contains(QRegExp("\\]\\>"))) {
          weAreDone = true;
        }
      }
    }
  } else {
    printError("openBMCConnection(): Unable to connect...");
    closeBMCConnection();
  }
  return returnValue;
}

void sccApi::closeBMCConnection() {

  // Sanitiy check...
  if (!tcpSocket) {
    return;
  }

  // Close connection
  tcpSocket->disconnectFromHost();
  if (tcpSocket->state() == QAbstractSocket::UnconnectedState || tcpSocket->waitForDisconnected(1000)) {
    printInfo("closeBMCConnection(): Disconnected from server...");
  } else {
    printError("closeBMCConnection(): Issue while disconnecting... Deleting socket!");
  }
  delete tcpSocket;
  tcpSocket = NULL;
  return;
}

QString sccApi::executeBMCCommand(QString command) {
  static QString returnValue;
  static QString tmpString;
  static char result[1024];
  static int minSeparationLines, separationLineCnt;
  static bool workaroundEnabled;

  // Sanity check
  if (!tcpSocket) {
    printError("executeBMCCommand(): Not connected to server... Skipping BMC command request!");
    return "";
  }

  // Init
  returnValue="";

  // Copperridge workaround
  separationLineCnt = 0;
  workaroundEnabled = ((platform == "Copperridge") && command.contains(QRegExp("power on")));
  if (workaroundEnabled) {
    // autorun interface sends some more Prompts during power-on. Ignore everyone before we found 2 separation lines...
    minSeparationLines = 2;
  } else {
    minSeparationLines = 0;
  }

  // Send command
  tcpSocket->write((command+"\r").toStdString().c_str());
  // Read result...
  bool weAreDone = false;
  struct timeval readTimeout, currentTime;
  // Get time and prepare timeout counter
  gettimeofday(&readTimeout, NULL);
  readTimeout.tv_sec += BMC_TIMEOUT;
  while (tcpSocket->waitForReadyRead(50) || !weAreDone) {
    while (tcpSocket->readLine((char*)&result, 1024)>0) {
      tmpString = result;
      tmpString.replace(QRegExp("\\r"), "");
      returnValue += tmpString;
      if (workaroundEnabled) {
        if (QString(result).contains(QRegExp("---------------------------------------------------------------------------"))) {
          separationLineCnt++;
        }
        if (QString(result).contains(QRegExp("already powered"))) {
          separationLineCnt+=2;
        }
      }
      if (QString(result).contains(QRegExp("\\]\\>"))) {
        if (separationLineCnt >= minSeparationLines)
          weAreDone = true;
      }
      gettimeofday(&currentTime, NULL);
      if (((currentTime.tv_sec == readTimeout.tv_sec) && (currentTime.tv_usec >= readTimeout.tv_usec)) || (currentTime.tv_sec > readTimeout.tv_sec)) {
        return "BMC read timeout...";
      }
    }
    gettimeofday(&currentTime, NULL);
    if (((currentTime.tv_sec == readTimeout.tv_sec) && (currentTime.tv_usec >= readTimeout.tv_usec)) || (currentTime.tv_sec > readTimeout.tv_sec)) {
      return "BMC read timeout...";
    }
  }
  // Remove Prompt
  returnValue.replace(QRegExp("\\]\\>"), "");
  returnValue = returnValue.trimmed();
  // That's it...
  return returnValue;
}

//#define ECHO_ON 0
//#define ECHO_OFF_RW 1
//#define ECHO_OFF_W 2
//echo ----FSM status-------\n

QString sccApi::processRCCFile(QString fileName, bool instantPrinting, int echo, sccProgressBar *pd) {
  int echoMode=echo; // initialize echoMode (can be overwritten by RCC-File)
  QFileInfo tmpInfo;
  QString retval;

  // Sanity check
  if (!tcpSocket) {
    printError("processRCCFile(): Not connected to server... Skipping RCC command file!");
    return "";
  }

  tmpInfo = fileName;
  // Process file
  if (tmpInfo.isReadable()) {
    // Invoke RCC File...
    QFile file(fileName);
    QString line;
    QStringList lines;
    int linecount=0, counter=0, digits=0;
    // Sanity check...
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      printError("processRCCFile(): Failed to open config file \"", fileName, "\"... File doesn't exist or is not readable. Aborting!");
      return "";
    }
    // Find out number of digits
    while (!file.atEnd()) {
      file.readLine();
      linecount++;
    }
    digits = QString::number(linecount).size();
    file.reset();
    // Progress bar?
    if (pd) {
      pd->setLabelText("processRCCFile(): Configuring SCC with content of file \"" + fileName + "\" (via BMC server " + CRBServer + ")!");
      pd->setWindowTitle("Progress");
      pd->setMinimum(0);
      pd->setMaximum(linecount);
      pd->setCancelButtonText("Abort");
      pd->setModal(true);
      pd->show();
    }
    // Read, split and send lines of file
    QString tmpMsg = "processRCCFile(): Configuring SCC with content of file \"" + fileName + "\" (via BMC server " + CRBServer + ").";
    QString tmpOutput;
    retval += tmpMsg + "\n";
    if (instantPrinting) printInfo(tmpMsg);
    while (!file.atEnd()) {
      counter++;
      line = file.readLine();
      // Remove comments...
      if (!line.contains(QRegExp("^[^#]*echo")))
        line = line.replace(QRegExp("#.*$"), "");
      line = line.replace("\\n", "");
      line = line.replace(QRegExp("^\\s*echo\\s*$"), "");
      line = line.trimmed();
      if (!line.contains(QRegExp("^\\s*$"))) {
        // Process command
        if (line.contains(QRegExp("^\\s*switch\\s+"))) {
          // Possible switch commands:
          // switch on
          // switch off_rw
          // switch off_w
          if (line.contains(QRegExp("^\\s*switch\\s+on\\s*"))) echoMode = ECHO_ON;
          else if (line.contains(QRegExp("^\\s*switch\\s+off\\s+rw\\s*"))) echoMode = ECHO_OFF_RW;
          else if (line.contains(QRegExp("^\\s*switch\\s+off\\s+w\\s*"))) echoMode = ECHO_OFF_W;
          else printWarning("Invalide switch command in line ", QString::number(counter), ": \"", line, "\"...");
        } else if (line.contains(QRegExp("^\\s*echo\\s+"))) { // Print echo
          tmpMsg = "[line " + messageHandler::toString(counter, 10, digits) + "]  " + line.replace(QRegExp("^\\s*echo\\s+"), "");
          retval += tmpMsg + "\n";
          if (instantPrinting) printInfo(tmpMsg);
        } else { // Forward to BMC
          if (echoMode == ECHO_ON) {
            tmpMsg = "[line " + messageHandler::toString(counter, 10, digits) + "]> " + line + (((tmpOutput=executeBMCCommand(line))=="")?"":("\n"+tmpOutput));
            retval += tmpMsg + "\n";
            if (instantPrinting) printInfo(tmpMsg);
          } else if (echoMode == ECHO_OFF_W) {
            tmpOutput=executeBMCCommand(line);
            if (tmpOutput!="") {
              tmpMsg = "[line " + messageHandler::toString(counter, 10, digits) + "]  " + tmpOutput;
              retval += tmpMsg + "\n";
              if (instantPrinting) printInfo(tmpMsg);
            }
          } else { // ECHO_OFF_RW
            executeBMCCommand(line);
          }
        }
      }
      if (pd) {
        pd->setValue(counter);
        qApp->processEvents(); // Update and allow user to press cancel...
        if (pd->wasCanceled()) {
          pd->hide();
          tmpMsg = "User pressed \"Cancel\"... Aborting!";
          retval += tmpMsg + "\n";
          if (instantPrinting) printInfo(tmpMsg);
          break;
        }
      }
    }
    if (pd) pd->hide();
  } else {
    printError("processRCCFile(): File doesn't exist or is not readable...");
    return "";
  }
  return retval;
}

void sccApi::setBMCServer(QString server) {
  CRBServer = server;
}

QString sccApi::getBMCServer() {
  return CRBServer;
}

QString sccApi::getPlatform() {
  return platform;
}

QString sccApi::platformSetupSuffix() {
  QString result;
  if (platform == "Copperridge") {
    result = "crb";
  } else {
    result = "rlb";
  }
  return result;
}

bool sccApi::connectedBMCServer() {
  return tcpSocket;
}

// ===========================================================================================
//  ____ ___ _____   _____          _       _
// / ___|_ _|  ___| |_   _| __ __ _(_)_ __ (_)_ __   __ _
// \___ \| || |_      | || '__/ _` | | '_ \| | '_ \ / _` |
//  ___) | ||  _|     | || | | (_| | | | | | | | | | (_| |
// |____/___|_|       |_||_|  \__,_|_|_| |_|_|_| |_|\__, |
//                                                  |___/
// ===========================================================================================

// Method to perform the actual training (depending on mode and frequency)...
// This method calibrates the SIF physical interface. Different RX CLK delay
// settings are configured and verified via pattern generators in RC and the FPGA.
// This is done in a two pass approach. First the RC pattern generator is enabled
// and the delay setting with the farthest data changes (data eye) is determined.
// Next the RC SIF loopback is enabled and the data eye for the patterns
// generated by the FPGA is determined.
// If no delay setting with valid sample point can be found an error is issued.

int sccApi::trainSIF(int mode, int freq) {
  // Execute SIF training...
  userAbort = false;
  if ((platform == "Copperridge") || (platform == "RockyLake") || (platform == "RockyLakeRx")) {
    return trainSIF_rx(mode, freq);
  } else if (platform == "RockyLakeRxTx"){
    return trainSIF_rxtx();
  } else {
    printError("trainSIF: Unknown platform"); return 1;
  }
}

void sccApi::abortTraining() {
  userAbort = true;
}

int sccApi::trainSIF_rx(int mode, int freq) {
  // Variables & initializations...

  int errorcount = 0;
  int rx_clk_delay_c;
  unsigned int result[64];
  int good_range[64];
  int opt_dly_setting[64];
  uInt32 data = 0;
  int rx_data_incs = 1;
  int trainingMode = mode;
  bool presetMode = false;

  // setting variables
  int good_result_found     = 0;
  int fw_first_good_result  = 0;
  int fw_last_good_result   = 0;
  int bw_first_good_result  = 0;
  int bw_last_good_result   = 0;
  int fw_range        = 0;
  int bw_range        = 0;
  int full_range      = 0;
  int optimal_setting       = 0;

  // Check if BMC is connected!
  if (!connectedBMCServer()) {
    printError("BMC server needs to be selected for SIF training! Aborting...");
    return 1;
  }

  // Automatic mode...
  if (trainingMode==AUTO_MODE) {
    degradeWarnings = true;
    abortAutoTraining = false;
    if ((errorcount=trainSIF(PRESET_MODE, freq)) && !abortAutoTraining) {
      if ((errorcount=trainSIF(FAST_MODE, freq)) && !abortAutoTraining) {
        if ((errorcount=trainSIF(EXTENDED_MODE, freq)) && !abortAutoTraining) {
          errorcount=trainSIF(FULL_MODE, freq);
        }
      }
    }
    degradeWarnings = false;
    if (errorcount) {
      printError("Failed to trim SIF! Please try to reload the driver or powercycle SCC board in case of repeated failure!");
    }
    return errorcount;
  }

  // Info
  printInfo("Trying to train in ", (trainingMode==FAST_MODE)?"fast":(trainingMode==EXTENDED_MODE)?"extended":(trainingMode==FULL_MODE)?"full (slowest)":"preset", " mode:");

  // Reset the actual board (BMC server must be connected)
  if (platform == "Copperridge") {
    printInfo("Resetting Copperridge Board: ", executeBMCCommand("board_reset"));
  } else {
    printInfo("Resetting Rocky Lake SCC device: ", executeBMCCommand("reset scc"));
    printInfo("Resetting Rocky Lake FPGA: ", executeBMCCommand("reset fpga"));
  }
  printInfo("Re-Initializing PCIe driver..."); reInitDriver();

  // Activate dropping of write responses in FPGA:
  if(!readFpgaGrb(SIFGRB_RESPONSE_ENABLE, &data)) {
    abortAutoTraining = true;
    return 1;
  }
  data |= 0x04;
  writeFpgaGrb(SIFGRB_RESPONSE_ENABLE, data);

  // Find out what FPGA revision we are using...
  readFpgaGrb(SIFCFG_BitSID, &data);
  BitSID = messageHandler::toString(data,16,8);
  printInfo("FPGA version register value: ", BitSID);

  // In PRESET_MODE, load presets and perform fast training...
  if (trainingMode == PRESET_MODE) {
    if (load_SIF_calibration_settings()) {
      printInfo("Programming done. Testing:");
      trainingMode = FAST_MODE;
      presetMode = true;
    } else {
      printInfo("No SIF system settings availabe...");
      return 1;
    }
  } else if (trainingMode == PRESET_NO_CHECK) {
    if (load_SIF_calibration_settings()) {
      printInfo("Programming done. No testing because we only load pre-sets...");
      printInfo("Loaded SIF presets. Please be aware that this only works after re-programming the FPGA (Hardware power cycle)...");
      printInfo("If you still get timeouts after using this option, please perform a power-cycle, followed by a regular SIF training!");
    } else {
      printInfo("No SIF system settings availabe...");
      return 1;
    }
    return 0;
  }

  // initialize results to zero
  for (int i = 0; i < 64; i++) {
    result[i] = 0;
  }

  printDebug("Creating SIF_phy_settings file in scenario directory...");
  QString filename = getTempPath()+"phy_settings_"+BitSID+".txt";
  QFile file(filename);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    printError("Failed to open phy settings file \"",filename,"\"... File is not writable. Aborting!");
    return 1;
  }

  QTextStream out(&file);
  // file header
  out << "# RX Data Delay    RX Clock delay    with good delay range" << endl;

  // LOOP RX Data delay in extended mode
  if (trainingMode != FAST_MODE) rx_data_incs = 64;

  // Init good_range array...
  for (int rx_data_dly = 0; rx_data_dly < rx_data_incs; rx_data_dly++) {
    opt_dly_setting[rx_data_dly] = 0;
    good_range[rx_data_dly] = -1;
  }

  // For SIF training debugging only
  #if false
    change_delay(RX_Data,INCR,28);
    printWarning("Spoiling of initial SIF settings is enabled!");
  #endif

  // iterate RX Date delay settings
  for (int rx_data_dly = 0; rx_data_dly < rx_data_incs; rx_data_dly++) {
    // iterate RCK_PATTERN_DUMP mode and LOOPBACK_PATTERN_DUMP mode
    for (int run_mode = 0; run_mode < 2; run_mode++) {
      sccProgressBar pd(false);
      pd.setMinimum(0);
      pd.setMaximum(63);
      // CONFIG
      // #define RCK_PATTERN_DUMP 0
      // #define LOOPBACK_PATTERN_DUMP 1
      // #define NO_PATTERN_DUMP 2

      // enable pattern generation + monitoring
      data = run_mode+0x00000002;

      if (run_mode == LOOPBACK_PATTERN_DUMP) {
        if (trainingMode != FAST_MODE) {
          printInfo("Enabling FPGA pattern generation + FPGA monitoring for RX Data inc-delay ", QString::number(rx_data_dly), "...");
        } else {
          printInfo("Enabling FPGA pattern generation + FPGA monitoring for current delay...");
        }
        set_sysif_jtag_mode(run_mode);
        writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);
      } else if (run_mode == RCK_PATTERN_DUMP) {
        if (trainingMode != FAST_MODE) {
          printInfo("Enabling RC pattern generation + FPGA monitoring for RX Data inc-delay ", QString::number(rx_data_dly), "...");
        } else {
          printInfo("Enabling RC pattern generation + FPGA monitoring for current delay...");
        }
        writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);
        set_sysif_jtag_mode(run_mode);
      }

      printDebug("Disable RX Data delay change in increment direction.");
      data = 0x00000000;
      writeFpgaGrb(SIFPHY_PHYiodEnDataIn, data);
      writeFpgaGrb(SIFPHY_PHYiodDirDataIn, data);
      writeFpgaGrb(SIFPHY_PHYiodEnDataIn+4, data); // Next register -> 4 bytes offset...
      writeFpgaGrb(SIFPHY_PHYiodDirDataIn+4, data); // Next register -> 4 bytes offset...

      printDebug("Enable RX CLK delay change in increment direction.");
      data = 0x00000200;
      writeFpgaGrb(SIFPHY_PHYiodEnRXIn, data);
      writeFpgaGrb(SIFPHY_PHYiodDirRXIn, data);

      // SAMPLE
      // sample phy interface errors for all possible 64 RX CLK delay settings
      for (rx_clk_delay_c = 0; rx_clk_delay_c < 64; rx_clk_delay_c++) {
        if (log->printsToShell()) {
          pd.setValue(rx_clk_delay_c);
        }
        // User abort...
        if (userAbort) {
          abortAutoTraining = true;
          return 0;
        }

        // accumulate errors from both run_modes (RCK_PATTERN_DUMP and LOOPBACK_PATTERN_DUMP)
        result[rx_clk_delay_c] = result[rx_clk_delay_c] + unexpected_flits_found_at_delay(rx_clk_delay_c,run_mode);

        data = SIFPHY_PHYiodPassPhrase_Trigger;
        writeFpgaGrb(SIFPHY_PHYiodClockPulse, data);
        sprintf(message, "Triggered delay increment for RX CLK to delay value %03d.", rx_clk_delay_c+1 ); if (SIFTRAINING_DEBUG) printInfo(message);

        if (run_mode == RCK_PATTERN_DUMP) {
          set_sysif_jtag_mode(NO_PATTERN_DUMP);
        }

        if (SIFTRAINING_DEBUG) printInfo("Toggling pattern generation enable to reset SIF pattern capture statemachine!");
        data = 0x00000000;
        writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);
        data = run_mode+0x00000002;
        writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);

        if (run_mode == RCK_PATTERN_DUMP) {
          set_sysif_jtag_mode(RCK_PATTERN_DUMP);
        }

      } // rx_clk_delay_c

      // EVALUATE
      // examine pattern dump results and find optimal delay setting for best data launch and capture

      for (int loop_counter = 0; loop_counter < 64; loop_counter++) {
        sprintf(message, "*** Data mismatches found in flits with rx delay #%03d: %03d ***", loop_counter, result[loop_counter]); if (SIFTRAINING_DEBUG) printInfo(message);
      }

      // seach both forward
      good_result_found  = 0;
      for (int loop_counter = 0; loop_counter < 64; loop_counter++) {
        if (result[loop_counter] == 0) {
          if (!good_result_found) {fw_first_good_result = loop_counter;}
          good_result_found  = 1;
          fw_last_good_result = loop_counter;
        } else {
          // exit forward search if data eye found
          if (good_result_found) { loop_counter = 64; }
        }
      }


      // ... and backward
      good_result_found  = 0;
      for (int loop_counter = 63; loop_counter > -1; loop_counter--) {
        if (result[loop_counter] == 0) {
          if (!good_result_found) {bw_first_good_result = loop_counter;}
          good_result_found  = 1;
          bw_last_good_result = loop_counter;
        } else {
          // exit backward search if data eye found
          if (good_result_found) { loop_counter = -1; }
        }
      }

      sprintf(message, "*** forward seach:  first_good_result = %03d ***", fw_first_good_result); if (SIFTRAINING_DEBUG) printInfo(message);
      sprintf(message, "*** forward seach:  last_good_result  = %03d ***", fw_last_good_result); if (SIFTRAINING_DEBUG) printInfo(message);
      sprintf(message, "*** backward seach: first_good_result = %03d ***", bw_first_good_result); if (SIFTRAINING_DEBUG) printInfo(message);
      sprintf(message, "*** backward seach: last_good_result  = %03d ***", bw_last_good_result); if (SIFTRAINING_DEBUG) printInfo(message);

      // if there is a good results at the start and end of the calibration run, the calculation gets more tricky
      if ((fw_first_good_result == 0) && (bw_first_good_result == 63)) {
        fw_range = fw_last_good_result - fw_first_good_result + 1;
        bw_range = bw_first_good_result - bw_last_good_result + 1;
        full_range = fw_range + bw_range;

        if (fw_range > bw_range) {
          optimal_setting = (full_range/2) - bw_range;
        }
        else {
          optimal_setting = 63 - ((full_range/2) - fw_range);
        }
      }
      else {
        full_range = fw_last_good_result-fw_first_good_result+1;
        optimal_setting = (full_range/2)+fw_first_good_result;
        if ( full_range < (bw_first_good_result - bw_last_good_result + 1)) {
          full_range = bw_first_good_result - bw_last_good_result + 1;
          optimal_setting = bw_first_good_result - (full_range/2);
        }
      }

      // if no working delay setting found reset full_range to zero
      if (!good_result_found) { full_range = 0; }

      good_range[rx_data_dly] = full_range;
      opt_dly_setting[rx_data_dly] = optimal_setting;

      // TRIM
      if (run_mode == LOOPBACK_PATTERN_DUMP) {
        out << "" << messageHandler::toString(rx_data_dly, 10,2) << "                 "<< messageHandler::toString(opt_dly_setting[rx_data_dly], 10,2) << "                 "<< messageHandler::toString(good_range[rx_data_dly], 10,2)<< endl;
        // set delay for optimal timings in EXTENDED_MODE
        if (!(trainingMode != FAST_MODE)) { change_delay(RX_Clock,INCR,optimal_setting); }
      }

      if (run_mode == LOOPBACK_PATTERN_DUMP) {
        printInfo("Done... Disabling pattern generation -> Good range of this run is ", QString::number(good_range[rx_data_dly]),"...");
      }

      // Success message for fast training...
      if (trainingMode == FAST_MODE) {
        if (full_range > 6) {
          sprintf(message, "*** Optimal delay setting after fast training is %03d in a %03d wide range ***", optimal_setting, full_range); if (SIFTRAINING_DEBUG) printInfo(message);
        } else {
          printfInfo("%s training unsuccessful... Trying more sophisticated training option:", (presetMode)?"Preset":"Fast");
          if (presetMode) {
            restore_SIF_calibration_settings();
          }
          errorcount++;
        }
      }

      // first disable FPGA pattern gen
      data = 0x00000002;
      writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);
      // then disable RC pttern generation
      set_sysif_jtag_mode(NO_PATTERN_DUMP);
      // finally disable FPGA patten monitoring to return to a functional state
      data = 0x00000000;
      writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);

    } // run_mode

    if (trainingMode != FAST_MODE) {

      change_delay(RX_Data,INCR,1);
      sprintf(message, "Triggered delay increment for RX Data to delay value %03d.", rx_data_dly+1 ); if (SIFTRAINING_DEBUG) printInfo(message);

      // initialize results to zero
      for (int i = 0; i < 64; i++) {
        result[i] = 0;
      }
    } // extended_mode

    if (trainingMode == EXTENDED_MODE) { // Allows some shortcuts compared to FULL_MODE...
      if (good_range[rx_data_dly]==0) {
        // Speed up... Good range doesn't seem to be in the neighbourhood...
        change_delay(RX_Data,INCR,5);
        rx_data_dly += 5;
      } else if (good_range[rx_data_dly]<4) {
        // Speed up... Good range seems to be close but not the direct neighbour...
        change_delay(RX_Data,INCR,2);
        rx_data_dly += 2;
      } else if (good_range[rx_data_dly]<6) {
        // Speed up... Good range seems to be close but not the direct neighbour...
        change_delay(RX_Data,INCR,1);
        rx_data_dly += 1;
      } else if ((rx_data_dly > 0) && (good_range[rx_data_dly-1]>=7)) {
        if (good_range[rx_data_dly-1] > good_range[rx_data_dly]) {
          // It get's worse from here on... Stop here! Increment delay to rx_data_incs...
          while (++rx_data_dly < rx_data_incs) {
            change_delay(RX_Data,INCR,1);
          }
        }
      }
    }

  } // rx_data_dly

  // Evaluation in extended & full mode...
  if (trainingMode != FAST_MODE) {
    // find maximum good_range
    int max_range = good_range[0];
    int max_range_delay = 0;

    for (int i = 0; i < 64; i++) {
      if (good_range[i]>=0) {
        sprintf(message, "Good_Range with RX DATA delay setting %03d is %03d.", i, good_range[i] ); printInfo(message);
        if (max_range < good_range[i]) {
          max_range = good_range[i];
          max_range_delay = i;
        }
      }
    }

    if (good_range[max_range_delay] > 6) {
      printfInfo( "*** Max Good_Range is %03d at RX DATA delay setting %03d and RX CLK delay setting %03d.", good_range[max_range_delay], max_range_delay, opt_dly_setting[max_range_delay] );;
      sifSettings->setValue("sifRxDataDelay", max_range_delay);
      sifSettings->setValue("sifClockDelay", opt_dly_setting[max_range_delay]);
    } else {
      if (trainingMode == FULL_MODE) {
        printfError("Training unsuccessful: Too small optimal data eye: optimal delay setting is %03d in a %03d wide range, select slower/different SIF phy operation speed ***", opt_dly_setting[max_range_delay], good_range[max_range_delay]);
      } else {
        printInfo( "Training unsuccessful... Trying more full training option:");
      }
      errorcount++;
    }

    out << "# Overall Best Delay Settings" << endl;
    out << "# RX Data Delay    RX Clock delay    with good delay range" << endl;
    out << "" << messageHandler::toString(max_range_delay, 10,2) << "                 "<< messageHandler::toString(opt_dly_setting[max_range_delay], 10,2) << "                 "<< messageHandler::toString(good_range[max_range_delay], 10,2)<< endl;

    change_delay(RX_Data,INCR,max_range_delay);
    change_delay(RX_Clock,INCR,opt_dly_setting[max_range_delay]);

  } // extended_mode


  file.close();

  // Reset the actual board (BMC server must be connected)
  if ((full_range > 6)) {
    if (platform == "Copperridge") {
      printInfo("Resetting Copperridge Board: ", executeBMCCommand("board_reset"));
    } else {
      printInfo("Resetting Rocky Lake SCC device: ", executeBMCCommand("reset scc"));
      printInfo("Resetting Rocky Lake FPGA: ", executeBMCCommand("reset fpga"));
    }
  }

  return errorcount;
}

//
// SIF training for Rocky Lake boards
//
int sccApi::trainSIF_rxtx() {
  // Variables & initializations...
  int    errorcount = 0;
  uInt32 data = 0;

  // Check if BMC is connected!
  if (!connectedBMCServer()) {
    printError("BMC server needs to be selected for SIF training! Aborting...");
    return 1;
  }

  // Reset the actual board (BMC server must be connected)
  if (platform == "Copperridge") {
    printInfo("Resetting Copperridge Board: ", executeBMCCommand("board_reset"));
  } else {
    printInfo("Resetting Rocky Lake SCC device: ", executeBMCCommand("reset scc"));
    printInfo("Resetting Rocky Lake FPGA: ", executeBMCCommand("reset fpga"));
  }
  printInfo("Re-Initializing PCIe driver..."); reInitDriver();

  // Activate dropping of write responses in FPGA:
  if(!readFpgaGrb(SIFGRB_RESPONSE_ENABLE, &data)) {
    abortAutoTraining = true;
    return 1;
  }
  data |= 0x04;
  writeFpgaGrb(SIFGRB_RESPONSE_ENABLE, data);

  // Find out what FPGA revision we are using...
  readFpgaGrb(SIFCFG_BitSID, &data);
  BitSID = messageHandler::toString(data,16,8);
  printInfo("FPGA version register value: ", BitSID);
  if(!data) {
    printError("FPGA is not loaded correctly (version register 0x00000000). Aborting!");
    abortAutoTraining = true;
    return 1;
  }


  /* not supported in HW currently
    // reset IODelay settings
    data[0] = SIFPHY_PHYiodPassPhrase_Reset;
    writeFpgaGrb(SIFPHY_PHYiodClockPulse, data);
    data[0] = 0x00000000;
    writeFpgaGrb(SIFPHY_PHYiodClockPulse, data);
   */

  //
  // SIF training, extended mode:
  //
  // 1. step: SCC_PATTERN_DUMP      (find optimal rx_clk_delay {w/ rx_data_dly=0})
  // 2. step: LOOPBACK_PATTERN_DUMP (find optimal tx_clk_delay)
  // [ 3. step: optimize rx data delay for every data bit               ]
  // [ 4. step: LOOPBACK_PATTERN_DUMP (find optimal tx_clk_delay again) ]
  //


  trainSIF_rlb_rxclk();

  if (rx_good_range > 4) {
    sprintf(message, "*** Max Good_Range is %03d at RX CLK delay setting %03d.", rx_good_range, rx_opt_dly_setting );
    printInfo(message);
  }
  else {
    sprintf(message, "*** Training Unsuccessful: Too small optimal data eye: optimal delay setting is %03d in a %03d wide range, select slower/different SIF phy operation speed ***", rx_opt_dly_setting, rx_good_range);
    printError(message);
    errorcount++;
  }


  if (errorcount > 0)
  {
    printError("Failed to trim SIF! Please try to reload the driver or powercycle SCC board in case of repeated failure!");
    return errorcount;
  }


  // set optimal RX clock delay
  change_delay(RX_Clock,INCR,rx_opt_dly_setting);


  trainSIF_rlb_txclk();

  if (tx_good_range > 4) {
    sprintf(message, "*** LOOPBACK_PATTERN_DUMP: Max Good_Range is %03d at RX CLK delay setting %03d and TX CLK delay setting %03d.",
                     tx_good_range, rx_opt_dly_setting, tx_opt_dly_setting );
    printInfo(message);
  } else {
    sprintf(message, "*** Training Unsuccessful: Too small optimal data eye: optimal delay setting is %03d in a %03d wide range, select slower/different SIF phy operation ***", tx_opt_dly_setting, tx_good_range);
    printError(message);
    errorcount++;
  }

  change_delay(TX_Clock,INCR,tx_opt_dly_setting);

  if (platform == "Copperridge") {
    printInfo("Resetting Copperridge Board: ", executeBMCCommand("board_reset"));
  } else {
    printInfo("Resetting Rocky Lake SCC device: ", executeBMCCommand("reset scc"));
    printInfo("Resetting Rocky Lake FPGA: ", executeBMCCommand("reset fpga"));
  }

  if (errorcount) {
    printError("Failed to trim SIF! Please try to reload the driver or powercycle SCC board in case of repeated failure!");
  }

  return errorcount;
}

//
// SIF training for Rocky Lake boards: RX clock delay
//
void sccApi::trainSIF_rlb_rxclk()
{
  //int    mode = 0;
  uInt32 data = 0;
  unsigned int result[64];
  int rx_clk_delay_c;
  int good_result_found     = 0;
  int fw_first_good_result  = 0;
  int fw_last_good_result   = 0;
  int bw_first_good_result  = 0;
  int bw_last_good_result   = 0;
  int fw_range        = 0;
  int bw_range        = 0;
  int full_range      = 0;
  uInt32 mism_31_00, mism_63_32, mism_71_64;
  int optimal_setting       = 0;

  // initialize results to zero
  for (int i = 0; i < 64; i++) {
    result[i] = 0;
  }

  sprintf(message, "*** SIF Training: RX Clock delay in SCC_PATTERN_DUMP mode...."); printInfo(message);

  // enable pattern generation + monitoring
  data = SCC_PATTERN_DUMP+0x00000002;

  if (SIFTRAINING_DEBUG) printInfo("Enabling RC pattern generation + FPGA monitoring...");
  writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);
  set_sysif_jtag_mode(SCC_PATTERN_DUMP);

  printDebug("Disable RX Data delay change in increment direction.");
  data = 0x00000000;
  writeFpgaGrb(SIFPHY_PHYiodEnDataIn, data);
  writeFpgaGrb(SIFPHY_PHYiodDirDataIn, data);
  writeFpgaGrb(SIFPHY_PHYiodEnDataIn+4, data); // Next register -> 4 bytes offset...
  writeFpgaGrb(SIFPHY_PHYiodDirDataIn+4, data); // Next register -> 4 bytes offset...

  printDebug("Enable RX CLK delay change in increment direction.");
  data = 0x00000200;
  writeFpgaGrb(SIFPHY_PHYiodEnRXIn, data);
  writeFpgaGrb(SIFPHY_PHYiodDirRXIn, data);

  // SAMPLE
  // sample phy interface errors for all possible 64 RX CLK delay settings
  for (rx_clk_delay_c = 0; rx_clk_delay_c < 64; rx_clk_delay_c++)
  {
    // accumulate errors from SCC_PATTERN_DUMP mode
    result[rx_clk_delay_c] = locate_phit_errors(SCC_PATTERN_DUMP, &mism_31_00, &mism_63_32, &mism_71_64);

    sprintf(message, "*** rx_clk_delay_c=%d %04d bit errors - %02x_%8x%8x",
        rx_clk_delay_c, result[rx_clk_delay_c], mism_71_64, mism_63_32, mism_31_00);
    if (SIFTRAINING_DEBUG) printInfo(message);

    data = SIFPHY_PHYiodPassPhrase_Trigger;
    writeFpgaGrb(SIFPHY_PHYiodClockPulse, data);
    sprintf(message, "Triggered delay increment for RX CLK to delay value %03d.", rx_clk_delay_c+1 );
    if (SIFTRAINING_DEBUG) printInfo(message);

    set_sysif_jtag_mode(NO_PATTERN_DUMP);

    if (SIFTRAINING_DEBUG) printInfo("Toggling pattern generation enable to reset SIF pattern capture statemachine!");
    data = 0x00000000;
    writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);
    data = SCC_PATTERN_DUMP+0x00000002;
    writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);

    set_sysif_jtag_mode(SCC_PATTERN_DUMP);
  } // rx_clk_delay_c

  // EVALUATE
  // examine pattern dump results and find optimal delay setting for best data launch and capture

  for (int loop_counter = 0; loop_counter < 64; loop_counter++) {
    sprintf(message, "*** Data mismatches found in flits with rx delay #%03d: %03d ***", loop_counter, result[loop_counter]);
    if (SIFTRAINING_DEBUG) printInfo(message);
  }

  // seach both forward
  good_result_found  = 0;
  for (int loop_counter = 0; loop_counter < 64; loop_counter++) {
    if (result[loop_counter] == 0) {
      if (!good_result_found) {fw_first_good_result = loop_counter;}
      good_result_found  = 1;
      fw_last_good_result = loop_counter;
    } else {
      // exit forward search if data eye found
      if (good_result_found) { loop_counter = 64; }
    }
  }

  // ... and backward
  good_result_found  = 0;
  for (int loop_counter = 63; loop_counter > -1; loop_counter--) {
    if (result[loop_counter] == 0) {
      if (!good_result_found) {bw_first_good_result = loop_counter;}
      good_result_found  = 1;
      bw_last_good_result = loop_counter;
    } else {
      // exit backward search if data eye found
      if (good_result_found) { loop_counter = -1; }
    }
  }

  // if there is a good results at the start and end of the calibration run, the calculation gets more tricky
  if ((fw_first_good_result == 0) && (bw_first_good_result == 63)) {
    fw_range = fw_last_good_result - fw_first_good_result + 1;
    bw_range = bw_first_good_result - bw_last_good_result + 1;
    full_range = fw_range + bw_range;

    if (fw_range > bw_range) {
      optimal_setting = (full_range/2) - bw_range;
    }
    else {
      optimal_setting = 63 - ((full_range/2) - fw_range);
    }
  }
  else {
    full_range = fw_last_good_result-fw_first_good_result+1;
    optimal_setting = (full_range/2)+fw_first_good_result;
    if ( full_range < (bw_first_good_result - bw_last_good_result + 1)) {
      full_range = bw_first_good_result - bw_last_good_result + 1;
      optimal_setting = bw_first_good_result - (full_range/2);
    }
  }

  // if no working delay setting found reset full_range to zero
  if (!good_result_found) { full_range = 0; }


  if (SIFTRAINING_DEBUG) printInfo("Disabling pattern generation!");
  // first disable FPGA pattern gen
  data = 0x00000002;
  writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);
  // then disable RC pttern generation
  set_sysif_jtag_mode(NO_PATTERN_DUMP);
  // finally disable FPGA pattern monitoring to return to a functional state
  data = 0x00000000;
  writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);

  rx_good_range = full_range;
  rx_opt_dly_setting = optimal_setting;

}



//
// SIF training for Rocky Lake boards: TX clock delay
//
void sccApi::trainSIF_rlb_txclk()
{
  //int    mode = 0;
  uInt32 data = 0;
  unsigned int result[64];
  int tx_clk_delay_c;
  int good_result_found     = 0;
  int fw_first_good_result  = 0;
  int fw_last_good_result   = 0;
  int bw_first_good_result  = 0;
  int bw_last_good_result   = 0;
  int fw_range        = 0;
  int bw_range        = 0;
  int full_range      = 0;
  uInt32 mism_31_00, mism_63_32, mism_71_64;
  int optimal_setting       = 0;


  // initialize results to zero
  for (int i = 0; i < 64; i++) {
    result[i] = 0;
  }

  sprintf(message, "*** SIF training: TX Clock delay in LOOPBACK_PATTERN_DUMP mode...."); printInfo(message);

  // enable pattern generation + monitoring
  data = LOOPBACK_PATTERN_DUMP+0x00000002;

  if (SIFTRAINING_DEBUG) printInfo("Enabling FPGA pattern generation + FPGA monitoring...");
  set_sysif_jtag_mode(LOOPBACK_PATTERN_DUMP);
  writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);

  printDebug("Disable RX Data delay change in increment direction.");
  data = 0x00000000;
  writeFpgaGrb(SIFPHY_PHYiodEnDataIn, data);
  writeFpgaGrb(SIFPHY_PHYiodDirDataIn, data);
  writeFpgaGrb(SIFPHY_PHYiodEnDataIn+4, data); // Next register -> 4 bytes offset...
  writeFpgaGrb(SIFPHY_PHYiodDirDataIn+4, data); // Next register -> 4 bytes offset...

  printDebug("Enable TX CLK delay change in increment direction.");
  data = 0x00000100;
  writeFpgaGrb(SIFPHY_PHYiodEnRXIn, data);
  writeFpgaGrb(SIFPHY_PHYiodDirRXIn, data);

  // SAMPLE
  // sample phy interface errors for all possible 64 TX CLK delay settings
  for (tx_clk_delay_c = 0; tx_clk_delay_c < 64; tx_clk_delay_c++)
  {
    // accumulate errors from LOOPBACK_PATTERN_DUMP
    result[tx_clk_delay_c] = locate_phit_errors(LOOPBACK_PATTERN_DUMP, &mism_31_00, &mism_63_32, &mism_71_64);

    sprintf(message, "*** tx_clk_delay_c=%d %04d bit errors - %02x_%08x_%08x",
        tx_clk_delay_c, result[tx_clk_delay_c], mism_71_64, mism_63_32, mism_31_00);
    if (SIFTRAINING_DEBUG) printInfo(message);

    amism_31_00[tx_clk_delay_c] = mism_31_00;
    amism_63_32[tx_clk_delay_c] = mism_63_32;
    amism_71_64[tx_clk_delay_c] = mism_71_64;

    data = SIFPHY_PHYiodPassPhrase_Trigger;
    writeFpgaGrb(SIFPHY_PHYiodClockPulse, data);
    sprintf(message, "Triggered delay increment for TX CLK to delay value %03d.", tx_clk_delay_c+1 );
    if (SIFTRAINING_DEBUG) printInfo(message);

    set_sysif_jtag_mode(NO_PATTERN_DUMP);

    if (SIFTRAINING_DEBUG) printInfo("Toggling pattern generation enable to reset SIF pattern capture statemachine!");
    data = 0x00000000;
    writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);

    set_sysif_jtag_mode(LOOPBACK_PATTERN_DUMP);
    data = LOOPBACK_PATTERN_DUMP+0x00000002;
    writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);
  } // tx_clk_delay_c

  // EVALUATE
  // examine pattern dump results and find optimal delay setting for best data launch and capture
  for (int loop_counter = 0; loop_counter < 64; loop_counter++) {
    sprintf(message, "*** Data mismatches found in flits with tx clk delay #%03d: %03d ***", loop_counter, result[loop_counter]);
    if (SIFTRAINING_DEBUG) printInfo(message);
  }

  // seach both forward
  good_result_found  = 0;
  for (int loop_counter = 0; loop_counter < 64; loop_counter++) {
    if (result[loop_counter] == 0) {
      if (!good_result_found) {fw_first_good_result = loop_counter;}
      good_result_found  = 1;
      fw_last_good_result = loop_counter;
    } else {
      // exit forward search if data eye found
      if (good_result_found) { loop_counter = 64; }
    }
  }

  // ... and backward
  good_result_found  = 0;
  for (int loop_counter = 63; loop_counter > -1; loop_counter--) {
    if (result[loop_counter] == 0) {
      if (!good_result_found) {bw_first_good_result = loop_counter;}
      good_result_found  = 1;
      bw_last_good_result = loop_counter;
    } else {
      // exit backward search if data eye found
      if (good_result_found) { loop_counter = -1; }
    }
  }

  // if there is a good results at the start and end of the calibration run, the calculation gets more tricky
  if ((fw_first_good_result == 0) && (bw_first_good_result == 63)) {
    fw_range = fw_last_good_result - fw_first_good_result + 1;
    bw_range = bw_first_good_result - bw_last_good_result + 1;
    full_range = fw_range + bw_range;

    if (fw_range > bw_range) {
      optimal_setting = (full_range/2) - bw_range;
    } else {
      optimal_setting = 63 - ((full_range/2) - fw_range);
    }
  } else {
    full_range = fw_last_good_result-fw_first_good_result+1;
    optimal_setting = (full_range/2)+fw_first_good_result;
    if ( full_range < (bw_first_good_result - bw_last_good_result + 1)) {
      full_range = bw_first_good_result - bw_last_good_result + 1;
      optimal_setting = bw_first_good_result - (full_range/2);
    }
  }

  // if no working delay setting found reset full_range to zero
  if (!good_result_found) { full_range = 0; }

  if (SIFTRAINING_DEBUG) printInfo("Disabling pattern generation!");
  // first disable FPGA pattern gen
  data = 0x00000002;
  writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);
  // then disable RC pattern generation
  set_sysif_jtag_mode(NO_PATTERN_DUMP);
  // finally disable FPGA pattern monitoring to return to a functional state
  data = 0x00000000;
  writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);

  tx_good_range      = full_range;
  tx_opt_dly_setting = optimal_setting;
}

//
// SIF training for Rocky Lake boards: RX data delay
// Actually, no training is performed here, instead
// we evaluate the results of the previous training
// (in LOOPBACK mode)
//
void sccApi::trainSIF_rlb_rxdata()
{
  int i_setting;
  int fw_first_bad_result  = 0;
  int bw_first_bad_result  = 0;
  bool mismatch_found;


  //
  // find out how to change RX data delay..
  //
  for (int j = 0; j < 72; j++) {
    // for every data bit: find first value of tx_clk_delay_c with bit error,
    // starting from tx_opt_dly_setting in both INCR and DECR direction.
    // then compare the center of the good range for this data bit with
    // tx_opt_dly_setting and change RX data delay accordingly.

    // search forward for first mismatch..
    mismatch_found = false;
    i_setting      = tx_opt_dly_setting;

    while (mismatch_found == false)
    {
      if (i_setting < 63)
        i_setting++;
      else
        i_setting = 0;

      mismatch_found =  mism_bit(i_setting, j);

      if (mismatch_found)
        fw_first_bad_result = i_setting;
    }

    // search backward for first mismatch..
    mismatch_found = false;
    i_setting      = tx_opt_dly_setting;

    while (mismatch_found == false)
    {
      if (i_setting > 0)
        i_setting--;
      else
        i_setting = 63;

      mismatch_found =  mism_bit(i_setting, j);

      if (mismatch_found)
        bw_first_bad_result = i_setting;
    }

    // add 64 to any value wrapped-around
    if (bw_first_bad_result > fw_first_bad_result)
      fw_first_bad_result += 64;
    if (tx_opt_dly_setting < bw_first_bad_result)
      tx_opt_dly_setting += 64;

    // straight-forward calculation (w/o wrap-around)
    i_setting = (fw_first_bad_result+bw_first_bad_result)/2;

    if (i_setting > tx_opt_dly_setting)
    {
      crd_direction[j] = DECR;
      crd_steps[j]     = i_setting - tx_opt_dly_setting;
    }
    else
    {
      crd_direction[j] = INCR;
      crd_steps[j]     = tx_opt_dly_setting - i_setting;
    }

    // correct tx_opt_dly_setting value
    tx_opt_dly_setting %= 64;

    sprintf(message, "j=%2d: bw_first_bad_result=%2d fw_first_bad_result=%2d",
        j, bw_first_bad_result, fw_first_bad_result);
    if (SIFTRAINING_DEBUG) printInfo(message);

    // limit max steps to 5
    // if (crd_steps[j] > 5) crd_steps[j] = 5;

  } // (int j = 0; j < 72; j++)
}

//
//
//
int sccApi::locate_phit_errors(int mode, uInt32 *mism_31_00, uInt32 *mism_63_32, uInt32 *mism_71_64) {
  uInt32 data = 0;
  unsigned int result = 0;
  int timeout;
  uInt32 phit0_31_00, phit1_31_00;
  uInt32 phit0_63_32, phit1_63_32;
  uInt32 phit0_71_64, phit1_71_64;
  int phit0_pattern;
  int phit1_pattern;
  unsigned int result_55;
  unsigned int result_AA;
  uInt32 tmp_mism_31_00, tmp_mism_63_32, tmp_mism_71_64;

  // initialize flags for phit errors
  tmp_mism_31_00 = 0x00000000;
  tmp_mism_63_32 = 0x00000000;
  tmp_mism_71_64 = 0x00000000;

  data = mode+0x0000000a;
  writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);

  // Check if trigger on write worked...
  data = 0x00000000;
  timeout = 10;
  while (data&0x08) {
    readFpgaGrb(SIFPHY_PHYPatternGenOn, &data);
    if (!timeout--) {
      printError("Trigger on write failed (timeout)! Aborting...");
      data = 0x00000000;
      writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);
      return 1;
    }
  }


  for (int loop_counter = 0; loop_counter < 32; loop_counter++)
  {
    // get Next Flit
    data = mode+0x00000012;
    writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);

    // read Flit data
    unsigned int fifo_content[5];
    data =  0x00000000;
    for (int loop_fifo = 0; loop_fifo < 5; loop_fifo++) {
      readFpgaGrb(SIFPHY_PHYFlitData+(4*loop_fifo), &data);
      fifo_content[loop_fifo] = data;
    }

    phit0_31_00 = fifo_content[0];
    phit0_63_32 = fifo_content[1];
    phit0_71_64 = fifo_content[4] & 0xff;

    phit1_31_00 = fifo_content[2];
    phit1_63_32 = fifo_content[3];
    phit1_71_64 = (fifo_content[4] & 0xff00) >> 8;

    // three possible expected pattern (FLIT0, FLIT1, FLIT2):
    // phit1                  phit0
    // 55_55555555_55555555   AA_AAAAAAAA_AAAAAAAA
    // AA_AAAAAAAA_AAAAAAAA   AA_AAAAAAAA_AAAAAAAA
    // 55_55555555_55555555   55_55555555_55555555

    // check FIFO content for possible patterns; we assume that the pattern
    // with the least number of bit mismatches is the correct one
    // phit0 first
    result_55  = count_1s(phit0_31_00^0x55555555) + count_1s(phit0_63_32^0x55555555) + count_1s(phit0_71_64^0x00000055);
    result_AA  = count_1s(phit0_31_00^0xAAAAAAAA) + count_1s(phit0_63_32^0xAAAAAAAA) + count_1s(phit0_71_64^0x000000AA);
    if (result_55 < result_AA)
    {
      result += result_55;
      phit0_pattern = PATTERN_55;
    }
    else
    {
      result += result_AA;
      phit0_pattern = PATTERN_AA;
    }

    // .. then phit1
    result_55 = count_1s(phit1_31_00^0x55555555) + count_1s(phit1_63_32^0x55555555) + count_1s(phit1_71_64^0x00000055);
    result_AA = count_1s(phit1_31_00^0xAAAAAAAA) + count_1s(phit1_63_32^0xAAAAAAAA) + count_1s(phit1_71_64^0x000000AA);
    if (result_55 < result_AA)
    {
      result += result_55;
      phit1_pattern = PATTERN_55;
    }
    else
    {
      result += result_AA;
      phit1_pattern = PATTERN_AA;
    }

    if (phit0_pattern == PATTERN_55)
    {
      tmp_mism_31_00 |= (phit0_31_00^0x55555555);
      tmp_mism_63_32 |= (phit0_63_32^0x55555555);
      tmp_mism_71_64 |= (phit0_71_64^0x00000055);
    }
    else
    {
      tmp_mism_31_00 |= (phit0_31_00^0xAAAAAAAA);
      tmp_mism_63_32 |= (phit0_63_32^0xAAAAAAAA);
      tmp_mism_71_64 |= (phit0_71_64^0x000000AA);
    }

    if (phit1_pattern == PATTERN_55)
    {
      tmp_mism_31_00 |= (phit1_31_00^0x55555555);
      tmp_mism_63_32 |= (phit1_63_32^0x55555555);
      tmp_mism_71_64 |= (phit1_71_64^0x00000055);
    }
    else
    {
      tmp_mism_31_00 |= (phit1_31_00^0xAAAAAAAA);
      tmp_mism_63_32 |= (phit1_63_32^0xAAAAAAAA);
      tmp_mism_71_64 |= (phit1_71_64^0x000000AA);
    }


  } // loop_counter


  *mism_31_00 = tmp_mism_31_00;
  *mism_63_32 = tmp_mism_63_32;
  *mism_71_64 = tmp_mism_71_64;

  return result;
}

bool sccApi::mism_bit(int delay, int iBit)
{
  uInt32 mask = 0x00000001;
  uInt32 mism = 0x00000000;

  if ((iBit >= 0) && (iBit <= 71) && (delay >= 0) && (delay <= 63))
  {
    // ...ok
  }
  else
  {
    sprintf(message, "sccApi::mism_bit() called with illegal parameters: delay=%2d,  iBit=%2d",
        delay, iBit); printError(message);
        return true; // return mismatch found
  }

  if (iBit <= 31)
  {
    mask = mask << iBit;
    mism = amism_31_00[delay] & mask;
  }
  else if (iBit <= 63)
  {
    mask = mask << (iBit-32);
    mism = amism_63_32[delay] & mask;
  }
  else
  {
    mask = mask << (iBit-64);
    mism = amism_71_64[delay] & mask;
  }

  if (mism == 0x00000000)
    return false;
  else
    return true;
}

int sccApi::count_1s(uInt32 data)
{
  unsigned int result = 0;

  for (int loop_counter = 0; loop_counter < 32; loop_counter++)
  {
    result += (data & 0x00000001);
    data = (data >> 1);
  }

  return result;
}

void sccApi::change_rxdata_delay(int direction, int steps, int iBit)
{
  // change rx data delay for data bit with index [iBit]
  uInt32 data = 0;
  uInt32 data_31_00 = 0x00000000;
  uInt32 data_63_32 = 0x00000000;
  uInt32 data_71_64 = 0x00000000;

  if ((iBit >= 0 && iBit <= 71) && (direction == DECR || direction == INCR) && (steps >= 0 && steps <= 32))
  {
    // limit max number of steps
    if (steps > RXDATA_MAXSHIFT)
      steps = RXDATA_MAXSHIFT;

    sprintf(message, "Triggering delay increment for RX DATA [ %2d ] in %s direction %2d times",
        iBit, (direction == DECR) ? "DECR" : "INCR", steps);
    //if (SIFTRAINING_DEBUG)
      printInfo(message);
  }
  else
  {
    sprintf(message, "change_rxdata_delay() called with illegal parameters: direction=%d, steps=%2d, iBit=%2d",
        direction, steps, iBit);
    printInfo(message);
    return;
  }

  if (iBit <= 31)
  {
    data_31_00 = 0x00000001;
    data_31_00 = data_31_00 << iBit;
  }
  else if (iBit <= 63)
  {
    data_63_32 = 0x00000001;
    data_63_32 = data_63_32 << (iBit-32);
  }
  else
  {
    data_71_64 = 0x00000001;
    data_71_64 = data_71_64 << (iBit-64);
  }

  writeFpgaGrb(SIFPHY_PHYiodEnDataIn,    data_31_00);
  writeFpgaGrb(SIFPHY_PHYiodEnDataIn+4,  data_63_32);
  writeFpgaGrb(SIFPHY_PHYiodEnDataIn+8,  data_71_64);

  if (direction == DECR)
  {
    writeFpgaGrb(SIFPHY_PHYiodDirDataIn,   0x00000000);
    writeFpgaGrb(SIFPHY_PHYiodDirDataIn+4, 0x00000000);
    writeFpgaGrb(SIFPHY_PHYiodDirDataIn+8, 0x00000000);
  }
  else // INCR
  {
    writeFpgaGrb(SIFPHY_PHYiodDirDataIn,   0xffffffff);
    writeFpgaGrb(SIFPHY_PHYiodDirDataIn+4, 0xffffffff);
    writeFpgaGrb(SIFPHY_PHYiodDirDataIn+8, 0x000000ff);
  }

  data = SIFPHY_PHYiodPassPhrase_Trigger;
  for (int i = 0; i < steps; i++) {
    writeFpgaGrb(SIFPHY_PHYiodClockPulse, data);
  }
}

int sccApi::unexpected_flits_found_at_delay(int curr_delay, int mode) {

  uInt32 data = 0;
  unsigned int result = 0;
  int timeout;

  sprintf(message,"*** curr_delay=%03d", curr_delay);if (SIFTRAINING_DEBUG) printInfo(message);

  if (SIFTRAINING_DEBUG) printInfo("Starting capture cycle.");
  data = mode+0x0000000a;
  writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);

  // Check if trigger on write worked...
  data = 0x00000000;
  timeout = 10;
  while (data&0x08) {
    readFpgaGrb(SIFPHY_PHYPatternGenOn, &data);
    if (!timeout--) {
      printError("Trigger on write failed (timeout)! Aborting...");
      data = 0x00000000;
      writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);
      return 1;
    }
  }
  if (SIFTRAINING_DEBUG) printInfo("Capture cycle completed.");

  // Dump FIFO content!
  if (SIFTRAINING_DEBUG) printInfo("************************************************************************************");
  sprintf(message, "*** RX CLK Delay:  %03d * 78ps = %03d ps (max: 63*78ps=4914ps)", curr_delay,  curr_delay*78 );
  if (SIFTRAINING_DEBUG) printInfo(message);
  for (int loop_counter = 0; loop_counter < 32; loop_counter++) {
    // get Next Flit
    data = mode+0x00000012;
    writeFpgaGrb(SIFPHY_PHYPatternGenOn, data);

    // read Flit data
    unsigned int fifo_content[5];
    data =  0x00000000;
    // pkt_rx.data[031:000] = pkt_tx.data[031:000];
    // pkt_rx.data[063:032] = pkt_tx.data[031:000];
    // pkt_rx.data[095:064] = pkt_tx.data[031:000];
    // pkt_rx.data[127:096] = pkt_tx.data[031:000];
    // pkt_rx.data[159:128] = pkt_tx.data[031:000];
    // phy_monitor_data[2*loop_counter]   = {pkt_rx.data[135:128], pkt_rx.data[063:000]};
    // phy_monitor_data[2*loop_counter+1] = {pkt_rx.data[143:136], pkt_rx.data[127:064]};
    for (int loop_fifo = 0; loop_fifo < 5; loop_fifo++) {
      readFpgaGrb(SIFPHY_PHYFlitData+(4*loop_fifo), &data);
      fifo_content[loop_fifo] = data;
    }
    if (((fifo_content[0] != 0xaaaaaaaa) & (fifo_content[0] != 0x55555555)) |
        ((fifo_content[1] != 0xaaaaaaaa) & (fifo_content[1] != 0x55555555)) |
        ((fifo_content[2] != 0xaaaaaaaa) & (fifo_content[2] != 0x55555555)) |
        ((fifo_content[3] != 0xaaaaaaaa) & (fifo_content[3] != 0x55555555)) |
        !(((fifo_content[4] & 0x0000ffff) == 0x0000aaaa) | ((fifo_content[4] & 0x0000ffff) == 0x00005555)
            | ((fifo_content[4] & 0x0000ffff) == 0x000055aa)))
    {
      sprintf(message, "*** Loop %02d/32: Flit is %04x_%08x%08x%08x%08x (Phit 0 = %02x_%08x%08x, Phit 1 = %02x_%08x%08x)", loop_counter+1, fifo_content[4] & 0x0ffff, fifo_content[3], fifo_content[2], fifo_content[1], fifo_content[0], fifo_content[4] & 0x0ff, fifo_content[1], fifo_content[0], (fifo_content[4] & 0x0ff00)>>8, fifo_content[3], fifo_content[2]);
      if (SIFTRAINING_DEBUG) printWarning(message);
      result++;
    }
    else
    {
      sprintf(message, "*** Loop %02d/32: Flit is %04x_%08x%08x%08x%08x (Phit 0 = %02x_%08x%08x, Phit 1 = %02x_%08x%08x)", loop_counter+1, fifo_content[4] & 0x0ffff, fifo_content[3], fifo_content[2], fifo_content[1], fifo_content[0], fifo_content[4] & 0x0ff, fifo_content[1], fifo_content[0], (fifo_content[4] & 0x0ff00)>>8, fifo_content[3], fifo_content[2]);
      if (SIFTRAINING_DEBUG) printInfo(message);
    }
  }
  sprintf(message, "*** Data mismatches found in flits with rx delay #%03d: %03d ***", curr_delay, result); if (SIFTRAINING_DEBUG) printInfo(message);
  sprintf(message,"************************************************************************************"); if (SIFTRAINING_DEBUG) printInfo(message);

  return result;
}


void sccApi::change_delay(int target, int direction, int count) {

  uInt32 data = 0;
  int increments;

  // sprintf(message, "change_delay: target=%d, direction=%d, count=%d",target,direction,count);printInfo(message);

  if (direction == DECR) {
    increments = 64-count;
  } else {
    increments = count;
  }

  if (target == RX_Data) {
    sprintf(message, "Triggering delay increment for RX DATA %03d times.", increments );
    if (SIFTRAINING_DEBUG) printInfo(message);
  }
  else if (target == RX_Clock) {
    sprintf(message, "Triggering delay increment for RX Clock %03d times.", increments );
    if (SIFTRAINING_DEBUG) printInfo(message);
  }
  else if (target == TX_Clock) {
    sprintf(message, "Triggering delay increment for TX Clock %03d times.", increments );
    if (SIFTRAINING_DEBUG) printInfo(message);
  }
  else
  {
    sprintf(message, "change_delay: illegal value for target:%d", target); printError(message); return;
  }

  if (target == RX_Data)
    data = 0xFFFFFFFF;
  else if (target == RX_Clock)
    data = 0x00000000;
  else if (target == TX_Clock)
    data = 0x00000000;

  writeFpgaGrb(SIFPHY_PHYiodEnDataIn, data);
  writeFpgaGrb(SIFPHY_PHYiodDirDataIn, data);
  writeFpgaGrb(SIFPHY_PHYiodEnDataIn+4, data); // Next register -> 4 bytes offset...
  writeFpgaGrb(SIFPHY_PHYiodDirDataIn+4, data); // Next register -> 4 bytes offset...


  if (target == RX_Data)
    data = 0x000000FF;
  else if (target == RX_Clock)
    data = 0x00000200;
  else if (target == TX_Clock)
    data = 0x00000100;

  writeFpgaGrb(SIFPHY_PHYiodEnRXIn, data);
  writeFpgaGrb(SIFPHY_PHYiodDirRXIn, data);

  data = SIFPHY_PHYiodPassPhrase_Trigger;
  for (int i = 0; i < increments; i++) {
    writeFpgaGrb(SIFPHY_PHYiodClockPulse, data);
  }
}

bool sccApi::load_SIF_calibration_settings() {
  bool ok;
  int rx_data_delay=-1;
  int rx_clock_delay=-1;

  // Load settings
  rx_data_delay = sifSettings->value("sifRxDataDelay", -1).toInt(&ok);
  rx_clock_delay = sifSettings->value("sifClockDelay", -1).toInt(&ok);

  // Program presets (if available)...
  if ((rx_data_delay!=-1) && (rx_clock_delay!=-1)) {
    printfInfo("Loading SIF system settings: RX Data delay is %02d and RX Clock delay is %02d... ", rx_data_delay, rx_clock_delay);

    change_delay(RX_Data,INCR,rx_data_delay);
    change_delay(RX_Clock,INCR,rx_clock_delay);

    usleep(100);
    return true;
  } else {
    return false;
  }
}

void sccApi::restore_SIF_calibration_settings() {
  bool ok;
  int rx_data_delay=-1;
  int rx_clock_delay=-1;

  // Load settings
  rx_data_delay = sifSettings->value("sifRxDataDelay", -1).toInt(&ok);
  rx_clock_delay = sifSettings->value("sifClockDelay", -1).toInt(&ok);

  // Program presets (if available)...
  if ((rx_data_delay!=-1) && (rx_clock_delay!=-1)) {
    printfInfo("Restoring SIF system settings... ");

    change_delay(RX_Data,DECR,rx_data_delay);
    change_delay(RX_Clock,DECR,rx_clock_delay);

    usleep(100);
  }
}

void sccApi::set_sysif_jtag_mode(int jtag_mode) {

  QString BMC_CMD, value;
  QStringList BMC_CMD_L,BMC_CMD_L2;
  uInt64 jtag_read_data=0;
  uInt64 jtag_write_data=0;
  bool ok;

  // read-modify-write to SYSIF JTAG CHAIN
  BMC_CMD = executeBMCCommand("jtag read right sysif");
  if (SIFTRAINING_JTAG_DEBUG) printInfo(BMC_CMD);

  BMC_CMD_L = BMC_CMD.split(" ");

  if (SIFTRAINING_JTAG_DEBUG) {
    for (int index = 0; index < BMC_CMD_L.size(); index++) {
      sprintf(message, "BMC_CMD_L [%03d] ", index ); printInfo(message);
      printInfo(BMC_CMD_L.at(index));
    }
  }

  BMC_CMD_L2 = BMC_CMD_L.at(5).split("\n");

  if (SIFTRAINING_JTAG_DEBUG) {
    for (int index = 0; index < BMC_CMD_L2.size(); index++) {
      sprintf(message, "BMC_CMD_L2 [%03d] ", index ); printInfo(message);
      printInfo(BMC_CMD_L2.at(index));
    }
  }

  jtag_read_data = BMC_CMD_L2.at(0).toULongLong(&ok,16);
  sprintf(message, "jtag_read_data:   %016llx ", jtag_read_data ); if (SIFTRAINING_JTAG_DEBUG) printInfo(message);
  // set the JTAG pattern generation mode bit
  if (jtag_mode == LOOPBACK_PATTERN_DUMP) {
    jtag_write_data = jtag_read_data | 0x200000000000ull;
  } else if (jtag_mode == SCC_PATTERN_DUMP) {
    jtag_write_data = jtag_read_data | 0x100000000000ull;
  } else if (jtag_mode == NO_PATTERN_DUMP) {
    jtag_write_data = jtag_read_data & 0x0FFFFFFFFFFFull;
  }
  sprintf(message, "jtag write right sysif %016llx ", jtag_write_data ); if (SIFTRAINING_JTAG_DEBUG) printInfo(message);
  value = QString::number(jtag_write_data,16);
  BMC_CMD = "jtag write right sysif ";
  BMC_CMD.append(value);
  executeBMCCommand(BMC_CMD);

}

// ==============================================
// Extension hooks for Soft-RAM and IO support...
// ==============================================

void sccApi::slotSoftramRequest(uInt32* msg) {
  // This is a Softram Access to the host. This is only a hook for implementation in extended APIs...
  printWarning("Received unexpected Soft-RAM request:");
  printPacket(msg, "Unexpected Soft-RAM packet", false, false);
  msg[10]=0; // Signal that we're done...
}

void sccApi::slotIoRequest(uInt32* msg) {
  // This is an IO Access to the host. Currently this is not supported, so print a warning...
  // In future this section may contain IO signals for IORD and IOWR that can be connected by user scenarios...
  printWarning("Received unexpected IO-Packet:");
  printPacket(msg, "Unexpected IO packet", false, false);
  msg[10]=0; // Signal that we're done...
}

void sccApi::softramSlotConnected(bool enable) {
  isActiveSoftram = enable;
}

bool sccApi::isSoftramSlotConnected() {
  return isActiveSoftram;
}

// =============================================================================================
// Helper methods
// =============================================================================================

int sccApi::getSizeOfMC() {
  return memorySize;
}

void sccApi::printPacket(const uInt32* message, const char* prefix, bool toPacketLog, bool trailingSpaces) {
  int tmp;
  int sif_port_recipient;
  int sif_port_sender;
  int transid;
  int byteenable;
  int destid;
  int routeid;
  int cmd;
  uInt64 address;
  uInt64 data[4];
  tmp=message[8];
  sif_port_sender = (tmp >> 16) & 0x0ff;
  transid = (tmp >> 8) & 0x0ff;
  byteenable = tmp & 0x0ff;
  address = message[9];
  tmp=message[10];
  address += ((uInt64)(tmp & 0x03))<<32;
  destid = tmp>>22 & 0x07;
  routeid = tmp>>14 & 0x0ff;
  cmd = tmp>>2 & 0x01ff;
  sif_port_recipient = tmp>>11 & 0x03;
  data[3] = ((uInt64)message[7]<<32)+(uInt64)message[6];
  data[2] = ((uInt64)message[5]<<32)+(uInt64)message[4];
  data[1] = ((uInt64)message[3]<<32)+(uInt64)message[2];
  data[0] = ((uInt64)message[1]<<32)+(uInt64)message[0];
  if (toPacketLog) {
    PRINT_PACKET_LOG("%s %03d from %s to %s -> transferPacket(0x%02x, %s, 0x%01x_%08x, %s, 0x%02x, 0x%016llx_%016llx_%016llx_%016llx);\n", prefix, transid, decodePort(sif_port_sender, trailingSpaces).toStdString().c_str(), decodePort(sif_port_recipient, trailingSpaces).toStdString().c_str(), routeid, decodeDestID(destid, trailingSpaces).toStdString().c_str(), (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), decodeCommand(cmd, trailingSpaces).toStdString().c_str(), byteenable, data[3], data[2], data[1], data[0]);
  } else {
    char logMsg[255];
    snprintf(logMsg, 255, "%s %03d from %s to %s -> transferPacket(0x%02x, %s, 0x%01x_%08x, %s, 0x%02x, 0x%016llx_%016llx_%016llx_%016llx);", prefix, transid, decodePort(sif_port_sender, trailingSpaces).toStdString().c_str(), decodePort(sif_port_recipient, trailingSpaces).toStdString().c_str(), routeid, decodeDestID(destid, trailingSpaces).toStdString().c_str(), (unsigned int)(address>>32), (unsigned int)(address & 0x0ffffffff), decodeCommand(cmd, trailingSpaces).toStdString().c_str(), byteenable, data[3], data[2], data[1], data[0]);
    printInfo(logMsg);
  }
}

QString sccApi::decodeCommand(int cmd, bool trailingSpaces) {
  // Find out command
  QString command;
  int value = cmd & 0x01ff; // Only evaluate lower 9 bits (of 12 bit command)...
  if      (value==DATACMP) command =    "DATACMP"+(QString)(trailingSpaces?"   ":"");
  else if (value==NCDATACMP) command =  "NCDATACMP"+(QString)(trailingSpaces?" ":"");
  else if (value==MEMCMP) command =     "MEMCMP"+(QString)(trailingSpaces?"    ":"");
  else if (value==ABORT_MESH) command = "ABORT_MESH";
  else if (value==RDO) command =        "RDO"+(QString)(trailingSpaces?"       ":"");
  else if (value==WCWR) command =       "WCWR"+(QString)(trailingSpaces?"      ":"");
  else if (value==WBI) command =        "WBI"+(QString)(trailingSpaces?"       ":"");
  else if (value==NCRD) command =       "NCRD"+(QString)(trailingSpaces?"      ":"");
  else if (value==NCWR) command =       "NCWR"+(QString)(trailingSpaces?"      ":"");
  else if (value==NCIORD) command =     "NCIORD"+(QString)(trailingSpaces?"    ":"");
  else if (value==NCIOWR) command =     "NCIOWR"+(QString)(trailingSpaces?"    ":"");
  else command="Unknown command (0x"+QString::number(value,16)+")";
  return command;
}

QString sccApi::decodeCommandPrefix(int cmd, bool trailingSpaces) {
  // Find out command offset
  QString result;
  int value = cmd & 0x0e00; // Only evaluate upper 3 bits (of 12 bit comm...
  if      (value==BLOCK)   result =    "BLOCK"+(QString)(trailingSpaces?"  ":"");
  else if (value==BCMC)    result =    "BLOCK"+(QString)(trailingSpaces?"  ":"");
  else if (value==BCTILE)  result =    "BCTILE"+(QString)(trailingSpaces?" ":"");
  else if (value==BCBOARD) result =    "BCBOARD"+(QString)(trailingSpaces?"":"");
  else if (value==NOGEN)   result =    "NOGEN"+(QString)(trailingSpaces?"  ":"");
  else result="Unknown command prefix (0x"+QString::number(value,16)+")";
  return result;
}

QString sccApi::decodePort(int port, bool trailingSpaces) {
  QString dest;
  int value = port & 0x03; // Only evaluate lower 2 bits...
  if      (value == SIF_HOST_PORT) dest = "HOST";
  else if (value == SIF_RC_PORT) dest =   "RC"+(QString)(trailingSpaces?"  ":"");
  else if (value == SIF_GRB_PORT) dest =  "GRB"+(QString)(trailingSpaces?" ":"");
  else if (value == SIF_MC_PORT) dest =   "MC"+(QString)(trailingSpaces?"  ":"");
  else if (value == SIF_SATA_PORT) dest = "SATA";
  else dest = "Unknown destination (0x"+QString::number(value,16)+")";
  return dest;
}

QString sccApi::decodeDestID(int destID, bool trailingSpaces) {
  QString IDString;
  int value = destID & 0x07; // Only evaluate lower 3 bits...
  if      (value == CORE0) IDString = "CORE0";
  else if (value == CORE1) IDString = "CORE1";
  else if (value == CRB) IDString =   "CRB"+(QString)(trailingSpaces?"  ":"");
  else if (value == MPB) IDString =   "MPB"+(QString)(trailingSpaces?"  ":"");
  else if (value == PERIE) IDString = "PERIE";
  else if (value == PERIS) IDString = "PERIS";
  else if (value == PERIW) IDString = "PERIW";
  else if (value == PERIN) IDString = "PERIN";
  else IDString = "Unknown sub destination ID (0x"+QString::number(value,16)+")";
  return IDString;
}

void sccApi::initPacketLogging() {
#if DEBUG_GRB_ACCESS == 0
  debugPacket = userSettings->value("PacketDebug/TraceMode", NO_DEBUG_PACKETS).toInt();
#else
  debugPacket = userSettings->value("PacketDebug/TraceMode", RAW_DEBUG_PACKETS).toInt();
#endif
  printInfo("Packet tracing is ", (debugPacket==NO_DEBUG_PACKETS)?"disabled":(debugPacket==RAW_DEBUG_PACKETS)?"enabled in RAW mode":"enabled in text mode", "...");
  if (debugPacket!=NO_DEBUG_PACKETS) {
    printWarning("Packet tracing is enabled and will degrade the performance! Please ignore this warning if tracing is intended!");
  }
  // Default packetLogName is getTempPath()"+"/trace.log"...
  packetLogName = getTempPath()+"trace.log";
  packetLogName = userSettings->value("PacketDebug/traceLog", packetLogName).toString();
  packetLog.setFileName(packetLogName);
  if (packetLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (debugPacket!=NO_DEBUG_PACKETS) printInfo("Opened \"",packetLogName,"\" for writing...");
  } else {
    log->logMessage((debugPacket!=NO_DEBUG_PACKETS)?log_error:log_warning, "Failed to open logfile in overwrite mode... Make sure that \"",packetLogName,"\"exists and\nis writable or change your userSettings. Then re-invoke sccApi reset to get logfile capabilities...");
  }
}

void sccApi::setPacketLog(QString file) {
  // Re-open logfile...
  if (packetLog.isOpen()) {
    packetLog.close();
  }
  packetLog.setFileName(file);
  if (packetLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
    packetLogName = file;
    printInfo("Initialized logfile \"",packetLogName,"\"...");
    userSettings->setValue("PacketDebug/traceLog", packetLogName);
  } else {
    // Try to (re-)open old logfile instead...
    packetLog.setFileName(packetLogName);
    log->logMessage((debugPacket!=NO_DEBUG_PACKETS)?log_error:log_warning, "Failed to open logfile in write-mode... Make \"",file,"\" writable or change trace logfile name... Ingnoring selection!");
    if(packetLog.open(QIODevice::Append | QIODevice::Text)) printInfo("(Re-)opened logfile \"",packetLogName,"\" in append mode...");
  }
}

QString sccApi::getPacketLog() {
  return packetLog.fileName();
}

void sccApi::setDebugPacket(int mode) {
  debugPacket = mode;
  userSettings->setValue("PacketDebug/TraceMode", debugPacket);
  printInfo("Packet tracing is ", (debugPacket==NO_DEBUG_PACKETS)?"disabled":(debugPacket==RAW_DEBUG_PACKETS)?"enabled in RAW mode":"enabled in text mode", " from now on...");
  if (!packetLog.isOpen()) {
    printError("Trace logfile \"",packetLogName,"\" cannot be opened for writing... Your selection won't have effect!");
  } else {
    if (debugPacket!=NO_DEBUG_PACKETS) printInfo("Dumping to tracefile \"",packetLogName,"\"...");
  }
}

int sccApi::getDebugPacket() {
  return debugPacket;
}

txThread::txThread(sccApi* sccAccess, messageHandler* log, int fd) : QThread() {
  serverOn = false;
  serverRunning = false;
  this->sccAccess = sccAccess;
  this->log = log;
  CONNECT_MESSAGE_HANDLER;
  this->fd = fd;
  txQueue = (uInt32*)malloc(1024*12*sizeof(uInt32));
  txQueueWrite = 0;
  txQueueRead = 0;
}

txThread::~txThread() {
  stop();
  free(txQueue);
}

void txThread::run() {
  if (serverRunning) return;
  serverRunning = serverOn = true;
  uInt32 writeSnapshot;
#ifndef BMC_IS_HOST
  bool sendAbort = false;
  bool transferComplete = false;
  uInt32 transferred;
  int result;
#endif
  do {
#ifdef BMC_IS_HOST
    // This physical interface is used on the BMC (16 bit IF to FPGA)
    writeSnapshot=txQueueWrite;
    while (txQueueRead!=writeSnapshot) {
      if (!(CPLD_GPIOPR0 & 0x02)) printWarning("txThread assertion failed: FPGA FIFO is full..."); // Should this assertion ever fire: Implement wait-states...
      for (int loop = 0; loop < 12; loop++) {
        *((uInt32*)fpgaBusBase) = txQueue[12*txQueueRead+loop];
      }
      txQueueRead++;
      txQueueRead %= 1024;
    }
    usleep(1);
#else
    // This physical interface is used on the Host PC (PCIe IF to FPGA)
    if ((writeSnapshot=txQueueWrite) > txQueueRead) {
      // One part write...
      sendTimeout = PCI_EXPRESS_TIMEOUT;
      transferComplete = false;
      transferred = 0;
      while (!sendAbort && !transferComplete) {
        result = write(fd, (const void*)&txQueue[12*txQueueRead+(transferred/sizeof(uInt32))], (writeSnapshot-txQueueRead)*48-transferred);
        if (result == -1) {
          if (errno != EAGAIN) {
            printError("Driver error on PCIe write... Aborting request!");
            sendAbort = true;
          } else {
            // Wait for PCIe driver to get ready
          }
          if (!sendTimeout--) {
            printError("Timeout on PCIe write... Aborting request!");
            sendAbort = true;
          }
        } else {
          transferred += result;
          if ((writeSnapshot-txQueueRead)*48==transferred) transferComplete = true;
        }
      }
      txQueueRead++;
      txQueueRead %= 1024;
    } else if (writeSnapshot < txQueueRead) {
      // Two part write...
      sendTimeout = PCI_EXPRESS_TIMEOUT;
      transferComplete = false;
      transferred = 0;
      while (!sendAbort && !transferComplete) {
        result = write(fd, (const void*)&txQueue[12*txQueueRead+(transferred/sizeof(uInt32))], (1024-txQueueRead)*48-transferred);
        if (result == -1) {
          if (errno != EAGAIN) {
            printError("Driver error on PCIe write... Aborting request!");
            sendAbort = true;
          } else {
            // Wait for PCIe driver to get ready
          }
          if (!sendTimeout--) {
            printError("Timeout on PCIe write... Aborting request!");
            sendAbort = true;
          }
        } else {
          transferred += result;
          if ((1024-txQueueRead)*48==transferred) transferComplete = true;
        }
      }
      txQueueRead+= 1024-txQueueRead;
      txQueueRead %= 1024;
      sendTimeout = PCI_EXPRESS_TIMEOUT;
      transferComplete = false;
      transferred = 0;
      while (!sendAbort && !transferComplete) {
        result = write(fd, (const void*)&txQueue[transferred/sizeof(uInt32)], writeSnapshot*48-transferred);
        if (result == -1) {
          if (errno != EAGAIN) {
            printError("Driver error on PCIe write... Aborting request!");
            sendAbort = true;
          } else {
            // Wait for PCIe driver to get ready
          }
          if (!sendTimeout--) {
            printError("Timeout on PCIe write... Aborting request!");
            sendAbort = true;
          }
        } else {
          transferred += result;
          if (writeSnapshot*48==transferred) transferComplete = true;
        }
      }
      txQueueRead+=writeSnapshot;
      txQueueRead %= 1024;
    } else {
      usleep(1);
    }
#endif
} while(serverOn);
  serverRunning = false;
}

void txThread::stop() {
  struct timeval timeout, current;
  serverOn = false;
  // Get time and prepare timeout counter
  gettimeofday(&timeout, NULL);
  timeout.tv_sec += 3;
  while (serverRunning) {
    usleep(100);
    // Check if timout timer expired
    gettimeofday(&current, NULL);
    if (((current.tv_sec == timeout.tv_sec) && (current.tv_usec >= timeout.tv_usec)) || (current.tv_sec > timeout.tv_sec)) {
      printf("txThread::stop(): Unable to stop thread... Forced close!\n"); fflush(0);
      serverRunning = false;
    }
  }
}

void txThread::sendMessage(uInt32* message) {
  memcpy((void*) &txQueue[12*txQueueWrite], (void*) message, 12*sizeof(uInt32));
  txQueueWrite++;
  txQueueWrite %= 1024;
  // Prevent overflow...
  while (!(txQueueRead - txQueueWrite - 1) || (!txQueueRead && (txQueueWrite==1023))) {
    usleep(1);
  }
}

bool txThread::isEmpty() {
  return (txQueueRead == txQueueWrite);
}

rxThread::rxThread(sccApi* sccAccess, messageHandler* log, int fd) : QThread() {
  serverOn = false;
  serverRunning = false;
  this->sccAccess = sccAccess;
  this->log = log;
  CONNECT_MESSAGE_HANDLER;
  this->fd = fd;
  rxQueue = (uInt32*)malloc(1024*12*sizeof(uInt32));
  rxQueueSize = 0;
}

rxThread::~rxThread() {
  stop();
  free(rxQueue);
}

void rxThread::run() {
  if (serverRunning) return;
  serverRunning = serverOn = true;
#ifndef BMC_IS_HOST
  bool immediateRetry = false;
  uInt32 loop;
#endif
  do {
#ifdef BMC_IS_HOST
    if (CPLD_GPIOPR0 & 0x01) {
      // This physical interface is used on the BMC (16 bit IF to FPGA)
      for (int loop = 0; loop < 12; loop++) {
        rxQueue[loop] = *((uInt32*)fpgaBusBase);
      }
      sccAccess->receiveMessage(&rxQueue[0]);
    } else {
      usleep(1);
    }
  #else
    // This physical interface is used on the Host PC (PCIe IF to FPGA)
    if (immediateRetry) {
      immediateRetry = false;
    } else {
      usleep(1);
    }
    // Data available?
    rxQueueSize = read(fd, rxQueue, 1024*12*sizeof(uInt32));
    if (rxQueueSize == -1) {
      if (errno != EAGAIN) {
        printfFatal("Error (%0d) while trying to read from PCIe driver device... Closing device!", errno);
        close(fd);
        exit(-1);
      }
    } else {
      for (loop = 0; loop < (rxQueueSize/sizeof(uInt32)); loop+=12) {
        sccAccess->receiveMessage(&rxQueue[loop]);
      }
      // If there was data, immediatly try again... Make a short break otherwise to avoid idle-loop.
      immediateRetry = true;
    }
#endif
  } while(serverOn);
  serverRunning = false;
}

void rxThread::stop() {
  struct timeval timeout, current;
  serverOn = false;
  // Get time and prepare timeout counter
  gettimeofday(&timeout, NULL);
  timeout.tv_sec += 3;
  while (serverRunning) {
    usleep(100);
    // In case of IO-Event: Complete the handshake by overwriting cmd field.
    // Usually the ioSlot needs to overwrite cmd with zero in order to continue...
    for (uInt32 loop = 0; loop < (1024*12); loop+=12) {
      (&rxQueue[loop])[10] = 0;
    }
    // Check if timout timer expired
    gettimeofday(&current, NULL);
    if (((current.tv_sec == timeout.tv_sec) && (current.tv_usec >= timeout.tv_usec)) || (current.tv_sec > timeout.tv_sec)) {
      serverRunning = false;
      printf("rxThread::stop(): Unable to stop thread... Forced close!\n"); fflush(0);
    }
  }
}
