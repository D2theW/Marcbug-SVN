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

#include "asciiProgress.h"
// ================================================================================
// Class asciiProgress
// ================================================================================

asciiProgress::asciiProgress(QString text, uInt64 min, uInt64 max) {
  this->min = min;
  this->max = max;
  this->text = text;
  done = false;
  oldValue = 0;
}

asciiProgress::~asciiProgress() {
}

void asciiProgress::show() {
  if (text != "") {
    cout << text.toStdString().c_str();
  }
  done = false;
  setPercent(0);
}

void asciiProgress::hide() {
  setPercent(100);
}

void asciiProgress::setText(QString text) {
  this->text = text.replace(QRegExp("\n*$"), "\n");
}

void asciiProgress::setMinimum(uInt64 min) {
  this->min = min;
}

void asciiProgress::setMaximum(uInt64 max) {
  this->max = max;
}

void asciiProgress::setValue(uInt64 value) {
  if ((value < min) || (value > max)) {
    return;
  }
  double tmp = value - min;
  tmp /= max - min;
  tmp *= 100;
  int percent = (tmp - ((int)tmp) >= 0.5)?1+(int)tmp:(int)tmp;
  setPercent(percent);
}

void asciiProgress::setPercent(int percent) {
  if (done) return;
  if (percent <= 0) {
    oldValue = 0;
  } else if (percent >= 100) {
    cout<<"\r                                                                                \r";
    fflush(0);
    done = true;
    return;
  } else {
    if (percent == oldValue) {
      // Only update when the value changed. Otherwise we waste precious time for useless screen updates!
      return;
    }
    oldValue = percent;
  }
  // "Draw" ASCII bar...
  int ticks = (int)(oldValue / ((double)100/(double)TICKS));
  for(int i=0; i < TICKS; i++) {
    if(i <= ticks) {
      bar.replace(i, 1, "=");
    } else {
      bar.replace(i, 1, " ");
    }
  }
  cout<<"\r["<<bar.toStdString()<<"]";
  cout.width(4);
  cout<<oldValue<<"%";
  fflush(0);
}

bool asciiProgress::wasCanceled() {
  return false;
}


