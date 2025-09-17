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

#ifndef COREMASKDIALOG_H
#define COREMASKDIALOG_H

#include <QApplication>
#include <QtGui>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLayout>
#include <QMap>
#include <QLabel>
#include "sccDefines.h"
#include "coreMask.h"

// ================================================================================
// Class myQCheckBox
// ================================================================================
// Customized checkbox class with explicit "check" and "uncheck" slots...
class myQCheckBox : public QCheckBox {
  Q_OBJECT
  public:
  myQCheckBox ( const QString & text, QWidget * parent = NULL);
  public slots:
  void check();
  void uncheck();
};

// ================================================================================
// coreselection disalog class...
// ================================================================================
class coreMaskDialog : public QDialog, public coreMask {
  Q_OBJECT

public:
  coreMaskDialog(bool allowZeroSelection=true);
  ~coreMaskDialog();

signals:
  void widgetClosing();

private:
  bool allowZeroSelection;

  // Dialog elements
  QGridLayout *tileLayout;
  QMap <int, QVBoxLayout*> xLayout;
  QMap <int, QLabel*> xLabel;
  QMap <int, QPushButton*> xButtonCheck;
  QMap <int, QPushButton*> xButtonUncheck;
  QMap <int, QHBoxLayout*> yLayout;
  QMap <int, QLabel*> yLabel;
  QMap <int, QPushButton*> yButtonCheck;
  QMap <int, QPushButton*> yButtonUncheck;
  QMap <int, QGridLayout*> coreLayout;
  QMap <int, myQCheckBox*> validPIDCheck;
  QDialogButtonBox *confirmButtons;
  QPushButton *okayButton;
  QPushButton *cancelButton;
  QPushButton *toggleButton;
  QPushButton *setButton;
  QPushButton *resetButton;
  QVBoxLayout *mainLayout;

  // Other dialog related stuff
  QLabel statusMessage;
  QString defaultStatus;
  bool dialogOpen;
  bool dialogSuccess;

public:
  void setEnabled(int x, int y, int core, bool enabled);
  bool getZeroSelectionPolicy();
  void setZeroSelectionPolicy(bool allowZeroSelection);
  bool selectionDialog(QString title, QString tooltip = "");
  bool dialogIsOpen();
  void abortDialog();

public slots:
  void selectOk();
  void selectCancel();
  void maskChanged();

};

#endif
