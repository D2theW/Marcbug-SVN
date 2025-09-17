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

  #include "flitWidget.h"

  flitWidget::flitWidget(sccExtApi *sccAccess) : QWidget() {
    ui.setupUi(this);
    QWidget::setWindowFlags(Qt::Window);
    this->sccAccess = sccAccess;
  }

  void flitWidget::closeEvent(QCloseEvent *event) {
     emit widgetClosing();
     QWidget::closeEvent(event);
  }

  void flitWidget::subDestIDBoxChanged ( const QString text) {
    if (text == "CRB") {
      ui.addressBox->setCurrentIndex(0);
      ui.addressBox->setEnabled(1);
    } else {
      ui.addressBox->setCurrentIndex(9);
      ui.addressBox->setEnabled(0);
    }
  }

  void flitWidget::addressBoxChanged (const QString text) {
    if (text == "GLCFG0") {
      ui.addressEdit->setText("10");
      SetHexData("df8");
      ui.addressEdit->setEnabled(0);
      ui.dataEdit->setToolTip("Read Only bits:\nGLSTAT_RANGE\t25:12\nGLSTAT_XBP3_BIT\t25\nGLSTAT_XBP2_BIT\t24\nGLSTAT_XPM1_BIT\t23\nGLSTAT_XPM0_BIT\t22\nGLSTAT_XIERRNN_BIT\t21\nGLSTAT_XFERRNN_BIT\t20\nGLSTAT_XPRDY_BIT\t19\nGLSTAT_XSMIACTNN_BIT\t18\nGLSTAT_SHDWN_BIT\t17\nGLSTAT_FLUACK_BIT\t16\nGLSTAT_HALT_BIT\t15\nGLSTAT_WRBACK_BIT\t14\nGLSTAT_FLUSH_BIT\t13\nGLSTAT_BRTRMSG_BIT\t12\n\nRead/Write bits:\nGLCFG_XPICD_RANGE\t11:10\nGLCFG_CPUTYP_BIT\t09\nGLCFG_XA20MNN_BIT\t08\nGLCFG_XSMINN_BIT\t07\nGLCFG_XSTPCLKNN_BIT\t06\nGLCFG_XRSNN_BIT\t05\nGLCFG_XIGNNENN_BIT\t04\nGLCFG_XFLSHNN_BIT\t03\nGLCFG_XINIT_BIT\t02\nGLCFG_XINTR_BIT\t01\nGLCFG_XNMI_BIT\t00\n\nDefault value (RW bits only): 0xDF8");
    } else if (text == "GLCFG1") {
      ui.addressEdit->setText("18");
      SetHexData("df8");
      ui.addressEdit->setEnabled(0);
      ui.dataEdit->setToolTip("Read Only bits:\nGLSTAT_RANGE\t25:12\nGLSTAT_XBP3_BIT\t25\nGLSTAT_XBP2_BIT\t24\nGLSTAT_XPM1_BIT\t23\nGLSTAT_XPM0_BIT\t22\nGLSTAT_XIERRNN_BIT\t21\nGLSTAT_XFERRNN_BIT\t20\nGLSTAT_XPRDY_BIT\t19\nGLSTAT_XSMIACTNN_BIT\t18\nGLSTAT_SHDWN_BIT\t17\nGLSTAT_FLUACK_BIT\t16\nGLSTAT_HALT_BIT\t15\nGLSTAT_WRBACK_BIT\t14\nGLSTAT_FLUSH_BIT\t13\nGLSTAT_BRTRMSG_BIT\t12\n\nRead/Write bits:\nGLCFG_XPICD_RANGE\t11:10\nGLCFG_CPUTYP_BIT\t09\nGLCFG_XA20MNN_BIT\t08\nGLCFG_XSMINN_BIT\t07\nGLCFG_XSTPCLKNN_BIT\t06\nGLCFG_XRSNN_BIT\t05\nGLCFG_XIGNNENN_BIT\t04\nGLCFG_XFLSHNN_BIT\t03\nGLCFG_XINIT_BIT\t02\nGLCFG_XINTR_BIT\t01\nGLCFG_XNMI_BIT\t00\n\nDefault value (RW bits only): 0xDF8");
    } else if (text == "L2CFG0") {
      ui.addressEdit->setText("20");
      SetHexData("6cf");
      ui.addressEdit->setEnabled(0);
      ui.dataEdit->setToolTip("L2CFG_RANGE\t13:00\nL2CFG_STOPL2CCCLK\t13\nL2CFG_STOPL2ARRAYCLK\t12\nL2CFG_BLFLOATEN_BIT\t11\nL2CFG_WLSLPEN_BIT\t10\nL2CFG_WTSLPEN_BIT\t09\nL2CFG_FLIPEN_BIT\t08\nL2CFG_DATAECCEN_BIT\t07\nL2CFG_TAGECCEN_BIT\t06\nL2CFG_SLPBYPASS_BIT\t05\nL2CFG_WAYDISABLE_BIT\t04\nL2CFG_BBL2SLPPGM\t03:00\n\nDefault: 0x6CF");
    } else if (text == "L2CFG1") {
      ui.addressEdit->setText("28");
      SetHexData("6cf");
      ui.addressEdit->setEnabled(0);
      ui.dataEdit->setToolTip("L2CFG_RANGE\t13:00\nL2CFG_STOPL2CCCLK\t13\nL2CFG_STOPL2ARRAYCLK\t12\nL2CFG_BLFLOATEN_BIT\t11\nL2CFG_WLSLPEN_BIT\t10\nL2CFG_WTSLPEN_BIT\t09\nL2CFG_FLIPEN_BIT\t08\nL2CFG_DATAECCEN_BIT\t07\nL2CFG_TAGECCEN_BIT\t06\nL2CFG_SLPBYPASS_BIT\t05\nL2CFG_WAYDISABLE_BIT\t04\nL2CFG_BBL2SLPPGM\t03:00\n\nDefault: 0x6CF");
    } else if (text == "SENSOR") {
      ui.addressEdit->setText("40");
      SetHexData("0");
      ui.addressEdit->setEnabled(0);
      ui.dataEdit->setToolTip("SENSOR_EN_BIT\t\t13\nSENSOR_GATE_PULSE_CNT_RANGE\t12:00\n\nDefault: 0x0");
    } else if (text == "GCBCFG") {
      ui.addressEdit->setText("80");
      SetHexData("a8e2ff");
      ui.addressEdit->setEnabled(0);
      ui.dataEdit->setToolTip("GCBCFG_RXB_CLKRATIO_RANGE\t25:19\nGCBCFG_TILE_CLKRATIO_RANGE\t18:12\nGCBCFG_TILE_CLKDIV_RANGE\t11:08\nGCBCFG_L2_1_SYNCRESETEN_BIT\t07\nGCBCFG_L2_0_SYNCRESETEN_BIT\t06\nGCBCFG_CORE1_SYNCRESETEN_BIT\t05\nGCBCFG_CORE0_SYNCRESETEN_BIT\t04\nGCBCFG_L2_1_RESET_BIT \t03\nGCBCFG_L2_0_RESET_BIT \t02\nGCBCFG_CORE1_RESET_BIT\t01\nGCBCFG_CORE0_RESET_BIT\t00\n\nDefault: 0xa8e2ff");
    } else if (text == "TILEID") {
      ui.addressEdit->setText("100");
      SetHexData("0");
      ui.addressEdit->setEnabled(0);
      ui.dataEdit->setToolTip("Read Only bits:\nTID_XPOS_RANGE\t10:07\nTID_YPOS_RANGE\t06:03\nTID_SYS2MIFDESTID\t02:00");
    } else if (text == "LOCK0") {
      ui.addressEdit->setText("200");
      SetHexData("0");
      ui.addressEdit->setEnabled(0);
      ui.dataEdit->setToolTip("LOCK_BIT\t00");
    } else if (text == "LOCK1") {
      ui.addressEdit->setText("400");
      SetHexData("0");
      ui.addressEdit->setEnabled(0);
      ui.dataEdit->setToolTip("LOCK_BIT\t00");
    } else {
      ui.addressEdit->setText("0");
      SetHexData("0");
      ui.addressEdit->setEnabled(1);
      ui.dataEdit->setToolTip("");
    }
  }


  void flitWidget::HexClicked() {
    bool ok;
    if (!ui.radioButtonHex->isChecked()) return;
    ui.dataEdit->setText(QString::number(ui.dataEdit->displayText().toULongLong(&ok, Radix),16));
    ui.dataLabel->setText(QApplication::translate("Dialog", "Data (hex):", 0, QApplication::UnicodeUTF8));
    Radix=16;
  }

  void flitWidget::BinClicked() {
    bool ok;
    if (!ui.radioButtonBin->isChecked()) return;
    ui.dataEdit->setText(QString::number(ui.dataEdit->displayText().toULongLong(&ok, Radix),2));
    ui.dataLabel->setText(QApplication::translate("Dialog", "Data (bin):", 0, QApplication::UnicodeUTF8));
    Radix=2;
  }

  void flitWidget::SetHexData(QString text) {
    ui.dataEdit->setText(text);
    // If old Radix was not Hex: Re-Translate...
    if (Radix==2) {
      Radix=16;
      BinClicked();
    }
  }

  void flitWidget::read() {
    bool ok;
    SetHexData(QString::number(sccAccess->readFlit(lookupRoute(), lookupSubDestID(), ui.addressEdit->displayText().toULongLong(&ok, 16)),16));
  }

  void flitWidget::write() {
    bool ok;
    ok=sccAccess->writeFlit(lookupRoute(), lookupSubDestID(), ui.addressEdit->displayText().toULongLong(&ok, 16) , ui.dataEdit->displayText().toULongLong(&ok, Radix));
  }

  int flitWidget::lookupRoute() {
   int route=0;
   bool ok;
   route = ui.routeBox->currentText().mid(7, 1).toInt(&ok, 10); // x
   route += (ui.routeBox->currentText().mid(12, 1).toInt(&ok, 10)) << 4; // y
   return route;
  }

  int flitWidget::lookupSubDestID() {
    int subdestid=0;
    if      (ui.subDestIDBox->currentText() == "CORE0") subdestid = CORE0;
    else if (ui.subDestIDBox->currentText() == "RTG")   subdestid = RTG;
    else if (ui.subDestIDBox->currentText() == "CORE1") subdestid = CORE1;
    else if (ui.subDestIDBox->currentText() == "CRB")   subdestid = CRB;
    else if (ui.subDestIDBox->currentText() == "MPB")   subdestid = MPB;
    else if (ui.subDestIDBox->currentText() == "PERIE") subdestid = PERIE;
    else if (ui.subDestIDBox->currentText() == "PERIS") subdestid = PERIS;
    else if (ui.subDestIDBox->currentText() == "PERIW") subdestid = PERIW;
    else if (ui.subDestIDBox->currentText() == "PERIN") subdestid = PERIN;
    return subdestid;
  }

