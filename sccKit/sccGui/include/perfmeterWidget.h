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

#ifndef PERFMETERWIDGET_H
#define PERFMETERWIDGET_H

#include <QPixmap>
#include <QWidget>
#include <QVector>
#include <QColor>
#include <sys/time.h>
#include <QtGui>
#include <QMessageBox>
#include "sccExtApi.h"

#define COCKPIT 1
#define TASKMGR 2
#define COCKPIT_TASKMGR 3
#define DEFAULT_STYLE COCKPIT
#define PERF_POLL_INTERVALL 1000
#define PERF_SHM_OFFSET 0x900

// Pre-declaration
class perfmeterWidget;

// ================================================================================
// viewCPU widget class...
// ================================================================================

#define CPUWIDGET_HEIGHT 110
#define CPUWIDGET_WIDTH 130
#define GRID_STEP_HOR 10
#define TIME_INTERVAL 1000
#define MAX_INTERVAL CPUWIDGET_WIDTH
#define PREC 10

class viewCPU : public QWidget
{
  Q_OBJECT

public:
  /* Constructor*/
  viewCPU ( QWidget *parent = 0, QString name = "" );

  /* Destructor */
  ~viewCPU ();

  /* CPU viewer name*/
  QString cpuViewerName;

  /* Viewer's parent form. In the rule main Window*/
  QWidget *parentFrm;

protected:
  /* Current performance value */
  int cpuPerf;

  /* Number of received performance samples during current interval */
  int perfSamples;

  /* This event is executed whenever the widget is repainted. It calls drawBlocks()
     and drawTimeLine() functions*/
  void paintEvent(QPaintEvent *event);

  void resizeEvent(QResizeEvent * /* event */);

private:
  /* Object used as graphical output */
  QPainter *painter;

  /* List holding performance values */
  QList <int> pmVal;

  /* Timer used for update the view*/
  QTimer *timer;

  /* Holds the current point in time*/
  int timeOffset;

  /* Horizontal scale factor*/
  double horScale;

  /* Vertical scale factor*/
  double verScale;

  /* Display the cpu ID*/
  void displayId(QPainter *painter);

  /* Draw grid on screen*/
  void drawGrid(QPainter *painter);

  /* Draw the performance lines */
  void drawPerformance(QPainter *painter);

  private slots:

  /* Update the current view*/
  void updateView();

  /* Update value of the current performance */
  void updatePerformanceValue();

  public slots:

  /* Set performance value for next interval(s) */
  void setValue(int cpuPerf);

  /* Return performance value */
  int value();

};

// ================================================================================
// perfmeter widget class...
// ================================================================================
class perfmeterWidget : public QWidget {
  Q_OBJECT

public:
  perfmeterWidget(int mesh_x, int mesh_y, int tile_cores, sccExtApi *sccAccess, QWidget *parent = 0);
  ~perfmeterWidget();
  void initialize();
  void updatePower(double power);
  int mesh_x;
  int mesh_y;
  int tile_cores;

protected:
  void closeEvent(QCloseEvent *event);

private slots:
void setStyle(int layout);
  void updatePerformanceValues();

public slots:
  void setValue(int x, int y, int core, int value);

signals:
  void widgetClosing();

private:
  sccExtApi* sccAccess;

private:
  QGridLayout *individualLayout;
  QHBoxLayout *totalLayout;
  QVBoxLayout *powerUsageLayout;
  QHBoxLayout *powerLayout;
  QGroupBox *powerGroup;
  QVBoxLayout *mainLayout;
  QHBoxLayout *configButtonLayout;
  QRadioButton *cockpitButton;
  QRadioButton *taskmgrButton;
  QRadioButton *cockpitTaskmgrButton;
  QButtonGroup *configButtonGroup;

  QGroupBox *individualGroup;
  QGroupBox *configGroup;
  QGroupBox *totalDialGroup;

  QDial *individualDial[NUM_COLS][NUM_ROWS][NUM_CORES];
  viewCPU *individualPerf[NUM_COLS][NUM_ROWS][NUM_CORES];
  int newValue[NUM_COLS][NUM_ROWS][NUM_CORES];
  int currentValue[NUM_COLS][NUM_ROWS][NUM_CORES];

  QDial *totalDial;
  QLCDNumber *powerLCD;
  viewCPU *totalPerf;

  QPalette *fgPalette;

  QTimer *timer;

public slots:
  void togglePerfmeter();

};

#endif
