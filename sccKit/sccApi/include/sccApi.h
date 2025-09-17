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

#ifndef SCCAPI_H_
#define SCCAPI_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <QtCore>
#include <QObject>
#include <QTcpSocket>
#include "messageHandler.h"
#include "sccProgressBar.h"
#include "sccDefines.h"

// Cacheline type...
typedef struct {
  uInt64 data[4];
  uInt64 addr;
} mem32Byte;

// Echo mode for RCC files
#define ECHO_ON 0
#define ECHO_OFF_RW 1
#define ECHO_OFF_W 2

// Define max. Transaction ID:
// System w/ EMAC uses "0x40"... Legacy systems use "0x100"!
#define DEFAULT_MAX_TRANS_ID 0x40

// PCIe driver related defines...
#define PCI_EXPRESS_TIMEOUT 10000
#define MCEDEV_MAJOR              1024
#define CRBIF_SET_SUBNET_SIZE     _IOWR(MCEDEV_MAJOR, 1, uInt64 )
#define CRBIF_SET_TX_BASE_ADDRESS _IOWR(MCEDEV_MAJOR, 2, uInt64 )
#define CRBIF_RESET               _IOWR(MCEDEV_MAJOR, 3, uInt64 )
#define CRBIF_GET_FIFO_DEPTH      _IOR (MCEDEV_MAJOR, 4, uInt64 )
#define CRBIF_SET_MAX_TID         _IOWR(MCEDEV_MAJOR, 5, uInt64 )

// Forward declarations
class txThread;
class rxThread;

// =============================================================================================
// sccApi
// =============================================================================================
class sccApi : public QObject {
  Q_OBJECT

public:
  sccApi(messageHandler* log);
  ~sccApi();

protected:
  messageHandler* log;

protected:
 char formatMsg[8192];
signals:
 void logMessageSignal(LOGLEVEL level, const char *logmsg);
 void logMessageSignal(LOGLEVEL level, const QString a, const QString b = "", const QString c = "", const QString d = "", const QString e = "", const QString f = "", const QString g = "", const QString h = "", const QString i = "", const QString j = "", const QString k = "", const QString l = "", const QString m = "", const QString n = "", const QString o = "", const QString p = "", const QString q = "", const QString r = "", const QString s = "", const QString t = "", const QString u = "" );

// =============================================================================================
// Protection members & methods...
// =============================================================================================
protected:
  QSemaphore protectSysIF;
  bool       releaseSemaphores;

public:
  bool acquireSysIF();
  bool tryAcquireSysIF();
  bool availableSysIF();
  void releaseSysIF();

// =============================================================================================
//  Basic members and their initialization methods...
// =============================================================================================

protected:
  QSettings *userSettings;
  QSettings *sysSettings;
  QSettings *sifSettings;
  QDir      tmpPath;

public:
  // Initializes tmpPath with system default or user setting...
  void initTempPath();

  // Sets the temporary path and updates user setting...
  void setTempPath(QString path);

  // returns the temporary path (that's been used for packet tracing) to the user...
  QString getTempPath();

  // setSubnetSize(): Sets the subnet size parameter of the CRBIF driver. This is only possible if the driver has been
  // opened with connectSysIF() before...
  void setSubnetSize(uInt32 size);

  // setTxBaseAddr(): Sets the tx base address parameter of the CRBIF driver. This is only possible if the driver has been
  // opened with connectSysIF() before...
  void setTxBaseAddr(uInt64 addr);

  // reInitDriver(): Re-Initializes the CRBIF driver. This is only possible if the driver has been
  // opened with connectSysIF() before...
  void reInitDriver();

  // getCrbFifoDepth(): Finds out how deep the SCEMI FIFO is (in bytes)...
  uInt32 getCrbFifoDepth();
  uInt32 crbFifoDepth;

  // setMaxTid(): Defines maximum transaction ID (0x100 for legacy bitstreams and 0x40 for newer bitstreams)
  void setMaxTid(uInt32 maxTid);

// =============================================================================================
// PCIe access members & methods...
// =============================================================================================
protected:
  int               fd; // file descriptor for other devices (PCIe, DMA etc)

public:
  // RX
  uInt32*           rxMsg;
  bool              rxMsgAvail;
  rxThread*         rxServer;
  int               rxTransid;
  int               rxCmd;

