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

#include "Hydra_EVSE.h"

// "display" global variable definition
DISPLAY_DEF(display);

// You REALLY want to exclude the entries from this list that exceed the
// specifications of your hardware. It's actually somewhat questionable that this
// is actually a menu, but it's just conceivable that a Hydra might be portable,
// which means you might need to adjust it on the fly.
unsigned char currentMenuChoices[] = { 12, 16, 20, 24, 28, 30, 32, 36, 40, 44, 50 /*, 60, 75, 80*/ };

static unsigned char calib_struct::menuItem;


persisted_struct persisted;

// menu 0: operating mode
#define MENU_OPERATING_MODE 0
#define OPTION_SHARED_TEXT "Shared"
#define OPTION_SEQUENTIAL_TEXT "Sequential"
#define MENU_OPERATING_MODE_HEADER "Operating Mode"
// menu 1: current available
#define MENU_CURRENT_AVAIL 1


#define CURRENT_AVAIL_MENU_MAX (sizeof(currentMenuChoices) - 1)
#define MENU_CURRENT_AVAIL_HEADER "Current Avail."

#define OPTION_YES_TEXT "Yes"
#define OPTION_NO_TEXT "No"
// menu 2: set time
#define MENU_CLOCK 2
#define MENU_CLOCK_HEADER "Set Clock?"
// menu 3: DST
#define MENU_DST 3
#define MENU_DST_HEADER "Enable DST?"
// menu 4: event alarms
#define MENU_EVENT 4
#define MENU_EVENT_HEADER "Config Events?"

//menu 5: calibrations
#define MENU_CALIB 5
#define MENU_CALIB_HEADER "Calibration?"

// menu 6: exit
#define MENU_EXIT 6
#define MENU_EXIT_HEADER "Exit Menus?"

// end menus
#define MAX_MENU_NUMBER MENU_EXIT

#define DAY_FLAGS "SMTWTFS"

// What range of years are we going to allow? As time goes by, this can be incremented.
#define FIRST_YEAR 2010
#define LAST_YEAR 2020

// We don't really need to support timezones. We just want to perform automatic DST switching.
// The following are the U.S. DST rules. If you live elsewhere, the customize these. The params
// are a descriptive string (not used, but must be 5 chars or less), a "descriptor" for which
// weekday in the active month is in the rule (first, second... last), the day of the week,
// the month, and the hour of the day. The last parameter is the offset in minutes. You should
// have the "winter" rule have a 0 offset so that turning off DST returns you to winter time.
// The summer offset should be relative to winter time (probably +60 minutes).
#if 1
// US rules
TimeChangeRule summer = { "DST", Second, Sun, Mar, 2, 60 };
TimeChangeRule winter = { "ST", First, Sun, Nov, 2, 0 };
#endif
#if 0
// European Union
TimeChangeRule summer = { "DST", Last, Sun, Mar, 1, 60 };
TimeChangeRule winter = { "ST", Last, Sun, Oct, 1, 0 };
#endif
#if 0
// Australia - note the reversal due to the Southern hemisphere
TimeChangeRule summer = { "DST", First, Sun, Oct, 2, 60 };
TimeChangeRule winter = { "ST", First, Sun, Apr, 2, 0 };
#endif
Timezone dst(summer, winter);

// Thanks to Gareth Evans at http://todbot.com/blog/2008/06/19/how-to-do-big-strings-in-arduino/
// Note that you must be careful not to use this macro more than once per "statement", lest you
// risk overwriting the buffer before it is used. So no using it inside methods that return
// strings that are then used in snprintf statements that themselves use this macro.
char p_buffer[96];

unsigned long car_a_current_samples[ROLLING_AVERAGE_SIZE], car_b_current_samples[ROLLING_AVERAGE_SIZE];
//unsigned long incomingPilotMilliamps;
unsigned int last_car_a_state, last_car_b_state;
unsigned long car_a_overdraw_begin, car_b_overdraw_begin;
unsigned long car_a_request_time, car_b_request_time;
unsigned long car_a_error_time, car_b_error_time;
unsigned long last_current_log_car_a, last_current_log_car_b;
unsigned long last_state_log;
unsigned long sequential_pilot_timeout;
boolean seq_car_a_done = false, seq_car_b_done = false;
unsigned int pilot_state_a, pilot_state_b;
unsigned int operatingMode, sequential_mode_tiebreak;
unsigned long button_press_time, button_debounce_time;
#ifdef GROUND_TEST
unsigned char current_ground_status;
#endif
#ifdef QUICK_CYCLING_WORKAROUND
unsigned long pilot_release_holdoff_time;
#endif
// These volatile ones are touched by the GFI interrupt handler
volatile unsigned int relay_state_a, relay_state_b;
volatile unsigned long relay_change_time;
volatile boolean gfiTriggered = false;
boolean paused = false;
boolean enterPause = false;
boolean inMenu = false;

// current menu handler
void (*doMenuFunc)(boolean) = doMenu;

unsigned int menu_number; // which menu are we presently in?
unsigned int menu_item; // which item within the present menu is the currently displayed option?
unsigned int menu_item_max; // for the current menu, what's the maximum item number?
unsigned int menu_item_selected;  // for the current menu, which option is presently selected?
int last_minute;
unsigned char editHour, editMinute, editDay, editMonth, editCursor, editEvent, editDOW, editType;
#ifndef CLOCK_24HOUR
unsigned char editMeridian;
#endif
unsigned int editYear;
boolean blink;
boolean enable_dst;

/////////////////////////////////////////
// persisted_struct implementation

void persisted_struct::validate() {

  boolean valid = false;

  // Check signature. if we did not ever write this in compatible way, reset everything to defaults and re-write.
  if ( signature != PERSIST_SIG) {

    reset();
    eepromWrite();
    return;

  }
  if ( operatingMode != MODE_SHARED && operatingMode != MODE_SEQUENTIAL ) operatingMode = MODE_SHARED;

  for (int i = 0; i < sizeof (currentMenuChoices); i++ ) if ( currentMenuChoices[i] == max_amps ) valid = true;
  if ( !valid) max_amps = currentMenuChoices[0];

  for (int i = 0; i < EVENT_COUNT; i ++ ) events [i].validate();

  calib.validate();
}

void persisted_struct::reset() {
  signature = PERSIST_SIG;
  operatingMode = MODE_SHARED;
  max_amps = currentMenuChoices[0];
  enable_dst = false;
  rtc_cal = 0;
  for (int i = 0; i < EVENT_COUNT; i++) events[i].reset();
  calib.reset();
}

// persisted_struct implementation
/////////////////////////////////////////


void logImpl(unsigned int level, const char * fmt_str, ...) {
#if SERIAL_LOG_LEVEL > 0
  if (level > SERIAL_LOG_LEVEL) return;
  char buf[96]; // Danger, Will Robinson!
  va_list argptr;
  va_start(argptr, fmt_str);
  vsnprintf(buf, sizeof(buf), fmt_str, argptr);
  va_end(argptr);

  switch (level) {
    case LOG_INFO:
      Serial.print(P("INFO: "));
      break;
    case LOG_DEBUG:
      Serial.print(P("DEBUG: "));
      break;
    case LOG_TRACE:
      Serial.print(millis());
      Serial.print(P(" TRACE: "));
      break;
    default:
      Serial.print(P("UNKNOWN: "));
      break;
  }
  Serial.println(buf);
#endif
}

static inline const char *car_str(unsigned int car) {
  switch (car) {
    case CAR_A: return "car A";
    case CAR_B: return "car B";
    case BOTH: return "both car";
    default: return "UNKNOWN";
  }
}

static inline const char *logic_str(unsigned int state) {
  switch (state) {
    case LOW: return "LOW";
    case HIGH: return "HIGH";
    case HALF: return "HALF";
    case FULL: return "FULL";
    default: return "UNKNOWN";
  }
}

static inline const char* state_str(unsigned int state) {
  switch (state) {
    case STATE_A:
      return "A";
    case STATE_B:
      return "B";
    case STATE_C:
      return "C";
    case STATE_D:
      return "D";
    case STATE_E:
      return "E";
    default:
      return "UNKNOWN";
  }
}

// Convert a milliamp allowance into an outgoing pilot duty cycle.
// In lieu of floating point, this is duty in mils (tenths of a percent)
static inline unsigned int MAToDuty(unsigned long milliamps) {
  if (milliamps < 6000) {
    return 9999; // illegal - set pilot to "high"
  }
  else if (milliamps < 51000) {
    return milliamps / 60;
  }
  else if (milliamps <= 80000) {
    return (milliamps / 250) + 640;
  }
  else {
    return 9999; // illegal - set pilot to "high"
  }
}

// Convert a milliamp allowance into a value suitable for
// pwmWrite - a scale from 0 to 255.
inline static unsigned int MAtoPwm(unsigned long milliamps) {
  unsigned int out = MAToDuty(milliamps);

  if (out >= 1000) return 255; // full on

  out = (unsigned int)((out * 256L) / 1000);

  return out;
}

