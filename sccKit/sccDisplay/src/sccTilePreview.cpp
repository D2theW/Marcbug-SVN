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
#include "sccTilePreview.h"

// Color defines for Frames...
#define SELECTED_COLOR Qt::blue
#define ACTIVE_COLOR Qt::white
#define INACTIVE_COLOR  0x303030
#define BACKGROUND_COLOR Qt::black

sccTilePreview::sccTilePreview(sccDisplayWidget* parentWidget) {
  // Init vars
  fs = false;
  rightCtrlActive=false;
  this->parentWidget = parentWidget;

  // Create tile grid
  individualLayout = new QGridLayout;
  for (int x=0; x<NUM_COLS; x++) {
    for (int y=0; y<NUM_ROWS; y++) {
      for (int core=0; core<NUM_CORES; core++) {
        tile[PID(x,y,core)] = new QLabel();
        tile[PID(x,y,core)]->setAlignment(Qt::AlignCenter);
        tile[PID(x,y,core)]->setMinimumSize(width()/NUM_COLS-2,height()/NUM_ROWS/2-2);
        tile[PID(x,y,core)]->setMaximumSize(width()/NUM_COLS-2,height()/NUM_ROWS/2-2);
        tile[PID(x,y,core)]->setFrameShape(QFrame::Box);
        tile[PID(x,y,core)]->setLineWidth(1);
        palet.setColor(tile[PID(x,y,core)]->backgroundRole(), BACKGROUND_COLOR );
        if (parentWidget->coreAvailable(x,y,core)) {
          tile[PID(x,y,core)]->setToolTip(("Core " + QString::number(core) + " on Tile x=" + QString::number(x) + ", y=" + QString::number(y) + ". Processor-ID (PID) = " + messageHandler::toString(PID(x, y, core), 16, 2) + " (" + messageHandler::toString(PID(x, y, core), 10, 2) +  " dec)..."));
        }
        individualLayout->addWidget(tile[PID(x,y,core)], NUM_ROWS*NUM_CORES-y*NUM_CORES-core, x);
      }
    }
  }
  setLayout(individualLayout);

  // Define widget behaviour
  setMouseTracking(true);
  setFocusPolicy(Qt::WheelFocus);
}

sccTilePreview::~sccTilePreview() {
  delete individualLayout;
}

void sccTilePreview::refresh() {
  for (int x=0; x<NUM_COLS; x++) {
    for (int y=0; y<NUM_ROWS; y++) {
      for (int core=0; core<NUM_CORES; core++) {
        tile[PID(x,y,core)]->setMinimumSize(width()/NUM_COLS-2,height()/NUM_ROWS/2-2);
        tile[PID(x,y,core)]->setMaximumSize(width()/NUM_COLS-2,height()/NUM_ROWS/2-2);
        tile[PID(x,y,core)]->setLineWidth(1);
      }
    }
  }
}

bool sccTilePreview::event( QEvent *event ) {
  if ( event->type() == QEvent::KeyPress ) {
      QKeyEvent *keyboardEvent = (QKeyEvent *)event;
    if ( keyboardEvent->key() == Qt::Key_Tab ) {
      keyboardEvent->accept();
      return TRUE;
    }
  }
  return QWidget::event( event );
}

