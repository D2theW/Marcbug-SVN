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

#ifndef MESSAGEHANDLER_H_
#define MESSAGEHANDLER_H_

#include <QString>
#include <QTextEdit>
#include <QStatusBar>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QTime>
#include <QMutex>
#include "sccDefines.h"

enum LOGLEVEL {
  log_fatal = 0,
  log_error = 1,
  log_warning = 2,
  log_info = 3,
  log_command = 4,
  log_debug = 5
};
#define MAX_LOGLEVEL log_debug

// Macros for comfortable access to "logMessage" method. May also be used by other threads...
// In case you plan to use sccApi, it's mandatory to do the following steps!
//
// Requires definition and connection of logMessageSignal signals as well as a formatMsg in calling object!
// Example:
//
// Definition in class header:
// ==============================
// protected:
//   messageHandler* log;
// protected:
//   char formatMsg[8192];
// signals:
//   void logMessageSignal(LOGLEVEL level, const char *logmsg);
//   void logMessageSignal(LOGLEVEL level, const QString a, const QString b = "", const QString c = "", const QString d = "", const QString e = "", const QString f = "", const QString g = "", const QString h = "", const QString i = "", const QString j = "", const QString k = "", const QString l = "", const QString m = "", const QString n = "", const QString o = "", const QString p = "", const QString q = "", const QString r = "", const QString s = "", const QString t = "", const QString u = "" );
//
// Implementation in constructor:
// ==============================
// log = new messageHandler(...);
// CONNECT_MESSAGE_HANDLER;

// Regular printing, using signal and slot mechanism...
// This is threadsafe and should be your first choice for usage within classes!
#define printFatal(args...)  logMessageSignal(log_fatal, ##args)
#define printError(args...) logMessageSignal(log_error, ##args)
#define printWarning(args...) logMessageSignal(log_warning, ##args)
#define printInfo(args...)  logMessageSignal(log_info, ##args)
#define printCommand(args...) logMessageSignal(log_command, ##args)
#define printDebug(args...) logMessageSignal(log_debug, ##args)

// Formatted printing, using signal and slot mechanism...
// This is threadsafe and should be your first choice for usage within classes!
#define printfFatal(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); printFatal(formatMsg);}
#define printfError(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); printError(formatMsg);}
#define printfWarning(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); printWarning(formatMsg);}
#define printfInfo(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); printInfo(formatMsg);}
#define printfCommand(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); printCommand(formatMsg);}
#define printfDebug(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); printDebug(formatMsg);}

// Regular printing, w/o signal and slot mechanism...
// NOT Threadsafe! However, doesn't require the definition and connection of slots.
// This might be useful for usage in a main program.
#define logFatal(args...) log->logMessage(log_fatal, ##args)
#define logError(args...) log->logMessage(log_error, ##args)
#define logWarning(args...) log->logMessage(log_warning, ##args)
#define logInfo(args...)  log->logMessage(log_info, ##args)
#define logCommand(args...) log->logMessage(log_command, ##args)
#define logDebug(args...) log->logMessage(log_debug, ##args)

// Formatted printing, w/o signal and slot mechanism...
// NOT Threadsafe! However, doesn't require the definition and connection of slots.
// This might be useful for usage in a main program.
#define logfFatal(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); logFatal(formatMsg);}
#define logfError(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); logError(formatMsg);}
#define logfWarning(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); logWarning(formatMsg);}
#define logfInfo(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); logInfo(formatMsg);}
#define logfCommand(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); logCommand(formatMsg);}
#define logfDebug(fmt, args...) {snprintf(formatMsg, 255, fmt, ## args); logDebug(formatMsg);}

#define CONNECT_MESSAGE_HANDLER \
qRegisterMetaType<LOGLEVEL>("LOGLEVEL");\
connect(this, SIGNAL(logMessageSignal(LOGLEVEL, const char *)), log, SLOT(logMessage(LOGLEVEL, const char *)));\
connect(this, SIGNAL(logMessageSignal(LOGLEVEL, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString)),\
         log,   SLOT(  logMessage(LOGLEVEL, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString, const QString)));

class messageHandler : public QObject {
  Q_OBJECT
public:
  messageHandler(bool logShell, QString logName = "", QTextEdit *logText = NULL, QStatusBar *status = NULL);
  ~messageHandler();
  void reset();
  void setLogLevel(LOGLEVEL level);
  LOGLEVEL getLogLevel();
  void setMsgPrefix(LOGLEVEL level, QString prefix);
  void setShell(bool logShell = true);
  void setLog(QTextEdit *logText = NULL);
  void setStatus(QStatusBar *status = NULL);
  bool printsToShell();
  bool printsToLog();

public slots:
  void logMessage(LOGLEVEL level, const char *logmsg);
  void logMessage(LOGLEVEL level,
      const QString a,
      const QString b = "",
      const QString c = "",
      const QString d = "",
      const QString e = "",
      const QString f = "",
      const QString g = "",
      const QString h = "",
      const QString i = "",
      const QString j = "",
      const QString k = "",
      const QString l = "",
      const QString m = "",
      const QString n = "",
      const QString o = "",
      const QString p = "",
      const QString q = "",
      const QString r = "",
      const QString s = "",
      const QString t = "",
      const QString u = "" );

public:
  // Helper function to convert a value to a string with some formatting...
  static QString toString(uInt64 value, int radix, int length);

public slots:
  void clear();
  void statusSlot(QString message);

private:
  // Private methods
  void updateStatus(QString message);

private:
  // Default message prefixes...
  unsigned int msgCount[MAX_LOGLEVEL+1];
  QString logMsgText[MAX_LOGLEVEL+1];
  LOGLEVEL logLevel;
  QString logStatus;

  // Message output channels
  bool printShell;
  QTextEdit *printWindow;
  QFile *printFile;
  QTextStream out;
  QStatusBar *status;

  // Message strings
  QString logMsg;
  QString statusMsg;

  // Mutex to make (only) logMessage(...) method thread-safe
  QMutex logMessageMutex;

public:
  // Datatype for printf commands
  char formatMsg[1024];

};

#endif /* MESSAGEHANDLER_H_ */