// Turn a millamp value into nn.n as amps, with the tenth rounded near.
char *formatMilliamps(unsigned long milliamps) {
  static char out[6];

  if (milliamps < 1000) {
    // truncate the units digit - there's no way we're that accurate.
    milliamps /= 10;
    milliamps *= 10;

    sprintf(out, P("%3lumA"), milliamps);
  }
  else {
    int hundredths = (milliamps / 10) % 100;
    int tenths = hundredths / 10 + (((hundredths % 10) >= 5) ? 1 : 0);
    int units = milliamps / 1000;
    if (tenths >= 10) {
      tenths -= 10;
      units++;
    }

    sprintf(out, P("%2d.%01dA"), units, tenths);
  }

  return out;
}

char errLetter(unsigned int status) {
  switch ( status & STATUS_ERR_MASK ) {
    case STATUS_ERR_O: return 'O';
    case STATUS_ERR_F: return 'F';
    case STATUS_ERR_G: return 'G';
    case STATUS_ERR_T: return 'T';
    case STATUS_ERR_R: return 'R';
    case STATUS_ERR_E: return 'E';
    default: return '?';
  }
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
  display.print(':');  // 2

  logDebug(P("status: %x, CAR:%c, status bits: %x, status mask: %x."), status, carLetter, status & STATUS_MASK, STATUS_MASK);

  switch ( status & STATUS_MASK) { // 7
    case STATUS_UNPLUGGED: display.print(P(" --- ")); break;
    case  STATUS_OFF: display.print(P(" off ")); break;
    case  STATUS_ON: display.print(P(" on  ")); break;
    case  STATUS_WAIT: display.print(P(" wait")); break;
    case  STATUS_DONE: display.print(P(" done")); break;

    case  STATUS_ERR:
    default:
      // ERROR
      display.setBacklight(RED);
      display.print(P("ERR "));
      display.print(errLetter(status & STATUS_ERR_MASK));
      break;
  }
  display.print(" "); // 8
  if ( (status & STATUS_TIEBREAK) != 0) {
    display.setCursor(col + 6, 1);
    display.print('*');
  }

}

void error(unsigned int status) {
  unsigned long now = millis(); // so both cars get the same time.
  // Set the pilot to constant 12: indicates an EVSE error.
  // We can't use -12, because then we'd never detect a return
  // to state A. But the spec says we're allowed to return to B1
  // (that is, turn off the oscillator) whenever we want.
  // Stop flipping, one way or another
  unsigned int car = status & CAR_MASK;
  sequential_pilot_timeout = 0;
  if (car == BOTH || car == CAR_A) {
    setPilot(CAR_A, HIGH);
    if (last_car_a_state != STATE_E) {
      last_car_a_state = STATE_E;
      car_a_error_time = now;
    }
    car_a_request_time = 0;
  }
  if (car == BOTH || car == CAR_B) {
    setPilot(CAR_B, HIGH);
    if (last_car_b_state != STATE_E) {
      last_car_b_state = STATE_E;
      car_b_error_time = now;
    }
    car_b_request_time = 0;
  }

  //  display.setBacklight(RED);
  //  if (car == BOTH || car == CAR_A) {
  //    display.setCursor(0, 1);
  //    display.print(P("A:ERR "));
  //    display.print(err);
  //    display.print(' ');
  //  }
  //  if (car == BOTH || car == CAR_B) {
  //    display.setCursor(8, 1);
  //    display.print(P("B:ERR "));
  //    display.print(err);
  //    display.print(' ');
  //  }
  //
  displayStatus(status);
  logInfo(P("Error %c on %s"), errLetter(status), car_str(car));
}

void gfi_trigger() {
  // Make sure both relays are *immediately* flipped off.
  digitalWrite(CAR_A_RELAY, LOW);
  digitalWrite(CAR_B_RELAY, LOW);
  // Now make the data consistent. Make sure that anything you touch here is declared "volatile"
  relay_state_a = relay_state_b = LOW;
  relay_change_time = millis();
  // We don't have time in an IRQ to do more than that.
  gfiTriggered = true;
}

void setRelay(unsigned int car, unsigned int state) {
  if (relay_state_a == LOW && relay_state_b == LOW && state == HIGH) {
    // We're transitioning from no car to one car - insert a GFI self test.
    gfiSelfTest();
  }
  logDebug(P("Setting %s relay to %s"), car_str(car), logic_str(state));
  switch (car) {
    case CAR_A:
      if (relay_state_a == state) return; // did nothing.
      digitalWrite(CAR_A_RELAY, state);
      relay_state_a = state;
      break;
    case CAR_B:
      if (relay_state_b == state) return; // did nothing.
      digitalWrite(CAR_B_RELAY, state);
      relay_state_b = state;
      break;
  }
  // This only counts if we actually changed anything.
  relay_change_time = millis();
}

// If it's in an error state, it's not charging (the relay may still be on during error delay).
// If it's in a transition delay, then it's "charging" (the relay is off during transition delay).
// Otherwise, check the state of the relay.
static inline boolean isCarCharging(unsigned int car) {
  if (paused) return false;
  switch (car) {
    case CAR_A:
      if (last_car_a_state == STATE_E) return LOW;
      if (car_a_request_time != 0) return HIGH;
      return relay_state_a;
      break;
    case CAR_B:
      if (last_car_b_state == STATE_E) return LOW;
      if (car_b_request_time != 0) return HIGH;
      return relay_state_b;
      break;
    default:
      return LOW; // This should not be possible
  }
}

// Set the pilot for the car as appropriate. 'which' is either HALF, FULL, LOW or HIGH.
// HIGH sets a constant +12v, which is the spec for state A, but we also use it for
// state E. HALF means that the other car is charging, so we only can have half power.

void setPilot(unsigned int car, unsigned int which) {
  logDebug(P("Setting %s pilot to %s"), car_str(car), logic_str(which));
  // set the outgoing pilot for the given car to either HALF state, FULL state, or HIGH.
  int pin;
  char pilot_derate;
  switch (car) {
    case CAR_A:
      pin = CAR_A_PILOT_OUT_PIN;
      pilot_state_a = which;
      pilot_derate = persisted.calib.pilot_a;
      break;
    case CAR_B:
      pin = CAR_B_PILOT_OUT_PIN;
      pilot_state_b = which;
      pilot_derate = persisted.calib.pilot_b;
      break;
    default: return;
  }
  if (which == LOW || which == HIGH) {
    // This is what the pwm library does anyway.
    logTrace(P("Pin %d to digital %d"), pin, which);
    digitalWrite(pin, which);
  }
  else {
    unsigned long ma = 1000ul * persisted.max_amps;
    if (which == HALF) ma /= 2;
    // Calibrate
    if (pilot_derate != 0) {
      // pilot_derate is usally negative percentages (0, -1, -2 .. -CALIB_PILOT_MAX)
      ma = ma * (100 + pilot_derate ) / 100;
      // but no less than the minimum.
      if (ma < 6000) ma = 6000;
    }
    if (ma > MAXIMUM_OUTLET_CURRENT) ma = MAXIMUM_OUTLET_CURRENT;
    unsigned int val = MAtoPwm(ma);
    logTrace(P("Pin %d to PWM %d"), pin, val);
    pwmWrite(pin, val);
  }
}

static inline unsigned int pilotState(unsigned int car) {
  return (car == CAR_A) ? pilot_state_a : pilot_state_b;
}

int checkState(unsigned int car) {
  // poll the pilot state pin for 10 ms (should be 10 pilot cycles), looking for the low and high.
  unsigned int low = 9999, high = 0;
  unsigned int car_pin = (car == CAR_A) ? CAR_A_PILOT_SENSE_PIN : CAR_B_PILOT_SENSE_PIN;
  unsigned long count = 0;
  for (unsigned long start = millis(); millis() - start < STATE_CHECK_INTERVAL; ) {
    unsigned int val = analogRead(car_pin);
    if (val > high) high = val;
    if (val < low) low = val;
    count++;
  }

  logTrace(P("%s high %u low %u count %lu"), car_str(car), high, low, count);

  // If the pilot low was below zero, then that means we must have
  // been oscillating. If we were, then perform the diode check.
  if (low < PILOT_0V && low > PILOT_DIODE_MAX) {
    return STATE_E; // diode check fail
  }
  if (high >= STATE_A_MIN) {
    return STATE_A;
  }
  else if (high >= STATE_B_MIN && high <= STATE_B_MAX) {
    return STATE_B;
  }
  else if (high >= STATE_C_MIN && high <= STATE_C_MAX) {
    return STATE_C;
  }
  else if (high >= STATE_D_MIN && high <= STATE_D_MAX) {
    return STATE_D;
  }
  // I dunno how we got here, but we fail it.
  return STATE_E;
}

static inline unsigned long ulong_sqrt(unsigned long in) {
  unsigned long out;
  // find the last int whose square is not too big
  // Yes, it's wasteful, but we only theoretically ever have to go to 512.
  // Removing floating point saves us almost 1K of flash.
  for (out = 1; out * out <= in; out++) ;
  return out - 1;
  //  return (unsigned long)sqrt((float)in);
}

