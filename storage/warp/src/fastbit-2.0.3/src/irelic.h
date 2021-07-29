//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
//         Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_IRELIC_H
#define IBIS_IRELIC_H
///@file
/// Define ibis::relic and its derived classes
///@verbatim
/// relic -> skive, fade, bylt (pack), zona (zone), fuzz
/// fade -> sbiad, sapid
///@endverbatim
///
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)	// some identifier longer than 256 characters
#endif
#include "index.h"

/// The basic bitmap index.  It generates one bitmap for each distinct
/// value.
class ibis::relic : public ibis::index {
public:
    virtual ~relic() {clear();};
    relic(const relic&);
    relic(const ibis::column* c, const char* f = 0);
    relic(const ibis::column* c, uint32_t popu, uint32_t ntpl=0);
    relic(const ibis::column* c, uint32_t card, array_t<uint32_t>& ints);
    relic(const ibis::column* c, ibis::fileManager::storage* st,
	  size_t start = 8);
    relic(const ibis::column* c, uint32_t nb, double *keys, int64_t *offs);
    relic(const ibis::column* c, uint32_t nb, double *keys, int64_t *offs,
          uint32_t *bms);
    relic(const ibis::column* c, uint32_t nb, double *keys, int64_t *offs,
          void *bms, FastBitReadBitmaps rd);

    virtual index* dup() const;
    virtual void print(std::ostream& out) const;
    virtual void serialSizes(uint64_t&, uint64_t&, uint64_t&) const;
    virtual int write(ibis::array_t<double> &,
                      ibis::array_t<int64_t> &,
                      ibis::array_t<uint32_t> &) const;
    virtual int  write(const char* dt) const;
    virtual int  read(const char* idxfile);
    virtual int  read(ibis::fileManager::storage* st);
    virtual long append(const char* dt, const char* df, uint32_t nnew);

    virtual long select(const ibis::qContinuousRange&, void*) const;
    virtual long select(const ibis::qContinuousRange&, void*,
			ibis::bitvector&) const;

