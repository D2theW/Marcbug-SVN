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

#ifndef RCKEXTAPI_H_
#define RCKEXTAPI_H_

#include "sccApi.h"
#include "asciiProgress.h"

class sccExtApi : public sccApi {

public:
  sccExtApi(messageHandler* log);
  ~sccExtApi();

private:
  bool showAsciiProgress;
  asciiProgress progress;

// =============================================================================================
// SCC DMA access methods and members...
// =============================================================================================
public:
  int loadOBJ(QString objFile, sccProgressBar *pd=NULL, int sif_port=SIF_RC_PORT);
  int writeMemFromOBJ(int routeid, int destid, QString objFile, sccProgressBar *pd=NULL, int sif_port=SIF_RC_PORT, int commandPrefix = NOGEN);

  // WriteFlitsFromFile: This method provides the possibility to write multiple flits that are specified in a File
  // The file format is as follows (one flit per line): 8-bit-route-in-HEX-format 3-bit-dest-id-in-DEC-format 34-bit-address-in-HEX-format 64-bit-data-in-HEX-format
  // In case that the grouping is enabled, transfers to subsequent addresses get grouped to cacheline writes. This should not be used in case of
  // CRB access!
  bool WriteFlitsFromFile(QString FileName, QString ContentType, sccProgressBar *pd=NULL, int sif_port=SIF_RC_PORT, bool groupingEnabled=false);

// =============================================================================================
// BMC methods
// =============================================================================================
public:
  double getCurrentPowerconsumption();
  uInt32 getFastClock();

// =============================================================================================
// SCC IP address-handling...
// =============================================================================================
private:
  QString sccFirstIp;
  QString sccHostIp;
  QString sccGateway;
  QString sccFirstMac;

public:
  QString getSccIp(int pid);
  uInt32  getFirstSccIp();
  uInt32  getHostIp();
  uInt32  getGateway();
  uInt64  getFirstSccMac();
  void getMacEnableConfig(bool *a, bool *b, bool *c, bool *d);
  void setMacEnable(bool a, bool b, bool c, bool d);
  void setMacEnable();

// =============================================================================================
// SCC virtual display related...
// =============================================================================================
public:
  void setDisplayGeometry(uInt32 x, uInt32 y, uInt32 depth);
  uInt32 getDisplayX();
  uInt32 getDisplayY();
  uInt32 getDisplayDepth();

// =============================================================================================
// (Re-) configuration of registers...
// =============================================================================================
  void readEmacConfig();
  void programGrbEmacRegisters();
  void programGrbVgaRegisters();
  void programAllGrbRegisters();
  void initCrbGcbcfg();
  void initCrbRegs();
  void enableFastRpc();  // Enable fast RPC (10 MHz instad of 0.65MHz)...

};

#endif /* RCKEXTAPI_H_ */