  // TX
  uInt32*           txMsg;
  txThread*         txServer;
  bool              txRequest;
  uInt32            txIntTransid;
  unsigned char     txTransid;
  int               sendTimeout;
  bool              sendAbort;
  uInt32            maxTransId;

public:
  // connectSysIF(): NEW! This routine opens the PCIe driver but doesn't initialize FPGA or driver. Allows
  // "re-connection". This is important to enable standalone applications like e.g. reset or bootLinux...
  // It returns "true" when the device can be opened, "false" otherwise...
  bool connectSysIF();

  // sendMessage(): Writes packet to the SIF.
  void sendMessage(uInt32* message);

public slots:
  // receiveMessage(): Handles new packets from the System interface...
  //                   This method will be invoked by the rxServer (Thread)...
  void receiveMessage(uInt32* messagePtr);

// =============================================================================================
// SCC PIO access methods...
// =============================================================================================
public:
  // writeFlit: This method sends a non-cachable Flit (64 bit data) to the specified SCC System address
  bool writeFlit(int Route, int SubdestID, uInt64 Address, uInt64 Data, int sif_port=SIF_RC_PORT);

  // readFlit: This method reads a non-cachable Flit (64 bit data) from the specified SCC System address
  uInt64 readFlit(int Route, int SubdestID, uInt64 Address, int sif_port=SIF_RC_PORT);

  // readFlit: This method does the same but allows to evaluate the status of transferPacket()
  bool readFlit(int Route, int SubdestID, uInt64 Address, uInt64 *Data, int sif_port=SIF_RC_PORT);

  // transferPacket: This is a PIO transfer routine (also used by WriteFlit and ReadFlit) that allows to read or write from/to a specified SCC
  // system address. The transfer may be non-cachable (64bit) or Cacheline based (256 bit).
  bool transferPacket(int routeid, int destid, uInt64 address, int command, unsigned char byteenable, uInt64* data, int sif_port, int commandPrefix = NOGEN);

// =============================================================================================
// SCC FPGA register access...
// =============================================================================================

// Some GRB defines (for SIF training)...
#define SIFGRB_RESPONSE_ENABLE (4*0x2007)
#define SIFPHY_PHYiodEnDataIn (4*4096)
#define SIFPHY_PHYiodEnRXIn (4*4098)
#define SIFPHY_PHYiodDirDataIn (4*4099)
#define SIFPHY_PHYiodDirRXIn (4*4101)
#define SIFPHY_PHYiodClockPulse (4*4102)
#define SIFPHY_PHYPatternGenOn (4*4108)
#define SIFPHY_PHYFlitData (4*4103)
#define SIFCFG_DDR2IDelayCtrlRdy (4*8195)
#define SIFCFG_BitSID (4*0x2004)
#define SIFCFG_CONFIG (4*0x2007)

// Some more GRB defines (used for configuring default Linux image...)
#define SIFGRB_EMACBASEADDRH 0x7e00 /* 16 bit */
#define SIFGRB_EMACBASEADDRL 0x7e04 /* 32 bit */
#define SIFGRB_EMACCOIPADDR 0x7e08 /* 32 bit */
#define SIFGRB_EMACHTIPADDR 0x7e0c /* 32 bit */
#define SIFGRB_EMACGATEADDR 0x7e10 /* 32 bit */
#define SIFGRB_FASTCLOCK 0x8230 /* 16 bit */
#define SIFGRB_RESX 0x8238 /* 16 bit */
#define SIFGRB_RESY 0x823c /* 16 bit */
#define SIFGRB_RESDEPTH 0x8240 /* 8 bit */
#define SIFGRB_PMEMSLOT 0x8244 /* 8 bit */
#define SIFGRB_FPGAIPSTAT  0x822c /* 2bit SATA, 4 bit eMAC, 9 bit SCEMI Size [14:0] */
#define SIFGRB_FPGAIPCONFIG 0x822c /* 2bit SATA, 4 bit eMAC, 9 bit SCEMI Size [30:16] */
#define SIFGRB_FIRST_IRQ_MASK 0x0d200 /* 48*64 bit: High-active mask bit to disable interrupt delivery individually
                                         - 0x00+8*PID -> 31:06: IPI25...IPI0
                                         - 0x00+8*PID -> 05:00: SATA1...SATA0, EMAC3...EMAC0
                                         - 0x04+8*PID -> [22:0] = MCPC, IPI47...IPI26 */
#define SIFGRB_RX_NETWORK_PORT_ENABLE_A 0x9800
#define SIFGRB_RX_NETWORK_PORT_ENABLE_B 0xa800
#define SIFGRB_RX_NETWORK_PORT_ENABLE_C 0xb800
#define SIFGRB_RX_NETWORK_PORT_ENABLE_D 0xc800

protected:
  uInt64 readFpgaGrbData[4];
  uInt64 readFpgaGrbAddr;
  uInt32 readFpgaGrbByteEn;
  uInt64 writeFpgaGrbData[4];
  uInt64 writeFpgaGrbAddr;
  uInt32 writeFpgaGrbByteEn;

public:
  // readFpgaGrb: Read 32 bit register from GRB configuration space of FPGA
  bool readFpgaGrb(uInt32 address, uInt32 *value);

