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

// This is the main window widget. It parses the command line arguments and then
// invokes sccDisplayWidget with the actual visual content. The widget also contains
// a help message that is displayed when no core is reachable or the user invokes
// the application with --help...

#ifndef SCCDISPLAY_H
#define SCCDISPLAY_H

#include <QApplication>
#include <QtGui>
#include <QSettings>
#include <QFile>
#include "coreMask.h"
#include "sccExtApi.h"
#include "sccKitVersion.h"
#include "sccDisplayWidget.h"
#include "perfmeterWidget.h"

// Socket to use for remote HID
#define HID_UDP 0
#define HID_TCP 1
#define DEFAULT_HID_SOCKET HID_TCP

// Pre-declaration
class sccDisplayWidget;

// Power thread for polling of power values from BMC (otherwise the picture updates are not fluent enough...)
class powerThread : public QThread {
  Q_OBJECT
  public:
    powerThread(char* status);
    ~powerThread();
    void showPerfmeter(bool show);
    void togglePerfmeter();
    bool perfmeterVisible();

  private:
    void run();
    void stop();
    messageHandler* log;
    sccExtApi* sccAccess;
    perfmeterWidget* PerfmeterWidget;
    char* status;
    double power;
    bool serverRunning;
    bool serverOn;

  signals:
    void signalValue(int x, int y, int core, int value);
};

class sccDisplay : public QMainWindow {
  Q_OBJECT

public:
  sccDisplay(QWidget *parent = 0);
  ~sccDisplay();
  void closeEvent(QCloseEvent *event);
  void showPerfmeter(bool show);
  void togglePerfmeter();

public slots:
  void clipboardDataChanged();

// =============================================================================================
// Protection members & methods...
// =============================================================================================
private:
  coreMask displayMask;
  bool notAllSelectedCoresAvailable;

private:
  // Widget methods
  void printUsage(int exitCode);

public:
  // Main window including log text pane...
  QMainWindow *mW;
  messageHandler *log;
  // Widgets
  sccDisplayWidget *display;

protected:
 char formatMsg[8192];
signals:
 void logMessageSignal(LOGLEVEL level, const char *logmsg);
 void logMessageSignal(LOGLEVEL level, const QString a, const QString b = "", const QString c = "", const QString d = "", const QString e = "", const QString f = "", const QString g = "", const QString h = "", const QString i = "", const QString j = "", const QString k = "", const QString l = "", const QString m = "", const QString n = "", const QString o = "", const QString p = "", const QString q = "", const QString r = "", const QString s = "", const QString t = "", const QString u = "" );

private:
  // Config values...
  QSettings *settings;

private:
  // SCC API object
  sccExtApi *sccAccess;

// Performance widget...
// ======================
public:
  void pollPerfmeterWidget();
  char perfStatus[64];
  int perfRoute, perfDestid, perfSIFPort;
  uInt64 perfApertureStart;
  powerThread *fetchPower;

};

#endif // SCCDISPLAY_H
