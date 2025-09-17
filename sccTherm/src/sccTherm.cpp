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

#include "sccTherm.h"
#include <qwaitcondition.h>


// The LOG_DEBUG enables extensive debug messages if set to "1" (should be 0 for releases)
#define LOG_DEBUG 0

bool initTherm=false;
bool readTherm=false;
bool readloop=false;
bool myOk;
int readsecs=0;
uInt64 data_write = 0;
uInt64 read_data1[4] = {0,0,0,0};
QString host = "";
QString init = "";


sccTherm::sccTherm() {
  // Load settings
  settings = new QSettings("sccKit", "sccGui");

  // Create UI and message handler
  QFont font;
  log = new messageHandler(true, "", NULL, NULL);
  #if LOG_DEBUG == 1
    log->setLogLevel(log_debug);
  #endif

  // Parse commandline...
  if (QCoreApplication::arguments().at(1) == "-initTherm") {
    initTherm = true;
    data_write = (uInt64) QCoreApplication::arguments().at(2).toInt(&myOk);	
  }
  else if (QCoreApplication::arguments().at(1) == "-read") {
    readTherm = true;
  }
  else if (QCoreApplication::arguments().at(1) == "-readloop") {
    readloop = true;
    readsecs = QCoreApplication::arguments().at(2).toInt(&myOk);	
  }
  else {
    printError("Invalid Arguements only vallid arguments are -initTherm <cfg value in decimal> or -read -readloop <secs>");
    exit(-1);
  }
	

  // Invoke sccApi and prepare widgets as well as semaphores...
  sccAccess = new sccApi(log);
  // Open driver if necessary...
  bool initFailed = false;
  if (!sccAccess->connectSysIF()) {
    // As we won't open the GUI, print error message to shell instead...
    delete log;
    log = new messageHandler(true);
    initFailed = true;
  } else {
    printInfo("Successfully connected to PCIe driver...");
  }
  

  // Don't open GUI if initialization failed. Otherwise open it...
  if (!initFailed) {
    printInfo("Welcome to sccBmc ", VERSION_TAG, (QString)((BETA_FEATURES!=0)?"beta":""), " (build date ", __DATE__, " - ", __TIME__, ")...");
  } else {
    printError("Unable to connect to PCIe driver...\n       Make sure that PCIe driver is installed and\n       not in use by someone else... Aborting!");
    exit(-1);
  }

  
    if (initTherm) {
       for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
           for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
               sccAccess->transferPacket(TID(loop_x,loop_y), CRB, 0x40, NCWR, 0xff, (uInt64*)&data_write, SIF_RC_PORT, NOGEN);
           }      
       } 
    }

    if (readTherm) {
       for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
           for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
               sccAccess->transferPacket(TID(loop_x,loop_y), CRB, 0x48, NCRD, 0xff, (uInt64*)&read_data1, SIF_RC_PORT, NOGEN);
               printInfo("X = ", QString::number(loop_x), " Y = ", QString::number(loop_y), " Data = ", QString::number(read_data1[0],16), "");
	      
           }      
       }    
 
    }
  
    if (readloop) {
       printInfo("Reading Therm sensor values for ", QString::number(readsecs), " seconds...");
       readsecs = readsecs * 100;
       for(int i=0;i<readsecs;i++) {
         for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
           for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
               sccAccess->transferPacket(TID(loop_x,loop_y), CRB, 0x48, NCRD, 0xff, (uInt64*)&read_data1, SIF_RC_PORT, NOGEN);
	       printInfo("X = ", QString::number(loop_x), " Y = ", QString::number(loop_y), "Data = ", QString::number(read_data1[0],16), "");	
           }      
         }
         usleep(10000);    
       }
	
   
    }

    printInfo("Bye...");

}


sccTherm::~sccTherm() {
  delete settings;
  if (sccAccess) delete sccAccess;
  delete log;
}




