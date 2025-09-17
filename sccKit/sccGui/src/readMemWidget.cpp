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

#include "readMemWidget.h"
#include <QByteArray>

// Enable/Disable debugging information in this "module"
#define mce_DEBUG_MESSAGES 0

// Initialize noMem...
QByteArray readMemWidget::noMem=0;

//Constructor of readMemWidget widget
readMemWidget::readMemWidget(sccExtApi *sccAccess) : QWidget() {
  ui.setupUi(this);
  QWidget::setWindowFlags(Qt::Window);
  this->sccAccess = sccAccess;
  dumpToFile = false;
}

void readMemWidget::closeEvent(QCloseEvent *event) {
  emit widgetClosing();
  QWidget::closeEvent(event);
}

void readMemWidget::closeWidget() {
  this->close();
}

void readMemWidget::setHeader(QString newHeader) {
  header=newHeader;
}

void readMemWidget::subDestChanged(const QString newSubDest) {
  printCrb = false;
  ui.rB_8bit->setEnabled(true);
  ui.rB_16bit->setEnabled(true);
  ui.rB_32bit->setEnabled(true);
  ui.rB_64bit->setEnabled(true);
  ui.lE_StartAddr->setEnabled(true);
  ui.lE_numBytes->setEnabled(true);
  if ((newSubDest == "MPB") || (newSubDest == "CRB")) {
    if (newSubDest == "MPB") {
      memLimit = MPB_MEMLIMIT;
      sifPort = SIF_RC_PORT;
      subDestID = MPB;
    } else if (newSubDest == "CRB") {
      printCrb = true;
      ui.rB_8bit->setEnabled(false);
      ui.rB_16bit->setEnabled(false);
      ui.rB_32bit->setEnabled(false);
      ui.rB_64bit->setEnabled(false);
      ui.lE_StartAddr->setEnabled(false);
      ui.lE_numBytes->setEnabled(false);
    }
    ui.cB_TileID->clear();
    ui.cB_TileID->insertItems(0, QStringList()
     << "x=0, y=0"
     << "x=1, y=0"
     << "x=2, y=0"
     << "x=3, y=0"
     << "x=4, y=0"
     << "x=5, y=0"
     << "x=0, y=1"
     << "x=1, y=1"
     << "x=2, y=1"
     << "x=3, y=1"
     << "x=4, y=1"
     << "x=5, y=1"
     << "x=0, y=2"
     << "x=1, y=2"
     << "x=2, y=2"
     << "x=3, y=2"
     << "x=4, y=2"
     << "x=5, y=2"
     << "x=0, y=3"
     << "x=1, y=3"
     << "x=2, y=3"
     << "x=3, y=3"
     << "x=4, y=3"
     << "x=5, y=3"
    );
  } else if (newSubDest == "MC") {
    memLimit = sccAccess->getSizeOfMC()*SIZE_1GB;
    sifPort = SIF_RC_PORT;
    subDestID = PERIW;
    ui.cB_TileID->clear();
    ui.cB_TileID->insertItems(0, QStringList()
     << "x=0, y=0"
     << "x=5, y=0"
     << "x=0, y=2"
     << "x=5, y=2"
    );
  } else if (newSubDest == "DDR2") {
    memLimit = SIZE_1GB;
    sifPort = SIF_MC_PORT;
    subDestID = PERIS;
    int Route = 0x03;
    ui.cB_TileID->clear();
    ui.cB_TileID->insertItem(0, ((QString)"x="+QString::number(X_TID(Route),10)+", y="+QString::number(Y_TID(Route),10)));
  } else if (newSubDest == "SoftRAM") {
    memLimit = MC_MCEMU_MEMLIMIT;
    sifPort = SIF_HOST_PORT;
    subDestID = PERIS;
    int Route = 0x03;
    ui.cB_TileID->clear();
    ui.cB_TileID->insertItem(0, ((QString)"x="+QString::number(X_TID(Route),10)+", y="+QString::number(Y_TID(Route),10)));
  }
  if (mce_DEBUG_MESSAGES) printf ("subDestChanged: Changed Port to %0d, SubDestID to %0d and memory limit to %016llx...\n", sifPort, subDestID, memLimit);
}