    using ibis::index::estimate;
    using ibis::index::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual long evaluate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& hits) const;

    virtual void estimate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& lower,
			  ibis::bitvector& upper) const {
	(void) evaluate(expr, lower);
	upper.clear();
    }
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    /// This class and its derived classes should produce exact answers,
    /// therefore no undecidable rows.
    virtual float undecidable(const ibis::qContinuousRange &,
			      ibis::bitvector &iffy) const {
	iffy.clear();
	return 0.0;
    }
    virtual void estimate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& lower,
			  ibis::bitvector& upper) const {
	evaluate(expr, lower);
	upper.clear();
    }
    virtual uint32_t estimate(const ibis::qDiscreteRange&) const;
    virtual float undecidable(const ibis::qDiscreteRange&,
			      ibis::bitvector& iffy) const {
	iffy.clear();
	return 0.0;
    }

    virtual double estimateCost(const ibis::qContinuousRange& expr) const;
    virtual double estimateCost(const ibis::qDiscreteRange& expr) const;

    /// Estimate the pairs for the range join operator.  Only records that
    /// are masked are evaluated.
    virtual void estimate(const ibis::relic& idx2,
			  const ibis::deprecatedJoin& expr,
			  const ibis::bitvector& mask,
			  ibis::bitvector64& lower,
			  ibis::bitvector64& upper) const;
    virtual void estimate(const ibis::relic& idx2,
			  const ibis::deprecatedJoin& expr,
			  const ibis::bitvector& mask,
			  const ibis::qRange* const range1,
			  const ibis::qRange* const range2,
			  ibis::bitvector64& lower,
			  ibis::bitvector64& upper) const;
    /// Estimate an upper bound for the number of pairs produced from
    /// marked records.
    virtual int64_t estimate(const ibis::relic& idx2,
			     const ibis::deprecatedJoin& expr,
			     const ibis::bitvector& mask) const;
    virtual int64_t estimate(const ibis::relic& idx2,
			     const ibis::deprecatedJoin& expr,
			     const ibis::bitvector& mask,
			     const ibis::qRange* const range1,
			     const ibis::qRange* const range2) const;

    virtual INDEX_TYPE type() const {return RELIC;}
    virtual const char* name() const {return "basic";}
    // bin boundaries and counts of each bin
    virtual void binBoundaries(std::vector<double>& b) const;
    virtual void binWeights(std::vector<uint32_t>& b) const;

    virtual long getCumulativeDistribution(std::vector<double>& bds,
					   std::vector<uint32_t>& cts) const;
    virtual long getDistribution(std::vector<double>& bds,
				 std::vector<uint32_t>& cts) const;
    virtual double getMin() const {return (vals.empty()?DBL_MAX:vals[0]);}
    virtual double getMax() const {return (vals.empty()?-DBL_MAX:vals.back());}
    virtual double getSum() const;

    virtual void speedTest(std::ostream& out) const;
    long append(const ibis::relic& tail);
    long append(const array_t<uint32_t>& ind);
    array_t<uint32_t>* keys(const ibis::bitvector& mask) const;

    /// A single value with known positions.
    template <typename T>
    struct valpos {
	/// The value.
	T val;
	/// The index set representing the positions with the given value.
	bitvector::indexSet ind;
	/// The current index value inside the index set.  If the index set
	/// is a range, this is the actual position (RID), otherwise the
	/// positions()[j] holds the position (RID).
	bitvector::word_t j;

	/// Default constrtuctor.
	valpos<T>() : val(0), j(0) {}
	/// Specifying the values.
	valpos<T>(const T v, const bitvector& b)
	: val(v), ind(b.firstIndexSet()), j(0) {
	    if (ind.nIndices() > 0 && ind.isRange())
		j = *(ind.indices());
	}

	/// Current position (RID).
	bitvector::word_t position() const {
	    if (ind.isRange())
		return j;
	    else
		return ind.indices()[j];
	}

	/// Move to the next row.
	void next() {
	    ++ j;
	    if (ind.isRange()) {
		if (j >= ind.indices()[1]) {
		    ++ ind;
		    if (ind.nIndices() > 0 && ind.isRange())
			j = ind.indices()[0];
		    else
			j = 0;
		}
	    }
	    else if (j >= ind.nIndices()) {
		++ ind;
		if (ind.nIndices() > 0 && ind.isRange())
		    j = ind.indices()[0];
		else
		    j = 0;
	    }
	}
    }; // valpos

    /// The comparator used to build a min-heap based on positions.
    template<typename T>
    struct comparevalpos {
	bool operator()(const valpos<T>* x, const valpos<T>* y) {
	    return (x->position() > y->position());
	}
    }; // comparevalpos

    void construct(const char* f = 0);
    template <typename E>
    void construct(const array_t<E>& arr);

    void     locate(const ibis::qContinuousRange& expr,
		    uint32_t& hit0, uint32_t& hit1) const;

protected:
    // protected member variables
    array_t<double> vals;

    // protected member functions
    int write32(int fdes) const;
    int write64(int fdes) const;
    uint32_t locate(const double& val) const;

    // a dummy constructor
    relic() : ibis::index() {}
    // free current resources, re-initialized all member variables
    virtual void clear();
    virtual double computeSum() const;
    virtual size_t getSerialSize() const throw();

    long mergeValues(uint32_t, uint32_t, void*) const;
    template <typename T> static long
    mergeValuesT(const array_t<T>& vs,
		 const array_t<const bitvector*>& ps,
		 array_t<T>& res);

    template <typename T> struct mappedValues;

