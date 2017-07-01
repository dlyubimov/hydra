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
#include "onlineSum.h"

// If unit tests are declared, this is called the last thing from setup().

static void testEepromSetup() {

  persisted.reset();
  persisted.eepromWrite();


  //read/write test
  persisted.operatingMode = MODE_SEQUENTIAL;
  persisted.enable_dst = true;
  persisted.eepromWrite();
  persisted_struct clone1;
  if (clone1.operatingMode != MODE_SEQUENTIAL || !clone1.enable_dst) {
    logInfo(P("eeprom write/read UNIT FAIL (actual: %d)."), (int)clone1.operatingMode);
    return;
  }

  // invalid signature reset test
  persisted.signature = 0xff;
  persisted.validate();

  persisted_struct clone2;
  if ( persisted.signature != PERSIST_SIG || clone2.signature != PERSIST_SIG || clone2.operatingMode != MODE_SHARED ) {
    logInfo(P("eeprom sig validation UNIT FAIL."));
    return;
  }

  logDebug(P("eeprom UNIT OK."));

}


static void showDS(char* desc, unsigned int status) {
  display.clear();
  display.print (desc);
  displayStatus(status);
  Delay(200);
}

void testDisplayStatus() {
  showDS(P("A&B UNPL"), BOTH | STATUS_UNPLUGGED);
  showDS(P("A&B off"), BOTH | STATUS_OFF);
  showDS(P("B off tie"), CAR_B | STATUS_OFF | STATUS_TIEBREAK);
  showDS(P("A off tie"), CAR_A | STATUS_OFF | STATUS_TIEBREAK);
  showDS(P("A&B on"), BOTH | STATUS_ON);
  showDS(P("A&B done"), BOTH | STATUS_DONE);
  showDS(P("A&B wait"), BOTH | STATUS_WAIT);

  char* msg = P("incorrect letter %c UNIT FAIL");
  if ( 'F' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_F) )
    logInfo(msg, 'F');
  if ( 'O' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_O) )
    logInfo(msg, 'O');
  if ( 'G' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_G) )
    logInfo(msg, 'G');
  if ( 'T' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_T) )
    logInfo(msg, 'T');
  if ( 'R' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_R) )
    logInfo(msg, 'R');
  if ( 'E' != errLetter(BOTH | STATUS_ERR | STATUS_ERR_E) )
    logInfo(msg, 'E');

  showDS(P("A&B ERR G"), BOTH | STATUS_ERR | STATUS_ERR_G);
  showDS(P("A&B ERR F"), BOTH | STATUS_ERR | STATUS_ERR_F);
  showDS(P("A&B ERR T"), BOTH | STATUS_ERR | STATUS_ERR_T);
  showDS(P("A&B ERR O"), BOTH | STATUS_ERR | STATUS_ERR_O);
  showDS(P("A&B ERR E"), BOTH | STATUS_ERR | STATUS_ERR_E);
  showDS(P("A&B ERR R"), BOTH | STATUS_ERR | STATUS_ERR_R);

  if ( cars[0].carLetter() != 'A' || cars[1].carLetter() != 'B' )
    logInfo(P("carLetter() UNIT FAIL"));

  if ( cars[0].dispCol() != 0 || cars[1].dispCol() != 8 )
    logInfo(P("dispCol() UNIT FAIL"));

  logInfo (P("displayStatus DONE."));
}


static void testMenuSetup() {
  inMenu = true;
  doMenu(true);
}

static void testEWASumSetup() {
  //  logInfo(P("sizeof(double)=%d"), sizeof(double));
  //  logInfo(P("sizeof(float)=%d"), sizeof(float));

  EWASumD sum(100);

  boolean fail = false;
  sum.update(5, 1000);
  if ( abs(sum.ewa() - 5.0) > 1e-10) fail = true;

    sum.update(10, 1100);
  if ( abs(sum.ewa() - 7.5) > 1e-10) fail = true;

  // update-in-the-past test.
  sum.reset();

  sum.update(10, 1100);
  if ( abs(sum.ewa() - 10) > 1e-10) fail = true;

  sum.update(5, 1000);
  if ( abs(sum.ewa() - 7.5) > 1e-10) fail = true;

  if ( fail) logInfo(P("EWASum UNIT FAIL"));
  else logInfo(P("EWASum DONE."));
}


int unitsSetup() {

  testEepromSetup();
  testEWASumSetup();
  testDisplayStatus();
  testMenuSetup();
  return false;
}

// This is called
int unitsLoop() {
  return false;
}