void sccTilePreview::keyPressEvent(QKeyEvent *event) {
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
<Right CTRL>+<P>: Leave preview display\n\
<Right CTRL>+<T>: Toggle perfmeter...\n\
<Right CTRL>+<H>: Display this help\n\
         ", QMessageBox::Ok);
    rightCtrlActive = false;
    return;
  }
  if ( rightCtrlActive && (event->key() == Qt::Key_F)) {
    toggleFullscreen();
    parentWidget->setWasFullscreen(fs);
    return;
  }
  if ( rightCtrlActive && (event->key() == Qt::Key_P)) {
    // Switch to selected core...
    if (parentWidget->coreAvailable(X_PID(highlightedPid),Y_PID(highlightedPid),Z_PID(highlightedPid))) {
      parentWidget->showDisplay(fs, X_PID(highlightedPid), Y_PID(highlightedPid), Z_PID(highlightedPid));
    }
    return;
  }
  if ( rightCtrlActive && (event->key() == Qt::Key_T)) {
    parentWidget->togglePerfmeter();
    return;
  }
  if ( rightCtrlActive && (event->key() == Qt::Key_Q)) {
    parentWidget->close();
  }
  if (event->key() == Qt::Key_Right) {
    int currentHighlightedPid = highlightedPid;
    if (X_PID(highlightedPid)<(NUM_COLS-1)) {
      highlightedPid=PID(X_PID(highlightedPid)+1, Y_PID(highlightedPid), Z_PID(highlightedPid));
    } else {
      highlightedPid=PID(0, Y_PID(highlightedPid), Z_PID(highlightedPid));
    }
    if (!parentWidget->coreAvailable(X_PID(highlightedPid),Y_PID(highlightedPid),Z_PID(highlightedPid))) {
      highlightedPid = parentWidget->nextCorePid(currentHighlightedPid);
    }
    highlightTile(highlightedPid);
    return;
  }
  if (event->key() == Qt::Key_Left) {
    int currentHighlightedPid = highlightedPid;
    if (X_PID(highlightedPid)>0) {
      highlightedPid=PID(X_PID(highlightedPid)-1, Y_PID(highlightedPid), Z_PID(highlightedPid));
    } else {
      highlightedPid=PID(NUM_COLS-1, Y_PID(highlightedPid), Z_PID(highlightedPid));
    }
    if (!parentWidget->coreAvailable(X_PID(highlightedPid),Y_PID(highlightedPid),Z_PID(highlightedPid))) {
      highlightedPid = parentWidget->prevCorePid(currentHighlightedPid);
    }
    highlightTile(highlightedPid);
    return;
  }
  if (event->key() == Qt::Key_Up) {
    int currentHighlightedPid = highlightedPid;
    if (Z_PID(highlightedPid)==0) {
      highlightedPid=PID(X_PID(highlightedPid), Y_PID(highlightedPid), 1);
    } else if (Y_PID(highlightedPid)<(NUM_ROWS-1)) {
      highlightedPid=PID(X_PID(highlightedPid), Y_PID(highlightedPid)+1, 0);
    } else {
      highlightedPid=PID(X_PID(highlightedPid), 0, 0);
    }
    if (!parentWidget->coreAvailable(X_PID(highlightedPid),Y_PID(highlightedPid),Z_PID(highlightedPid))) {
      highlightedPid = parentWidget->nextCorePid(currentHighlightedPid);
    }
    highlightTile(highlightedPid);
    return;
  }
  if (event->key() == Qt::Key_Down) {
    int currentHighlightedPid = highlightedPid;
    if (Z_PID(highlightedPid)==1) {
      highlightedPid=PID(X_PID(highlightedPid), Y_PID(highlightedPid), 0);
    } else if (Y_PID(highlightedPid)>0) {
      highlightedPid=PID(X_PID(highlightedPid), Y_PID(highlightedPid)-1, 1);
    } else {
      highlightedPid=PID(X_PID(highlightedPid), NUM_ROWS-1, 1);
    }
    if (!parentWidget->coreAvailable(X_PID(highlightedPid),Y_PID(highlightedPid),Z_PID(highlightedPid))) {
      highlightedPid = parentWidget->prevCorePid(currentHighlightedPid);
    }
    highlightTile(highlightedPid);
    return;
  }
  if (event->key() == Qt::Key_Return) {
    // Switch to selected core...
    if (parentWidget->coreAvailable(X_PID(highlightedPid),Y_PID(highlightedPid),Z_PID(highlightedPid))) {
      parentWidget->showDisplay(fs, X_PID(highlightedPid), Y_PID(highlightedPid), Z_PID(highlightedPid));
    }
  }
}

void sccTilePreview::keyReleaseEvent(QKeyEvent *event) {
  // Check for right ctrl key...
  if ((event->key() == Qt::Key_Control) && (event->nativeScanCode() == 0x6d)) {
    rightCtrlActive = false;
    return;
  }
}

