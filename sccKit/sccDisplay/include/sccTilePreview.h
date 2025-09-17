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

// This widget generates a Tile preview of all available cores (aligned in a 6x4 array)...
#ifndef sccTilePreview_H
#define sccTilePreview_H
#include <QPixmap>
#include <QWidget>
#include <QtGui>
#include <QMessageBox>
#include "coreMask.h"
#include "sccDisplayWidget.h"

// Pre-declaration...
class sccDisplayWidget;

// Image label with mouse eventhandling
class sccTilePreview : public QWidget {
  Q_OBJECT
public:
  sccTilePreview(sccDisplayWidget* parentWidget);
  ~sccTilePreview();
  void refresh();
  void setPixmap(QImage* image, int pid);

private:
  bool event( QEvent *event );
  void keyPressEvent(QKeyEvent *event);
  void keyReleaseEvent(QKeyEvent *event);
  void mousePressEvent ( QMouseEvent * event );
  sccDisplayWidget* parentWidget;
  QGridLayout* individualLayout;
  QLabel* tile[NUM_COLS*NUM_ROWS*NUM_CORES];
  QObject* displayWidget;
  uInt32 displayWidth;
  uInt32 displayHeight;
  uInt32 displayHorizontalScrollValue;
  uInt32 displayVerticalScrollValue;
  bool fs;
  bool rightCtrlActive;
  int highlightedPid;
  QPalette palet;

public:
  void toggleFullscreen();
  void highlightTile(int pid);

};

// --------------------------------------------------------------------------
#endif
// !!! do not add anything behind the #endif line above !!!