unsigned long readCurrent(unsigned int car) {
  unsigned int car_pin = (car == CAR_A) ? CAR_A_CURRENT_PIN : CAR_B_CURRENT_PIN;
  calib_struct& calib = persisted.calib;
  char calib_amm = car == CAR_A ? calib.amm_a : calib.amm_b;
  unsigned long sum = 0;
  unsigned int zero_crossings = 0;
  unsigned long last_zero_crossing_time = 0, now_ms;
  long last_sample = -1; // should be impossible - the A/d is 0 to 1023.
  unsigned int sample_count = 0;
  for (unsigned long start = millis(); (now_ms = millis()) - start < CURRENT_SAMPLE_INTERVAL; ) {
    long sample = analogRead(car_pin);
    // If this isn't the first sample, and if the sign of the value differs from the
    // sign of the previous value, then count that as a zero crossing.
    if (last_sample != -1 && ((last_sample > 512) != (sample > 512))) {
      // Once we've seen a zero crossing, don't look for one for a little bit.
      // It's possible that a little noise near zero could cause a two-sample
      // inversion.
      if (now_ms - last_zero_crossing_time > CURRENT_ZERO_DEBOUNCE_INTERVAL) {
        zero_crossings++;
        last_zero_crossing_time = now_ms;
      }
    }
    last_sample = sample;
    switch (zero_crossings) {
      case 0:
        continue; // Still waiting to start sampling
      case 1:
      case 2:
        // Gather the sum-of-the-squares and count how many samples we've collected.
        sum += (unsigned long)((sample - 512) * (sample - 512));
        sample_count++;
        continue;
      case 3:
        // The answer is the square root of the mean of the squares.
        // But additionally, that value must be scaled to a real current value.
        sum = ulong_sqrt(sum / sample_count) * CURRENT_SCALE_FACTOR;
        // Only apply calibration on readings meaningfully high.
        if ( sum > 5000 ) sum += 100 * calib_amm;
        return sum;
    }
  }
  // ran out of time. Assume that it's simply not oscillating any.
  return 0;
}

unsigned long rollRollingAverage(unsigned long array[], unsigned long new_value) {
#if ROLLING_AVERAGE_SIZE == 0
  return new_value;
#else
  unsigned long sum = new_value;
  for (int i = ROLLING_AVERAGE_SIZE - 1; i >= 1; i--) {
    array[i] = array[i - 1];
    sum += array[i];
  }
  array[0] = new_value;
  return (sum / ROLLING_AVERAGE_SIZE);
#endif
}

// So the desired logic is as follows:
// (1) If one or none cars are plugged, the behavior is really no different from shared mode.
// (2) if two cars are plugged,
// (2a) on un-pause the tie is broken with last car charging during last non-pause, or last car plugged during pause.
// (2b) once cars are done charging, do not keep flipping -- keep their state B-HIGH. This is reset by entering pause,
// OR any of the cars unplugged, in which case "done" restrictions are foregone, as flipping becomes non-issue -- the
// car will keep itself off even if we are at FULL advertisement.

void sequential_mode_transition(unsigned int us, unsigned int car_state) {
  unsigned int them = (us == CAR_A) ? CAR_B : CAR_A;
  unsigned int *last_car_state = (us == CAR_A) ? &last_car_a_state : &last_car_b_state;
  unsigned int their_state = (us == CAR_A) ? last_car_b_state : last_car_a_state;
  boolean& us_done = us == CAR_A ? seq_car_a_done : seq_car_b_done;
  boolean& them_done = them == CAR_A ? seq_car_a_done : seq_car_b_done;

  switch (car_state) {
    case STATE_A:
      // No matter what, insure that the pilot and relay are off.
      setRelay(us, LOW);
      setPilot(us, HIGH);
      // We're not both in state B anymore.
      sequential_pilot_timeout = 0;
      // We don't exist. If they're waiting, they can have it.
      if (their_state == STATE_B)
      {
        setPilot(them, FULL);
        displayStatus( them | STATUS_OFF );
        //        display.setCursor((them == CAR_A) ? 0 : 8, 1);
        //        display.print((them == CAR_A) ? "A" : "B");
        //        display.print(P(": off  "));
      }
      displayStatus( us | STATUS_UNPLUGGED );
      //      display.setCursor((us == CAR_A) ? 0 : 8, 1);
      //      display.print((us == CAR_A) ? "A" : "B");
      //      display.print(P(": ---  "));
      // reset done state for all
      us_done = false;
      them_done = false;
      break;
    case STATE_B:
      // No matter what, insure that the relay is off.
      setRelay(us, LOW);
      if (*last_car_state == STATE_C || *last_car_state == STATE_D) {
        // We transitioned from C/D to B. That means we're passing the batton
        // to the other car, if they want it and not marked "done" yet.
        if (their_state == STATE_B) {
          if ( ! them_done) {
            // flip only if they are not done yet, otherwise wait for pilot timeout before we do again.
            setPilot(us, HIGH);
            setPilot(them, FULL);
          }
          displayStatus(them | ( them_done ? STATUS_DONE : STATUS_OFF ));
          //          display.setCursor((them == CAR_A) ? 0 : 8, 1);
          //          display.print((them == CAR_A) ? "A" : "B");
          //          if (them_done) display.print(P(": done ")); else display.print(P(": off  "));


          // differentiated from "wait" because a C/D->B transition has occurred.
          displayStatus(us | STATUS_DONE);
          //          display.setCursor((us == CAR_A) ? 0 : 8, 1);
          //          display.print((us == CAR_A) ? "A" : "B");
          //          display.print(P(": done ")); // differentiated from "wait" because a C/D->B transition has occurred.
          // Disable future charges for this car until re-unpaused or re-plugged.
          us_done = true;
          sequential_pilot_timeout = millis(); // We're both now in B. Start flipping.
        } else {

          displayStatus(us | STATUS_OFF);
          //          display.setCursor((us == CAR_A) ? 0 : 8, 1);
          //          display.print((us == CAR_A) ? "A" : "B");
          //          display.print(P(": off  "));
          // their state is not B, so we're not "flipping"
          sequential_pilot_timeout = 0;
        }
      } else {
        if (their_state == STATE_A) {
          // We can only grab the batton if they're not plugged in at all.
          setPilot(us, FULL);
          us_done = false;
          them_done = false;
          sequential_pilot_timeout = 0;

          displayStatus(us | STATUS_OFF );
          //          display.setCursor((us == CAR_A) ? 0 : 8, 1);
          //          display.print((us == CAR_A) ? "A" : "B");
          //          display.print(P(": off  "));

          break;
        } else if (their_state == STATE_B || their_state == DUNNO) {
          // BUT if we're *both* in state b, then that's a tie. We break the tie with our saved tiebreak value.
          // If it's not us, then we simply ignore this transition entirely. The other car will wind up in this same place,
          // we'll turn their pilot on, and then clear the tiebreak. Next time we roll through, we'll go into the other
          // half of this if/else and we'll get the "wait" display
          if (sequential_mode_tiebreak != us ) {
            return;
          }
          // But if it IS us, then clear the tiebreak.
          if (!us_done && (sequential_mode_tiebreak == us )) {
            setPilot(us, FULL);
            sequential_pilot_timeout = millis();

            displayStatus (us | STATUS_OFF);
            //            display.setCursor((us == CAR_A) ? 0 : 8, 1);
            //            display.print((us == CAR_A) ? "A" : "B");
            //            display.print(P(": off  "));
            break;
          }
        }
        // Either they are in state C/D or they're in state B and we lost the tiebreak.
        displayStatus(us | ( us_done ? STATUS_DONE : STATUS_WAIT));
        //        display.setCursor((us == CAR_A) ? 0 : 8, 1);
        //        display.print((us == CAR_A) ? "A" : "B");
        //        if ( us_done)
        //          display.print(P(": done "));
        //        else
        //          display.print(P(": wait "));
      }
      break;
    case STATE_C:
    case STATE_D:
      if (isCarCharging(us)) {
        // we're already charging. This might be a flip from C to D. Ignore it.
        break;
      }
      if (pilotState(us) != FULL) {
        error(us | STATUS_ERR | STATUS_ERR_T); // illegal transition: no state C without a pilot
        return;
      }
      // We're not both in state B anymore
      sequential_pilot_timeout = 0;
      displayStatus(us | STATUS_ON );
      //      display.setCursor((us == CAR_A) ? 0 : 8, 1);
      //      display.print((us == CAR_A) ? "A" : "B");
      //      display.print(P(": ON   "));
      setRelay(us, HIGH); // turn on the juice
      break;
    case STATE_E:
      error(us | STATUS_ERR | STATUS_ERR_E);
      return;
  }
  *last_car_state = car_state;
}