void readMemWidget::TileIDChanged(const QString newTileID) {
  bool ok;
  Route = newTileID.mid(2, 1).toInt(&ok, 10); // x
  if (ui.cB_SubDest->currentText()=="MC") subDestID = (Route==0)?PERIW:PERIE;
  Route += (newTileID.mid(7, 1).toInt(&ok, 10)) << 4; // y
  if (mce_DEBUG_MESSAGES) printf ("TileIDChanged: Changed Route/DestID to 0x%02x/%0d...\n", Route, subDestID);
}

void readMemWidget::displayAligned() {
  //This function monitors memory at system level.
  uInt32 length=mem.size();
  if(!length) {
    return;
  }

  QApplication::setOverrideCursor(Qt::WaitCursor);
  QString lenmess;

  int addrFact=1; //this factor applies for octet addresses
  if(ui.rB_8bit->isChecked()) {
    addrFact=1;
  }
  if(ui.rB_16bit->isChecked()) {
    addrFact=2;
  }
  if(ui.rB_32bit->isChecked()) {
    addrFact=4;
  }
  if(ui.rB_64bit->isChecked()) {
    addrFact=8;
  }

  uInt64 word;
  char _Byte[addrFact];

  uInt32 x = 0;
  QString res = "";
  signed int y = 0;


  QString printAddr="";
  QString contAscii="";
  QString winMem="";

  ui.memWindow->clear();

  ui.memWindow->append(header);

  ui.memWindow->append( "--------------------------------------------------");
  lenmess= "** " + QString::number( (uInt32) 8*addrFact, 10) + " bits alignment";
  ui.memWindow->append(lenmess);
  ui.memWindow->append( "--------------------------------------------------");
  x = 0;
  y = 0;

  bool dispBar=false;

  if ((length/addrFact)>100000)
    dispBar=true;
  sccProgressBar *pd = NULL;
  int idx=0;
  if (dispBar) {
    pd = new sccProgressBar();
    pd->setLabelText("Display in progress.");
    pd->setMinimum(0);
    pd->setMaximum(length/addrFact);
    pd->setWindowTitle("Progress");
    pd->show();
  }

  for (uInt64 i = 0; i < (length/addrFact); i++) {
    if(dispBar && !(x % 50000)) {
      pd->setValue(i);
      ui.memWindow->append( winMem);
      winMem="";
      qApp->processEvents(QEventLoop::ExcludeUserInputEvents,5);
    }
    word=0;
    idx=x;
    for (int j=addrFact-1; j>=0; j--) {
      _Byte[j] =  (unsigned char) mem.at(x++) & 0xFF; //(*mem)[x++] & 0xFF;
      //    }
      //for (int j=0; j<addrFact; j++) {
      word <<= 8;
      word |= (unsigned char)  mem.at(idx+j) & 0xFF; //_Byte[j] & 0xFF;
      //substituting codes below 0x20 by "." for ASCII printing
      _Byte[j]=(_Byte[j]<0x20)?0x2E:_Byte[j];
    }
    res += QString("%1").arg(QString::number( (uInt64) word, 16), 2*addrFact, QLatin1Char(0x30));
    res += " ";

    contAscii += QString::fromLatin1 (_Byte, addrFact );


    if(++y == 8/addrFact) {
      printAddr = QString("%1").arg(QString::number( (uInt64)startAddress + x - 8, 16), 16, QLatin1Char(0x30)) + " | ";
      {
	res += " | " + contAscii;
	//ui.memWindow->append( printAddr + res);
	winMem+=printAddr + res + "\n";
      }
      contAscii="";
      res = "";
      y = 0;
    }
  }
  ui.memWindow->append( winMem);
  qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
  if (dispBar)
    pd->~sccProgressBar();
  QApplication::restoreOverrideCursor();
}

uInt32 readMemWidget::getDispBytes (uInt32 numBytes) {
  uInt32 rndFact= (numBytes % 8)?(8 - numBytes % 8):0;
  uInt32 dispBytes=numBytes + rndFact; // bytes to be read
  return dispBytes;
}

