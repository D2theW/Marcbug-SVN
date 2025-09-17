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

#include "sccProgressBar.h"

// ================================================================================
// Class sccProgressBar
// ================================================================================

sccProgressBar::sccProgressBar(bool showDialog) {
  isGraphical = showDialog;
  if (isGraphical) {
    qtBar = new QProgressDialog();
    asciiBar = NULL;
  } else {
    qtBar = NULL;
    asciiBar = new asciiProgress();
  }
}

sccProgressBar::~sccProgressBar() {
  if (isGraphical) {
    delete qtBar;
  } else {
    delete asciiBar;
  }
}

void sccProgressBar::setLabelText(QString text) {
  if (isGraphical) {
    qtBar->setLabelText(text);
  } else {
    asciiBar->setText(text);
  }
}

void sccProgressBar::setWindowTitle(QString text) {
  if (isGraphical) {
    qtBar->setWindowTitle(text);
  } else {
    // Ignore...
  }
}


void sccProgressBar::setMinimum(uInt64 min) {
  if (isGraphical) {
    qtBar->setMinimum(min);
  } else {
    asciiBar->setMinimum(min);
  }
}


void sccProgressBar::setMaximum(uInt64 max) {
  if (isGraphical) {
    qtBar->setMaximum(max);
  } else {
    asciiBar->setMaximum(max);
  }
}


void sccProgressBar::setCancelButtonText(QString text) {
  if (isGraphical) {
    qtBar->setCancelButtonText(text);
  } else {
    // Ignore
  }
}


void sccProgressBar::setModal(bool enable) {
  if (isGraphical) {
    qtBar->setModal(enable);
  } else {
    // Ignore
  }
}


void sccProgressBar::show() {
  if (isGraphical) {
    qtBar->show();
  } else {
    asciiBar->show();
  }
}


void sccProgressBar::hide() {
  if (isGraphical) {
    qtBar->hide();
  } else {
    asciiBar->hide();
  }
}

void sccProgressBar::reset() {
  if (isGraphical) {
    qtBar->reset();
  } else {
    asciiBar->hide();
  }
}

void  sccProgressBar::cancel() {
  if (isGraphical) {
    qtBar->cancel();
  } else {
    asciiBar->hide();
  }
}

void sccProgressBar::setValue(uInt64 value) {
  if (isGraphical) {
    qtBar->setValue(value);
  } else {
    asciiBar->setValue(value);
  }
}

bool sccProgressBar::wasCanceled() {
  if (isGraphical) {
    return qtBar->wasCanceled();
  } else {
    return asciiBar->wasCanceled();
  }
}

bool sccProgressBar::isVisible() {
  if (isGraphical) {
    return qtBar->isVisible();
  } else {
    return true;
  }
}