private:
    // private member functions
    int64_t equiJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     ibis::bitvector64& hits) const;
    int64_t deprecatedJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		      const double& delta, ibis::bitvector64& hits) const;
    int64_t compJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     const ibis::math::term& delta,
		     ibis::bitvector64& hits) const;

    int64_t equiJoin(const ibis::relic& idx2,
		     const ibis::bitvector& mask) const;
    int64_t deprecatedJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		      const double& delta) const;
    int64_t compJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     const ibis::math::term& delta) const;

    int64_t equiJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     const ibis::qRange* const range1,
		     const ibis::qRange* const range2,
		     ibis::bitvector64& hits) const;
    int64_t deprecatedJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		      const ibis::qRange* const range1,
		      const ibis::qRange* const range2,
		      const double& delta, ibis::bitvector64& hits) const;
    /// Can not make good use of range restrictions when the distance
    /// function in the join expression is an arbitrary function.
    int64_t compJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     const ibis::qRange* const range1,
		     const ibis::qRange* const range2,
		     const ibis::math::term& delta,
		     ibis::bitvector64& hits) const {
	return compJoin(idx2, mask, delta, hits);
    }

    int64_t equiJoin(const ibis::relic& idx2,
		     const ibis::bitvector& mask,
		     const ibis::qRange* const range1,
		     const ibis::qRange* const range2) const;
    int64_t deprecatedJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		      const ibis::qRange* const range1,
		      const ibis::qRange* const range2,
		      const double& delta) const;
    /// Can not make good use of range restrictions when the distance
    /// function in the join expression is an arbitrary function.
    int64_t compJoin(const ibis::relic& idx2, const ibis::bitvector& mask,
		     const ibis::qRange* const range1,
		     const ibis::qRange* const range2,
		     const ibis::math::term& delta) const {
	return compJoin(idx2, mask, delta);
    }

    relic& operator=(const relic&);
}; // ibis::relic

/// The binary encoded index with recoding of keyvalues.
///
/// @note The work skive is the Danish for slice.  This is a weired version
/// of the bit-sliced index because it encodes the keyvalues to be between
/// 0 and cnts.size()-1.  The alternative version ibis::slice will use bit
/// slices more strictly.
class ibis::skive : public ibis::relic {
public:
    virtual ~skive();
    skive(const ibis::column* c = 0, const char* f = 0);
    skive(const ibis::column* c, ibis::fileManager::storage* st,
	  size_t start = 8);

    virtual int  write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual int  read(const char* idxfile);
    virtual int  read(ibis::fileManager::storage* st);

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    virtual long select(const ibis::qContinuousRange&, void*) const {
	return -1;}
    virtual long select(const ibis::qContinuousRange&, void*,
			ibis::bitvector&) const {
	return -1;}

    using ibis::relic::estimate;
    using ibis::relic::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual long evaluate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& hits) const;

    virtual void estimate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& lower,
			  ibis::bitvector& upper) const;
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    virtual INDEX_TYPE type() const {return SKIVE;}
    virtual const char* name() const {return "binary-encoded";}
    // number of records in each bin
    virtual void binWeights(std::vector<uint32_t>& b) const;
    virtual double getSum() const;

    virtual void speedTest(std::ostream& out) const;
    virtual double estimateCost(const ibis::qContinuousRange&) const {
	double ret;
	if (offset64.size() > bits.size())
	    ret = offset64.back();
	else if (offset32.size() > bits.size())
	    ret = offset32.back();
	else
	    ret = 0.0;
	return ret;
    }
    virtual double estimateCost(const ibis::qDiscreteRange& expr) const {
	double ret;
	if (offset64.size() > bits.size())
	    ret = offset64.back();
	else if (offset32.size() > bits.size())
	    ret = offset32.back();
	else
	    ret = 0.0;
	return ret;
    }

protected:
    virtual void clear();
    virtual size_t getSerialSize() const throw();

    array_t<uint32_t> cnts; // the counts for each distinct value

    int write32(int fdes) const;
    int write64(int fdes) const;
    void evalGE(ibis::bitvector& res, uint32_t b) const;
    void evalEQ(ibis::bitvector& res, uint32_t b) const;

