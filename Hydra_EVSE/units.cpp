/*

  J1772 Hydra (EVSE variant) for Arduino
  Copyright 2014 Nicholas W. Sayer, Dmitriy Lyubimov

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

///////////////////////////////////////////////////

#include "Hydra_EVSE.h"

// If unit tests are declared, this is called the last thing from setup().

static void testEepromSetup() {

  persisted.reset();
  persisted.eepromWrite();


  //read/write test
  persisted.operatingMode = MODE_SEQUENTIAL;
  persisted.eepromWrite();
  persisted_struct clone1;
  if (clone1.operatingMode != MODE_SEQUENTIAL) {
    log(LOG_INFO, P("eeprom write/read UNIT FAIL (actual: %d)."), (int)clone1.operatingMode);
    return;
  }

  // invalid signature reset test
  persisted.signature = 0xff;
  persisted.validate();

  persisted_struct clone2;
  if ( persisted.signature != PERSIST_SIG || clone2.signature != PERSIST_SIG || clone2.operatingMode != MODE_SHARED ) {
    log(LOG_INFO, P("eeprom sig validation UNIT FAIL."));
    return;
  }

  log(LOG_DEBUG, P("eeprom UNIT OK."));

}

// Status is combination of (CAR_A or CAR_B or CAR_BOTH) | (one of STATUS_XXX) [ | STATUS_TIEBREAK ]
void displayStatus(unsigned int status) {
  unsigned int car = status & CAR_MASK;
  int col;
  char carLetter;
  switch (car) {
    case CAR_A: col = 0; carLetter = 'A'; break;
    case CAR_B: col = 8; carLetter = 'B'; break;
    default:
      displayStatus( (status & ~CAR_MASK) | CAR_A);
      displayStatus( (status & ~CAR_MASK) | CAR_B);
      return;
  }
  //  display.setCursor(col,1);
  //  for (int i = 0; i < 8; i++) display.print(' ');
  display.setCursor(col, 1);
  display.print(carLetter); // 1
  display.print(P(": "));  // 3

  log(LOG_DEBUG, P("status: %x, CAR:%c, status bits: %x, status mask: %x."), status, carLetter, status & STATUS_MASK, STATUS_MASK);
  
  switch ( status & STATUS_MASK) { // 7
    case STATUS_UNPLUGGED: display.print(P("--- ")); break;
    case  STATUS_OFF: display.print(P("off ")); break;
    case  STATUS_ON: display.print(P("on  ")); break;
    case  STATUS_WAIT: display.print(P("wait")); break;
    case  STATUS_DONE: display.print(P("done")); break;
    case  STATUS_ERR:
    default: display.print(P("ERR ")); break;
  }
  display.print(" "); // 8
  if ( (status & STATUS_TIEBREAK) != 0) {
    display.setCursor(col + 6, 1);
    display.print('*');
  }

}

static void testDS(char* desc, unsigned int status) {
  display.clear();
  display.print (desc);
  displayStatus(status);
  Delay(3000);
}

void testDisplayStatus() {
  testDS(P("A&B UNPL"),BOTH|STATUS_UNPLUGGED);
  testDS(P("A&B off"),BOTH|STATUS_OFF);
  testDS(P("B off tie"),CAR_B|STATUS_OFF|STATUS_TIEBREAK);
  testDS(P("A&B on"),BOTH|STATUS_ON);
  testDS(P("A&B done"),BOTH|STATUS_DONE);
  testDS(P("A&B wait"),BOTH|STATUS_WAIT);
}


static void testMenuSetup() {
  inMenu = true;
  doMenu(true);
}


int unitsSetup() {
  testEepromSetup();
  testDisplayStatus();
  testMenuSetup();
  return false;
}

// This is called
int unitsLoop() {
  return false;
}


