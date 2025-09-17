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

// This widget is the actual "screen" that is invoked by sccDisplayWidget. It also grabs the keyboard and mouse
// events and processes them. Special keys (with <Right CTRL>+<Some other key>) are also evaluated in this widget.
// The available special keys are:
// <Right CTRL>+<Q>: Quit
// <Right CTRL>+<F>: Fullscreen
// <Right CTRL>+<B>: Enable/Disable Broadcasting
// <Right CTRL>+<H>: Help dialog with special keys
// <Right CTRL>+<Cursor>: Change Tab

#ifndef sccVirtualDisplay_H
#define sccVirtualDisplay_H
#include <QPixmap>
#include <QWidget>
#include <QtGui>
#include <QMessageBox>
#include "coreMask.h"
#include "sccDisplayWidget.h"

#define EVENT_KEYBOARD_PRESS    0x00
#define EVENT_KEYBOARD_RELEASE  0x01
#define EVENT_MOUSE_PRESS       0x02
#define EVENT_MOUSE_RELEASE     0x03
#define EVENT_MOUSE_MOVE        0x04
#define EVENT_WHEEL             0x05
#define EVENT_SOUND_ENABLE      0x06
#define EVENT_SET_CLIPBOARD     0x07

// Pre-declaration...
class sccDisplayWidget;

// Image label with mouse eventhandling
class sccVirtualDisplay : public QLabel {
  Q_OBJECT
public:
  sccVirtualDisplay(QTabBar* tabBar, sccDisplayWidget* parentWidget);
  ~sccVirtualDisplay();
  QTabBar* tabBar;
  sccDisplayWidget* parentWidget;

private:
  int keyTranslation(int scanIn, int qtKeyIn, QString debugPrefix="KEY DBG");

protected:
  bool event( QEvent *event );
  void keyPressEvent(QKeyEvent *event);
  void keyReleaseEvent(QKeyEvent *event);
  void mousePressEvent ( QMouseEvent * event );
  void mouseReleaseEvent ( QMouseEvent * event );
  void mouseMoveEvent ( QMouseEvent * event );
  void wheelEvent(QWheelEvent *event);
  void focusOutEvent ( QFocusEvent * event );

private:
  QObject* displayWidget;
  uInt32 displayWidth;
  uInt32 displayHeight;
  uInt32 displayHorizontalScrollValue;
  uInt32 displayVerticalScrollValue;
  bool fs;
  bool rightCtrlActive;
  bool dumpKeyStrokes;
  QPalette palet;

public:
  void toggleFullscreen();
};

// --------------------------------------------------------------------------
#endif
// !!! do not add anything behind the #endif line above !!!
