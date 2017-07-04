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



static char* strDate(time_t time) {
  static char str[11];
  tmElements_t els;
  breakTime(time, els);
  snprintf(str, sizeof(str), "%d/%d/%d", (int)els.Month, (int)els.Day, (int)tmYearToCalendar(els.Year));
  return str;
}

///////////////////////////////////////////////////
// If unit tests are declared, these tests are called the last thing from setup().

#define assert(_cond_, _str_) if (! _cond_) { logInfo(P("%s UNIT FAIL."), _str_); return; }
#define ok(_str_) logInfo(P("%s UNIT OK."), _str_);

static void testDstSetup() {
  tmElements_t els;
  els.Year = CalendarYrToTm(2017);
  els.Month = 7;
  els.Day = 3;
  els.Hour = 0;

  assert(!strcmp(strDate(makeTime(els)), "7/3/2017"), "strDate");

  // Tests for finding previous. successive day of week. 
  // Corner case: same day must point at the day in both cases.
  assert(!strcmp(strDate(nextDow(makeTime(els), 2)), "7/3/2017"), "nextDow"); // Next monday
  assert(!strcmp(strDate(previousDow(makeTime(els), 2)), "7/3/2017"), "prevDow"); // previous monday

  // not same day.
  assert(!strcmp(strDate(nextDow(makeTime(els), 3)), "7/4/2017"), "nextDow(2)"); // Next Tuesday 7/4/2017
  assert(!strcmp(strDate(previousDow(makeTime(els), 3)), "6/27/2017"), "prevDow(2)"); // previous Tuesday 6/27/2017

  // Month begin.
  assert(!strcmp(strDate(monthBegin(makeTime(els))), "7/1/2017"), "moBegin"); // this month begin 7/1/2017
  assert(!strcmp(strDate(nextMonthBegin(makeTime(els))), "8/1/2017"), "neMoBeg"); // next month begin 8/1/2017


}

static void testEepromSetup() {

  persisted.reset();
  persisted.eepromWrite();


  //read/write test
  persisted.operatingMode = MODE_SEQUENTIAL;
  persisted.enable_dst = true;
  persisted.eepromWrite();
  persisted_struct clone1;

  assert (clone1.operatingMode == MODE_SEQUENTIAL, "eeprom-mode");
  assert(clone1.enable_dst, "eeprom-dst");

  // invalid signature reset test
  persisted.signature = 0xff;
  persisted.validate();

  persisted_struct clone2;
  assert(persisted.signature == PERSIST_SIG, "eeprom-sig");
  assert(clone2.signature == PERSIST_SIG, "eeprom-sig(2)");
  assert(clone2.operatingMode == MODE_SHARED, "eeprom-mode(2)");

  ok("eeprom");
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
  assert( 'F' == errLetter(BOTH | STATUS_ERR | STATUS_ERR_F), "errLetter");
  assert( 'O' == errLetter(BOTH | STATUS_ERR | STATUS_ERR_O), "errLetter" );
  assert( 'G' == errLetter(BOTH | STATUS_ERR | STATUS_ERR_G), "errLetter" );
  assert( 'T' == errLetter(BOTH | STATUS_ERR | STATUS_ERR_T), "errLetter" );
  assert( 'R' == errLetter(BOTH | STATUS_ERR | STATUS_ERR_R), "errLetter" );
  assert( 'E' == errLetter(BOTH | STATUS_ERR | STATUS_ERR_E), "errLetter" );

  showDS(P("A&B ERR G"), BOTH | STATUS_ERR | STATUS_ERR_G);
  showDS(P("A&B ERR F"), BOTH | STATUS_ERR | STATUS_ERR_F);
  showDS(P("A&B ERR T"), BOTH | STATUS_ERR | STATUS_ERR_T);
  showDS(P("A&B ERR O"), BOTH | STATUS_ERR | STATUS_ERR_O);
  showDS(P("A&B ERR E"), BOTH | STATUS_ERR | STATUS_ERR_E);
  showDS(P("A&B ERR R"), BOTH | STATUS_ERR | STATUS_ERR_R);

  assert( cars[0].carLetter() == 'A' && cars[1].carLetter() == 'B', "ds-init");
  assert( cars[0].dispCol() == 0 && cars[1].dispCol() == 8, "ds-col" );

  ok("displayStatus");
}


static void testMenuSetup() {
  inMenu = true;
  doMenu(true);
}

//static void testEWASumSetup() {
//  //  logInfo(P("sizeof(double)=%d"), sizeof(double));
//  //  logInfo(P("sizeof(float)=%d"), sizeof(float));
//
//  EWASumD sum(100);
//
//  sum.update(5, 1000);
//  assert( abs(sum.ewa() - 5.0) < 1e-10, "ewa-sum");
//
//  sum.update(10, 1100);
//  assert( abs(sum.ewa() - 7.5) < 1e-10, "ewa-sum");
//
//  // update-in-the-past test.
//  sum.reset();
//
//  sum.update(10, 1100);
//  assert ( abs(sum.ewa() - 10) < 1e-10, "ewa-sum");
//
//  sum.update(5, 1000);
//  assert( abs(sum.ewa() - 7.5) < 1e-10, "ewa-sum");
//
//  ok("ewa-sum");
//}


int unitsSetup() {

  testEepromSetup();
  testDstSetup();
  //  testEWASumSetup();
  testDisplayStatus();
  testMenuSetup();
  return false;
}

// This is called
int unitsLoop() {
  return false;
}


