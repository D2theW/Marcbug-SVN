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

#include "coreMaskDialog.h"

// QMessageBox::information(this, "Info", "Messagetext...\n", QMessageBox::Ok);

// ================================================================================
// Class myQCheckBox
// ================================================================================
// Customized checkbox class with explicit "check" and "uncheck" slots...
myQCheckBox::myQCheckBox ( const QString & text, QWidget * parent ) : QCheckBox ( text, parent ) {
  return;
}

void myQCheckBox::check() {
  setChecked(true);
}

void myQCheckBox::uncheck() {
  setChecked(false);
}

// ================================================================================
// Class coreMaskDialog
// ================================================================================

coreMaskDialog::coreMaskDialog(bool allowZeroSelection) {
  // Initialize mask...
  this->allowZeroSelection = allowZeroSelection;
  enableAll();
  mainLayout = NULL;
  dialogOpen = false;
  dialogSuccess = false;
}

coreMaskDialog::~coreMaskDialog() {
  if (mainLayout) delete mainLayout;
}

void coreMaskDialog::setEnabled(int x, int y, int core, bool enabled) {
  coreMask::setEnabled(x, y, core, enabled);
  if (mainLayout) {
    validPIDCheck[PID(x, y, core)]->setChecked(enabled);
  }
}

bool coreMaskDialog::getZeroSelectionPolicy(){
  return allowZeroSelection;
}

void coreMaskDialog::setZeroSelectionPolicy(bool allowZeroSelection) {
  this->allowZeroSelection = allowZeroSelection;
}

bool coreMaskDialog::selectionDialog(QString title, QString status) {
  // Create Dialog when opened first time!
  if (!mainLayout) {
    setWindowFlags(Qt::SubWindow);
    tileLayout = new QGridLayout;
    // Add x-labels
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      xLabel[loop_x] = new QLabel();
      xLabel[loop_x]->setText(" x="+QString::number(loop_x));
      xButtonCheck[loop_x] = new QPushButton();
      xButtonCheck[loop_x]->setMaximumSize(30, 30);
      xButtonUncheck[loop_x] = new QPushButton();
      xButtonUncheck[loop_x]->setMaximumSize(30, 30);
      xLayout[loop_x] = new QVBoxLayout();
      xLayout[loop_x]->addWidget(xLabel[loop_x]);
      xLayout[loop_x]->addWidget(xButtonCheck[loop_x]);
      xLayout[loop_x]->addWidget(xButtonUncheck[loop_x]);
      xLayout[loop_x]->setAlignment(Qt::AlignHCenter);
      tileLayout->addLayout(xLayout[loop_x], NUM_ROWS, loop_x+1);
      xButtonCheck[loop_x]->setText("+");
      xButtonCheck[loop_x]->setToolTip("Enable all cores in this column");
      xButtonUncheck[loop_x]->setText("-");
      xButtonUncheck[loop_x]->setToolTip("Disable all cores in this column");
    }
    // Add y-labels
    for (int grid_y=0; grid_y < NUM_ROWS; grid_y++) {
      int loop_y = NUM_ROWS - grid_y - 1;
      yLabel[loop_y] = new QLabel();
      yLabel[loop_y]->setText(" y="+QString::number(loop_y));
      yButtonCheck[loop_y] = new QPushButton();
      yButtonCheck[loop_y]->setMaximumSize(30, 30);
      yButtonUncheck[loop_y] = new QPushButton();
      yButtonUncheck[loop_y]->setMaximumSize(30, 30);
      yLayout[loop_y] = new QHBoxLayout();
      yLayout[loop_y]->addWidget(yButtonCheck[loop_y]);
      yLayout[loop_y]->addWidget(yButtonUncheck[loop_y]);
      yLayout[loop_y]->addWidget(yLabel[loop_y]);
      yLayout[loop_y]->setAlignment(Qt::AlignVCenter);
      tileLayout->addLayout(yLayout[loop_y], grid_y, 0);
      yButtonCheck[loop_y]->setText("+");
      yButtonCheck[loop_y]->setToolTip("Enable all cores in this row");
      yButtonUncheck[loop_y]->setText("-");
      yButtonUncheck[loop_y]->setToolTip("Disable all cores in this row");
    }
    // Add two checkboxes per tile and initialize them with current validPID configuration...
    for (int grid_y=0; grid_y < NUM_ROWS; grid_y++) {
      int loop_y = NUM_ROWS - grid_y - 1;
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        coreLayout[TID(loop_x, loop_y)] = new QGridLayout();
        for (int grid_core=0; grid_core < NUM_CORES; grid_core++) {
          int loop_core = NUM_CORES - grid_core - 1;
          validPIDCheck[PID(loop_x, loop_y, loop_core)] = new myQCheckBox("Core"+QString::number(loop_core));
          validPIDCheck[PID(loop_x, loop_y, loop_core)]->setChecked(currentMask[PID(loop_x, loop_y, loop_core)]);
          validPIDCheck[PID(loop_x, loop_y, loop_core)]->setToolTip("Core " + QString::number(loop_core) + " of Tile x=" + QString::number(loop_x) + ", y=" + QString::number(loop_y) + ". Processor-ID (PID) = " + messageHandler::toString(PID(loop_x, loop_y, loop_core), 16, 2) + " (" + messageHandler::toString(PID(loop_x, loop_y, loop_core), 10, 2) +  " dec)...");
          coreLayout[TID(loop_x, loop_y)]->addWidget(validPIDCheck[PID(loop_x, loop_y, loop_core)], grid_core, 0);
          connect(validPIDCheck[PID(loop_x, loop_y, loop_core)], SIGNAL(stateChanged(int)), this, SLOT(maskChanged()));
        }
        tileLayout->addLayout(coreLayout[TID(loop_x, loop_y)], grid_y, loop_x+1);
      }
    }
    // Add okay and cancel buttons...
    confirmButtons = new QDialogButtonBox();
    okayButton = new QPushButton("Okay");
    okayButton->setToolTip("Apply current selection...");
    cancelButton = new QPushButton("Cancel");
    cancelButton->setToolTip("Discard current selection...");
    setButton = new QPushButton("All");
    setButton->setToolTip("Check all checkboxes...");
    resetButton = new QPushButton("None");
    resetButton->setToolTip("Uncheck all checkboxes...");
    toggleButton = new QPushButton("Toggle selection");
    toggleButton->setToolTip("Toggle (invert) all checkboxes...");
    confirmButtons->addButton(setButton, QDialogButtonBox::ActionRole);
    confirmButtons->addButton(resetButton, QDialogButtonBox::ActionRole);
    confirmButtons->addButton(toggleButton, QDialogButtonBox::ActionRole);
    confirmButtons->addButton(okayButton, QDialogButtonBox::ActionRole);
    confirmButtons->addButton(cancelButton, QDialogButtonBox::ActionRole);
    connect(okayButton, SIGNAL(clicked()), this, SLOT(selectOk()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(selectCancel()));
    connect(this, SIGNAL(rejected()), this, SLOT(selectCancel()));
    for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
      for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
        for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
          connect(setButton, SIGNAL(clicked()), validPIDCheck[PID(loop_x, loop_y, loop_core)], SLOT(check()));
          connect(resetButton, SIGNAL(clicked()), validPIDCheck[PID(loop_x, loop_y, loop_core)], SLOT(uncheck()));
          connect(toggleButton, SIGNAL(clicked()), validPIDCheck[PID(loop_x, loop_y, loop_core)], SLOT(toggle()));
          connect(xButtonCheck[loop_x], SIGNAL(clicked()), validPIDCheck[PID(loop_x, loop_y, loop_core)], SLOT(check()));
          connect(xButtonUncheck[loop_x], SIGNAL(clicked()), validPIDCheck[PID(loop_x, loop_y, loop_core)], SLOT(uncheck()));
          connect(yButtonCheck[loop_y], SIGNAL(clicked()), validPIDCheck[PID(loop_x, loop_y, loop_core)], SLOT(check()));
          connect(yButtonUncheck[loop_y], SIGNAL(clicked()), validPIDCheck[PID(loop_x, loop_y, loop_core)], SLOT(uncheck()));
        }
      }
    }
    // Configure and start Dialog
    mainLayout = new QVBoxLayout;
    mainLayout->addLayout(tileLayout);
    mainLayout->addSpacing(20);
    mainLayout->addWidget(confirmButtons, 0, Qt::AlignHCenter);
    mainLayout->addWidget(&statusMessage, 0, Qt::AlignHCenter);
    setLayout(mainLayout);
  }

  // Initialization
  setWindowTitle(title);
  defaultStatus = status;
  maskChanged();

  // Show widget
  show();
  raise();
  activateWindow();

  // Center dialog...
  setGeometry((int)(QApplication::desktop()->width() - width()) / 2, (int)(QApplication::desktop()->height() - height()) / 2,width(), height());

  // Lock window size...
  setMaximumSize(size());
  setMinimumSize(size());

  // Wait until user closes the dialog...
  dialogOpen = true;
  while(dialogOpen) {
    qApp->processEvents();
  }

  return dialogSuccess;
}