uInt32 readMemWidget::getNumBytesAlign (uInt32 numBytes,  uInt64 startAddress, uInt64 lastAddress){
  uInt32 numBytesA=numBytes;
  numBytesA+=(numBytes % 32)?(32 - numBytes % 32):0; // rounding to multiples of 32, since it is the minimal amount to be read
  while ((uInt64) (32*((uInt64)(startAddress / 32)) + numBytesA) < lastAddress) {
    numBytesA+=32; // to ensure the desired address is displayed
  }

  numBytesA=(numBytesA % 32)?(32*(1 + int(numBytesA/32))):(32*int(numBytesA/32));
  return numBytesA;
}

uInt64 readMemWidget::getStartAddrAlign (uInt64 startAddress) {
  //rounding addresses and num of bytes acordingly for readMem function
  uInt64 startAddressA= 32*((uInt64)(startAddress / 32));
  return startAddressA;
}

uInt32 readMemWidget::readMem(uInt64 startAddress, uInt32 numBytes, QByteArray *qmem, QString file ) {
  bool ofileOK = false;
  FILE* ofile = NULL;
  if (file != "") {
    if(!(ofile = fopen (file.toStdString().c_str(), "w"))) {
      QMessageBox::warning(this, "Error", "Could not open selected output file for writing... Continuing without creation of dumpfile...", QMessageBox::Ok);
    }
    else
      ofileOK = true;
  }

  //proceeding to align the address and the length of the block  to be read
  //32 bits

  //validating number of bytes. Qutting if 0.
  if(!numBytes) {
    ui.memWindow->append("Requested 0 bytes. We're done...");
    return 0;
  }

  uInt32 dispBytes=getDispBytes(numBytes);
  uInt64 lastAddress=startAddress+dispBytes; // last address to be read

  uInt32 numBytesA=getNumBytesAlign(numBytes,startAddress,lastAddress);

  //rounding addresses and num of bytes acordingly for transfer packet function
  uInt64 startAddressA = getStartAddrAlign(startAddress);

  //validating is start address is in range. Qutting if not.
  if(startAddressA >= memLimit) {
    ui.memWindow->append("Memory access out of range. Please reduce start address (aligned startaddress is 0x"+QString::number(startAddressA,16)+" but limit is 0x"+QString::number(memLimit,16)+").");
    return 0;
  }

  //Ensuring memory limit is not exceeded
  if(startAddressA < memLimit && lastAddress > memLimit) {
      lastAddress=memLimit-1;
      numBytes=memLimit-startAddress;
      numBytesA=getNumBytesAlign(numBytes,startAddressA,lastAddress);
      dispBytes=numBytes;
  }
  QApplication::setOverrideCursor(Qt::WaitCursor);
  ui.pB_Reload->setEnabled(false);
  ui.lE_StartAddr->setEnabled(false);
  ui.lE_numBytes->setEnabled(false);
  ui.rB_8bit->setEnabled(false);
  ui.rB_16bit->setEnabled(false);
  ui.rB_64bit->setEnabled(false);
  ui.rB_32bit->setEnabled(false);
  ui.cB_TileID->setEnabled(false);
  ui.cB_SubDest->setEnabled(false);
  ui.pB_Close->setEnabled(false);
  ui.memWindow->append(" Contacting SystemIF to read memory. Please be patient ...");

  char mem[numBytesA];
  dispBytes = sccAccess->read32(Route, subDestID, startAddressA, (char*)&mem, numBytesA, sifPort, NULL);
  uInt32 idx=startAddress-startAddressA;
  for (uInt32 i = idx; i < (idx+dispBytes); i++) {
    if (qmem != &noMem) qmem->append((unsigned char) mem[i]);
    if (ofileOK) putc(mem[i],ofile);
  }

  ui.memWindow->append(" done System Read.");
  ui.pB_Reload->setEnabled(true);
  ui.lE_StartAddr->setEnabled(true);
  ui.lE_numBytes->setEnabled(true);
  ui.rB_8bit->setEnabled(true);
  ui.rB_16bit->setEnabled(true);
  ui.rB_64bit->setEnabled(true);
  ui.rB_32bit->setEnabled(true);
  ui.cB_TileID->setEnabled(true);
  ui.cB_SubDest->setEnabled(true);
  ui.pB_Close->setEnabled(true);
  QApplication::restoreOverrideCursor();

  if (ofileOK)
    fclose(ofile);

  return 0;
}

