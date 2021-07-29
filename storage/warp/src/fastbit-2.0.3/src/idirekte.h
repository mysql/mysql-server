//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2006-2016 the Regents of the University of California
#ifndef IBIS_DIREKTE_H
#define IBIS_DIREKTE_H
///@file
/// This is an implementation of the the simple bitmap index without the
/// first binning step.  It directly uses the integer values as bin number.
/// The word @c direkte in Danish means @c direct.
#include "index.h"

/// A version of precise index that directly uses the integer values.  It
/// can avoid some intemdiate steps during index building and query
/// answering.  However, this class can only be used with integer column
/// with nonnegative values.  Ideally, the values should start with 0, and
/// only use small positive integers.
class ibis::direkte : public ibis::index {
public:
    virtual INDEX_TYPE type() const {return DIREKTE;}
    virtual const char* name() const {return "direct";}

    using ibis::index::evaluate;
    using ibis::index::estimate;
    virtual long evaluate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& hits) const;
    virtual void estimate(const ibis::qContinuousRange& expr,
			  ibis::bitvector& lower,
			  ibis::bitvector& upper) const;
    virtual uint32_t estimate(const ibis::qContinuousRange& expr) const;
    virtual float undecidable(const ibis::qContinuousRange& expr,
			      ibis::bitvector& iffy) const {
	iffy.clear();
	return 0.0;
    }

    virtual long evaluate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& hits) const;
    virtual void estimate(const ibis::qDiscreteRange& expr,
			  ibis::bitvector& lower,
			  ibis::bitvector& upper) const;
    virtual uint32_t estimate(const ibis::qDiscreteRange& expr) const;
    virtual float undecidable(const ibis::qDiscreteRange& expr,
			      ibis::bitvector& iffy) const {
	iffy.clear();
	return 0.0;
    }

    virtual double estimateCost(const ibis::qContinuousRange& expr) const;
    virtual double estimateCost(const ibis::qDiscreteRange& expr) const;

    virtual long select(const ibis::qContinuousRange&, void*) const {
	return -1;}
    virtual long select(const ibis::qContinuousRange&, void*,
			ibis::bitvector&) const {
	return -1;}

    virtual void print(std::ostream& out) const;
    virtual void serialSizes(uint64_t&, uint64_t&, uint64_t&) const;
    virtual int write(ibis::array_t<double> &,
                      ibis::array_t<int64_t> &,
                      ibis::array_t<uint32_t> &) const;
    virtual int write(const char* name) const;
    virtual int read(const char* name);
    virtual int read(ibis::fileManager::storage* st);

    virtual long append(const char* dt, const char* df, uint32_t nnew);

    long append(const ibis::direkte& tail);
    long append(const array_t<uint32_t>& ind);

    void ints(array_t<uint32_t>&) const;
    int remapKeys(const ibis::array_t<uint32_t>&);
    array_t<uint32_t>* keys(const ibis::bitvector& mask) const;

    /// Time some logical operations and print out their speed.  This
    /// version does nothing.
    virtual void speedTest(std::ostream& out) const {};

    virtual void binBoundaries(std::vector<double>&) const;
    virtual void binWeights(std::vector<uint32_t>&) const;

    virtual double getMin() const {return 0.0;}
    virtual double getMax() const {return(bits.size()-1.0);}
    virtual double getSum() const;
    virtual long getCumulativeDistribution
    (std::vector<double>& bds, std::vector<uint32_t>& cts) const;
    virtual long getDistribution
    (std::vector<double>& bbs, std::vector<uint32_t>& cts) const;

    virtual index* dup() const;
    virtual ~direkte() {clear();}
    direkte(const direkte &rhs) : index(rhs) {};
    direkte(const ibis::column* c, const char* f = 0);
    direkte(const ibis::column* c, ibis::fileManager::storage* st);
    direkte(const ibis::column* c, uint32_t popu, uint32_t ntpl=0);
    direkte(const ibis::column* c, uint32_t card, array_t<uint32_t>& ints);

protected:
    template <typename T>
    int construct(const char* f);
    template <typename T>
    int construct0(const char* f);

    void locate(const ibis::qContinuousRange& expr,
		uint32_t& hit0, uint32_t& hit1) const;
    virtual size_t getSerialSize() const throw();

    direkte();
    direkte& operator=(const direkte&);
}; // ibis::direkte

#endif