  // writeFpgaGrb: Write 32 bit register to GRB configuration space of FPGA
  bool writeFpgaGrb(uInt32 address, uInt32 value);

// =============================================================================================
// SCC DMA access methods and members...
// =============================================================================================
public:
  // read32: This method allows block reads (with DMA, if enabled) from a memory that is specified with a SCC System address
  uInt32 read32(int routeid, int destid, uInt64 startAddress, char memBlock[], uInt32 numBytes, int sif_port=SIF_RC_PORT, sccProgressBar *pd=NULL);

  // write32: This method allows block write access (with DMA, if enabled) to a memory that is specified with a SCC System address
  uInt32 write32(int routeid, int destid, uInt64 startAddress, char memBlock[], uInt32 numBytes, int sif_port=SIF_RC_PORT, sccProgressBar *pd=NULL);

protected:
  void dmaPrepare(int routeid, int destid, uInt64 address, int command, unsigned char byteenable, int sif_port, int commandPrefix = NOGEN, int count = 0);
  void dmaTransfer(uInt64* data);
  void dmaReceive(const uInt32* message);
  void dmaAbort();
  bool dmaComplete();

protected:
  int DMACommand;
  int DMASifPort;
  int DMATransferID;
  uInt64 DMAAddress;
  uInt64 *DMAData;
  int DMACurrentTID;
  bool DMATransferInProgress;

// =============================================================================================
// SCC methods
// =============================================================================================
public:
  // Get L2 mode of selected core
  bool getL2(int route, int core, bool *l2Enabled);

  // Set L2 mode of selected core
  bool setL2(int route, int core, bool l2Enabled);

  // Get current reset value of selected core
  bool getReset(int route, int core, bool *resetEnabled);

  // Set reset bits of selected core
  bool setReset(int route, int core, bool resetEnabled);

  // Toggle (pull and release) reset...
  bool toggleReset(int route, int core, bool performBist=false);

// =============================================================================================
// BMC methods
// =============================================================================================
protected:
  QString CRBServer;
  int memorySize;
  QString platform;
  QTcpSocket *tcpSocket;
  bool bmcResetEnabled;

public:
  bool openBMCConnection();
  void closeBMCConnection();
  void setBMCServer(QString server);
  QString getBMCServer();
  QString getPlatform();
  QString platformSetupSuffix();
  bool connectedBMCServer();
  QString executeBMCCommand(QString command);
  QString processRCCFile(QString fileName, bool instantPrinting, int echo = ECHO_ON, sccProgressBar *pd = NULL);

  // ===========================================================================================
  //  ____ ___ _____   _____          _       _
  // / ___|_ _|  ___| |_   _| __ __ _(_)_ __ (_)_ __   __ _
  // \___ \| || |_      | || '__/ _` | | '_ \| | '_ \ / _` |
  //  ___) | ||  _|     | || | | (_| | | | | | | | | | (_| |
  // |____/___|_|       |_||_|  \__,_|_|_| |_|_|_| |_|\__, |
  //                                                  |___/
  // ===========================================================================================

  // defines for pattern matching
  #define PATTERN_55 0
  #define PATTERN_AA 1
  // max number of steps for RXDATA delay
  #define RXDATA_MAXSHIFT 1
  // defines for FPGA PHYpgControl register(5)
  #define SCC_PATTERN_DUMP 0
  #define LOOPBACK_PATTERN_DUMP 1
  #define NO_PATTERN_DUMP 2

  public:
    int trainSIF(int mode, int freq);
    void abortTraining();