void shared_mode_transition(unsigned int us, unsigned int car_state) {
  unsigned int them = (us == CAR_A) ? CAR_B : CAR_A;
  unsigned int *last_car_state = (us == CAR_A) ? &last_car_a_state : &last_car_b_state;
  unsigned long *car_request_time = (us == CAR_A) ? &car_a_request_time : &car_b_request_time;

  *last_car_state = car_state;
  switch (car_state) {
    case STATE_A:
    case STATE_B:
      // We're in an "off" state of one sort or other.
      // In either case, clear any connection delay timer,
      // make sure the relay is off, and set the diplay
      // appropriately. For state A, set our pilot high,
      // and fot state B, set it to half if the other car
      // is charging, full otherwise.
      setRelay(us, LOW);
      setPilot(us, car_state == STATE_A ? HIGH : (isCarCharging(them) ? HALF : FULL));
      displayStatus(us | (car_state == STATE_A ? STATUS_UNPLUGGED : STATUS_OFF));
      //      display.setCursor((us == CAR_A) ? 0 : 8, 1);
      //      display.print((us == CAR_A) ? "A" : "B");
      //      display.print(car_state == STATE_A ? ": ---  " : ": off  ");
      *car_request_time = 0;
#ifdef QUICK_CYCLING_WORKAROUND
      if (!isCarCharging(them)) {
        // If the other car isn't actually charging, then we don't
        // need to bother being tricky.
        if (pilotState(them) == HALF)
          setPilot(them, FULL);
        pilot_release_holdoff_time = 0;
      } else
#endif
        if (pilotState(them) == HALF) {
#ifdef QUICK_CYCLING_WORKAROUND
          // Since they're charging, in *this* much time, we'll give the other car a full pilot.
          pilot_release_holdoff_time = millis() + (PILOT_RELEASE_HOLDOFF_MINUTES * (1000L * 60));
#else
          setPilot(them, FULL);
#endif
        }
      break;
    case STATE_C:
    case STATE_D:
      if (isCarCharging(us)) {
        // we're already charging. This might be a flip from C to D. Ignore it.
        break;
      }
      if (isCarCharging(them)) {
#ifdef QUICK_CYCLING_WORKAROUND
        if (pilot_release_holdoff_time != 0) {
          // We turned back on before the grace period. We can just go, since they
          // still have a half-pilot.
          pilot_release_holdoff_time = 0; // cancel the grace period
          setPilot(them, HALF); // *should* be redundant
          setPilot(us, HALF); // redundant, unless we went straight from A to C.
          displayStatus(us | STATUS_ON);
          //          display.setCursor((us == CAR_A) ? 0 : 8, 1);
          //          display.print((us == CAR_A) ? "A" : "B");
          //          display.print(P(": ON   "));
          setRelay(us, HIGH);
        } else {
#endif
          // if they are charging, we must transition them.
          *car_request_time = millis();
          // Drop car A down to 50%
          setPilot(them, HALF);
          setPilot(us, HALF); // this is redundant unless we are transitioning from A to C suddenly.
          displayStatus(us | STATUS_WAIT);
          //          display.setCursor((us == CAR_A) ? 0 : 8, 1);
          //          display.print((us == CAR_A) ? "A" : "B");
          //          display.print(P(": wait "));
#ifdef QUICK_CYCLING_WORKAROUND
        }
#endif
      } else {
        // if they're not charging, then we can just go. If they have a full pilot, they get downshifted.
        if (pilotState(them) == FULL)
          setPilot(them, HALF);
        setPilot(us, FULL); // this is redundant unless we are going directly from A to C.
        *car_request_time = 0;
        displayStatus(us | STATUS_ON);
        //        display.setCursor((us == CAR_A) ? 0 : 8, 1);
        //        display.print((us == CAR_A) ? "A" : "B");
        //        display.print(P(": ON   "));
        setRelay(us, HIGH);
      }
      break;
    case STATE_E:
      error(us | STATUS_ERR | STATUS_ERR_E);
      break;
  }
}

unsigned int checkTimer() {

  event_struct* events(persisted.events);

  unsigned char ev_hour = hour(localTime());
  unsigned char ev_minute = minute(localTime());
  unsigned char ev_dow = dayOfWeek(localTime());
  unsigned char ev_dow_mask = 1 << (ev_dow - 1);
  for (unsigned int i = 0; i < EVENT_COUNT; i++) {
    if (events[i].event_type == TE_NONE) continue; // This one doesn't count. It's turned off.
    if (events[i].hour == ev_hour && events[i].minute == ev_minute && (events[i].dow_mask & ev_dow_mask) != 0) {
      // match!
      return events[i].event_type;
    }
  }
  return TE_NONE;
}

unsigned int checkEvent() {
  logTrace(P("Checking for button event"));
  if (button_debounce_time != 0 && millis() - button_debounce_time < BUTTON_DEBOUNCE_INTERVAL) {
    // debounce is in progress
    return EVENT_NONE;
  } else {
    // debounce is over
    button_debounce_time = 0;
  }
  unsigned int buttons = display.readButtons();
  logTrace(P("Buttons %d"), buttons);
  if ((buttons & BUTTON) != 0) {
    logTrace(P("Button is down"));
    // Button is down
    if (button_press_time == 0) { // this is the start of a press.
      button_debounce_time = button_press_time = millis();
    }
    return EVENT_NONE; // We don't know what this button-push is going to be yet
  } else {
    logTrace(P("Button is up"));
    // Button released
    if (button_press_time == 0) return EVENT_NONE; // It wasn't down anyway.
    // We are now ending a button-push. First, start debuncing.
    button_debounce_time = millis();
    unsigned long button_pushed_time = button_debounce_time - button_press_time;
    button_press_time = 0;
    if (button_pushed_time > BUTTON_LONG_START) {
      logDebug(P("Button long-push event"));
      return EVENT_LONG_PUSH;
    } else {
      logDebug(P("Button short-push event"));
      return EVENT_SHORT_PUSH;
    }
  }
}

// Just like delay(), but petting the watchdog while we're at it
void Delay(unsigned int t) {
  while (t > 100) {
    delay(100);
    t -= 100;
    wdt_reset();
  }
  delay(t);
  wdt_reset();
}

static void die() {
  // set both pilots to -12
  setPilot(CAR_A, LOW);
  setPilot(CAR_B, LOW);
  // make sure both relays are off
  setRelay(CAR_A, LOW);
  setRelay(CAR_B, LOW);
  logInfo(P("UNIT IS DEAD."));
  // and goodnight
  do {
    wdt_reset(); // keep petting the dog, but do nothing else.
  } while (1);
}

static void gfiTestFailure(unsigned char state) {
  display.setBacklight(RED);
  display.clear();
  display.print(P("GFI Test Failure"));
  display.setCursor(0, 1);
  display.print(P("Stuck "));
  if (state)
    display.print(P("set"));
  else
    display.print(P("clear"));
  die();
}

static void gfiSelfTest() {
  gfiTriggered = false;
  for (int i = 0; i < GFI_TEST_CYCLES; i++) {
    digitalWrite(GFI_TEST_PIN, HIGH);
    delayMicroseconds(GFI_PULSE_DURATION_MS);
    digitalWrite(GFI_TEST_PIN, LOW);
    delayMicroseconds(GFI_PULSE_DURATION_MS);
    if (gfiTriggered) break; // no need to keep trying.
    wdt_reset();
  }
  if (!gfiTriggered) gfiTestFailure(0);
  unsigned long clearStart = millis();
  while (digitalRead(GFI_PIN) == HIGH) {
    wdt_reset();
    if (millis() > clearStart + GFI_TEST_CLEAR_TIME) gfiTestFailure(1);
  }
  Delay(GFI_TEST_DEBOUNCE_TIME);
  gfiTriggered = false;
}

static inline time_t localTime() {
  return enable_dst ? dst.toLocal(now()) : now();
}

void doClockMenu(boolean initialize) {
  unsigned int event = checkEvent();
  if (initialize) {
    display.clear();
    display.print(P("Set Clock"));
#ifdef CLOCK_24HOUR
    editHour = hour(localTime());
#else
    editHour = hourFormat12(localTime());
    editMeridian = isPM(localTime()) ? 1 : 0;
#endif

    editMinute = minute(localTime());
    editDay = day(localTime());
    editMonth = month(localTime());
    editYear = year(localTime());
    if (editYear < FIRST_YEAR || editYear > LAST_YEAR) editYear = FIRST_YEAR;
    editCursor = 0;
    event = EVENT_LONG_PUSH; // we did a long push to get here.
  }
  if (event == EVENT_SHORT_PUSH) {
    switch (editCursor) {
      case 0: editHour++;
#ifdef CLOCK_24HOUR
        if (editHour > 23) editHour = 0;
#else
        if (editHour > 12) editHour = 1;
#endif
        break;
      case 1: editMinute++;
        if (editMinute > 59) editMinute = 0;
        break;
#ifndef CLOCK_24HOUR
      case 2: editMeridian++;
        if (editMeridian > 1) editMeridian = 0;
        break;
#endif
      case 3: editDay++;
        if (editDay > 31) editDay = 1;
        break;
      case 4: editMonth++;
        if (editMonth > 12) editMonth = 1;
        break;
      case 5: editYear++;
        if (editYear > LAST_YEAR) editYear = FIRST_YEAR;
        break;
    }
  }
  if (event == EVENT_LONG_PUSH) {
    if (!initialize) editCursor++;
#ifdef CLOCK_24HOUR
    if (editCursor == 2) editCursor++; // skip the meridian
#endif
    if (editCursor > 5) {
      // convert hour back to 24 hours format
#ifndef CLOCK_24HOUR
      if (editMeridian == 0 && editHour == 12) editHour = 0;
      if (editMeridian == 1 && editHour != 12) editHour += 12;
#endif
      tmElements_t tm;
      tm.Year = editYear - 1970;
      tm.Month = editMonth;
      tm.Day = editDay;
      tm.Hour = editHour;
      tm.Minute = editMinute;
      tm.Second = 0;
      time_t toSet = makeTime(tm);
      // The underlying system clock is always winter time.
      // Note that setting the time during the repeated hour in
      // the fall will assume winter time - the hour will NOT repeat.
      if (enable_dst) toSet = dst.toUTC(toSet);
      setTime(toSet);
      RTC.set(toSet);
      doMenuFunc = doMenu;
      inMenu = false; // exit all menus
      display.clear();
      return;
    }
  }
  boolean new_blink = (millis() % 1000) >= 500;
  if (event == EVENT_NONE && blink == new_blink) return; // no change to the display - no need to re-render it;
  blink = new_blink;
  // render the display
  display.setCursor(0, 1);
  char buf[5];
#ifdef CLOCK_24HOUR
  sprintf(buf, P("%2d"), editHour);
#else
  sprintf(buf, P("%2d"), editHour);
#endif
  if (editCursor == 0 && blink)
    display.print(P("  "));
  else
    display.print(buf);
  display.print(':');
  sprintf(buf, P("%02d"), editMinute);
  if (editCursor == 1 && blink)
    display.print(P("  "));
  else
    display.print(buf);
#ifdef CLOCK_24HOUR
  display.print(' ');
#else
  if (editCursor == 2 && blink)
    display.print(' ');
  else if (editMeridian == 0)
    display.print('A');
  else
    display.print('P');
#endif
  sprintf(buf, P(" %2d"), editDay);
  if (editCursor == 3 && blink)
    display.print(P("   "));
  else
    display.print(buf);
  display.print('-');
  if (editCursor == 4 && blink)
    display.print(P("   "));
  else
    display.print(monthShortStr(editMonth));
  display.print('-');
  sprintf(buf, P("%2d"), editYear % 100);
  if (editCursor == 5 && blink)
    display.print(P("  "));
  else
    display.print(buf);
}

