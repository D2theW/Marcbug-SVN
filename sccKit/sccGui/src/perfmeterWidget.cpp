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

#include "perfmeterWidget.h"

// QMessageBox::information(this, "Info", "Messagetext...\n", QMessageBox::Ok);

// ================================================================================
// Class viewCPU
// ================================================================================

viewCPU::viewCPU (QWidget *parent, QString name) {
  cpuViewerName=name;
  parentFrm=parent;
  horScale=PREC;
  verScale=PREC;
  cpuPerf = 0;
  perfSamples = 0;
  setAutoFillBackground(true);
  setBackgroundRole(QPalette::Shadow);
  // setBackgroundRole(QPalette::Window);
  // initializing performance values in list
  for (int i=0; i<MAX_INTERVAL; i++)
    pmVal.append(0);
  timer = new QTimer (this);
  connect(timer, SIGNAL(timeout()), this, SLOT(updateView()));
  timer->start(TIME_INTERVAL);
  timeOffset=0;
}

viewCPU::~viewCPU() {
  delete timer;
}

void viewCPU::paintEvent(QPaintEvent * /* event */) {
  QPainter painter(this);
  drawGrid(&painter);
  displayId(&painter);
  drawPerformance(&painter);
}

void viewCPU::displayId(QPainter *painter) {
  painter->setPen(Qt::green);
  painter->drawText ( 35,15,cpuViewerName);

}

void viewCPU::drawGrid(QPainter *painter) {
  int stepH=(int)(GRID_STEP_HOR * horScale/PREC);
  //int stepV=(int)(GRID_STEP_VER * verScale/PREC);
  int stepV=height()/11;

  int vWidth =width();
  int vHeight=height();

  painter->setPen(Qt::darkGreen);

  int tOffset=int(timeOffset*horScale/PREC) % stepH;

  //for (int y=stepV; y<=vHeight; y=y+stepV)
  int y=0;
  for (y=vHeight; y>=stepV; y=y-stepV)
    painter->drawLine (0,y, vWidth,y);

  for (int x=0; x<=vWidth+stepH; x=x+stepH)
    painter->drawLine (x-tOffset,y+stepV, x-tOffset,vHeight);

}

void viewCPU::drawPerformance(QPainter *painter) {
  //int vWidth =width();
  int vHeight=height();
  int stepV=vHeight/11; //10 x stepV represents 100%

  painter->setPen(Qt::green);
  for (int i=0; i<MAX_INTERVAL-1; i++)
    painter->drawLine ((int)((i+1)*horScale/PREC),(int)(vHeight-stepV*pmVal.at(i)/10)-1,
                       (int)(horScale*(i+2)/PREC),(int)(vHeight-stepV*pmVal.at(i+1)/10)-1);

}

void viewCPU::updateView() {
  timeOffset++;
  timeOffset=timeOffset % MAX_INTERVAL;
  updatePerformanceValue();
  repaint();
  timer->start(TIME_INTERVAL);
}

void viewCPU::updatePerformanceValue() {

  //Managing FIFO with Qlist
  if (pmVal.size()>MAX_INTERVAL)
    pmVal.removeFirst();

  //At this point the hardware has to be read in order to monitor performance
  pmVal.append(cpuPerf);
  perfSamples = 0;
}

void viewCPU::resizeEvent(QResizeEvent * /* event */) {
  horScale=(double)(PREC*width() / MAX_INTERVAL);
}

int viewCPU::value() {
  return cpuPerf;
}

void viewCPU::setValue(int cpuPerf) {
  this->cpuPerf=(perfSamples*this->cpuPerf+cpuPerf) / ++perfSamples;
}

