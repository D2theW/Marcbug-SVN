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

#ifndef sccBoot_H
#define sccBoot_H

#include <QApplication>
#include <QSettings>
#include "sccExtApi.h"
#include "coreMask.h"
#include "sccKitVersion.h"
#include "perfmeterWidget.h"

class sccBoot : public QObject
{
  Q_OBJECT

public:
  sccBoot();
  ~sccBoot();

// =============================================================================================
// Protection members & methods...
// =============================================================================================
protected:
  coreMask linuxMask;
  bool showStatus;
  bool argMemory;
  bool argLinux;
  QString argGeneric;

public:
  void go();

private:
  // bootLinux(): This Routine runs an MMIO based Linux build on selected cores (dialog). No
  // checks are performed. The Linux images are distributed on all RCK memory Controllers!
  void bootLinux();
  // loadGeneric(): Preloads MCs and LUTs with generic content (pre-merged with sccMerge)
  void loadGeneric(QString objPath);
  // testMemory(): Tests complete DDR3 memory
  void testMemory();
  void printUsage(int exitCode);

private:
  // Main window including log text pane...
  messageHandler *log;
  QString userImage;
protected:
  char formatMsg[8192];
signals:
  void logMessageSignal(LOGLEVEL level, const char *logmsg);
  void logMessageSignal(LOGLEVEL level, const QString a, const QString b = "", const QString c = "", const QString d = "", const QString e = "", const QString f = "", const QString g = "", const QString h = "", const QString i = "", const QString j = "", const QString k = "", const QString l = "", const QString m = "", const QString n = "", const QString o = "", const QString p = "", const QString q = "", const QString r = "", const QString s = "", const QString t = "", const QString u = "" );

private:
  // Helper functions
  QString getSccKitPath();

private:
  // Config values...
  QSettings *settings;

private:
  // SCC API object
  sccExtApi *sccAccess;
};

#endif // sccBoot_H
