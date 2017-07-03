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

#include "dst.h"

#define sameWeekDow(_time_, _dow_)  ((_time_ / SECS_PER_DAY + _dow_ - dayOfWeek(_time_)) * SECS_PER_DAY)
#define monthBegin(_time_) ((_time_ / SECS_PER_DAY + 1 - day(_time_)) * SECS_PER_DAY) 
#define nextMonthBegin(_time_) monthBegin( ((_time_ / SECS_PER_DAY + 31 - day(_time_)) * SECS_PER_DAY)

// This returns next day of week w.r.t. current time. if today is the same day of week 
// as the one requested, returns today's midnight. 
inline time_t nextDow (time_t _time_, uint8_t _dow_) {
  time_t sameWDow = sameWeekDow( _time_, _dow_);
  return sameWDow >= previousMidnight(_time_)? sameWDow : sameWDow + SECS_PER_WEEK;
}
// returns previous closest day of week to the _time_. If today is the same as _dow_, returns today's midnight.
inline time_t previousDow (time_t _time_, uint8_t _dow_) {
  time_t sameWDow = sameWeekDow( _time_, _dow_);
  return sameWDow <= previousMidnight(_time_)? sameWDow : sameWDow - SECS_PER_WEEK;
}


boolean isSummer(DSTRule* rules , time_t t) {

  // The algorithm here is simple. in he current year,
  // if we are past the second rule, then we take that rule.
  // otherwise, if we are past the first rule, we take that rule.
  // otherwise, we take second rule.
  for (int i = 1; i >= 0; i--) {
    if ( rules[i] < t) return rules[i].dst;
  }
  return rules[1].dst;
}

time_t toDST(DSTRule *rules, time_t t) {
  return isSummer(rules, t) ? t + 3600 : t;

}

// Find dow "offset" days from now. Offset can be positive for days ahead or negative for days before.
static uint8_t dowOffset(uint8_t dow, int offset) {
  int rDow = (dow + offset - 1 ) % 7;
//  rDow += 7 * (rDow < 0);
  if ( rDow < 0) rDow += 7;
  return (uint8_t) rDow + 1;
}
boolean DSTRule::operator<(time_t& that) {

  uint8_t thatMo = month(that);
  
  // try to break based on the month.
  if ( mo < thatMo) return true;
  else if ( mo > thatMo) return false;

  // same month -- we will have to figure the day of week of the rule.
  if ( week != Last) {

   } else {
    // handle last day of week in the month
  }

}


