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
// <Right CTRL>+<P>: Preview displays of all Cores
// <Right CTRL>+<T>: Toggle perfmeter...
// <Right CTRL>+<B>: Enable/Disable Broadcasting
// <Right CTRL>+<H>: Help dialog with special keys
// <Right CTRL>+<Cursor>: Change Tab

#include "sccVirtualDisplay.h"
#include <qcursor.h>
#include <linux/input.h>

sccVirtualDisplay::sccVirtualDisplay(QTabBar* tabBar, sccDisplayWidget* parentWidget) {
  this->tabBar = tabBar;
  this->parentWidget = parentWidget;
  fs = false;
  rightCtrlActive=false;
  dumpKeyStrokes=false;
  palet.setColor( backgroundRole(), Qt::black );
  setPalette(palet);
  setMouseTracking(true);
  setFocusPolicy(Qt::WheelFocus);
  setCursor(QCursor(Qt::BlankCursor));
  setAlignment(Qt::AlignCenter);
}

sccVirtualDisplay::~sccVirtualDisplay() {
}

int sccVirtualDisplay::keyTranslation(int scanIn, int qtKeyIn, QString debugPrefix) {
  // Want to debug?
  if (dumpKeyStrokes) {
    printf("%s:   Scan/QT key = 0x%08x/0x%08x\n", debugPrefix.toStdString().c_str(), scanIn, qtKeyIn);
  }

  // Map w/ scancodes
  switch (scanIn) {
  }

  // Map remaining keys w/ QT codes...
  switch (qtKeyIn) {
    case Qt::Key_Escape           : return KEY_ESC;
    case Qt::Key_Tab              : return KEY_TAB;
    case Qt::Key_Backspace        : return KEY_BACKSPACE;
    case Qt::Key_Return           : return KEY_ENTER;
    case Qt::Key_Enter            : return KEY_KPENTER;
    case Qt::Key_Insert           : return KEY_INSERT;
    case Qt::Key_Delete           : return KEY_DELETE;
    case Qt::Key_Pause            : return KEY_PAUSE;
    case Qt::Key_Print            : return KEY_PRINT;
    case Qt::Key_SysReq           : return KEY_SYSRQ;
    case Qt::Key_Clear            : return KEY_CLEAR;
    case Qt::Key_Home             : return KEY_HOME;
    case Qt::Key_End              : return KEY_END;
    case Qt::Key_Left             : return KEY_LEFT;
    case Qt::Key_Up               : return KEY_UP;
    case Qt::Key_Right            : return KEY_RIGHT;
    case Qt::Key_Down             : return KEY_DOWN;
    case Qt::Key_PageUp           : return KEY_PAGEUP;
    case Qt::Key_PageDown         : return KEY_PAGEDOWN;
    case Qt::Key_Shift            : return KEY_LEFTSHIFT;
    case Qt::Key_Control          : return KEY_LEFTCTRL;
    case Qt::Key_Meta             : return KEY_LEFTMETA;
    case Qt::Key_Alt              : return KEY_LEFTALT;
    case Qt::Key_AltGr            : return KEY_RIGHTALT;
    case Qt::Key_CapsLock         : return KEY_CAPSLOCK;
    case Qt::Key_NumLock          : return KEY_NUMLOCK;
    case Qt::Key_ScrollLock       : return KEY_SCROLLLOCK;
    case Qt::Key_F1               : return KEY_F1;
    case Qt::Key_F2               : return KEY_F2;
    case Qt::Key_F3               : return KEY_F3;
    case Qt::Key_F4               : return KEY_F4;
    case Qt::Key_F5               : return KEY_F5;
    case Qt::Key_F6               : return KEY_F6;
    case Qt::Key_F7               : return KEY_F7;
    case Qt::Key_F8               : return KEY_F8;
    case Qt::Key_F9               : return KEY_F9;
    case Qt::Key_F10              : return KEY_F10;
    case Qt::Key_F11              : return KEY_F11;
    case Qt::Key_F12              : return KEY_F12;
    case Qt::Key_F13              : return KEY_F13;
    case Qt::Key_F14              : return KEY_F14;
    case Qt::Key_F15              : return KEY_F15;
    case Qt::Key_F16              : return KEY_F16;
    case Qt::Key_F17              : return KEY_F17;
    case Qt::Key_F18              : return KEY_F18;
    case Qt::Key_F19              : return KEY_F19;
    case Qt::Key_F20              : return KEY_F20;
    case Qt::Key_F21              : return KEY_F21;
    case Qt::Key_F22              : return KEY_F22;
    case Qt::Key_F23              : return KEY_F23;
    case Qt::Key_F24              : return KEY_F24;
    case Qt::Key_Menu             : return KEY_MENU;
    case Qt::Key_Help             : return KEY_HELP;
    case Qt::Key_Space            : return KEY_SPACE;
    case Qt::Key_QuoteDbl         : return KEY_APOSTROPHE;
    case Qt::Key_Apostrophe       : return KEY_EQUAL;
    case Qt::Key_Plus             : return KEY_KPPLUS;
    case Qt::Key_Comma            : return KEY_COMMA;
    case Qt::Key_Minus            : return KEY_MINUS;
    case Qt::Key_Period           : return KEY_DOT;
    case Qt::Key_Slash            : return KEY_SLASH;
    case Qt::Key_Colon            : return KEY_SEMICOLON;
    case Qt::Key_Semicolon        : return KEY_SEMICOLON;
    case Qt::Key_Less             : return KEY_COMMA;
    case Qt::Key_Equal            : return KEY_EQUAL;
    case Qt::Key_Greater          : return KEY_DOT;
    case Qt::Key_Question         : return KEY_SLASH;
    case Qt::Key_1                : return KEY_1;
    case Qt::Key_Exclam           : return KEY_1;
    case Qt::Key_2                : return KEY_2;
    case Qt::Key_At               : return KEY_2;
    case Qt::Key_3                : return KEY_3;
    case Qt::Key_NumberSign       : return KEY_3;
    case Qt::Key_4                : return KEY_4;
    case Qt::Key_Dollar           : return KEY_4;
    case Qt::Key_5                : return KEY_5;
    case Qt::Key_Percent          : return KEY_5;
    case Qt::Key_6                : return KEY_6;
    case Qt::Key_AsciiCircum      : return KEY_6;
    case Qt::Key_7                : return KEY_7;
    case Qt::Key_Ampersand        : return KEY_7;
    case Qt::Key_8                : return KEY_8;
    case Qt::Key_Asterisk         : return KEY_8;
    case Qt::Key_9                : return KEY_9;
    case Qt::Key_ParenLeft        : return KEY_9;
    case Qt::Key_0                : return KEY_0;
    case Qt::Key_ParenRight       : return KEY_0;
    case Qt::Key_A                : return KEY_A;
    case Qt::Key_B                : return KEY_B;
    case Qt::Key_C                : return KEY_C;
    case Qt::Key_D                : return KEY_D;
    case Qt::Key_E                : return KEY_E;
    case Qt::Key_F                : return KEY_F;
    case Qt::Key_G                : return KEY_G;
    case Qt::Key_H                : return KEY_H;
    case Qt::Key_I                : return KEY_I;
    case Qt::Key_J                : return KEY_J;
    case Qt::Key_K                : return KEY_K;
    case Qt::Key_L                : return KEY_L;
    case Qt::Key_M                : return KEY_M;
    case Qt::Key_N                : return KEY_N;
    case Qt::Key_O                : return KEY_O;
    case Qt::Key_P                : return KEY_P;
    case Qt::Key_Q                : return KEY_Q;
    case Qt::Key_R                : return KEY_R;
    case Qt::Key_S                : return KEY_S;
    case Qt::Key_T                : return KEY_T;
    case Qt::Key_U                : return KEY_U;
    case Qt::Key_V                : return KEY_V;
    case Qt::Key_W                : return KEY_W;
    case Qt::Key_X                : return KEY_X;
    case Qt::Key_Y                : return KEY_Y;
    case Qt::Key_Z                : return KEY_Z;
    case Qt::Key_BracketLeft      : return KEY_LEFTBRACE;
    case Qt::Key_Backslash        : return KEY_BACKSLASH;
    case Qt::Key_BracketRight     : return KEY_RIGHTBRACE;
    case Qt::Key_Underscore       : return KEY_MINUS;
    case Qt::Key_QuoteLeft        : return KEY_GRAVE;
    case Qt::Key_BraceLeft        : return KEY_LEFTBRACE;
    case Qt::Key_Bar              : return KEY_BACKSLASH;
    case Qt::Key_BraceRight       : return KEY_RIGHTBRACE;
    case Qt::Key_AsciiTilde       : return KEY_GRAVE;
    case Qt::Key_yen              : return KEY_YEN;
    case Qt::Key_Agrave           : return KEY_GRAVE;
    case Qt::Key_Adiaeresis       : return KEY_APOSTROPHE;
    case Qt::Key_Odiaeresis       : return KEY_SEMICOLON;
    case Qt::Key_Udiaeresis       : return KEY_LEFTBRACE;
    case Qt::Key_ssharp           : return KEY_MINUS;
    case Qt::Key_Muhenkan         : return KEY_MUHENKAN;
    case Qt::Key_Henkan           : return KEY_HENKAN;
    case Qt::Key_Hiragana         : return KEY_HIRAGANA;
    case Qt::Key_Katakana         : return KEY_KATAKANA;
    case Qt::Key_Back             : return KEY_BACK;
    case Qt::Key_Forward          : return KEY_FORWARD;
    case Qt::Key_Stop             : return KEY_STOP;
    case Qt::Key_Refresh          : return KEY_REFRESH;
    case Qt::Key_VolumeDown       : return KEY_VOLUMEDOWN;
    case Qt::Key_VolumeMute       : return KEY_MUTE;
    case Qt::Key_VolumeUp         : return KEY_VOLUMEUP;
    case Qt::Key_BassBoost        : return KEY_BASSBOOST;
    case Qt::Key_HomePage         : return KEY_HOMEPAGE;
    case Qt::Key_Favorites        : return KEY_FAVORITES;
    case Qt::Key_Search           : return KEY_SEARCH;
    case Qt::Key_unknown          : return KEY_UNKNOWN;
    case Qt::Key_Select           : return KEY_SELECT;
    case Qt::Key_Play             : return KEY_PLAY;
    case Qt::Key_Sleep            : return KEY_SLEEP;
    case Qt::Key_Zoom             : return KEY_ZOOM;
    case Qt::Key_Cancel           : return KEY_CANCEL;
  }
  return KEY_RESERVED;
}

