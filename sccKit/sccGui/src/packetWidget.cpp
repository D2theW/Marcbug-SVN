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

  #include "packetWidget.h"

  packetWidget::packetWidget(sccExtApi *sccAccess) : QWidget() {
    ui.setupUi(this);
    QWidget::setWindowFlags(Qt::Window);
    this->sccAccess = sccAccess;

    // Prepare GRB Symbol-List (Can be done automatically later...)
    GRBRegs["SIFPHY_PHYiodEnDataIn"  ] = (t_symAddr){4*4096,0,"DATA  input IODELAY enable (CE),     0: no delay change 1: modify delay according to direction setting" };
    GRBRegs["SIFPHY_PHYiodEnTXIn"    ] = (t_symAddr){4*4098,0,"TXCLK input IODELAY enable (CE),     0: no delay change 1: modify delay according to direction setting" };
    GRBRegs["SIFPHY_PHYiodEnRXIn"    ] = (t_symAddr){4*4098,0,"RXCLK input IODELAY enable (CE),     0: no delay change 1: modify delay according to direction setting" };
    GRBRegs["SIFPHY_PHYiodDirDataIn" ] = (t_symAddr){4*4099,0,"DATA  input IODELAY direction (INC), 0: decrement delay 1: increment delay" };
    GRBRegs["SIFPHY_PHYiodDirTXIn"   ] = (t_symAddr){4*4101,0,"TXCLK input IODELAY direction (INC), 0: decrement delay 1: increment delay" };
    GRBRegs["SIFPHY_PHYiodDirRXIn"   ] = (t_symAddr){4*4101,0,"RXCLK input IODELAY direction (INC), 0: decrement delay 1: increment delay" };
    GRBRegs["SIFPHY_PHYiodClockPulse"] = (t_symAddr){4*4102,0,"Modify input IODELAY values, Trigger on Write Signal: 0: no delay change 1: enable one clock pulse to the IODELAY control interface to modify the delay by one according to programmed settings for each bit" };
    GRBRegs["SIFPHY_PHYPatternGenOn" ] = (t_symAddr){4*4108,0,"Enables the pattern generation in the TX path to continuously sent a pre-determined flit pattern for delay adaptation" };
    GRBRegs["SIFPHY_PHYIoLoopBackOn" ] = (t_symAddr){4*4108,0,"Enables loopback mode to test RX/TX packetizers without attached SCC" };
    GRBRegs["SIFPHY_PHYCaptureFlits" ] = (t_symAddr){4*4108,0,"Trigger on Write: enables capturing 32 received flits in RX clock crossing fifo (rx_ccf)" };
    GRBRegs["SIFPHY_PHYNextFlit"     ] = (t_symAddr){4*4108,0,"Trigger on Write: selects next captured flit to be available in register PHYFlitData" };
    GRBRegs["SIFPHY_PHYFlitData"     ] = (t_symAddr){4*4103,0,"Captured data of the rx_ccf fifo (32 flits, incremented via PHYNextFlit)" };
    GRBRegs["SIFPCI_PCICfgStatus"    ] = (t_symAddr){4*8192,0,"Status Register: Status register from the Configuration Space Header." };
    GRBRegs["SIFPCI_PCICfgDStatus"   ] = (t_symAddr){4*8193,0,"Device Status Register: Device status register from the PCI Express Extended Capability Structure." };
    GRBRegs["SIFPCI_PCICfgLStatus"   ] = (t_symAddr){4*8194,0,"Link Status Register: Link status register from the PCI Express Extended Capability Structure." };

    // Prepare RCK Symbol-List
    RCKRegs["GLCFG0"] = (t_symAddr){0x010, 0x000df8, "Read Only bits:\nGLSTAT_RANGE\t25:12\nGLSTAT_XBP3_BIT\t25\nGLSTAT_XBP2_BIT\t24\nGLSTAT_XPM1_BIT\t23\nGLSTAT_XPM0_BIT\t22\nGLSTAT_XIERRNN_BIT\t21\nGLSTAT_XFERRNN_BIT\t20\nGLSTAT_XPRDY_BIT\t19\nGLSTAT_XSMIACTNN_BIT\t18\nGLSTAT_SHDWN_BIT\t17\nGLSTAT_FLUACK_BIT\t16\nGLSTAT_HALT_BIT\t15\nGLSTAT_WRBACK_BIT\t14\nGLSTAT_FLUSH_BIT\t13\nGLSTAT_BRTRMSG_BIT\t12\n\nRead/Write bits:\nGLCFG_XPICD_RANGE\t11:10\nGLCFG_CPUTYP_BIT\t09\nGLCFG_XA20MNN_BIT\t08\nGLCFG_XSMINN_BIT\t07\nGLCFG_XSTPCLKNN_BIT\t06\nGLCFG_XRSNN_BIT\t05\nGLCFG_XIGNNENN_BIT\t04\nGLCFG_XFLSHNN_BIT\t03\nGLCFG_XINIT_BIT\t02\nGLCFG_XINTR_BIT\t01\nGLCFG_XNMI_BIT\t00\n\nDefault value (RW bits only): 0xDF8" };
    RCKRegs["GLCFG1"] = (t_symAddr){0x018, 0x000df8, "Read Only bits:\nGLSTAT_RANGE\t25:12\nGLSTAT_XBP3_BIT\t25\nGLSTAT_XBP2_BIT\t24\nGLSTAT_XPM1_BIT\t23\nGLSTAT_XPM0_BIT\t22\nGLSTAT_XIERRNN_BIT\t21\nGLSTAT_XFERRNN_BIT\t20\nGLSTAT_XPRDY_BIT\t19\nGLSTAT_XSMIACTNN_BIT\t18\nGLSTAT_SHDWN_BIT\t17\nGLSTAT_FLUACK_BIT\t16\nGLSTAT_HALT_BIT\t15\nGLSTAT_WRBACK_BIT\t14\nGLSTAT_FLUSH_BIT\t13\nGLSTAT_BRTRMSG_BIT\t12\n\nRead/Write bits:\nGLCFG_XPICD_RANGE\t11:10\nGLCFG_CPUTYP_BIT\t09\nGLCFG_XA20MNN_BIT\t08\nGLCFG_XSMINN_BIT\t07\nGLCFG_XSTPCLKNN_BIT\t06\nGLCFG_XRSNN_BIT\t05\nGLCFG_XIGNNENN_BIT\t04\nGLCFG_XFLSHNN_BIT\t03\nGLCFG_XINIT_BIT\t02\nGLCFG_XINTR_BIT\t01\nGLCFG_XNMI_BIT\t00\n\nDefault value (RW bits only): 0xDF8" };
    RCKRegs["L2CFG0"] = (t_symAddr){0x020, 0x0006cf, "L2CFG_RANGE\t13:00\nL2CFG_STOPL2CCCLK\t13\nL2CFG_STOPL2ARRAYCLK\t12\nL2CFG_BLFLOATEN_BIT\t11\nL2CFG_WLSLPEN_BIT\t10\nL2CFG_WTSLPEN_BIT\t09\nL2CFG_FLIPEN_BIT\t08\nL2CFG_DATAECCEN_BIT\t07\nL2CFG_TAGECCEN_BIT\t06\nL2CFG_SLPBYPASS_BIT\t05\nL2CFG_WAYDISABLE_BIT\t04\nL2CFG_BBL2SLPPGM\t03:00\n\nDefault: 0x6CF" };
    RCKRegs["L2CFG1"] = (t_symAddr){0x028, 0x0006cf, "L2CFG_RANGE\t13:00\nL2CFG_STOPL2CCCLK\t13\nL2CFG_STOPL2ARRAYCLK\t12\nL2CFG_BLFLOATEN_BIT\t11\nL2CFG_WLSLPEN_BIT\t10\nL2CFG_WTSLPEN_BIT\t09\nL2CFG_FLIPEN_BIT\t08\nL2CFG_DATAECCEN_BIT\t07\nL2CFG_TAGECCEN_BIT\t06\nL2CFG_SLPBYPASS_BIT\t05\nL2CFG_WAYDISABLE_BIT\t04\nL2CFG_BBL2SLPPGM\t03:00\n\nDefault: 0x6CF" };
    RCKRegs["SENSOR"] = (t_symAddr){0x040, 0x000000, "SENSOR_EN_BIT\t\t13\nSENSOR_GATE_PULSE_CNT_RANGE\t12:00\n\nDefault: 0x0" };
    RCKRegs["GCBCFG"] = (t_symAddr){0x080, 0xa8e2ff,  "GCBCFG_RXB_CLKRATIO_RANGE\t25:19\nGCBCFG_TILE_CLKRATIO_RANGE\t18:12\nGCBCFG_TILE_CLKDIV_RANGE\t11:08\nGCBCFG_L2_1_SYNCRESETEN_BIT\t07\nGCBCFG_L2_0_SYNCRESETEN_BIT\t06\nGCBCFG_CORE1_SYNCRESETEN_BIT\t05\nGCBCFG_CORE0_SYNCRESETEN_BIT\t04\nGCBCFG_L2_1_RESET_BIT \t03\nGCBCFG_L2_0_RESET_BIT \t02\nGCBCFG_CORE1_RESET_BIT\t01\nGCBCFG_CORE0_RESET_BIT\t00\n\nDefault: 0xa8e2ff" };
    RCKRegs["TILEID"] = (t_symAddr){0x100, 0x000000,  "Read Only bits:\nTID_XPOS_RANGE\t10:07\nTID_YPOS_RANGE\t06:03\nTID_SYS2MIFDESTID\t02:00" };
    RCKRegs["LOCK0" ] = (t_symAddr){0x200, 0x000000,  "LOCK_BIT\t00" };
    RCKRegs["LOCK1" ] = (t_symAddr){0x400, 0x000000,  "LOCK_BIT\t00" };
  }

  void packetWidget::closeEvent(QCloseEvent *event) {
    emit widgetClosing();
    QWidget::closeEvent(event);
  }

  void packetWidget::subDestIDBoxChanged ( const QString text) {
    if (text == "CRB") {
      ui.addressBox->setCurrentIndex(1);
      ui.addressBox->setEnabled(1);
    } else {
      ui.addressBox->setCurrentIndex(0);
      ui.addressBox->setEnabled(0);
    }
  }

  void packetWidget::addressBoxChanged (const QString text) {
    // RCK System Address map
    foreach (QString str, RCKRegs.keys()) {
      if (str == text) {
        QString value;
        ui.addressEdit->setText(QString::number(RCKRegs[str].Addr,16));
        value.sprintf("%016llx",RCKRegs[str].Data); SetHexData(value);
        ui.addressEdit->setEnabled(0);
        ui.dataEdit1->setToolTip(RCKRegs[str].Tooltip);
        ui.CMDBox->setCurrentIndex(7);
        return;
      }
    }
    // GRB entries for GRB_PORT of SIF
    foreach (QString str, GRBRegs.keys()) {
      if (str == text) {
        QString value;
        ui.addressEdit->setText(QString::number(GRBRegs[str].Addr,16));
        value.sprintf("%016llx",GRBRegs[str].Data); SetHexData(value);
        ui.addressEdit->setEnabled(1);
        ui.dataEdit1->setToolTip(GRBRegs[str].Tooltip);
        ui.CMDBox->setCurrentIndex(7);
        return;
      }
    }
    // Default...
    ui.addressEdit->setText("0");
    SetHexData("0000000000000000");
    ui.addressEdit->setEnabled(1);
    ui.dataEdit1->setToolTip("");
    return;
  }

  void packetWidget::SIFSubIDBoxChanged (const QString text) {
    if        (text == "HOST_PORT") {
      ui.addressBox->clear();
      ui.addressBox->addItem("CUSTOM");
      ui.routeBox->setCurrentIndex(0);
      ui.subDestIDBox->setCurrentIndex(0);
      ui.addressBox->setCurrentIndex(0);
      ui.routeBox->setEnabled(false);
      ui.subDestIDBox->setEnabled(false);
      ui.addressBox->setEnabled(false);
    } else if (text == "RC_PORT") {
      ui.addressBox->clear();
      ui.addressBox->addItem("CUSTOM");
      foreach (QString str, RCKRegs.keys()) {
        ui.addressBox->addItem(str);
      }
      ui.routeBox->setCurrentIndex(0);
      ui.subDestIDBox->setCurrentIndex(0);
      ui.addressBox->setCurrentIndex(1);
      ui.routeBox->setEnabled(true);
      ui.subDestIDBox->setEnabled(true);
      ui.addressBox->setEnabled(true);
    } else if (text == "GRB_PORT") {
      ui.addressBox->clear();
      ui.addressBox->addItem("CUSTOM");
      foreach (QString str, GRBRegs.keys()) {
        ui.addressBox->addItem(str);
      }
      ui.routeBox->setCurrentIndex(0);
      ui.subDestIDBox->setCurrentIndex(0);
      ui.addressBox->setCurrentIndex(0);
      ui.routeBox->setEnabled(false);
      ui.subDestIDBox->setEnabled(false);
      ui.addressBox->setEnabled(true);
    } else if (text == "MC_PORT") {
      ui.addressBox->clear();
      ui.addressBox->addItem("CUSTOM");
      ui.routeBox->setCurrentIndex(0);
      ui.subDestIDBox->setCurrentIndex(0);
      ui.addressBox->setCurrentIndex(0);
      ui.routeBox->setEnabled(false);
      ui.subDestIDBox->setEnabled(false);
      ui.addressBox->setEnabled(false);
    } else if (text == "SATA_PORT") {
      ui.addressBox->clear();
      ui.addressBox->addItem("CUSTOM");
      ui.routeBox->setCurrentIndex(0);
      ui.subDestIDBox->setCurrentIndex(0);
      ui.addressBox->setCurrentIndex(0);
      ui.routeBox->setEnabled(false);
      ui.subDestIDBox->setEnabled(false);
      ui.addressBox->setEnabled(true);
    }
  }

  void packetWidget::HexClicked() {
    bool ok;
    QString value;
    if (!ui.radioButtonHex->isChecked()) return;
    value.sprintf("%016llx",ui.dataEdit1->displayText().toULongLong(&ok, Radix)); ui.dataEdit1->setText(value);
    value.sprintf("%016llx",ui.dataEdit2->displayText().toULongLong(&ok, Radix)); ui.dataEdit2->setText(value);
    value.sprintf("%016llx",ui.dataEdit3->displayText().toULongLong(&ok, Radix)); ui.dataEdit3->setText(value);
    value.sprintf("%016llx",ui.dataEdit4->displayText().toULongLong(&ok, Radix)); ui.dataEdit4->setText(value);
    ui.dataLabel->setText(QApplication::translate("Dialog", "Data (hex):", 0, QApplication::UnicodeUTF8));
    Radix=16;
  }

  void packetWidget::BinClicked() {
    bool ok;
    if (!ui.radioButtonBin->isChecked()) return;
    ui.dataEdit1->setText(QString::number(ui.dataEdit1->displayText().toULongLong(&ok, Radix),2));
    ui.dataEdit2->setText(QString::number(ui.dataEdit2->displayText().toULongLong(&ok, Radix),2));
    ui.dataEdit3->setText(QString::number(ui.dataEdit3->displayText().toULongLong(&ok, Radix),2));
    ui.dataEdit4->setText(QString::number(ui.dataEdit4->displayText().toULongLong(&ok, Radix),2));
    ui.dataLabel->setText(QApplication::translate("Dialog", "Data (bin):", 0, QApplication::UnicodeUTF8));
    Radix=2;
  }

  void packetWidget::SetHexData(QString text) {
    ui.dataEdit1->setText(text);
    ui.dataEdit2->setText("0000000000000000");
    ui.dataEdit3->setText("0000000000000000");
    ui.dataEdit4->setText("0000000000000000");
    // If old Radix was not Hex: Re-Translate...
    if (Radix==2) {
      Radix=16;
      BinClicked();
    }
  }

  void packetWidget::transfer() {
    bool ok;
    uInt64 data[4];
    QString value;
    data[0]=ui.dataEdit1->displayText().toULongLong(&ok, Radix);
    data[1]=ui.dataEdit2->displayText().toULongLong(&ok, Radix);
    data[2]=ui.dataEdit3->displayText().toULongLong(&ok, Radix);
    data[3]=ui.dataEdit4->displayText().toULongLong(&ok, Radix);
    sccAccess->transferPacket(lookupRoute(), lookupSubDestID(), ui.addressEdit->displayText().toULongLong(&ok, 16), lookupCommandPrefix()+lookupCommand(), ui.byteEnEdit->displayText().toUInt(&ok, 16), (uInt64*)&data, lookupSIFSubID());
    value.sprintf("%016llx",data[0]); ui.dataEdit1->setText(value);
    value.sprintf("%016llx",data[1]); ui.dataEdit2->setText(value);
    value.sprintf("%016llx",data[2]); ui.dataEdit3->setText(value);
    value.sprintf("%016llx",data[3]); ui.dataEdit4->setText(value);
    // If old Radix was not Hex: Re-Translate...
    if (Radix==2) {
      Radix=16;
      BinClicked();
    }
  }

  int packetWidget::lookupRoute() {
   int route=0;
   bool ok;
   route = ui.routeBox->currentText().mid(7, 1).toInt(&ok, 10); // x
   route += (ui.routeBox->currentText().mid(12, 1).toInt(&ok, 10)) << 4; // y
   return route;
  }

  int packetWidget::lookupSubDestID() {
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

  int packetWidget::lookupCommand() {
    int result=0;
    if      (ui.CMDBox->currentText() == "DATACMP"   ) result = DATACMP;
    else if (ui.CMDBox->currentText() == "NCDATACMP" ) result = NCDATACMP;
    else if (ui.CMDBox->currentText() == "MEMCMP"    ) result = MEMCMP;
    else if (ui.CMDBox->currentText() == "ABORT_MESH") result = ABORT_MESH;
    else if (ui.CMDBox->currentText() == "RDO"       ) result = RDO;
    else if (ui.CMDBox->currentText() == "WCWR"      ) result = WCWR;
    else if (ui.CMDBox->currentText() == "WBI"       ) result = WBI;
    else if (ui.CMDBox->currentText() == "NCRD"      ) result = NCRD;
    else if (ui.CMDBox->currentText() == "NCWR"      ) result = NCWR;
    else if (ui.CMDBox->currentText() == "NCIORD"    ) result = NCIORD;
    else if (ui.CMDBox->currentText() == "NCIOWR"    ) result = NCIOWR;
    return result;
  }

  int packetWidget::lookupCommandPrefix() {
    int result=0;
    if      (ui.CMDPrefixBox->currentText() == "BLOCK"  ) result = BLOCK;
    else if (ui.CMDPrefixBox->currentText() == "BCMC"   ) result = BCMC;
    else if (ui.CMDPrefixBox->currentText() == "BCTILE" ) result = BCTILE;
    else if (ui.CMDPrefixBox->currentText() == "BCBOARD") result = BCBOARD;
    else if (ui.CMDPrefixBox->currentText() == "NOGEN"  ) result = NOGEN;
    return result;
  }

  int packetWidget::lookupSIFSubID() {
    int result=0;
    if      (ui.SIFSubIDBox->currentText() == "HOST_PORT") result = SIF_HOST_PORT;
    else if (ui.SIFSubIDBox->currentText() == "RC_PORT")   result = SIF_RC_PORT;
    else if (ui.SIFSubIDBox->currentText() == "GRB_PORT")  result = SIF_GRB_PORT;
    else if (ui.SIFSubIDBox->currentText() == "MC_PORT")   result = SIF_MC_PORT;
    else if (ui.SIFSubIDBox->currentText() == "SATA_PORT") result = SIF_SATA_PORT;
    return result;
  }
