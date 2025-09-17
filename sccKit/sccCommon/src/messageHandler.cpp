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

#include "../include/messageHandler.h"

messageHandler::messageHandler(bool logShell, QString logName, QTextEdit *logText, QStatusBar *status) {
  // Initialize members...
  msgCount[log_fatal] = 0;
  msgCount[log_error] = 0;
  msgCount[log_warning] = 0;
  msgCount[log_info] = 0;
  msgCount[log_command] = 0;
  msgCount[log_debug] = 0;
  logMsgText[log_fatal] = "FATAL";
  logMsgText[log_error] = "ERROR";
  logMsgText[log_warning] = "WARNING";
  logMsgText[log_info] = "INFO";
  logMsgText[log_command] = "COMMAND";
  logMsgText[log_debug] = "DEBUG";
  logLevel = log_command;
  printShell = logShell;

  // Connect statusbar (if provided) in order to make sure that we really own it...
  printWindow = logText;
  this->status = status;
  if (status) connect(status, SIGNAL(messageChanged(QString)), this, SLOT(statusSlot(QString)));

  // Define output channels
  if (logName != "") {
    printFile = new QFile(logName);
    if (!printFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
      logMessage(log_warning, "messageHandler(): Can't open log file \"",logName,"\". Disabling log-file creation!");
      delete printFile;
      printFile = NULL;
    } else {
      out.setDevice(printFile);
    }
  } else {
    printFile = NULL;
  }

  // We're done...
  updateStatus("");
}

messageHandler::~messageHandler() {
  if (printFile) {
    printFile->close();
    delete printFile;
    printFile = NULL;
  }
}

void messageHandler::reset() {
  msgCount[log_fatal] = 0;
  msgCount[log_error] = 0;
  msgCount[log_warning] = 0;
  msgCount[log_info] = 0;
  msgCount[log_command] = 0;
  msgCount[log_debug] = 0;
  logMsgText[log_fatal] = "FATAL";
  logMsgText[log_error] = "ERROR";
  logMsgText[log_warning] = "WARNING";
  logMsgText[log_info] = "INFO";
  logMsgText[log_command] = "COMMAND";
  logMsgText[log_debug] = "DEBUG";
  logLevel = log_command;
  logLevel = log_command;
  updateStatus("");
  if (printWindow) {
    printWindow->clear();
  }
  if (printFile) {
    printFile->close();
    if (!printFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
      logMessage(log_warning, "messageHandler(): Can't re-open log file \"",printFile->fileName(),"\". Disabling log-file creation!");
      delete printFile;
      printFile = NULL;
    } else {
      out.setDevice(printFile);
    }
  }
}

void messageHandler::setLogLevel(LOGLEVEL level) {
  if (((int) level > MAX_LOGLEVEL) | ((int) level < 0)) {
    logMessage(log_warning, "setLogLevel(): Undefined log level - Setting to max");
    logLevel = MAX_LOGLEVEL;
  } else {
    logLevel = level;
  }
}

LOGLEVEL messageHandler::getLogLevel() {
  return logLevel;
}

void messageHandler::setMsgPrefix(LOGLEVEL level, QString prefix) {
  logMsgText[level] = prefix;
}

void messageHandler::setShell(bool logShell) {
  printShell = logShell;
}

void messageHandler::setLog(QTextEdit *logText) {
  printWindow = logText;
}

void messageHandler::setStatus(QStatusBar *status) {
  if (status) disconnect(status, 0, this, 0);
  this->status = status;
  if (status) connect(status, SIGNAL(messageChanged(QString)), this, SLOT(statusSlot(QString)));
}

void messageHandler::logMessage(LOGLEVEL level, const char *logmsg) {
  logMessage(level, (QString) logmsg);
}

