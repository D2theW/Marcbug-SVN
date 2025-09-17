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

#ifndef COREMASK_H
#define COREMASK_H

#include <QApplication>
#include <QMap>
#include <QRegExp>
#include <QProcess>
#include "sccDefines.h"
#include "messageHandler.h"

// ================================================================================
// Mask class...
// ================================================================================
class coreMask {
public:
  coreMask();
  ~coreMask();

signals:
  void widgetClosing();

protected:
  // mask
  QMap<int, bool> currentMask;

public:
  void enableAll();
  void disableAll();
  void setEnabled(int x, int y, int core, bool enabled);
  bool getEnabled(int x, int y, int core);
  void invertMask();
  QString getMaskString();
  int getNumCoresEnabled();
  int getFirstActiveCore();
  void pingCores(QString startIp = "", int timeout=3);
};
#endif
