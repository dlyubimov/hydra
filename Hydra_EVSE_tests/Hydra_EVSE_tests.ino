#include "KUMAN.h"

#define DISPLAY_DECL(name) KUMAN(name)
#define HYDRA_UNIT_TESTS

#include "../Hydra_EVSE/Hydra_EVSE.ino"



void doUnits() {
  eepromUnit();
  menuUnit();
}

void eepromUnit() {
  persisted.eepromRead();
  persisted.validate();
  
  persisted.operatingMode = MODE_SEQUENTIAL;
  persisted.eepromWrite();
  persisted.eepromRead();
  persisted.validate();
  if ( persisted.operatingMode != MODE_SEQUENTIAL) {
    log(LOG_INFO, P("EEPROM r/w UNIT FAIL"));
    return;
  }

  // Invalid signature test
  persisted.signature = 0xFF;
  persisted.eepromWrite();
  persisted.eepromRead();
  Serial.print(sizeof(persisted.events));
  persisted.validate();
  if ( persisted.signature != PERSIST_SIG ) {
    log(LOG_INFO, P("EEPROM sig UNIT FAIL"));
    return;
  }

  // Invalid value test -- go back to default
  persisted.operatingMode = 35;
  persisted.eepromWrite();
  persisted.eepromRead();
  persisted.validate();
  if ( persisted.operatingMode != MODE_SHARED ) {
    log(LOG_INFO, P("EEPROM validate UNIT FAIL"));
    return;
  }

  log(LOG_INFO, P("EEPROM UNIT OK"));
    
}
void menuUnit() {
  inMenu = true;
  doMenu(true);
}