private:
    skive(const skive&);
    skive& operator=(const skive&);

    // private member functions
    void construct1(const char* f = 0); // uses more temporary storage
    void construct2(const char* f = 0); // passes through data twice
    void setBit(const uint32_t i, const double val);
}; // ibis::skive

/// The bit-sliced index.  This version strictly slices the binary bits of
/// the incoming values.  It also supports operations on bit slices.
class ibis::slice : public ibis::skive {
public:
    virtual ~slice();
    slice(const ibis::column* c = 0, const char* f = 0);
    slice(const ibis::column* c, ibis::fileManager::storage* st,
	  size_t start = 8);
    virtual INDEX_TYPE type() const {return SLICE;}
    virtual const char* name() const {return "bit-slice";}

    virtual int  write(const char* dt) const;
    virtual void print(std::ostream& out) const;

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    static bool isSuitable(const column&, const char*);

private:
    slice(const slice&);
    slice& operator=(const slice&);

    // private member functions
    int construct(const char* f = 0); // passes through data twice
    template <typename T> int constructT(const char*);
}; // ibis::slice

/// The multicomponent range-encoded index.  Defined by Chan and Ioannidis
/// (SIGMOD 98).
class ibis::fade : public ibis::relic {
public:
    virtual ~fade() {clear();};
    fade(const ibis::column* c = 0, const char* f = 0,
	 const uint32_t nbase = 2);
    fade(const ibis::column* c, ibis::fileManager::storage* st,
	 size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual int read(const char* idxfile);
    virtual int read(ibis::fileManager::storage* st);

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    virtual long select(const ibis::qContinuousRange&, void*) const {
	return -1;}
    virtual long select(const ibis::qContinuousRange&, void*,
			ibis::bitvector&) const {
	return -1;}

    using ibis::relic::estimate;
    using ibis::relic::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual long evaluate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& hits) const;

    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    virtual INDEX_TYPE type() const {return FADE;}
    virtual const char* name() const {return "multi-level range";}

    virtual void speedTest(std::ostream& out) const;
    // number of records in each bin
    virtual void binWeights(std::vector<uint32_t>& b) const;
    virtual double getSum() const;
    virtual double estimateCost(const ibis::qContinuousRange& expr) const;
    //virtual double estimateCost(const ibis::qDiscreteRange& expr) const;

protected:
    // protected member variables
    array_t<uint32_t> cnts; // the counts for each distinct value
    array_t<uint32_t> bases;// the values of the bases used

    // protected member functions to be used by derived classes
    int write32(int fdes) const;
    int write64(int fdes) const;
    virtual void clear();
    virtual size_t getSerialSize() const throw();

private:
    // private member functions
    void setBit(const uint32_t i, const double val);
    void construct1(const char* f = 0, const uint32_t nbase = 2);
    void construct2(const char* f = 0, const uint32_t nbase = 2);

    void evalEQ(ibis::bitvector& res, uint32_t b) const;
    void evalLE(ibis::bitvector& res, uint32_t b) const;
    void evalLL(ibis::bitvector& res, uint32_t b0, uint32_t b1) const;

    fade(const fade&);
    fade& operator=(const fade&);
}; // ibis::fade

/// The multicomponent interval encoded index.  Defined by Chan and
/// Ioannidis (SIGMOD 99).
class ibis::sbiad : public ibis::fade {
public:
    virtual ~sbiad() {clear();};
    sbiad(const ibis::column* c = 0, const char* f = 0,
	  const uint32_t nbase = 2);
    sbiad(const ibis::column* c, ibis::fileManager::storage* st,
	  size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual long append(const char* dt, const char* df, uint32_t nnew);

    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual long evaluate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& hits) const;

    virtual INDEX_TYPE type() const {return SBIAD;}
    virtual const char* name() const {return "multi-level interval";}

