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
  return isSummer(rules, t)? t + 3600 : t;
  
}


boolean DSTRule::operator<(time_t& that) {

  tmElements_t thatEls;
  breakTime (that, thatEls);
  
  // try to break based on the month.
  if (month < thatEls.Month) return true;
  else if ( month > thatEls.Month) return false;

  // same month -- we will have to figure the day of week of the rule.
  // TODO to be contd.
  
  
}


