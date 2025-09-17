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

#ifndef readMemWidget_H
#define readMemWidget_H

#include <QtGui>
#include <QWidget>
#include <QDialog>
#include <QFileInfo>
#include <QString>
#include "sccExtApi.h"
#include "ui_readMemWidget.h"

#define MC_MCEMU_MEMLIMIT 0x40000000ull
#define MPB_MEMLIMIT 0x04000ull

class mce_rckConfigPlugin;

class readMemWidget : public QWidget {

  Q_OBJECT

public:

  readMemWidget(sccExtApi *sccAccess);
  Ui::readMemWidget ui;
  static QByteArray noMem;
  uInt64 memLimit;
  int sifPort;
  int Route;
  int subDestID;
  bool printCrb;
  bool dumpToFile;

  // Former node access functions
  uInt32 getDispBytes (uInt32 numBytes);
  uInt32 getNumBytesAlign (uInt32 numBytes,  uInt64 startAddress, uInt64 lastAddress);
  uInt64 getStartAddrAlign (uInt64 startAddress);
  uInt32 readMem(uInt64 startAddress, uInt32 numBytes, QByteArray *qmem, QString file="" );

protected:
  void closeEvent(QCloseEvent *event);

public slots:
  void closeWidget();
  void readAndSaveMemSlot();
  void readMemSlot();
  void displayAligned();
  void setHeader(QString newHeader);
  void subDestChanged(const QString newSubDest);
  void TileIDChanged(const QString newTileID);

signals:
  void widgetClosing();

private:
  sccExtApi *sccAccess;
  QByteArray mem;
  QString header;
  uInt64 startAddress;
  uInt32 numBytesRequested;
};

// --------------------------------------------------------------------------
#endif
// !!! do not add anything behind the #endif line above !!!