void sccTilePreview::mousePressEvent ( QMouseEvent * event ) {
  // Calculate Core coordinates from Mouse pointer coordinates
  int x = event->x()/(width()/NUM_COLS);
  int y = (height()-event->y())/(height()/NUM_ROWS/2);
  int core = y%2;
  y/=2;
  // Switch to selected core...
  if (parentWidget->coreAvailable(x,y,core)) {
    parentWidget->showDisplay(fs, x, y, core);
  }
}

void sccTilePreview::setPixmap(QImage* image, int pid) {
  tile[pid]->setPixmap(QPixmap::fromImage(*image).scaled(width()/NUM_COLS,height()/NUM_ROWS/2, Qt::KeepAspectRatio, Qt::FastTransformation ));
}

void sccTilePreview::toggleFullscreen() {
  if (!fs) {
    displayWidget = parent();
    displayWidth = width();
    displayHeight = height();
    displayHorizontalScrollValue = parentWidget->getHorizontalScrollValue();
    displayVerticalScrollValue = parentWidget->getVerticalScrollValue();
    setParent(0);
    setWindowState(Qt::WindowFullScreen);
    setMaximumSize(QApplication::desktop()->width(),QApplication::desktop()->height());
    resize(QApplication::desktop()->size());
    qApp->processEvents();
    show();
    // Redefine max size of Labels...
    for (int x=0; x<NUM_COLS; x++) {
      for (int y=0; y<NUM_ROWS; y++) {
        for (int core=0; core<NUM_CORES; core++) {
          tile[PID(x,y,core)]->setMinimumSize(width()/NUM_COLS-10,height()/NUM_ROWS/2-10);
          tile[PID(x,y,core)]->setMaximumSize(width()/NUM_COLS-10,height()/NUM_ROWS/2-10);
        }
      }
    }
    highlightTile(parentWidget->currentCorePid());
    fs = true;
  } else {
    setParent((QWidget *)displayWidget);
    setWindowState(Qt::WindowActive);
    // Redefine max size of Labels...
    for (int x=0; x<NUM_COLS; x++) {
      for (int y=0; y<NUM_ROWS; y++) {
        for (int core=0; core<NUM_CORES; core++) {
          tile[PID(x,y,core)]->setMinimumSize(displayWidth/NUM_COLS-2,displayHeight/NUM_ROWS/2-2);
          tile[PID(x,y,core)]->setMaximumSize(displayWidth/NUM_COLS-2,displayHeight/NUM_ROWS/2-2);
        }
      }
    }
    setMinimumSize(displayWidth,displayHeight);
    setMaximumSize(displayWidth,displayHeight);
    parentWidget->setHorizontalScrollValue(displayHorizontalScrollValue);
    parentWidget->setVerticalScrollValue(displayVerticalScrollValue);
    setFocus();
    show();
    qApp->processEvents();
    highlightTile(parentWidget->currentCorePid());
    fs = false;
  }
}

void sccTilePreview::highlightTile(int pid) {
  // Define color role...
  palet.setColor( backgroundRole(), BACKGROUND_COLOR );
  setPalette(palet);

  for (int x=0; x<NUM_COLS; x++) {
    for (int y=0; y<NUM_ROWS; y++) {
      for (int core=0; core<NUM_CORES; core++) {
        tile[PID(x,y,core)]->setLineWidth(1);
        palet.setColor(tile[PID(x,y,core)]->backgroundRole(), BACKGROUND_COLOR );
        if (parentWidget->coreAvailable(x,y,core)) {
          palet.setColor(tile[PID(x,y,core)]->foregroundRole(), ACTIVE_COLOR);
          tile[PID(x,y,core)]->setPalette(palet);
        } else {
          palet.setColor(tile[PID(x,y,core)]->foregroundRole(), INACTIVE_COLOR);
          tile[PID(x,y,core)]->setPalette(palet);
        }
      }
    }
  }
  tile[pid]->setLineWidth(5);
  palet.setColor(tile[pid]->foregroundRole(), SELECTED_COLOR);
  tile[pid]->setPalette(QPalette::QPalette(palet));
  highlightedPid = pid;
}