    virtual void speedTest(std::ostream& out) const;
    //virtual double estimateCost(const ibis::qContinuousRange& expr) const;
    //virtual double estimateCost(const ibis::qDiscreteRange& expr) const;

private:
    // private member functions
    void setBit(const uint32_t i, const double val);
    void construct1(const char* f = 0, const uint32_t nbase = 2);
    void construct2(const char* f = 0, const uint32_t nbase = 2);

    void evalEQ(ibis::bitvector& res, uint32_t b) const;
    void evalLE(ibis::bitvector& res, uint32_t b) const;
    void evalLL(ibis::bitvector& res, uint32_t b0, uint32_t b1) const;

    sbiad(const sbiad&);
    sbiad& operator=(const sbiad&);
}; // ibis::sbiad

/// The multicomponent equality encoded index.
class ibis::sapid : public ibis::fade {
public:
    virtual ~sapid() {clear();};
    sapid(const ibis::column* c = 0, const char* f = 0,
	  const uint32_t nbase = 2);
    sapid(const ibis::column* c, ibis::fileManager::storage* st,
	  size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual long append(const char* dt, const char* df, uint32_t nnew);

    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual long evaluate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& hits) const;

    virtual INDEX_TYPE type() const {return SAPID;}
    virtual const char* name() const {return "multi-level equality";}

    virtual void speedTest(std::ostream& out) const;
    //virtual double estimateCost(const ibis::qContinuousRange& expr) const;
    //virtual double estimateCost(const ibis::qDiscreteRange& expr) const;

private:
    // private member functions
    void setBit(const uint32_t i, const double val);
    void construct1(const char* f = 0, const uint32_t nbase = 2);
    void construct2(const char* f = 0, const uint32_t nbase = 2);

    void addBits_(uint32_t ib, uint32_t ie, ibis::bitvector& res) const;
    void evalEQ(ibis::bitvector& res, uint32_t b) const;
    void evalLE(ibis::bitvector& res, uint32_t b) const;
    void evalLL(ibis::bitvector& res, uint32_t b0, uint32_t b1) const;

    sapid(const sapid&);
    sapid& operator=(const sapid&);
}; // ibis::sapid

