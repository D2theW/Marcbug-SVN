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

#include <QApplication>
#include "messageHandler.h"
#include "coreMask.h"
#include "coreMaskDialog.h"
#include "sccProgressBar.h"

// Test program for sccCommon objects...
int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  messageHandler *log;
  log = new messageHandler(true);
  logInfo("Log works fine...");
  coreMask test1;
  logInfo("coreMask default (enabled): ", test1.getMaskString());
  test1.disableAll();
  logInfo("coreMask enabled: ", test1.getMaskString());
  coreMaskDialog test2;
  logInfo("coreMaskDialog default (disabled): ", test1.getMaskString());
  test2.selectionDialog("Select cores", "Select cores");
  logInfo("coreMaskDialog user selection: ", test2.getMaskString());
  for (int loop = 0; loop <= 1; loop++) {
    sccProgressBar* pd;
    pd = new sccProgressBar((loop)?true:false);
    pd->setLabelText("My "+(QString)((loop)?"graphical":"ASCII")+" progress indicator");
    pd->setWindowTitle("Progress");
    pd->setMinimum(0);
    pd->setMaximum(100);
    pd->setCancelButtonText("Abort");
    pd->setModal(true);
    pd->show();
    for (int progress = 10; progress <= 100; progress+=10) {
      sleep(1); pd->setValue(progress);
    }
    pd->hide();
    delete pd;
  }
  return 0;
}
