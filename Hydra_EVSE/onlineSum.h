
// Exponentially Weighted Average summarizer
// http://tdunning.blogspot.com/2011/03/exponentially-weighted-averaging-for.html
// http://weatheringthrutechdays.blogspot.com/2011/04/follow-up-for-mean-summarizer-post.html

// T is double or float
template <class T>
class EWASum {

    T alpha;
    T w, s, tn;

  public:
    // tHalfPeriod is amount of t after which the observation is exponentially weighed at 0.5 of the current observation.
    EWASum(T tHalfPeriod) : alpha(-tHalfPeriod / log((T)0.5)) {
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
  T pi = exp(tn - t) / alpha;
  s += pi * s + x;
  w += pi * w + 1;
  if (t > tn) tn = t;
}

template <class T>
T EWASum<T>::ewa() {
  return (w == 0) ? 0 : s / w;
}

// On arduino, double seems to be the same as float and take 4 bytes!
typedef EWASum<double> EWASumD;
typedef EWASum<float> EWASumF;