// ================================================================================
// perfmeter widget class...
// ================================================================================
perfmeterWidget::perfmeterWidget(int mesh_x, int mesh_y, int tile_cores, sccExtApi *sccAccess, QWidget *parent) : QWidget(parent) {
  this->mesh_x = mesh_x;
  this->mesh_y = mesh_y;
  this->tile_cores = tile_cores;
  this->sccAccess = sccAccess;

  setWindowTitle("SCC performance meter");
  fgPalette = new QPalette();

  // Setup individual Group
  individualLayout = new QGridLayout;
  for (int x=0; x<NUM_COLS; x++) {
    for (int y=0; y<NUM_ROWS; y++) {
      for (int core=0; core<NUM_CORES; core++) {
        individualDial[x][y][core] = new QDial();
        individualDial[x][y][core]->setNotchesVisible(true);
        individualDial[x][y][core]->setEnabled(false);
        individualDial[x][y][core]->setToolTip(("Core " + QString::number(core) + " on Tile x=" + QString::number(x) + ", y=" + QString::number(y) + ". Processor-ID (PID) = " + messageHandler::toString(PID(x, y, core), 16, 2) + " (" + messageHandler::toString(PID(x, y, core), 10, 2) +  " dec)..."));
        individualPerf[x][y][core] = new viewCPU(this);
        individualPerf[x][y][core]->setToolTip(("Core " + QString::number(core) + " on Tile x=" + QString::number(x) + ", y=" + QString::number(y) + ". Processor-ID (PID) = " + messageHandler::toString(PID(x, y, core), 16, 2) + " (" + messageHandler::toString(PID(x, y, core), 10, 2) +  " dec)..."));
        individualLayout->addWidget(individualPerf[x][y][core], NUM_ROWS*NUM_CORES-y*NUM_CORES-core, x); // Needs to be first widget in order to allow "overlay mode"
        individualLayout->addWidget(individualDial[x][y][core], NUM_ROWS*NUM_CORES-y*NUM_CORES-core, x); // Needs to be second widget in order to allow "overlay mode"
        newValue[x][y][core] = 0;
        currentValue[x][y][core] = 0;
      }
    }
  }
  individualGroup = new QGroupBox("Individual CPU usage...");
  individualGroup->setLayout(individualLayout);
  individualGroup->setMaximumSize(600, 400);
  individualGroup->setMinimumSize(600, 400);

  // Setup config Group
  configButtonLayout = new QHBoxLayout;
  cockpitButton = new QRadioButton("Cockpit style");
  taskmgrButton = new QRadioButton("Taskmanager style");
  cockpitTaskmgrButton = new QRadioButton("Combined (overlay)");
  configButtonLayout->addWidget(cockpitButton);
  configButtonLayout->addWidget(taskmgrButton);
  configButtonLayout->addWidget(cockpitTaskmgrButton);
  configButtonGroup = new QButtonGroup();
  configButtonGroup->addButton(cockpitButton);
  configButtonGroup->setId(cockpitButton, COCKPIT);
  configButtonGroup->addButton(taskmgrButton);
  configButtonGroup->setId(taskmgrButton, TASKMGR);
  configButtonGroup->addButton(cockpitTaskmgrButton);
  configButtonGroup->setId(cockpitTaskmgrButton, COCKPIT_TASKMGR);
  configGroup = new QGroupBox("Set style of individual CPU usage section");
  configGroup->setLayout(configButtonLayout);
  connect(configButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(setStyle(int)));
  setStyle(DEFAULT_STYLE);

  // Setup total data Group
  totalLayout = new QHBoxLayout;
  powerUsageLayout = new QVBoxLayout;
  totalDial = new QDial();
  totalDial->setNotchesVisible(true);
  totalDial->setEnabled(false);
  fgPalette->setColor(QPalette::Button, Qt::green);
  totalDial->setPalette(*fgPalette);
  totalDial->setToolTip("Over-all CPU usage of enabled cores...");
  totalPerf = new viewCPU(this,"Over-all CPU usage over time");
  totalPerf->setToolTip("Over-all CPU usage of enabled cores...");
  powerLCD = new QLCDNumber();
  QPalette palette1;
  QBrush brush2(Qt::darkGreen);
  brush2.setStyle(Qt::SolidPattern);
  palette1.setBrush(QPalette::Active, QPalette::WindowText, brush2);
  palette1.setBrush(QPalette::Inactive, QPalette::WindowText, brush2);
  QBrush brush3(QColor(118, 116, 108, 255));
  brush3.setStyle(Qt::SolidPattern);
  palette1.setBrush(QPalette::Disabled, QPalette::WindowText, brush3);
  powerLCD->setPalette(palette1);
  powerLCD->setFrameShape(QFrame::NoFrame);
  powerLCD->setFrameShadow(QFrame::Sunken);
  // powerLCD->setSmallDecimalPoint(true);
  powerLCD->setNumDigits(6);
  powerLCD->setSegmentStyle(QLCDNumber::Flat);
  powerLayout = new QHBoxLayout();
  powerLayout->addWidget(powerLCD);
  powerGroup = new QGroupBox("Current power consumption (in Watt)");
  powerGroup->setLayout(powerLayout);
  powerUsageLayout->addWidget(totalDial);
  powerUsageLayout->addWidget(powerGroup);
  totalLayout->addLayout(powerUsageLayout);
  totalLayout->addWidget(totalPerf);
  totalDialGroup = new QGroupBox("Over-all CPU usage of enabled cores...");
  totalDialGroup->setMaximumSize(600, 200);
  totalDialGroup->setMinimumSize(totalDialGroup->maximumSize());
  totalDialGroup->setLayout(totalLayout);

  mainLayout=new QVBoxLayout;
  mainLayout->addWidget(individualGroup);
  mainLayout->addWidget(totalDialGroup);
  mainLayout->addWidget(configGroup);
  setLayout(mainLayout);

  // Init widget
  initialize();

  // Create timer
  timer = new QTimer (this);
  connect(timer, SIGNAL(timeout()), this, SLOT(updatePerformanceValues()));
}

