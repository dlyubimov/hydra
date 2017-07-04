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

#ifndef ___ONLINESUM_H___
#define ___ONLINESUM_H___

///////////////////////////////////////////////////////////////////
// Exponentially Weighted Average summarizer for irregularly sampled data.
// References: 
// http://tdunning.blogspot.com/2011/03/exponentially-weighted-averaging-for.html
// My mods for the updates-in-the past (ensures no NaNs created regardless of updates order):
// http://weatheringthrutechdays.blogspot.com/2011/04/follow-up-for-mean-summarizer-post.html

// T is double or float
template <class T>
class EWASum {

    T alpha;
    T w, s, tn;

  public:
  
    // tHalfWeightPeriod is the amount of t (timeline point) in the past at which the observation is 
    // exponentially weighed at exactly 0.5 vs. any observation right now. Since it is weighed 
    // exponentially rather than linearly, observations at full period still do have a nonzero weight, 
    // and in general with this summarizer most recent observations are being discounted at fastest 
    // weight but never actually discounted completely (indefinite history), down to the precision of
    // the arithmetics. At full period the weights are thus still coming in at exp(-2) = 0.135 (13.5%).
    EWASum(T tHalfWeightPeriod) : alpha(-tHalfWeightPeriod / log((T)0.5)) {
      reset();
    };

    void reset() {
      w = s = tn = 0;
    }

    void update(T x, T t);

    // Evaluate EWA
    T ewa();
};

template <class T>
void EWASum<T>::update(T x, T t) {
  T pi = exp(-abs(tn - t) / alpha);
  if (t > tn) {
    s = pi * s + x;
    w = pi * w + 1;
    tn = t;
  } else {
    s += pi * x;
    w += pi;
  }
}

template <class T>
T EWASum<T>::ewa() {
  // In this sum, weights low enough should not happen, as we never discount the most recent weight, 
  // which is at least one. So w>= 1 if there were at least one sample; and w = exactly 0 if there 
  // are no samples at all. With no samples, we just return 0. 
  return (abs(w) < 1e-6) ? 0 : s / w;
}

// On Arduino, double seems to be the same as float and take 4 bytes!
typedef EWASum<double> EWASumD;
typedef EWASum<float> EWASumF;

#endif // ___ONLINESUM_H___