void readMemWidget::readAndSaveMemSlot() {
  dumpToFile = true;
  readMemSlot();
  dumpToFile = false;
}

void readMemWidget::readMemSlot() {
  //This function monitors memory at system level.
  //Every system memory address range has to be converted(or decoded) to
  // nodeID, memory address range

  bool ok;
  QString lenmess = "--------------------------------------------------\n";
  //Reading 32 bits of the provided Address
  startAddress= (uInt64) ui.lE_StartAddr->text().right(9).toULongLong(&ok,16);
  // Now performing a primitive node decoding. This should be done according memory managing.
  uInt32 numBytes=(uInt32) ui.lE_numBytes->text().toULong(&ok,10); // bytes to be read
  if (numBytes > SIZE_1MB) {
    // Only show up to 1MB of memory...
    lenmess += "  Limit exceeded: Only showing first megabyte of requested data...\n";
    numBytes = SIZE_1MB;
  }
  numBytesRequested = numBytes;
  if (startAddress % 0x20) {
    numBytes += (startAddress % 0x20); // Read more...
    startAddress -= (startAddress % 0x20); // ... and start erlier so that the bytes of interest are contained in the dump!
    lenmess += "Start address is not 256-bit aligned... Using 0x"+ QString::number(startAddress, 16) +" instead!\n";
  }
  uInt64 dispAddress= startAddress; // address to be displayed, sinc startAddress will be rounded later on
  uInt32 dispBytes=getDispBytes(numBytes); //numBytes + rndFact; // bytes to be displayed
  uInt64 lastAddress=dispAddress+dispBytes-1; // last address to be displayed


  QString targetFile="";
  QTextBrowser *memWindow=ui.memWindow;

  int sliderPos=memWindow->verticalScrollBar()->sliderPosition();

  if (dumpToFile) {
    targetFile=QFileDialog::getSaveFileName(this,
					    "Choose target file to store Application Memory",
					    ".",
					    (printCrb)?"Any File (*);;Text File (*.txt)":"Raw Files (*.raw);;Out Files (*.out);;Any File (*)");

    if (targetFile.isEmpty())
    return; // breaking  if operation was canceled
  }

  if (printCrb) {
    ui.memWindow->clear();
    uInt64 lock0 = 0, lock1 = 0;
    int lutEntry;
    lenmess = "=============================================================";
    lenmess += "\nDumping CRB registers of Tile " + messageHandler::toString(Route, 16, 2);
    lenmess += "\n=============================================================";
    lenmess += "\nGLCFG0   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x010), 16, 8);
    lenmess += "\nGLCFG1   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x018), 16, 8);
    lenmess += "\nL2CFG0   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x020), 16, 8);
    lenmess += "\nL2CFG1   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x028), 16, 8);
    lenmess += "\nSENSOR   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x040), 16, 8);
    lenmess += "\nGCBCFG   = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x080), 16, 8);
    lenmess += "\nMYTILEID = " + messageHandler::toString((int)sccAccess->readFlit(Route, CRB, 0x0100), 16, 8);
    lenmess += "\nLOCK0    = " + messageHandler::toString(lock0=(int)sccAccess->readFlit(Route, CRB, 0x0200), 16, 8);
    lenmess += "\nLOCK1    = " + messageHandler::toString(lock1=(int)sccAccess->readFlit(Route, CRB, 0x0400), 16, 8);
    if (lock0 || lock1) {
      lenmess += "\n-------------------------------------------------------------";
      lenmess += "\nRestoring lock";
      lenmess += (lock0&&lock1)?"s: LOCK0 and LOCK1":(lock0)?": LOCK0":": LOCK1";
      if (lock0) sccAccess->writeFlit(Route, CRB, 0x200, 0x00000000);
      if (lock1) sccAccess->writeFlit(Route, CRB, 0x400, 0x00000000);
    }
    lenmess += "\n=============================================================";
    lenmess += "\nDumping LUTs of Tile " + messageHandler::toString(Route, 16, 2);
    lenmess += "\nFormat: Bypass(bin)_Route(hex)_subDestId(dec)_AddrDomain(hex)";
    lenmess += "\n=============================================================";
    for (int loopCore = 0; loopCore <2; loopCore++) {
      for (int loopLutIndex = 0; loopLutIndex < 0x100; loopLutIndex += 1) {
        lutEntry = sccAccess->readFlit(Route, CRB, 0x800 + loopCore*0x800 + 8*loopLutIndex);
        lenmess += "\nLUT" + QString::number(loopCore) + ", Entry " + messageHandler::toString(loopLutIndex, 16, 2) + " (CRB addr = " + messageHandler::toString(0x800 + loopCore*0x800 + 8*loopLutIndex, 16, 4) + "): "
                           + messageHandler::toString((lutEntry>>21)&0x01, 2, 1) + "_"
                           + messageHandler::toString((lutEntry>>13)&0x0ff, 16, 2) + "_"
                           + messageHandler::toString((lutEntry>>10)&0x07, 10, 1) + "(" + sccAccess->decodeDestID((lutEntry>>10)&0x07, true).replace(QRegExp("(\\S+)\\s+"), "-\\1-") + ")_"
                           + messageHandler::toString(lutEntry & 0x3ff, 16, 3);
      }
    }
    lenmess += "\n=============================================================";
    if (targetFile != "") {
      QFile file(targetFile);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Failed to open output file \"" + targetFile + "\"... File is not writable.", QMessageBox::Ok);
      } else {
        QTextStream out(&file);
        out << lenmess << endl;
      }
      file.close();
    }
    // Print to log in any case...
    memWindow->append( lenmess );
    return;
  };

  //validating is start address is in range. Qutting if not.
  if(startAddress >= memLimit) {
    lenmess+="Memory access out of range. Limit = "+QString::number( (uInt64 ) memLimit, 16)+" hex. Please reduce start address.";
    lenmess+="\n--------------------------------------------------";
    memWindow->clear();
    memWindow->append(lenmess);
    return;
  }

  //At this point each system address range has to be decoded in to node ID and node address range
  //since now only one node is implemented, only type conversion is required
  uInt64 nodeStartAddr=(uInt64) startAddress;
  uInt32 nodeNumBytes=(uInt32) numBytes;

  if(startAddress < memLimit && lastAddress >= memLimit) {
      lastAddress=memLimit-1;
      //nodeNumBytes=64;
  }

  mem.clear();

  if(dumpToFile)
    readMem(nodeStartAddr,nodeNumBytes,&mem,targetFile);
  else
    readMem(nodeStartAddr,nodeNumBytes,&mem);

  if(!mem.length()) {
    memWindow->append("Error while trying to read memory.");
    return;
  }

  lenmess += "  Reading on " +  sccAccess->decodeDestID(subDestID) + " of Tile " + ui.cB_TileID->currentText();
  lenmess += "\n  startAddress = " + QString::number( (uInt64) startAddress, 16) + " hex";
  lenmess += " (" + QString::number((uInt64)nodeStartAddr, 10) + " dec) ";
  lenmess += "\n  stopAddress  = " + QString::number( (uInt64) lastAddress, 16) + " hex";
  lenmess += "\n  Requested/Displayed length: " + QString::number(numBytesRequested, 10) + "/" + QString::number( (uInt32) mem.length(), 10) + " bytes";
  setHeader(lenmess);

  displayAligned();

  if (dispAddress + dispBytes -1 >= memLimit) {
    lenmess="--------------------------------------------------";
    lenmess+= "\nMemory limit " + QString::number( (uInt64) memLimit, 16) + " hex reached";
    lenmess+="\n--------------------------------------------------";
    memWindow->append( lenmess );
  }

  memWindow->verticalScrollBar()->setSliderPosition(sliderPos);
}