/// The precise version of two-level interval-equality index.
///
/// @note In fuzzy classification / clustering, many interval equality
/// conditions are used.  Hence the crazy name.
class ibis::fuzz : public ibis::relic {
public:
    virtual ~fuzz() {clear();};
    fuzz(const ibis::column* c = 0, const char* f = 0);
    fuzz(const ibis::column* c, ibis::fileManager::storage* st,
	 size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual int read(const char* idxfile);
    virtual int read(ibis::fileManager::storage* st);

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    using ibis::relic::evaluate;
    using ibis::relic::estimate;
    using ibis::relic::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    virtual double estimateCost(const ibis::qContinuousRange& expr) const;

    virtual INDEX_TYPE type() const {return FUZZ;}
    virtual const char* name() const {return "interval-equality";}

protected:
    virtual void clear();
    virtual size_t getSerialSize() const throw();

private:
    /// The fine level is stored in ibis::relic, the parent class, only
    /// the coarse bins are stored here.  The coarse bins use integer bin
    /// boundaries; these integers are indices to the array vals and bits.
    mutable array_t<bitvector*> cbits;
    array_t<uint32_t> cbounds;
    mutable array_t<int32_t> coffset32;
    mutable array_t<int64_t> coffset64;

    void coarsen(); // given fine level, add coarse level
    void activateCoarse() const; // activate all coarse level bitmaps
    void activateCoarse(uint32_t i) const; // activate one bitmap
    void activateCoarse(uint32_t i, uint32_t j) const;

    int writeCoarse32(int fdes) const;
    int writeCoarse64(int fdes) const;
    int readCoarse(const char *fn);
    void clearCoarse();

    /// Estimate the cost of answer a range query [lo, hi).
    long coarseEstimate(uint32_t lo, uint32_t hi) const;
    /// Evaluate the range condition [lo, hi) and place the result in @c res.
    long coarseEvaluate(uint32_t lo, uint32_t hi, ibis::bitvector& res) const;

    fuzz(const fuzz&);
    fuzz& operator=(const fuzz&);
}; // ibis::fuzz

/// The precise version of the two-level range-equality index.
///
/// @note Bylt is Danish word for pack, the name of the binned version of
/// the two-level range-equality code.
class ibis::bylt : public ibis::relic {
public:
    virtual ~bylt() {clear();};
    bylt(const ibis::column* c = 0, const char* f = 0);
    bylt(const ibis::column* c, ibis::fileManager::storage* st,
	 size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual int read(const char* idxfile);
    virtual int read(ibis::fileManager::storage* st);

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    using ibis::relic::evaluate;
    using ibis::relic::estimate;
    using ibis::relic::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    virtual double estimateCost(const ibis::qContinuousRange& expr) const;

    virtual INDEX_TYPE type() const {return BYLT;}
    virtual const char* name() const {return "range-equality";}

protected:
    virtual void clear();
    virtual size_t getSerialSize() const throw();

private:
    // the fine level is stored in ibis::relic, the parent class, only the
    // coarse bins are stored here.  The coarse bins use integer bin
    // boundaries; these integers are indices to the array vals and bits.
    mutable array_t<bitvector*> cbits;
    array_t<uint32_t> cbounds;
    mutable array_t<int32_t> coffset32;
    mutable array_t<int64_t> coffset64;

    void coarsen(); // given fine level, add coarse level
    void activateCoarse() const; // activate all coarse level bitmaps
    void activateCoarse(uint32_t i) const; // activate one bitmap
    void activateCoarse(uint32_t i, uint32_t j) const;

    int writeCoarse32(int fdes) const;
    int writeCoarse64(int fdes) const;
    int readCoarse(const char *fn);

    bylt(const bylt&);
    bylt& operator=(const bylt&);
}; // ibis::bylt

/// The precise version of the two-level equality-equality index.
///
/// @note Zona is Italian word for zone, the name of the binned version of
/// the two-level equality-equality code.
class ibis::zona : public ibis::relic {
public:
    virtual ~zona() {clear();};
    zona(const ibis::column* c = 0, const char* f = 0);
    zona(const ibis::column* c, ibis::fileManager::storage* st,
	 size_t start = 8);

    virtual int write(const char* dt) const;
    virtual void print(std::ostream& out) const;
    virtual int read(const char* idxfile);
    virtual int read(ibis::fileManager::storage* st);

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    using ibis::relic::evaluate;
    using ibis::relic::estimate;
    using ibis::relic::estimateCost;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    virtual double estimateCost(const ibis::qContinuousRange& expr) const;

    virtual INDEX_TYPE type() const {return ZONA;}
    virtual const char* name() const {return "equality-equality";}

protected:
    virtual void clear();
    virtual size_t getSerialSize() const throw();

private:
    // the fine level is stored in ibis::relic, the parent class, only the
    // coarse bins are stored here.  The coarse bins use integer bin
    // boundaries; these integers are indices to the array vals and bits.
    mutable array_t<bitvector*> cbits;
    array_t<uint32_t> cbounds;
    mutable array_t<int32_t> coffset32;
    mutable array_t<int64_t> coffset64;

    void coarsen(); // given fine level, add coarse level
    void activateCoarse() const; // activate all coarse level bitmaps
    void activateCoarse(uint32_t i) const; // activate one bitmap
    void activateCoarse(uint32_t i, uint32_t j) const;

    int writeCoarse32(int fdes) const;
    int writeCoarse64(int fdes) const;
    int readCoarse(const char *fn);

    zona(const zona&);
    zona& operator=(const zona&);
}; // ibis::zona

/// A struct to hold a set of values and their positions.  The values are
/// held in a heap according to their first positions.
template <typename T>
struct ibis::relic::mappedValues {
}; // ibis::relic::mappedValues
#endif // IBIS_IRELIC_H