// Catch TAB event at a lower level as it's usually handled by widget focus routines...
bool sccVirtualDisplay::event( QEvent *event ) {
  if ( event->type() == QEvent::KeyPress ) {
      QKeyEvent *keyboardEvent = (QKeyEvent *)event;
    if ( keyboardEvent->key() == Qt::Key_Tab ) {
      keyPressEvent(keyboardEvent);
      keyboardEvent->accept();
      return TRUE;
    }
  }
  return QWidget::event( event );
}

void sccVirtualDisplay::keyPressEvent(QKeyEvent *event) {
  // Check for right ctrl key...
  if ((event->key() == Qt::Key_Control) && ((event->nativeScanCode() == 0x69) || (event->nativeScanCode() == 0x6d))) {
    rightCtrlActive = true;
    return;
  }
  // Make sure that we don't miss to release rightCtrlActive
  if (!event->modifiers()) {
    rightCtrlActive = false;
  }
  if ( rightCtrlActive && (event->key() == Qt::Key_H)) {
    QMessageBox::information(this, "Help","\
Special keys:\n\n\
<Right CTRL>+<Q>: Quit\n\
<Right CTRL>+<F>: Fullscreen\n\
<Right CTRL>+<P>: Open preview display\n\
<Right CTRL>+<T>: Toggle perfmeter...\n\
<Right CTRL>+<B>: Enable/Disable Broadcasting\n\
<Right CTRL>+<H>: Display this help\n\
<Right CTRL>+<Cursor>: Change Tab\n\
         ", QMessageBox::Ok);
    rightCtrlActive = false;
    return;
  }
  if ( rightCtrlActive && (event->key() == Qt::Key_F)) {
    toggleFullscreen();
    return;
  }
  if ( rightCtrlActive && (event->key() == Qt::Key_P)) {
    parentWidget->showPreview((event->modifiers() & Qt::ShiftModifier)?true:fs, fs);
    return;
  }
  if ( rightCtrlActive && (event->key() == Qt::Key_T)) {
    parentWidget->togglePerfmeter();
    return;
  }
  if ( rightCtrlActive && (event->key() == Qt::Key_D)) {
    dumpKeyStrokes = !dumpKeyStrokes;
    printf("User pressed <Right CTRL>+<D>: Debug output is now %sabled...\n", dumpKeyStrokes?"en":"dis");
    return;
  }
  if ( rightCtrlActive && (event->key() == Qt::Key_B)) {
    parentWidget->setBroadcasting(!parentWidget->getBroadcasting());
    return;
  }
  if ( rightCtrlActive && (event->key() == Qt::Key_Q)) {
    parentWidget->close();
  }
  if ( rightCtrlActive && ((event->key() == Qt::Key_Right) || (event->key() == Qt::Key_Up))) {
    tabBar->setCurrentIndex((tabBar->currentIndex() == (tabBar->count()-1))?0:tabBar->currentIndex()+1);
    return;
  }
  if ( rightCtrlActive && ((event->key() == Qt::Key_Left) || (event->key() == Qt::Key_Down))) {
    tabBar->setCurrentIndex((tabBar->currentIndex() == 0)?tabBar->count()-1:tabBar->currentIndex()-1);
    return;
  }
  if (rightCtrlActive) return;
  // Forward if requested...
  char txData[5];
  txData[0] = EVENT_KEYBOARD_PRESS;
  *((int *)(&txData[1])) = keyTranslation(event->nativeScanCode(), event->key(), "PRESS  "); // Was: *((int *)(&txData[1])) = event->key();
  // Send character
  parentWidget->writeSocket((char*)&txData, 5);
}