void doEventMenu(boolean initialize) {

  event_struct* events(persisted.events);

  unsigned int event = checkEvent();
  if (initialize) {
    display.clear();
    editCursor = 0;
    editEvent = 0;
    event = EVENT_SHORT_PUSH; // we did a short push to get here.
  }
  if (event == EVENT_SHORT_PUSH) {
    switch (editCursor) {
      case 0: // the event ID
        if (!initialize) editEvent++;
        if (editEvent > EVENT_COUNT) editEvent = 0;
        if (editEvent < EVENT_COUNT) {
          editHour = events[editEvent].hour;
          editMinute = events[editEvent].minute;
          editDOW = events[editEvent].dow_mask;
          editType = events[editEvent].event_type;
          // Convert to 12 hour time
#ifndef CLOCK_24HOUR
          editMeridian = (editHour >= 12) ? 1 : 0;
          if (editHour == 0)
            editHour = 12;
          else if (editHour > 12)
            editHour -= 12;
#endif
        }
        break;
      case 1: // the hour
        editHour++;
#ifdef CLOCK_24HOUR
        if (editHour > 23) editHour = 0;
#else
        if (editHour > 12) editHour = 1;
#endif
        break;
      case 2: // the minute
        editMinute++;
        if (editMinute > 59) editMinute = 0;
        break;
#ifndef CLOCK_24HOUR
      case 3: // the meridian
        editMeridian = !editMeridian;
        break;
#endif
      case 4: // Sunday
      case 5: // Monday
      case 6: // Tuesday
      case 7: // Wednesday
      case 8: // Thursday
      case 9: // Friday
      case 10: // Saturday
        editDOW ^= 1 << (editCursor - 4);
        break;
      case 11: // event type
        editType++;
        if (editType > TE_LAST) editType = 0;
        break;
    }
  }
  if (event == EVENT_LONG_PUSH) {
    if (editCursor == 0 && editEvent == EVENT_COUNT) {
      doMenuFunc = doMenu;
      inMenu = false;
      display.clear();
      return;
    }
    editCursor++;
#ifdef CLOCK_24HOUR
    if (editCursor == 3) editCursor++; // skip the meridian
#endif
    if (editCursor > 11) {
      // convert hour back to 24 hours format
      unsigned char saveHour = editHour;
#ifndef CLOCK_24HOUR
      if (editMeridian == 0 && saveHour == 12) saveHour = 0;
      if (editMeridian == 1 && saveHour != 12) saveHour += 12;
#endif
      logDebug(P("Saving event %d - %d:%d dow_mask %x event %d"), editEvent, saveHour, editMinute, editDOW, editType);
      events[editEvent].hour = saveHour;
      events[editEvent].minute = editMinute;
      events[editEvent].dow_mask = editDOW;
      events[editEvent].event_type = editType;
      persisted.eepromWrite();
      editCursor = 0;
      return;
    }
  }
  boolean new_blink = (millis() % 1000) >= 500;
  if (event == EVENT_NONE && blink == new_blink) return; // no change to the display - no need to re-render it;
  blink = new_blink;
  // render the display
  display.setCursor(0, 0);
  display.print(P("Edit Event "));
  if (blink && editCursor == 0)
    display.print(P("    "));
  else if (editEvent == EVENT_COUNT)
    display.print(P("Exit"));
  else
    display.print(editEvent + 1);
  display.setCursor(0, 1);
  if (editEvent == EVENT_COUNT) {
    display.print(P("                "));
    return;
  }
  char buf[4];
#ifdef CLOCK_24HOUR
  sprintf(buf, P("%02d"), editHour);
#else
  sprintf(buf, P("%2d"), editHour);
#endif
  if (blink && editCursor == 1)
    display.print(P("  "));
  else
    display.print(buf);
  display.print(':');
  sprintf(buf, P("%02d"), editMinute);
  if (blink && editCursor == 2)
    display.print(P("  "));
  else
    display.print(buf);
#ifdef CLOCK_24HOUR
  display.print(' ');
#else
  if (blink && editCursor == 3)
    display.print(' ');
  else {
    display.print(editMeridian == 1 ? 'P' : 'A');
  }
#endif
  display.print(' ');
  for (unsigned int i = 0; i < strlen(P(DAY_FLAGS)); i++) {
    if (i + 4 == editCursor && blink) {
      display.print(' ');
      continue;
    }
    if ((1 << i) & editDOW)
      display.print(P(DAY_FLAGS)[i]);
    else
      display.print('-');
  }
  display.print(' ');
  if (editCursor == 11 && blink) {
    display.print(' ');
  } else if (editType == TE_NONE) {
    display.print('N');
  } else if (editType == TE_PAUSE) {
    display.print('S');
  } else if (editType == TE_UNPAUSE) {
    display.print('G');
  }

}

///////////////////////////
// calib_struct support

void doCalibMenu(boolean initialize) {
  persisted.calib.doMenu(initialize);
}

void calib_struct::doMenu(boolean initialize) {
#define MAX_ITEMS 4
  unsigned int event = checkEvent();
  char str[17];
  if (initialize) {
    display.clear();
    menuItem = 0;
    event = EVENT_SHORT_PUSH;
  } else {

    char& amm((menuItem & 1) == 0 ? amm_a : amm_b);
    char& pilot((menuItem & 1) == 0 ? pilot_a : pilot_b);

    switch (event ) {
      case EVENT_SHORT_PUSH:
        switch (menuItem)  {
          case 0:
          case 1: if ( ++amm > CALIB_AMM_MAX) amm = -CALIB_AMM_MAX; break;
          case 2:
          case 3: if ( --pilot < -CALIB_PILOT_MAX) pilot = 0; break;
        }
        break;
      case EVENT_LONG_PUSH:
        menuItem++;
        if ( menuItem >= MAX_ITEMS) {
          persisted.eepromWrite();
          doMenuFunc = ::doMenu;
          inMenu = false; // exit completely all the way
          return;
        }
        break;
      default: return;
    }
  }

  // drawing
  char* carSymb = (menuItem & 1) == 0 ? "A" : "B";
  char& amm((menuItem & 1) == 0 ? amm_a : amm_b);
  char& pilot((menuItem & 1) == 0 ? pilot_a : pilot_b);

  switch (menuItem) {
    case 0:
    case 1:
      display.clear();
      display.print(P("Ammeter"));

      display.setCursor(0, 1);
      snprintf(str, sizeof(str), P(" Car %s: %s0.%d"), carSymb, amm < 0 ? "-" : "+", abs(amm));
      display.print(str);
      break;

    case 2:
    case 3:
      display.clear();
      display.print(P("Pilot derate"));

      display.setCursor(0, 1);
      snprintf(str, sizeof(str), P(" Car %s: %d%%"), carSymb, (int)pilot);
      display.print(str);
      break;
  }
}
// calibration structure support
////////////////////////////////////////

