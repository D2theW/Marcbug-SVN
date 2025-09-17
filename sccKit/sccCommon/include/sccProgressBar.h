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

#ifndef SCCPROGRESSBAR_H
#define SCCPROGRESSBAR_H

#include <QApplication>
#include <QProgressDialog>
#include "asciiProgress.h"

// ================================================================================
// Mask class...
// ================================================================================
class sccProgressBar {
public:
  sccProgressBar(bool showDialog = true);
  ~sccProgressBar();
  void setLabelText(QString text);
  void setWindowTitle(QString text);
  void setMinimum(uInt64 min);
  void setMaximum(uInt64 max);
  void setCancelButtonText(QString text);
  void setModal(bool enable);
  void show();
  void hide();
  void reset();
  void cancel();
  void setValue(uInt64 value);
  bool wasCanceled();
  bool isVisible();

private:
  bool isGraphical;
  QProgressDialog* qtBar;
  asciiProgress* asciiBar;
};
#endif