void sccVirtualDisplay::keyReleaseEvent(QKeyEvent *event) {
  char txData[5];
  // Check for right ctrl key...
  if ((event->key() == Qt::Key_Control) && (event->nativeScanCode() == 0x6d)) {
    rightCtrlActive = false;
    return;
  }
  if (rightCtrlActive) return;
  // Forward if requested...
  txData[0] = EVENT_KEYBOARD_RELEASE;
    *((int *)(&txData[1])) = keyTranslation(event->nativeScanCode(), event->key(), "RELEASE");
  // Send character
  parentWidget->writeSocket((char*)&txData, 5);
}

void sccVirtualDisplay::mousePressEvent ( QMouseEvent * event ) {
  // Forward if requested...
  char txData[5];
  txData[0] = EVENT_MOUSE_PRESS;
  txData[1] = (char)event->buttons();
  parentWidget->writeSocket((char*)&txData, 5);
}

void sccVirtualDisplay::mouseReleaseEvent ( QMouseEvent * event ) {
  // Forward if requested...
  char txData[5];
  txData[0] = EVENT_MOUSE_RELEASE;
  txData[1] = (char)event->buttons();
  parentWidget->writeSocket((char*)&txData, 5);
}

void sccVirtualDisplay::mouseMoveEvent ( QMouseEvent * event ) {
  if (!this->pixmap()) return;
  uInt32 mouseX = event->x();
  uInt32 mouseY = event->y();
  // "Scale" information in case of fullscreen...
  if (fs) {
    mouseX = (int)(((double)displayWidth/this->pixmap()->width()) * (mouseX - 0.5*(width()-this->pixmap()->width())));
    mouseY = (int)(((double)displayHeight/this->pixmap()->height()) * (mouseY - 0.5*(height()-this->pixmap()->height())));
  }
  // Forward if requested...
  char txData[5];
  txData[0] = EVENT_MOUSE_MOVE;
  *((int *)(&txData[1])) = (mouseX << 16) + (mouseY & 0x0ffff);
  parentWidget->writeSocket((char*)&txData, 5);
}