void doMenu(boolean initialize) {
  unsigned int& max_current_amps(persisted.max_amps);
  unsigned int event = checkEvent();
  if (initialize) {
    display.setBacklight(YELLOW);
    display.clear();
    event = EVENT_LONG_PUSH; // we did a long push to get here.
    menu_number = 999; // way too high - increment and fall down to the beginning.
  }
  if (event == EVENT_NONE) return; // nothing happened
  if (event == EVENT_SHORT_PUSH) {
    menu_item++;
    if (menu_item > menu_item_max) menu_item = 0;
    // fall through to rendering the display
  } else if (event == EVENT_LONG_PUSH) {
    // Depending on which menu it is, now we commit the chosen value
    switch (menu_number) {
      case MENU_OPERATING_MODE:
        operatingMode = menu_item;
        persisted.eepromWrite();
        break;
      case MENU_CURRENT_AVAIL:
        max_current_amps = currentMenuChoices[menu_item];
        persisted.eepromWrite();
        break;
      case MENU_CLOCK:
        if (menu_item == 0) {
          doMenuFunc = doClockMenu;
          doClockMenu(true);
          return;
        }
        break;
      case MENU_DST:
        enable_dst = menu_item == 0;
        persisted.eepromWrite();
        break;
      case MENU_EVENT:
        if (menu_item == 0) {
          doMenuFunc = doEventMenu;
          doEventMenu(true);
          return;
        }
        break;
      case MENU_CALIB:
        if (menu_item == 0 ) {
          doMenuFunc = doCalibMenu;
          doCalibMenu(true);
          return;
        }
        break;
      case MENU_EXIT:
        if (menu_item == 0)
          inMenu = false;
        else
          menu_number = 999;
        return;
    }
    menu_number++;
    if (menu_number > MAX_MENU_NUMBER) menu_number = 0;
    // Now set the selected menu item for the next menu
    switch (menu_number) {
      case MENU_OPERATING_MODE:
        menu_item = operatingMode;
        menu_item_max = LAST_MODE;
        break;
      case MENU_CURRENT_AVAIL:

        menu_item = 0; // fallback in case not found
        for (unsigned int i = 0; i < sizeof(currentMenuChoices); i++) {
          if (max_current_amps == currentMenuChoices[i])
            menu_item = i;
        }
        menu_item_max = sizeof(currentMenuChoices) - 1;
        break;
      case MENU_CLOCK:
        menu_item = 1; // default to "No"
        menu_item_max = 1;
        break;
      case MENU_DST:
        menu_item_max = 1;
        menu_item = enable_dst ? 0 : 1;
        break;
      case MENU_EVENT:
        menu_item = 1; // default to "No"
        menu_item_max = 1;
        break;
      case MENU_CALIB:
        menu_item = 1; // default to "No"
        menu_item_max = 1;
        break;
      case MENU_EXIT:
        menu_item = 0; // default to "Yes"
        menu_item_max = 1;
        break;
    }
    menu_item_selected = menu_item;
    // fall through to rendering the display
  }
  // Render the menu on the display
  display.clear();
  switch (menu_number) {
    case MENU_OPERATING_MODE:
      display.print(P(MENU_OPERATING_MODE_HEADER));
      display.setCursor(0, 1);
      display.print((menu_item == menu_item_selected) ? '+' : ' ');
      switch (menu_item) {
        case MODE_SHARED:
          display.print(P(OPTION_SHARED_TEXT));
          break;
        case MODE_SEQUENTIAL:
          display.print(P(OPTION_SEQUENTIAL_TEXT));
          break;
      }
      break;
    case MENU_CURRENT_AVAIL:
      display.print(P(MENU_CURRENT_AVAIL_HEADER));
      display.setCursor(0, 1);
      display.print((menu_item == menu_item_selected) ? '+' : ' ');
      display.print(currentMenuChoices[menu_item]);
      display.print(P(" Amps"));
      break;
    case MENU_CLOCK:
      display.print(P(MENU_CLOCK_HEADER));
      display.setCursor(0, 1);
      display.print((menu_item == menu_item_selected) ? '+' : ' ');
      switch (menu_item) {
        case 0:
          display.print(P(OPTION_YES_TEXT));
          break;
        case 1:
          display.print(P(OPTION_NO_TEXT));
          break;
      }
      break;
    case MENU_DST:
      display.print(P(MENU_DST_HEADER));
      display.setCursor(0, 1);
      display.print((menu_item == menu_item_selected) ? '+' : ' ');
      switch (menu_item) {
        case 0:
          display.print(P(OPTION_YES_TEXT));
          break;
        case 1:
          display.print(P(OPTION_NO_TEXT));
          break;
      }
      break;
    case MENU_EVENT:
      display.print(P(MENU_EVENT_HEADER));
      display.setCursor(0, 1);
      display.print((menu_item == menu_item_selected) ? '+' : ' ');
      switch (menu_item) {
        case 0:
          display.print(P(OPTION_YES_TEXT));
          break;
        case 1:
          display.print(P(OPTION_NO_TEXT));
          break;
      }
      break;
    case MENU_CALIB:
      display.print(P(MENU_CALIB_HEADER));
      display.setCursor(0, 1);
      display.print((menu_item == menu_item_selected) ? '+' : ' ');
      switch (menu_item) {
        case 0:
          display.print(P(OPTION_YES_TEXT));
          break;
        case 1:
          display.print(P(OPTION_NO_TEXT));
          break;
      }
      break;
    case MENU_EXIT:
      display.print(P(MENU_EXIT_HEADER));
      display.setCursor(0, 1);
      display.print((menu_item == menu_item_selected) ? '+' : ' ');
      switch (menu_item) {
        case 0:
          display.print(P(OPTION_YES_TEXT));
          break;
        case 1:
          display.print(P(OPTION_NO_TEXT));
          break;
      }
      break;
  }
}


void setup() {

  MCUSR = 0; // changing the watchdog requires this first.
  wdt_enable(WDTO_1S);

  // Start serial logging first so we can detect a good CPU reset.
#if SERIAL_LOG_LEVEL > 0
  Serial.begin(SERIAL_BAUD_RATE);
#endif

  logDebug(P("Starting HW:%s SW:%s"), HW_VERSION, SW_VERSION);

  InitTimersSafe();

  display.setMCPType(LTI_TYPE_MCP23017);
  display.begin(16, 2);
  display.setBacklight(WHITE);
  display.clear();
  display.setCursor(0, 0);
  display.print(P("J1772 Hydra "));
  display.setCursor(0, 1);
  display.print(P("HW:"));
  display.print(P(HW_VERSION));
  display.setCursor(8, 1);
  display.print(P("SW:"));
  display.print(P(SW_VERSION));

  pinMode(GFI_TEST_PIN, OUTPUT);
  digitalWrite(GFI_TEST_PIN, LOW);
  pinMode(GFI_PIN, INPUT);
  //  attachInterrupt(GFI_IRQ, gfi_trigger, RISING);
  attachInterrupt(digitalPinToInterrupt(GFI_PIN), gfi_trigger, RISING);

  pinMode(CAR_A_PILOT_OUT_PIN, OUTPUT);
  pinMode(CAR_B_PILOT_OUT_PIN, OUTPUT);
  pinMode(CAR_A_RELAY, OUTPUT);
  pinMode(CAR_B_RELAY, OUTPUT);
#ifdef GROUND_TEST
  pinMode(GROUND_TEST_PIN, INPUT);
#endif
#ifdef RELAY_TEST
  pinMode(CAR_A_RELAY_TEST, INPUT);
  pinMode(CAR_B_RELAY_TEST, INPUT);
#endif

  // Enter state A on both cars
  setPilot(CAR_A, HIGH);
  setPilot(CAR_B, HIGH);
  // And make sure the power is off.
  setRelay(CAR_A, LOW);
  setRelay(CAR_B, LOW);

  memset(car_a_current_samples, 0, sizeof(car_a_current_samples));
  memset(car_b_current_samples, 0, sizeof(car_b_current_samples));
  last_car_a_state = DUNNO;
  last_car_b_state = DUNNO;
  car_a_request_time = 0;
  car_b_request_time = 0;
  car_a_overdraw_begin = 0;
  car_b_overdraw_begin = 0;
  last_current_log_car_a = 0;
  last_current_log_car_b = 0;
  button_debounce_time = 0;
  button_press_time = 0;
  sequential_pilot_timeout = 0;
  last_minute = 99;
  relay_change_time = 0;
#ifdef QUICK_CYCLING_WORKAROUND
  pilot_release_holdoff_time = 0;
#endif

  sequential_mode_tiebreak = DEFAULT_TIEBREAK;

  setSyncProvider(RTC.get);
  RTC.setCalibration(persisted.rtc_cal);

  boolean success = SetPinFrequencySafe(CAR_A_PILOT_OUT_PIN, 1000);
  if (!success) {
    logInfo(P("SetPinFrequency for car A failed!"));
    display.setBacklight(YELLOW);
  }
  success = SetPinFrequencySafe(CAR_B_PILOT_OUT_PIN, 1000);
  if (!success) {
    logInfo(P("SetPinFrequency for car B failed!"));
    display.setBacklight(BLUE);
  }
  // In principle, neither of the above two !success conditions should ever
  // happen.

  Delay(2000);
  display.clear();

  // Self test fails in simulated environment, so we disable it during setup in that case.
#ifndef UNIT_TESTS
  gfiSelfTest();
#endif

#if 0 // ground test is now only active while charging
  if (digitalRead(GROUND_TEST_PIN) != HIGH) {
    display.setBacklight(RED);
    display.clear();
    display.print(P("Ground Test Failure"));
    die();
  }
#endif
#if defined(RELAY_TEST) && !defined(UNIT_TESTS)
  {
    boolean test_a = digitalRead(CAR_A_RELAY_TEST) == HIGH;
    boolean test_b = digitalRead(CAR_B_RELAY_TEST) == HIGH;
    if (test_a || test_b) {
      display.setBacklight(RED);
      display.clear();
      display.print(P("Relay Failure: "));
      display.setCursor(0, 1);
      if (test_a) display.print('A');
      if (test_b) display.print('B');
      die();
    }
  }
#endif

#ifdef UNIT_TESTS
  if ( unitsSetup() ) die();
#endif

}