perfmeterWidget::~perfmeterWidget() {
  delete fgPalette;
  delete individualLayout;
  for (int x=0; x<NUM_COLS; x++) {
    for (int y=0; y<NUM_ROWS; y++) {
      for (int core=0; core<NUM_CORES; core++) {
        delete individualDial[x][y][core];
        delete individualPerf[x][y][core];
      }
    }
  }

  delete configButtonGroup;
  delete taskmgrButton;
  delete cockpitButton;
  delete configButtonLayout;

  delete individualGroup;
  delete totalLayout;
  delete totalDial;
  delete totalPerf;
  delete totalDialGroup;
  delete mainLayout;
  delete timer;
}

void perfmeterWidget::initialize() {
  powerLCD->setEnabled(false);
  powerLCD->display(0.0);
  fgPalette->setColor(QPalette::Button, Qt::gray);
  totalDial->setPalette(*fgPalette);
  totalDial->setValue(0);
  totalPerf->setValue(0);
  for (int x=0; x<NUM_COLS; x++) {
    for (int y=0; y<NUM_ROWS; y++) {
      for (int core=0; core<NUM_CORES; core++) {
        individualDial[x][y][core]->setPalette(*fgPalette);
        individualDial[x][y][core]->setValue(0);
        individualPerf[x][y][core]->setValue(0);
      }
    }
  }
}

void perfmeterWidget::setValue(int x, int y, int core, int value) {
  if ((x >= mesh_x) || (y >= mesh_y) || (core >= tile_cores)) return;
  if (value < 0) {
    newValue[x][y][core] = -1;
  } else if (value > 100) {
    newValue[x][y][core] = -1;
  } else {
    newValue[x][y][core] = value;
  }
  updatePerformanceValues();
}

void perfmeterWidget::updatePower(double power) {
  if ((int)power != powerLCD->value()) {
    powerLCD->display(power);
    powerLCD->setEnabled(true);
  }
}

void perfmeterWidget::setStyle(int layout) {
  // Update radio buttons
  if (layout==COCKPIT) {
    cockpitButton->setChecked(true);
  } else if (layout==TASKMGR) {
    taskmgrButton->setChecked(true);
  } else if (layout==COCKPIT_TASKMGR) {
    cockpitTaskmgrButton->setChecked(true);
  }
  // Update widget style
  for (int x=0; x<NUM_COLS; x++) {
    for (int y=0; y<NUM_ROWS; y++) {
      for (int core=0; core<NUM_CORES; core++) {
        if (layout==COCKPIT) {
          individualDial[x][y][core]->setVisible(true);
          individualPerf[x][y][core]->setVisible(false);
        } else if (layout==TASKMGR) {
          individualDial[x][y][core]->setVisible(false);
          individualPerf[x][y][core]->setVisible(true);
        } else if (layout==COCKPIT_TASKMGR) {
          individualPerf[x][y][core]->setVisible(true);
          individualDial[x][y][core]->setVisible(true);
        }
      }
    }
  }
}

void perfmeterWidget::updatePerformanceValues() {
  bool restartTimer = false;
  int totalValue = 0;
  bool allGrayed = true;
  for (int x=0; x<this->mesh_x; x++) {
    for (int y=0; y<this->mesh_y; y++) {
      for (int core=0; core<this->tile_cores; core++) {
        // Update individual values...
        if (currentValue[x][y][core] < newValue[x][y][core]) {
          currentValue[x][y][core]++;
        } else if (currentValue[x][y][core] > newValue[x][y][core]) {
          currentValue[x][y][core]--;
        }
        if (currentValue[x][y][core] == -1) {
          fgPalette->setColor(QPalette::Button, Qt::gray);
          currentValue[x][y][core] = 0;
        } else {
          fgPalette->setColor(QPalette::Button, QColor::fromRgb(((currentValue[x][y][core]<50)?5*currentValue[x][y][core]:255), 2*(100-currentValue[x][y][core]), 0));
          allGrayed = false;
        }
        individualDial[x][y][core]->setPalette(*fgPalette);
        individualDial[x][y][core]->setValue(currentValue[x][y][core]);
        if (currentValue[x][y][core] != newValue[x][y][core]) restartTimer = true;
        individualPerf[x][y][core]->setValue(currentValue[x][y][core]); // Always update because of periodical refresh...
        // Prepare total value
        totalValue+=currentValue[x][y][core];
      }
    }
  }
  // Update total value
  totalValue /= (mesh_x*mesh_y*tile_cores);
  if (allGrayed) {
    fgPalette->setColor(QPalette::Button, Qt::gray);
  } else  {
    fgPalette->setColor(QPalette::Button, QColor::fromRgb(((totalValue<50)?5*totalValue:255), 2*(100-totalValue), 0));
  }
  totalDial->setPalette(*fgPalette);
  totalDial->setValue(totalValue);
  totalPerf->setValue(totalValue);
  if (restartTimer) timer->start(10);
}

void perfmeterWidget::closeEvent(QCloseEvent *event) {
  emit widgetClosing();
  QWidget::closeEvent(event);
}

void perfmeterWidget::togglePerfmeter() {
  if (isVisible()) {
    hide();
  } else {
    // Show widget
    show();
    raise();
    activateWindow();
    // Lock window size...
    setMaximumSize(this->size());
    setMinimumSize(this->size());
  }
}