void messageHandler::logMessage(LOGLEVEL level,
                                QString a,
                                QString b,
                                QString c,
                                QString d,
                                QString e,
                                QString f,
                                QString g,
                                QString h,
                                QString i,
                                QString j,
                                QString k,
                                QString l,
                                QString m,
                                QString n,
                                QString o,
                                QString p,
                                QString q,
                                QString r,
                                QString s,
                                QString t,
                                QString u ) {

  // First of all lock logMessage method to make it thread safe...
  logMessageMutex.lock();

  // Define Log message prefix
  if (((int) level > MAX_LOGLEVEL) | ((int) level < 0)) {
    logMessage(log_warning, "logMessage(...): Undefined log level - Setting to max");
    logMsg = logMsgText[MAX_LOGLEVEL];
  } else {
    logMsg = logMsgText[level];
    msgCount[level]++;
    // Update status when first fatal, error or warning occurs...
    if (status && (msgCount[level] == 1) && (level <= log_warning)) {
      if (msgCount[log_fatal]) {
        updateStatus("Ups, fatal errors occurred! Refer to log for details...");
      } else if (msgCount[log_error]) {
        updateStatus("Ups, errors occurred! Refer to log for details...");
      } else if (msgCount[log_warning]) {
        updateStatus("Ups, Warnings occurred! Refer to log for details...");
      }
    }
  }
  if (logMsg != "") logMsg += ": ";

  // Add actual log message content...
  if(a != "")
    logMsg += a;
  if(b != "")
    logMsg += b;
  if(c != "")
    logMsg += c;
  if(d != "")
    logMsg += d;
  if(e != "")
    logMsg += e;
  if(f != "")
    logMsg += f;
  if(g != "")
    logMsg += g;
  if(h != "")
    logMsg += h;
  if(i != "")
    logMsg += i;
  if(j != "")
    logMsg += j;
  if(k != "")
    logMsg += k;
  if(l != "")
    logMsg += l;
  if(m != "")
    logMsg += m;
  if(n != "")
    logMsg += n;
  if(o != "")
    logMsg += o;
  if(p != "")
    logMsg += p;
  if(q != "")
    logMsg += q;
  if(r != "")
    logMsg += r;
  if(s != "")
    logMsg += s;
  if(t != "")
    logMsg += t;
  if(u != "")
    logMsg += u;

  // Print to log window if enabled and logLevel is reached...
  if (printWindow && (level <= logLevel)) {
    if (level == log_fatal) {
      printWindow->setTextColor(Qt::red);
      printWindow->setFontWeight(QFont::Bold);
    } else if (level == log_error) {
      printWindow->setTextColor(Qt::red);
      printWindow->setFontWeight(QFont::Normal);
    } else if (level == log_warning) {
      printWindow->setTextColor(QColor::fromRgb(255, 165, 0)); // orange
      printWindow->setFontWeight(QFont::Normal);
    } else {
      printWindow->setTextColor(Qt::black);
      printWindow->setFontWeight(QFont::Normal);
    }
    printWindow->append(logMsg);
  }

  // Print to log stdout if enabled and logLevel is reached...
  logMsg += "\n";
  if (printShell && (level <= logLevel)) {
    printf("%s", logMsg.toStdString().c_str());
    fflush(0);
  }

  // Print (everything) to log file if enabled...
  if (printFile && (level <= logLevel)) {
    out << QDate::currentDate().toString("MMM d, yyyy - ") << QTime::currentTime().toString("hh:mm:ss: ") << logMsg;
    out.flush();
  }

  // Unlock logMessage method...
  logMessageMutex.unlock();
}

// Helper function to convert a value to a string with some formatting...
QString messageHandler::toString(uInt64 value, int radix, int length) {
  QString retval;
  retval = QString::number(value, radix);
  while (retval.length() < length) {
    retval = "0"+retval;
  }
  if (radix == 16) retval = "0x"+retval;
  return retval;
}

void messageHandler::updateStatus(QString message) {
  if (!status) return;
  statusMsg = (message=="")?"":"Status: " + message;
  status->showMessage(statusMsg);
}

// SLOT: clear the logwindow...
void messageHandler::clear() {
  if (printWindow) {
    printWindow->clear();
    if (msgCount[log_fatal]) {
      logMessage(log_info, "Had fatal errors prior to clearing log window!", (printFile)?" Refer to log file for details...":"");
    } else if (msgCount[log_error]) {
      logMessage(log_info, "Had errors prior to clearing log window!", (printFile)?" Refer to log file for details...":"");
    } else if (msgCount[log_warning]) {
      logMessage(log_info, "Had warnings prior to clearing log window!", (printFile)?" Refer to log file for details...":"");
    }
  }
}

// SLOT: status line updated...
void messageHandler::statusSlot(QString message) {
  // Overwrite status if someone else wants to update it. It's ours! ;-)
  if (message != statusMsg) status->showMessage(statusMsg);
  return;
  // Fake code that will never be reached to prevent compiler to report unused parameter:
  if (message!="") return;
}

bool messageHandler::printsToShell() {
  return printShell;
}

bool messageHandler::printsToLog() {
  return (printWindow!=NULL);
}
