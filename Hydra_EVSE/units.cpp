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


static void testDS(char* desc, unsigned int status) {
  display.clear();
  display.print (desc);
  displayStatus(status);
  Delay(3000);
}

void testDisplayStatus() {
  testDS(P("A&B UNPL"), BOTH | STATUS_UNPLUGGED);
  testDS(P("A&B off"), BOTH | STATUS_OFF);
  testDS(P("B off tie"), CAR_B | STATUS_OFF | STATUS_TIEBREAK);
  testDS(P("A off tie"), CAR_A | STATUS_OFF | STATUS_TIEBREAK);
  testDS(P("A&B on"), BOTH | STATUS_ON);
  testDS(P("A&B done"), BOTH | STATUS_DONE);
  testDS(P("A&B wait"), BOTH | STATUS_WAIT);

  char* msg = P("incorrect letter %c UNIT FAIL");
  if ( 'F' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_F) )
    log(LOG_INFO, msg, 'F');
  if ( 'O' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_O) )
    log(LOG_INFO, msg, 'O');
  if ( 'G' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_G) )
    log(LOG_INFO, msg, 'G');
  if ( 'T' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_T) )
    log(LOG_INFO, msg, 'T');
  if ( 'R' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_R) )
    log(LOG_INFO, msg, 'R');
  if ( 'E' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_E) )
    log(LOG_INFO, msg, 'E');

  testDS(P("A&B ERR G"), BOTH | STATUS_ERR | STATUS_ERR_G);
  testDS(P("A&B ERR F"), BOTH | STATUS_ERR | STATUS_ERR_F);
  testDS(P("A&B ERR T"), BOTH | STATUS_ERR | STATUS_ERR_T);
  testDS(P("A&B ERR O"), BOTH | STATUS_ERR | STATUS_ERR_O);
  testDS(P("A&B ERR E"), BOTH | STATUS_ERR | STATUS_ERR_E);
  testDS(P("A&B ERR R"), BOTH | STATUS_ERR | STATUS_ERR_R);


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


