/* $Id$ */
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_TWISTER_H
#define IBIS_TWISTER_H
/**@file
 Pseudorandom number generators.

 MersenneTwister: A C++ class that use the similar interface as
 java.util.Random.  The basic algorithm is the Mersenne Twister by
 M. Matsumoto and T. Nishimura <http://dx.doi.org/10.1145/272991.272995>.

 MersenneTwister also include a function called nextZipf to generate Zipf
 distributed random numbers (floats).

 This file also contains additional classes that generate discrete Zipf and
 Poisson distributions (named discreteZipf and discretePoisson)
*/
#include <time.h>	// time_t time
#include <limits.h>	// DBL_MIN, ULONG_MAX
#include <float.h>	// DBL_MIN
#include <math.h>	// sqrt, log, exp, pow

#include <vector>	// std::vector<double> used by discrateZip1

namespace ibis {
    class uniformRandomNumber;	// the abstract base class
    class MersenneTwister;	// concrete uniform random number generator
    class discretePoisson;	// discrete Poisson
    class discretePoisson1;	// the spcial case for exp(-x)
    class discreteZipf;		// discrete Zipf 1/x^a (a > 0)
    class discreteZipf1;	// 1/x
    class discreteZipf2;	// 1/x^2
    class randomGaussian;	// continuous Gaussian
    class randomPoisson;	// continuous Posson
    class randomZipf;		// continuous Zipf
};

/// A functor to generate uniform random number in the range [0, 1).
class ibis::uniformRandomNumber {
public:
    virtual double operator()() = 0;
};

/// Mersenne Twister.  It generates uniform random numbers, which is
/// further used in other random number generators.
class ibis::MersenneTwister : public ibis::uniformRandomNumber {
public:
    /// Constructor.  This default constructor uses a value of the current
    /// time as the seed to initialize.  Define FASTBIT_USE_DEV_URANDOM if
    /// one desires to initialize the random number generator with a more
    /// unpredictable seed.
    MersenneTwister() {
	unsigned seed;
#if defined(FASTBIT_USE_DEV_URANDOM)
	FILE* fptr = fopen("/dev/urandom", "rb");
	if (fptr != 0) {
	    int ierr = fread(&seed, sizeof(seed), 1, fptr);
	    if (ierr < 1 || seed == 0)
		seed = time(0);
	}
	else {
#if defined(CLOCK_MONOTONIC) && !defined(__CYGWIN__)
	    struct timespec tb;
	    if (0 == clock_gettime(CLOCK_MONOTONIC, &tb))
		seed = (tb.tv_sec ^ tb.tv_nsec);
	    else
		seed = time(0);
#else
	seed = time(0);
#endif
	}
#else
#if defined(CLOCK_MONOTONIC) && !defined(__CYGWIN__)
	struct timespec tb;
	if (0 == clock_gettime(CLOCK_MONOTONIC, &tb))
	    seed = (tb.tv_sec ^ tb.tv_nsec);
	else
	    seed = time(0);
#else
	seed = time(0);
#endif
#endif
	setSeed(seed);
    }
    /// Constructor.  Uses a user specified integer as seed.
    explicit MersenneTwister(unsigned seed) {setSeed(seed);}

    /// Return a floating-point value in the range of [0, 1).
    virtual double operator()() {return nextDouble();}
    /// Next integer.
    int nextInt() {return next();}
    long nextLong() {return next();}
    float nextFloat() {return 2.3283064365386962890625e-10*next();}
    double nextDouble() {return 2.3283064365386962890625e-10*next();}
    /// Return integers in the range of [0, r)
    unsigned next(unsigned r) {return static_cast<unsigned>(r*nextDouble());}

    /// Initializing the array with a seed
    void setSeed(unsigned seed) {
	for (int i=0; i<624; i++) {
	    mt[i] = seed & 0xffff0000;
	    seed = 69069 * seed + 1;
	    mt[i] |= (seed & 0xffff0000) >> 16;
	    seed = 69069 * seed + 1;
	}
	mti = 624;
    }

