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

#ifndef SCCPERF_H
#define SCCPERF_H

#include <QApplication>
#include <QSettings>
#include <QFile>
#include "coreMask.h"
#include "sccApi.h"
#include "sccKitVersion.h"

#define FACT08BIT 1
#define FACT16BIT 2
#define FACT32BIT 4
#define FACT64BIT 8

class sccDump : public QObject {
  Q_OBJECT

public:
  sccDump();
  ~sccDump();

// =============================================================================================
// Protection members & methods...
// =============================================================================================
public:
  uInt32 getDispBytes (uInt32 numBytes);
  uInt32 getNumBytesAlign (uInt32 numBytes,  uInt64 startAddress, uInt64 lastAddress);
  uInt64 getStartAddrAlign (uInt64 startAddress);
  uInt32 readMem(uInt64 startAddress, uInt32 numBytes, QByteArray *qmem, QString file="" );
  void displayAligned();
  void readMemSlot(QString dumpFile = "");
  void printUsage(int exitCode);

public:
  // Main window including log text pane...
  messageHandler *log;

protected:
  char formatMsg[8192];
signals:
  void logMessageSignal(LOGLEVEL level, const char *logmsg);
  void logMessageSignal(LOGLEVEL level, const QString a, const QString b = "", const QString c = "", const QString d = "", const QString e = "", const QString f = "", const QString g = "", const QString h = "", const QString i = "", const QString j = "", const QString k = "", const QString l = "", const QString m = "", const QString n = "", const QString o = "", const QString p = "", const QString q = "", const QString r = "", const QString s = "", const QString t = "", const QString u = "" );

private:
  // Config values...
  QSettings *settings;

private:
  static QByteArray noMem;
  QByteArray mem;
  uInt64 memLimit;
  int Route;
  int subDestID;
  uInt64 startAddress;
  uInt64 requestedStartAddress;
  uInt32 numBytes;
  uInt32 numBytesRequested;
  int addrFact;
  QString fileName;

private:
  // SCC API object
  sccApi *sccAccess;
};

#endif // SCCPERF_H
