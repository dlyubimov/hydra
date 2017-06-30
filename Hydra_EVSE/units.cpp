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
  persisted.signature=0xff;
  persisted.validate();

  persisted_struct clone2;
  if ( persisted.signature != PERSIST_SIG || clone2.signature != PERSIST_SIG || clone2.operatingMode != MODE_SHARED ) {
    log(LOG_INFO, P("eeprom sig validation UNIT FAIL."));
    return;
  }
    
  log(LOG_DEBUG, "eeprom UNIT OK.");

}

static void testMenuSetup() {
  inMenu = true;
  doMenu(true);
}


int unitsSetup() {
  testEepromSetup();
  testMenuSetup();
  return false;
}

// This is called
int unitsLoop() {
  return false;
}