    /// Generate the next random integer in the range of 0-(2^{32}-1).
    unsigned next() {
	unsigned y;
	if (mti >= 624) { /* generate 624 words at one time */
	    static unsigned mag01[2]={0x0, 0x9908b0df};
	    int kk;

	    for (kk=0; kk<227; kk++) {
		y = (mt[kk]&0x80000000)|(mt[kk+1]&0x7fffffff);
		mt[kk] = mt[kk+397] ^ (y >> 1) ^ mag01[y & 0x1];
	    }
	    for (; kk<623; kk++) {
		y = (mt[kk]&0x80000000)|(mt[kk+1]&0x7fffffff);
		mt[kk] = mt[kk-227] ^ (y >> 1) ^ mag01[y & 0x1];
	    }
	    y = (mt[623]&0x80000000)|(mt[0]&0x7fffffff);
	    mt[623] = mt[396] ^ (y >> 1) ^ mag01[y & 0x1];
	    mti = 0;
	}
  
	y = mt[mti++];
	y ^= (y >> 11);
	y ^= (y << 7) & 0x9d2c5680;
	y ^= (y << 15) & 0xefc60000;
	y ^= (y >> 18);

	return y; 
    }

private:
    // all data members are private
    int mti;
    unsigned mt[624]; /* the array for the state vector  */
}; // class MersenneTwister

// ********** the following random number generators need a uniformation
// ********** random number generator as the input

/// Continuous Poisson distribution.
class ibis::randomPoisson {
public:
    /// Constructor.  Must be supplied with a uniform random number generator.
    randomPoisson(uniformRandomNumber& ur) : urand(ur) {}
    double operator()() {return next();}
    double next() {return -log(urand());}

private:
    uniformRandomNumber& urand;
}; // ibis::randomPoisson

/// Continuous Gaussian distribution.  It uses the Box-Mueller
/// transformation to convert a uniform random number into Gaussian
/// distribution.
class ibis::randomGaussian {
public:
    /// Constructor.  Must be supplied with a uniform random number generator.
    explicit randomGaussian(uniformRandomNumber& ur)
	: urand(ur), has_extra(false), extra(0.0) {}
    /// Operator that returns the next random numbers.
    double operator()() {return next();}
    /// Next random number.
    double next() {
	if (has_extra) { /* has extra value from the previous run */
	    has_extra = false;
	    return extra;
	}
	else { /* Box-Mueller transformation */
	    double v1, v2, r, fac;
	    do {
		v1 = 2.0 * urand() - 1.0;
		v2 = 2.0 * urand() - 1.0;
		r = v1 * v1 + v2 * v2;
	    } while (r >= 1.0 || r <= 0.0);
	    fac = sqrt((-2.0 * log(r))/r);
	    has_extra = false;
	    extra = v2 * fac;
	    v1 *= fac;
	    return v1;
	}
    }

private:
    uniformRandomNumber& urand;
    bool has_extra;
    double extra;
}; // ibis::randomGaussian

/// Continuous Zipf distribution.  The Zipf exponent must be no less than
/// 1.
class ibis::randomZipf {
public:
    /// Constructor.  Must be supplied with a uniform random number generator.
    randomZipf(uniformRandomNumber& ur, double a=1) : urand(ur), alpha(a-1) {}
    /// Operator that returns the next random number.
    double operator()() {return next();}
    /// Next random number.
    double next() {
	if (alpha > 0.0)
	    return (exp(-log(1 - urand())/(alpha)) - 1);
	else
	    return (1.0 / (1.0 - urand()) - 1.0);
    }

private:
    uniformRandomNumber& urand;
    const double alpha; // Zipf exponent - 1
}; // ibis::randomZipf

/// Discrete random number with Poisson distribution exp(-x/lambda).
/// Use the rejection-inversion algorithm of W. Hormann and G. Derflinger.
class ibis::discretePoisson {
public:
    discretePoisson(ibis::uniformRandomNumber& ur,
		    const double lam=1.0, long m=0)
	: min0(m), lambda(lam), urand(ur) {init();}

