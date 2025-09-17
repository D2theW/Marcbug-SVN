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

#ifndef packetWidget_H
#define packetWidget_H

#include <QtGui>
#include <QWidget>
#include <QDialog>
#include <QFileInfo>
#include <QString>
#include "sccExtApi.h"
#include "ui_packetWidget.h"

typedef struct {
  int Addr;
  uInt64 Data;
  QString Tooltip;
} t_symAddr;

class packetWidget : public QWidget {
  Q_OBJECT

  public:
    packetWidget(sccExtApi *sccAccess);
    Ui::packetWidget ui;
    int Radix;

  protected:
    sccExtApi *sccAccess;
    QMap<QString, t_symAddr> GRBRegs;
    QMap<QString, t_symAddr> RCKRegs;
    void closeEvent(QCloseEvent *event);

  public slots:
    void subDestIDBoxChanged ( const QString text );
    void addressBoxChanged( const QString text );
    void SIFSubIDBoxChanged( const QString text );
    int lookupRoute();
    int lookupSubDestID();
    int lookupCommand();
    int lookupCommandPrefix();
    int lookupSIFSubID();
    void HexClicked();
    void BinClicked();
    void SetHexData(QString text);
    void transfer();

signals:
    void widgetClosing();

};

// --------------------------------------------------------------------------
#endif
// !!! do not add anything behind the #endif line above !!!
