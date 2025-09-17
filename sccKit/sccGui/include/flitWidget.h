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

#ifndef flitWidget_H
#define flitWidget_H

#include <QtGui>
#include <QWidget>
#include <QDialog>
#include <QFileInfo>
#include <QString>
#include "sccExtApi.h"
#include "ui_flitWidget.h"

class flitWidget : public QWidget {
  Q_OBJECT

  public:
    flitWidget(sccExtApi *sccAccess);
    Ui::flitWidget ui;
    int Radix;

  protected:
    sccExtApi *sccAccess;
    void closeEvent(QCloseEvent *event);

  public slots:
    void subDestIDBoxChanged ( const QString text );
    void addressBoxChanged( const QString text );
    int lookupRoute();
    int lookupSubDestID();
    void HexClicked();
    void BinClicked();
    void SetHexData(QString text);
    void read();
    void write();

signals:
    void widgetClosing();

  private:
    void readMem();
};

// --------------------------------------------------------------------------
#endif
// !!! do not add anything behind the #endif line above !!!