    long operator()() {return next();}
    long next() {
	long k;
	double u, x;
	while (true) {
	    u = ym * (urand)();
	    x = - lambda * log(u * laminv);
	    k = static_cast<long>(x + 0.5);
	    if (k <= k0 && k-x <= xm)
		return k;
	    else if (u >= -exp(laminv*k+laminv2)*lambda-exp(laminv*k))
		return k;
	}
    } // next integer random number

private:
    // private member variables
    long min0, k0;
    double lambda, laminv, laminv2, xm, ym;
    uniformRandomNumber& urand;

    // private functions
    void init() { // check input parameters and initialize three constants
	if (! (lambda > DBL_MIN))
	    lambda = 1.0;
	laminv = -1.0 / lambda;
	laminv2 = 0.5*laminv;
	k0 = static_cast<long>(1.0+min0+1.0/(1.0-exp(laminv)));
	ym = -exp((min0+0.5)*laminv)*lambda - exp(min0*laminv);
	xm = min0 - log(ym*laminv);
    }
}; // class discretePoisson

/// Specialized version of the Poisson distribution exp(-x).
class ibis::discretePoisson1 {
public:
    explicit discretePoisson1(ibis::uniformRandomNumber& ur)
	: urand(ur) {init();}

    long operator()() {return next();}
    long next() {
	long k;
	double u, x;
	while (true) {
	    u = ym * (urand)();
	    x = - log(-u);
	    k = static_cast<long>(x + 0.5);
	    if (k <= k0 && k-x <= xm)
		return k;
	    else if (u >= -exp(-static_cast<double>(k)-0.5) -
		     exp(-static_cast<double>(k)))
		return k;
	}
    } // next integer random number

private:
    // private member variables
    double xm, ym;
    long k0;
    uniformRandomNumber& urand;

    // private functions
    void init() { // check input parameters and initialize three constants
	k0 = static_cast<long>(1.0+1.0/(1.0-exp(-1.0)));
	ym = - exp(-0.5) - 1.0;
	xm = - log(-ym);
    }
}; // class discretePoisson1

/// Discrete Zipf distribution.  The value returned follow the probability
/// distribution (1+k)^(-a) where a >= 0, k >= 0.  For a > 1, it uses the
/// rejection-inversion algorithm of W. Hormann and G. Derflinger.  For a
/// between 0 and 1, it uses a simple rejection method.
class ibis::discreteZipf {
public:
    discreteZipf(ibis::uniformRandomNumber& ur, double a=2.0,
		 unsigned long imax = ULONG_MAX)
	: urand(ur), max0(imax), alpha(a) {init();}

    /// Return a discrete random number in the range of [0, imax].
    unsigned long operator()() {return next();}
    unsigned long next() {
	if (alpha > 1.0) { // rejection-inversion
	    while (true) {
		double ur = (urand)();
		ur = hxm + ur * hx0;
		double x = Hinv(ur);
		unsigned long k = static_cast<unsigned long>(0.5+x);
		if (k - x <= ss)
		    return k;
		else if (ur >= H(0.5+k) - exp(-log(k+1.0)*alpha))
		    return k;
	    }
	}
	else { // simple rejection
	    unsigned long k = ((long) (urand() * max0)) % max0;
	    double freq = pow((1.0+k), -alpha);
	    while (urand() >= freq) {
		k = ((long) (urand() * max0)) % max0;
		freq = pow((1.0+k), -alpha);
	    }
	    return k;
	}
    } // next

private:
    // private member variables
    uniformRandomNumber& urand;
    long unsigned max0;
    double alpha, alpha1, alphainv, hx0, hxm, ss;

