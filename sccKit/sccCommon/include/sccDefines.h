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

#ifndef SCCDEFINES_H_
#define SCCDEFINES_H_

// SCC specific
#define NUM_ROWS 4
#define NUM_COLS 6
#define NUM_CORES 2
#define TOTAL_CORES (NUM_ROWS*NUM_COLS*NUM_CORES)

#define TID(x,y) (((y)<<4)+(x))
#define X_TID(tid) ((tid)&0x0f)
#define Y_TID(tid) ((tid)>>4)

#define PID(x,y,core) ((NUM_CORES*NUM_COLS*(y))+(NUM_CORES*(x))+(core))
#define TID_PID(pid) (TID(X_PID(pid), Y_PID(pid)))
#define X_PID(pid) (((pid)/NUM_CORES)-(NUM_COLS*Y_PID(pid)))
#define Y_PID(pid) (((pid)/NUM_CORES)/NUM_COLS)
#define Z_PID(pid) ((pid)%NUM_CORES)

#define MCHTID_XY(x,y) ((((x)<(NUM_COLS/2))&&((y)<(NUM_ROWS/2)))?0x00:(((x)>=(NUM_COLS/2))&&((y)<(NUM_ROWS/2)))?0x05:(((x)<(NUM_COLS/2))&&((y)>=(NUM_ROWS/2)))?0x20:0x25)
#define MCHTID_PID(pid) (MCHTID_XY((pid)&0x0f,(pid)>>4))
#define MCHPORT_XY(x,y) ((((x)<(NUM_COLS/2))&&((y)<(NUM_ROWS/2)))?PERIW:(((x)>=(NUM_COLS/2))&&((y)<(NUM_ROWS/2)))?PERIE:(((x)<(NUM_COLS/2))&&((y)>=(NUM_ROWS/2)))?PERIW:PERIE)
#define MCHPORT_PID(pid) (MCHPORT_XY((pid)&0x0f,(pid)>>4))

// SIF
#define SIFROUTE 0x03
#define SIFDESTID PERIS

// Symbols for CRB sub-addresses
#define GLCFG0   0x010
#define GLCFG1   0x018
#define L2CFG0   0x020
#define L2CFG1   0x028
#define SENSOR   0x040
#define GCBCFG   0x080
#define MYTILEID 0x100
#define LOCK0    0x200
#define LOCK1    0x400

// Symbols for Config values
#define ON 1
#define OFF 0
#define ENABLED 1
#define DISABLED 0
#define PIO_SCEMI 0
#define DMA_SCEMI 1
#define COPPERRIDGE 0
#define MCEMU_SIF 1
#define MCEMU_CD 2
#define UART 0
#define MMIO 1

#define DEFAULT_MC_SIZE 4
#define DEFAULT_MPB_INIT 0
#define DEFAULT_L2EN_INIT 1
#define DEFAULT_CRBSERVER "none"

// Symbols for System Interface FPGA related transactions
#define SIF_HOST_PORT 0x00 // port 0 : PCIE SCEMI
#define SIF_RC_PORT   0x01 // port 1 : SCC Mesh IF unit and PHY
#define SIF_GRB_PORT  0x02 // port 2 : GRB internal config register IF
#define SIF_MC_PORT   0x03 // port 3 : Memory Controller IF
#define SIF_SATA_PORT 0x07 // port 7 : SATA port...

// Commands
#define DATACMP 0x04f     // Response with data
#define NCDATACMP 0x04e   // SysIF specific: NCRD Response with data
#define MEMCMP 0x049      // Only Ack no data
#define ABORT_MESH 0x044  // Abort
#define RDO 0x008         // Burst line fill (Read for ownership)
#define WCWR 0x122        // Memory write (1,2,4 8 bytes)
#define WBI 0x02C         // Cacheline write / Write back invalidate
#define NCRD 0x007        // Non-cacheable read (1,2,4,8 bytes)
#define NCWR 0x022        // Non-cacheable write (1,2,4,8 bytes)
#define NCIORD 0x006      // I/O Read
#define NCIOWR 0x023      // I/O Write

// Command prefixes (for block transfers)
#define BLOCK 0x800       // Block transaction
#define BCMC 0x400        // Broadcast packets to all iMCs
#define BCTILE 0xc00      // Broadcast packets to all tiles (MPBs)
#define BCBOARD 0x200     // Broadcast packets to all boards
#define NOGEN 0x000       // No packet generation required

// Valid sub destination IDs
#define CORE0 0
#define RTG   0
#define CORE1 1
#define CRB   2
#define SUB_DESTID_CRB CRB
#define MPB   3
#define SUB_DESTID_MPB MPB
#define PERIE 4
#define PERIS 5
#define PERIW 6
#define PERIN 7

// Valid Nodecard Ports
#define NOPORT 0
#define SOUTH  1
#define EAST   2
#define NORTH  3
#define WEST   4
#define MIDDLE 5

// MMIO related
#define LUT0 0x00800
#define LUT1 0x01000
#define MC_SHMBASE_MC0 0x400
#define MC_SHMBASE_MC1 0x408
#define MC_SHMBASE_MC2 0x410
#define MC_SHMBASE_MC3 0x418
#define USE_SHM_THREADS 0

// Several debug packet modes and a print routine...
#define NO_DEBUG_PACKETS 0
#define RAW_DEBUG_PACKETS 1
#define TEXT_DEBUG_PACKETS 2
#define PRINT_PACKET_LOG(args...)  if (packetLog.isOpen()) { char msg[255]; sprintf(msg, ##args ); packetLog.write(msg); packetLog.flush(); }

#define RCK_PATTERN_DUMP 0
#define LOOPBACK_PATTERN_DUMP 1
#define NO_PATTERN_DUMP 2

#define RX_Data 0
#define RX_Clock 1
#define TX_Clock 2

#define INCR 0
#define DECR 1

#define SIFPHY_PHYiodPassPhrase_Reset   0xABBA0000
#define SIFPHY_PHYiodPassPhrase_Trigger 0xC48E0001

#define FAST_MODE       0
#define EXTENDED_MODE   1
#define FULL_MODE       2
#define PRESET_MODE     3
#define AUTO_MODE       4
#define PRESET_NO_CHECK 5

#define SUCCESS 0
#define ERROR 1
#define ABORT 2

typedef unsigned int uInt32;
typedef signed int int32;
typedef unsigned long long uInt64;
typedef signed long long int64;

#define SIZE_1MB 0x00100000ull
#define SIZE_1GB 0x40000000ull

#endif /* SCCDEFINES_H_ */