  private:
    int trainSIF_rx(int mode, int freq);
    int trainSIF_rxtx();
    void trainSIF_rlb_rxclk ();
    void trainSIF_rlb_txclk ();
    void trainSIF_rlb_rxdata();
    void set_sysif_jtag_mode(int jtag_mode);
    void change_delay(int target, int direction, int count);
    int unexpected_flits_found_at_delay(int curr_delay,int pattern_gen);
    bool load_SIF_calibration_settings();
    void restore_SIF_calibration_settings();
    void change_rxdata_delay(int direction, int steps, int iBit);
    bool mism_bit(int delay, int iBit);
    int  locate_phit_errors(int mode, uInt32 *data_31_00, uInt32 *data_63_32, uInt32 *data_71_64);
    int  count_1s(uInt32 data);

  private:
    char message[1024];
    QString BitSID;
    QString logPrefix;
    bool degradeWarnings;
    bool abortAutoTraining;
    bool userAbort;
    uInt32 amism_31_00[64];
    uInt32 amism_63_32[64];
    uInt32 amism_71_64[64];
    // optimal RLB SIF settings: rx clock delay, tx clock delay, rx data delay (72 bits)
    int rx_good_range;
    int rx_opt_dly_setting;
    int tx_good_range;
    int tx_opt_dly_setting;
    int crd_direction[72];
    int crd_steps[72];

// ==============================================
// Extension hooks for Soft-RAM and IO support...
// ==============================================
protected:
  bool isActiveSoftram;

signals:
  void softramRequest(uInt32* msg);
  void ioRequest(uInt32* msg);

protected slots:
  void slotSoftramRequest(uInt32* msg);
  void slotIoRequest(uInt32* msg);

public:
  void softramSlotConnected(bool enable);
  bool isSoftramSlotConnected();

// =============================================================================================
// Helper methods
// =============================================================================================
public:
  int getSizeOfMC();
  void printPacket(const uInt32* message, const char* prefix, bool toPacketLog = false, bool trailingSpaces=true);
  QString decodeCommand(int cmd, bool trailingSpaces=true);
  QString decodeCommandPrefix(int cmd, bool trailingSpaces=true);
  QString decodePort(int port, bool trailingSpaces=true);
  QString decodeDestID(int destID, bool trailingSpaces=true);

// Packet debugging member variables
public:
  QFile packetLog;
  QString packetLogName;
  int debugPacket;

protected:
  void initPacketLogging();

public:
  void setPacketLog(QString file);
  QString getPacketLog();
  void setDebugPacket(int mode);
  int getDebugPacket();

};

class txThread : public QThread {
  Q_OBJECT

private:
  bool serverOn;
  bool serverRunning;
  sccApi* sccAccess;
  messageHandler* log;
  int fd;
  int loop;
  int sendTimeout;
  uInt32* txQueue;
  uInt32 txQueueRead;
  uInt32 txQueueWrite;

public:
  txThread(sccApi* sccAccess, messageHandler* log, int fd);
  ~txThread();
  void run();
  void stop();

public slots:
  void sendMessage(uInt32* message);
  bool isEmpty();

protected:
 char formatMsg[8192];
signals:
 void logMessageSignal(LOGLEVEL level, const char *logmsg);
 void logMessageSignal(LOGLEVEL level, const QString a, const QString b = "", const QString c = "", const QString d = "", const QString e = "", const QString f = "", const QString g = "", const QString h = "", const QString i = "", const QString j = "", const QString k = "", const QString l = "", const QString m = "", const QString n = "", const QString o = "", const QString p = "", const QString q = "", const QString r = "", const QString s = "", const QString t = "", const QString u = "" );
};

class rxThread : public QThread {
  Q_OBJECT

private:
  bool serverOn;
  bool serverRunning;
  sccApi* sccAccess;
  messageHandler* log;
  int fd;
  uInt32* rxQueue;
  int rxQueueSize;

public:
  rxThread(sccApi* sccAccess, messageHandler* log, int fd);
  ~rxThread();
  void run();
  void stop();

protected:
 char formatMsg[8192];
signals:
 void logMessageSignal(LOGLEVEL level, const char *logmsg);
 void logMessageSignal(LOGLEVEL level, const QString a, const QString b = "", const QString c = "", const QString d = "", const QString e = "", const QString f = "", const QString g = "", const QString h = "", const QString i = "", const QString j = "", const QString k = "", const QString l = "", const QString m = "", const QString n = "", const QString o = "", const QString p = "", const QString q = "", const QString r = "", const QString s = "", const QString t = "", const QString u = "" );
};

#endif /* SCCAPI_H_ */