    // private member function
    double H(double x) {return (exp(alpha1*log(1.0+x)) * alphainv);}
    double Hinv(double x) {return exp(alphainv*log(alpha1*x)) - 1.0;}
    void init() {
	// enforce the condition that alpha >= 0 and max0 > 1
	if (max0 <= 1)
	    max0 = 100;
	if (! (alpha >= 0.0))
	    alpha = 1.0;
	if (alpha > 1.0) {
	    // the rejection-inversion algorithm of W. Hormann and
	    // G. Derflinger
	    alpha1 = 1.0 - alpha;
	    alphainv = 1.0 / alpha1;
	    hxm = H(max0 + 0.5);
	    hx0 = H(0.5) - 1.0 - hxm;
	    ss = 1 - Hinv(H(1.5)-exp(-alpha*log(2.0)));
	}
	else { // use a simple rejection scheme
	    alpha1 = 0.0;
	    alphainv = 0.0;
	    hxm = 0.0;
	    hx0 = 0.0;
	    ss  = 0.0;
	}
    }
}; // Zipf distribution

/// A specialized version of the Zipf distribution f(x) = 1/(1+x)^2.
/// Should be much faster than using discreteZipf(2,imax).
class ibis::discreteZipf2 {
public:
    discreteZipf2(ibis::uniformRandomNumber& ur,
		  unsigned long imax = ULONG_MAX) :
	max0(imax), urand(ur) {init();}

    /// Return a discrete random number in the range of [0, imax]
    unsigned long operator()() {return next();}
    unsigned long next() {
	while (true) {
	    double ur = (urand)();
	    ur = hxm + ur * hx0;
	    double x = Hinv(ur);
	    unsigned long k = static_cast<unsigned long>(0.5+x);
	    if (k - x <= ss)
		return k;
	    else if (ur >= H(0.5+k) - 1.0/((1.0+x)*(1.0+x)))
		return k;
	}
    } // next

private:
    // private member variables
    double hx0, hxm, ss;
    long unsigned max0;
    uniformRandomNumber& urand;

    // private member function
    double H(double x) {return -1.0 / (1.0 + x);}
    double Hinv(double x) {return (- 1.0 / x) - 1.0;}
    void init() {
	hxm = H(max0+0.5);
	hx0 = - 5.0/3.0 - hxm;
	ss = 1 - Hinv(H(1.5)-0.25);
    }
}; // Zipf2 distribution

/// A specialized case of the Zipf distribution f(x) = 1/(1+x).
///
/// @note This is an experimental approach; not necessarily faster than the
/// generic version.
class ibis::discreteZipf1 {
public:
    discreteZipf1(ibis::uniformRandomNumber& ur, unsigned long imax = 100) :
	card(imax+1), cpd(imax+1), urand(ur) {init();}

    /// Return a discrete random number in the range of [0, imax].
    unsigned long operator()() {return next();}
    unsigned long next() {
	double ur = (urand)();
	if (ur <= cpd[0]) return 0;
	// return the minimal i such that cdf[i] >= ur
	unsigned long i, j, k;
	i = 0;
	j = card-1;
	k = (i + j) / 2;
	while (i < k) {
	    if (cpd[k] > ur)
		j = k;
	    else if (cpd[k] < ur)
		i = k;
	    else
		return k;
	    k = (i + j) / 2;
	}
	if (cpd[i] >= ur)
	    return i;
	else
	    return j;
    } // next

private:
    // private member variables
    const unsigned long card;
    std::vector<double> cpd; // cumulative probability distribution
    uniformRandomNumber& urand;

    // private member function
    void init() { // generates the cpd
	const unsigned n = cpd.size();
	if (n < 2 || n > 1024*1024 || card != n)
	    throw "imax must be in [2, 1000000]";

	cpd[0] = 1.0;
	for (unsigned i = 1; i < n; ++i)
	    cpd[i] = cpd[i-1] + 1.0 / (1.0 + i);
	double ss = 1.0 / cpd.back();
	for (unsigned i = 0; i < n; ++i)
	    cpd[i] *= ss;
    } // init
}; // Zipf1 distribution
#endif // IBIS_TWISTER_H