void sccVirtualDisplay::wheelEvent(QWheelEvent *event) {
  // Forward if requested...
  char txData[5];
  txData[0] = EVENT_WHEEL;
  *((int *)(&txData[1])) = event->delta();
  parentWidget->writeSocket((char*)&txData, 5);
}

void sccVirtualDisplay::focusOutEvent ( QFocusEvent * event ) {
  char releaseKeys[4];
  char txData[5];
  txData[0] = EVENT_KEYBOARD_RELEASE;
  releaseKeys[0]=KEY_LEFTCTRL;
  releaseKeys[1]=KEY_LEFTSHIFT;
  releaseKeys[2]=KEY_LEFTALT;
  releaseKeys[3]=KEY_RIGHTALT;
  for (int loop = 0; loop < 4; loop++) {
    *((int *)(&txData[1])) = releaseKeys[loop];
    parentWidget->writeSocket((char*)&txData, 5);
  }
  if (dumpKeyStrokes) {
    printf("focusOutEvent(): Released all control keys...\n");
  }
  event->accept();
}

void sccVirtualDisplay::toggleFullscreen() {
  if (!fs) {
    displayWidget = parent();
    displayWidth = width();
    displayHeight = height();
    displayHorizontalScrollValue = parentWidget->getHorizontalScrollValue();
    displayVerticalScrollValue = parentWidget->getVerticalScrollValue();
    setParent(0);
    setWindowState(Qt::WindowFullScreen);
    resize(QApplication::desktop()->size());
    palet.setColor( backgroundRole(), Qt::black );
    setPalette(palet);
    qApp->processEvents();
    show();
    fs = true;
  } else {
    setParent((QWidget *)displayWidget);
    palet.setColor( backgroundRole(), Qt::black );
    setPalette(palet);
    setWindowState(Qt::WindowActive);
    resize(displayWidth,displayHeight);
    show();
    parentWidget->setHorizontalScrollValue(displayHorizontalScrollValue);
    parentWidget->setVerticalScrollValue(displayVerticalScrollValue);
    setFocus();
    qApp->processEvents();
    fs = false;
  }
}