bool coreMaskDialog::dialogIsOpen() {
  return dialogOpen;
}

void coreMaskDialog::abortDialog() {
  hide();
  dialogSuccess = false;
  dialogOpen = false;
}

// =======
//  SLOTS
// =======

// Apply user settings
void coreMaskDialog::selectOk() {
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        currentMask[PID(loop_x, loop_y, loop_core)]=validPIDCheck[PID(loop_x, loop_y, loop_core)]->isChecked();
      }
    }
  }
  hide();
  dialogSuccess = true;
  dialogOpen = false;
}

// Discard user settings
void coreMaskDialog::selectCancel() {
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        validPIDCheck[PID(loop_x, loop_y, loop_core)]->setChecked(currentMask[PID(loop_x, loop_y, loop_core)]);
      }
    }
  }
  hide();
  dialogSuccess = false;
  dialogOpen = false;
}

// Disable okay button if no core is selected!
void coreMaskDialog::maskChanged() {
  bool selection = false;
  for (int loop_y=0; loop_y < NUM_ROWS; loop_y++) {
    for (int loop_x=0; loop_x < NUM_COLS; loop_x++) {
      for (int loop_core=0; loop_core < NUM_CORES; loop_core++) {
        if (validPIDCheck[PID(loop_x, loop_y, loop_core)]->isChecked()) selection = true;
      }
    }
  }
  if (!selection && !allowZeroSelection) {
    okayButton->setEnabled(false);
    okayButton->setToolTip("Make sure to select at least one core...");
    statusMessage.setText("<font color=red>Make sure to select at least one core! Okay button is disabled for now...</font>");
  } else {
    okayButton->setEnabled(true);
    okayButton->setToolTip("Apply current selection...");
    statusMessage.setText(defaultStatus);
  }
}
