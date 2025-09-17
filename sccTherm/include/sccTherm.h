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
#include "perfmeterWidget.h"

class sccTherm : public QObject {
  Q_OBJECT

public:
  sccTherm();
  ~sccTherm();

// =============================================================================================
// Protection members & methods...
// =============================================================================================

public:
  // Main window including log text pane...
  messageHandler *log;
  // Widgets
  QTimer *perfPollingTimer;
  char perfStatus[64];
  int perfRoute, perfDestid, perfSIFPort;
  uInt64 perfApertureStart;

private:
  // Config values...
  QSettings *settings;

private:
  // SCC API object
  sccApi *sccAccess;
};

#endif // SCCPERF_H