void loop() {

  wdt_reset();

#ifdef UNIT_TESTS
  if ( unitsLoop() ) die();
#endif

  if (inMenu) {
    doMenuFunc(false);
    return;
  }

  if (gfiTriggered) {
    logInfo(P("GFI fault detected"));
    error(BOTH | STATUS_ERR | STATUS_ERR_G);
    gfiTriggered = false;
  }

#ifdef GROUND_TEST
  if ((relay_state_a == HIGH || relay_state_b == HIGH) && relay_change_time == 0) {
    unsigned char ground = digitalRead(GROUND_TEST_PIN) == HIGH;
    if (ground != current_ground_status) {
      current_ground_status = ground;
      if (!ground) {
        // we've just noticed a ground failure.
        log(LOG_INFO, P("Ground failure detected"));
        error(BOTH, 'F');
      }
    }
  } else {
    current_ground_status = 2; // Force the check to take place
  }
#endif

#ifdef RELAY_TEST
  if (relay_change_time == 0) {
    // If the power's off but there's still a voltage, that's a stuck relay
    if ((digitalRead(CAR_A_RELAY_TEST) == HIGH) && (relay_state_a == LOW)) {
      logInfo(P("Relay fault detected on car A"));
      error(CAR_A | STATUS_ERR | STATUS_ERR_R);
    }
    if ((digitalRead(CAR_B_RELAY_TEST) == HIGH) && (relay_state_b == LOW)) {
      logInfo(P("Relay fault detected on car B"));
      error(CAR_B | STATUS_ERR | STATUS_ERR_R);
    }
#ifdef RELAY_TESTS_GROUND
    // If the power's on, but there's no voltage, that's a ground impedance failure
    if ((digitalRead(CAR_A_RELAY_TEST) == LOW) && (relay_state_a == HIGH)) {
      logInfo(P("Ground failure detected on car A"));
      error(CAR_B | STATUS_ERR | STATUS_ERR_F);
    }
    if ((digitalRead(CAR_B_RELAY_TEST) == LOW) && (relay_state_b == HIGH)) {
      logInfo(P("Ground failure detected on car B"));
      error(CAR_B | STATUS_ERR | STATUS_ERR_F);
    }
#endif
  }
#endif

  unsigned long incomingPilotMilliamps = 1000ul * persisted.max_amps;

  if (relay_change_time != 0 && millis() > relay_change_time + RELAY_TEST_GRACE_TIME)
    relay_change_time = 0;

  // Update the display
  if (last_car_a_state == STATE_E || last_car_b_state == STATE_E) {
    // One or both cars in error state
    display.setBacklight(RED);
  }
  else {
    boolean a = isCarCharging(CAR_A);
    boolean b = isCarCharging(CAR_B);

    // Neither car
    if (!a && !b) display.setBacklight(paused ? YELLOW : GREEN);
    // Both cars
    else if (a && b) display.setBacklight(VIOLET);
    // One car or the other
    else if (a ^ b) display.setBacklight(TEAL);
  }

  if (enterPause) {
    if (!paused) {
      if (operatingMode == MODE_SEQUENTIAL) {
        // remember which car was active
        if (pilot_state_a == FULL) sequential_mode_tiebreak = CAR_A;
        else if (pilot_state_b == FULL) sequential_mode_tiebreak = CAR_B;
        else sequential_mode_tiebreak = DEFAULT_TIEBREAK;
      }
      // Turn off both pilots
      setPilot(CAR_A, HIGH);
      setPilot(CAR_B, HIGH);
      unsigned long now = millis();
      car_a_error_time = now;
      car_b_error_time = now;
      last_car_a_state = DUNNO;
      last_car_b_state = DUNNO;
      car_a_request_time = 0;
      car_b_request_time = 0;
      seq_car_a_done = false;
      seq_car_b_done = false;
      logInfo(P("Pausing."));
    }
    paused = true;
  } else {
    // reset car states if unpaused so that initial transitions may run on unpause for any plugged cars.
    if ( paused ) {
      last_car_a_state = DUNNO;
      last_car_b_state = DUNNO;
    }
    paused = false;
  }

  // Print the time of day
  display.setCursor(0, 0);
  char buf[17];
  if (RTC.isRunning()) {
#ifdef CLOCK_24HOUR
    snprintf(buf, sizeof(buf), P(" %02d:%02d  "), hour(localTime()), minute(localTime()));
#else
    snprintf(buf, sizeof(buf), P("%2d:%02d%cM "), hourFormat12(localTime()), minute(localTime()), isPM(localTime()) ? 'P' : 'A');
#endif
  } else {
    snprintf(buf, sizeof(buf), P("        "));
  }
  display.print(buf);

  if (paused) {
    display.print(P("M:PAUSED"));
  } else {
    display.print(P("M:"));
    switch (operatingMode) {
      case MODE_SHARED:
        display.print(P("shared")); break;
      case MODE_SEQUENTIAL:
        display.print(P("seqntl")); break;
      default:
        display.print(P("UNK")); break;
    }
  }

  // Check the pilot sense on each car.
  unsigned int car_a_state = checkState(CAR_A);

  if (paused || last_car_a_state == STATE_E) {
    switch (car_a_state) {
      case STATE_A:
        // we were in error, but the car's been disconnected.
        // If we are still paused, then
        // we can't clear the error.
        if (!paused) {
          // If not, clear the error state. The next time through
          // will take us back to state A.
          last_car_a_state = DUNNO;
          logInfo(P("Car A disconnected, clearing error"));
        } else {
          // We're paused. We will fix up the display ourselves.
          displayStatus(CAR_A | STATUS_UNPLUGGED);
          //          display.setCursor(0, 1);
          //          display.print(P("A: ---  "));
          last_car_a_state = car_a_state;
        }
      // fall through...
      case STATE_B:
        // If we see a transition to state B, the error is still in effect, but complete (and
        // cancel) any pending relay opening.
        if (car_a_error_time != 0) {
          car_a_error_time = 0;
          setRelay(CAR_A, LOW);
          if (isCarCharging(CAR_B) || last_car_b_state == STATE_B)
            setPilot(CAR_B, FULL);
        }
        if (paused && car_a_state == STATE_B) {
          // just plugged in -- set the tie break in sequential mode to this last plugged car during pause.
          if ( last_car_a_state == STATE_A ) {
            sequential_mode_tiebreak = CAR_A;
            last_car_a_state = STATE_B;
          }
          //          display.setCursor(0, 1);
          displayStatus(CAR_A | STATUS_OFF | (operatingMode == MODE_SEQUENTIAL && sequential_mode_tiebreak == CAR_A ? STATUS_TIEBREAK : 0));
          //            display.print(P("A: off* "));
          //          else
          //            display.print(P("A: off  "));
        }
        break;
    }
  } else if (car_a_state != last_car_a_state) {
    if (last_car_a_state != DUNNO)
      logInfo(P("Car A state transition: %s->%s."), state_str(last_car_a_state), state_str(car_a_state));
    switch (operatingMode) {
      case MODE_SHARED:
        shared_mode_transition(CAR_A, car_a_state);
        break;
      case MODE_SEQUENTIAL:
        sequential_mode_transition(CAR_A, car_a_state);
        break;
    }
  }

  unsigned int car_b_state = checkState(CAR_B);
  if (paused || last_car_b_state == STATE_E) {
    switch (car_b_state) {
      case STATE_A:
        // we were in error, but the car's been disconnected.
        // If we are still paused, then
        // we can't clear the error.
        if (!paused) {
          // If not, clear the error state. The next time through
          // will take us back to state A.
          last_car_b_state = DUNNO;
          logInfo(P("Car B disconnected, clearing error"));
        } else {
          // We're paused. We will fix up the display ourselves.
          displayStatus(CAR_B | STATUS_UNPLUGGED);
          //          display.setCursor(8, 1);
          //          display.print(P("B: ---  "));

          last_car_b_state = car_b_state;
        }
      // fall through...
      case STATE_B:
        // If we see a transition to state B, the error is still in effect, but complete (and
        // cancel) any pending relay opening.
        if (car_b_error_time != 0) {
          car_b_error_time = 0;
          setRelay(CAR_B, LOW);
          if (isCarCharging(CAR_A) || last_car_a_state == STATE_B)
            setPilot(CAR_A, FULL);
        }
        if (paused && car_b_state == STATE_B) {
          // just plugged in -- set the tie break in sequential mode to this last plugged car during pause.
          // this will not engage if the state ws "DUNNO" which i guess what it is going to be after just
          // entering pause or powering up. But i cannot use from DUNNO transition here since it may also be
          // just entering the pause, in which case it will always reset the tiebreak to B no matter what.
          // So resetting priority would require unplug-plug and not just pause-then-plug. It should be ok
          // though.
          if ( last_car_b_state == STATE_A ) {
            sequential_mode_tiebreak = CAR_B;
            last_car_b_state = STATE_B;
          }
          //          display.setCursor(8, 1);
          displayStatus(CAR_B | STATUS_OFF | (( operatingMode == MODE_SEQUENTIAL && sequential_mode_tiebreak == CAR_B) ? STATUS_TIEBREAK : 0 ));
          //            display.print(P("B: off* "));
          //          else
          //            display.print(P("B: off  "));
        }
        break;
    }
  } else if (car_b_state != last_car_b_state) {
    if (last_car_b_state != DUNNO)
      logInfo(P("Car B state transition: %s->%s."), state_str(last_car_b_state), state_str(car_b_state));
    switch (operatingMode) {
      case MODE_SHARED:
        shared_mode_transition(CAR_B, car_b_state);
        break;
      case MODE_SEQUENTIAL:
        sequential_mode_transition(CAR_B, car_b_state);
        break;
    }
  }
  if (sequential_pilot_timeout != 0) {
    unsigned long now = millis();
    if (now - sequential_pilot_timeout > SEQ_MODE_OFFER_TIMEOUT) {
      if (pilot_state_a == FULL) {
        logInfo(P("Sequential mode offer timeout, moving offer to %s"), car_str(CAR_B));
        setPilot(CAR_A, HIGH);
        setPilot(CAR_B, FULL);
        sequential_pilot_timeout = now;
        displayStatus(CAR_A | (seq_car_a_done ? STATUS_DONE : STATUS_WAIT));
        //        display.setCursor(0, 1);
        //        if ( seq_car_a_done )
        //          display.print(P("A: done "));
        //        else
        //          display.print(P("A: wait "));
        displayStatus(CAR_B | STATUS_OFF );
        //        display.print(P("B: off  "));
      } else if (pilot_state_b == FULL) {
        logInfo(P("Sequential mode offer timeout, moving offer to %s"), car_str(CAR_A));
        setPilot(CAR_B, HIGH);
        setPilot(CAR_A, FULL);
        sequential_pilot_timeout = now;
        displayStatus(CAR_B | (seq_car_b_done ? STATUS_DONE : STATUS_WAIT));
        displayStatus(CAR_A | STATUS_OFF);
        //        display.setCursor(0, 1);
        //        display.print(P("A: off  "));
        //        if ( seq_car_b_done )
        //          display.print(P("B: done "));
        //        else
        //          display.print(P("B: wait "));
      }
    }
  }

  {
    unsigned long now = millis();
    if (now - last_state_log > STATE_LOG_INTERVAL) {
      last_state_log = now;
      logInfo(P("States: Car A, %s; Car B, %s"), state_str(last_car_a_state), state_str(last_car_b_state));
      logInfo(P("Power available %lu mA"), incomingPilotMilliamps);
    }
  }

  // Update the ammeter display and check for overdraw conditions.
  // We allow a 5 second grace because the J1772 spec requires allowing
  // the car 5 seconds to respond to incoming pilot changes.
  // If the overdraw condition is acute enough, we'll be blowing fuses
  // in hardware, so this isn't as dire a condition as it sounds.
  // More likely what it means is that the car hasn't reacted to an
  // attempt to reduce it to half power (and the other car has not yet
  // been turned on), so we must error them out before letting the other
  // car start.
  if (relay_state_a == HIGH && last_car_a_state != STATE_E) { // Only check the ammeter if the power is actually on and we're not errored
    unsigned long car_a_draw = readCurrent(CAR_A);

    {
      unsigned long now = millis();
      if (now - last_current_log_car_a > CURRENT_LOG_INTERVAL) {
        last_current_log_car_a = now;
        logInfo(P("Car A current draw %lu mA"), car_a_draw);
      }
    }

    // If the other car is charging, then we can only have half power
    unsigned long car_a_limit = incomingPilotMilliamps / ((pilotState(CAR_A) == HALF) ? 2 : 1);

    if (car_a_draw > car_a_limit + OVERDRAW_GRACE_AMPS) {
      // car A has begun an over-draw condition. They have 5 seconds of grace before we pull the plug.
      if (car_a_overdraw_begin == 0) {
        car_a_overdraw_begin = millis();
      }
      else {
        if (millis() - car_a_overdraw_begin > OVERDRAW_GRACE_PERIOD) {
          error(CAR_A | STATUS_ERR | STATUS_ERR_O);
          return;
        }
      }
    }
    else {
      // car A is under its limit. Cancel any overdraw in progress
      car_a_overdraw_begin = 0;
    }
    display.setCursor(0, 1);
    display.print("A:");
    display.print(formatMilliamps(car_a_draw));
  }
  else {
    // Car A is not charging
    car_a_overdraw_begin = 0;
    memset(car_b_current_samples, 0, sizeof(car_b_current_samples));
  }

  if (relay_state_b == HIGH && last_car_b_state != STATE_E) { // Only check the ammeter if the power is actually on and we're not errored
    unsigned long car_b_draw = readCurrent(CAR_B);

    {
      unsigned long now = millis();
      if (now - last_current_log_car_b > CURRENT_LOG_INTERVAL) {
        last_current_log_car_b = now;
        logInfo(P("Car B current draw %lu mA"), car_b_draw);
      }
    }

    // If the other car is charging, then we can only have half power
    unsigned long car_b_limit = incomingPilotMilliamps / ((pilotState(CAR_B) == HALF) ? 2 : 1);

    if (car_b_draw > car_b_limit + OVERDRAW_GRACE_AMPS) {
      // car B has begun an over-draw condition. They have 5 seconds of grace before we pull the plug.
      if (car_b_overdraw_begin == 0) {
        car_b_overdraw_begin = millis();
      }
      else {
        if (millis() - car_b_overdraw_begin > OVERDRAW_GRACE_PERIOD) {
          error(CAR_B | STATUS_ERR | STATUS_ERR_O);
          return;
        }
      }
    }
    else {
      car_b_overdraw_begin = 0;
    }
    display.setCursor(8, 1);
    display.print("B:");
    display.print(formatMilliamps(car_b_draw));
  }
  else {
    // Car B is not charging
    car_b_overdraw_begin = 0;
    memset(car_b_current_samples, 0, sizeof(car_b_current_samples));
  }

  // We need to use labs() here because we cached now early on, so it may actually be
  // *before* the time in question
  if (car_a_request_time != 0 && (millis() - car_a_request_time) > TRANSITION_DELAY) {
    // We've waited long enough.
    logInfo(P("Delayed transition completed on car A"));
    car_a_request_time = 0;
    displayStatus(CAR_A | STATUS_ON );
    //    display.setCursor(0, 1);
    //    display.print(P("A: ON   "));
    setRelay(CAR_A, HIGH);
  }
  if (car_a_error_time != 0 && (millis() - car_a_error_time) > ERROR_DELAY) {
    car_a_error_time = 0;
    setRelay(CAR_A, LOW);
    if (paused) {
      displayStatus( CAR_A | STATUS_OFF );
      //      display.setCursor(0, 1);
      //      display.print(P("A: off  "));
      logInfo(P("Power withdrawn after pause delay on car A"));
    } else {
      logInfo(P("Power withdrawn after error delay on car A"));
    }
    if (isCarCharging(CAR_B) || last_car_b_state == STATE_B)
      setPilot(CAR_B, FULL);
  }
  if (car_b_request_time != 0 && (millis() - car_b_request_time) > TRANSITION_DELAY) {
    logInfo(P("Delayed transition completed on car B"));
    // We've waited long enough.
    car_b_request_time = 0;
    displayStatus(CAR_B | STATUS_ON);
    //    display.setCursor(8, 1);
    //    display.print(P("B: ON   "));
    setRelay(CAR_B, HIGH);
  }
  if (car_b_error_time != 0 && (millis() - car_b_error_time) > ERROR_DELAY) {
    car_b_error_time = 0;
    setRelay(CAR_B, LOW);
    if (paused) {
      displayStatus(CAR_B | STATUS_OFF);
      //      display.setCursor(8, 1);
      //      display.print(P("B: off  "));
      logInfo(P("Power withdrawn after pause delay on car B"));
    } else
      logInfo(P("Power withdrawn after error delay on car B"));
    if (isCarCharging(CAR_A) || last_car_a_state == STATE_B)
      setPilot(CAR_A, FULL);
  }

#ifdef QUICK_CYCLING_WORKAROUND
  if (pilot_release_holdoff_time != 0 && millis() > pilot_release_holdoff_time) {
    log(LOG_INFO, P("Pilot release interval elapsed. Raising pilot to full on remaining car."));
    if (isCarCharging(CAR_A)) {
      setPilot(CAR_A, FULL);
    } else if (isCarCharging(CAR_B)) {
      setPilot(CAR_B, FULL);
    } else {
      log(LOG_INFO, P("Pilot release interval elapsed, but no car is charging??"));
    }
    pilot_release_holdoff_time = 0;
  }
#endif

  unsigned int event = checkEvent();
  if (event == EVENT_SHORT_PUSH)
    enterPause = !paused;
  if (car_a_state == STATE_A && car_b_state == STATE_A) {
    // Allow playing with the menu button only when both plugs are out
    if (event == EVENT_LONG_PUSH) {
      inMenu = true;
      last_car_a_state = DUNNO;
      last_car_b_state = DUNNO;
      doMenu(true);
    }
  }

  if (last_minute != minute(localTime())) {
    last_minute = minute(localTime());
    event = checkTimer();
    switch (event) {
      case TE_PAUSE:
        if (!paused) enterPause = true;
        break;
      case TE_UNPAUSE:
        if (paused) enterPause = false;
        break;
    }
  }

}


