// File: $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
#ifndef IBIS_BORD_H
#define IBIS_BORD_H
#include "table.h"	// ibis::table
#include "util.h"	// ibis::partList
#include "part.h"	// ibis::part
#include "selectClause.h"// ibis::selectClause
#include "dictionary.h"	// ibis::dictionary

///@file
/// Defines ibis::bord.  This is an in-memory data table, with a single
/// data partition completely residing in memory.
namespace ibis {
    class bord;
    class hyperslab;
} // namespace ibis

/// Class ibis::hyperslab for recording a HDF5 style hyperslab.  It is a
/// generic specification of subsets of coordinates on a regular mesh.
class FASTBIT_CXX_DLLSPEC ibis::hyperslab {
 public:
    /// Default constructor.  By design, the unspecified dimensions are
    /// assumed to cover the whole extends of dimensions.
    hyperslab() : ndim(0) {}
    hyperslab(unsigned, const uint64_t*, const uint64_t*,
              const uint64_t*, const uint64_t*);
    void tobitvector(uint32_t, const uint64_t*, ibis::bitvector&) const;

    /// The number of dimensions of the mesh.  By default, ndim = 0, which
    /// indicates that everyone mesh point is selected.
    unsigned ndim;
    /// An array of size 4*ndim with ndim quadruples of (start, stride,
    /// count, block).  These four elements are in the same order as
    /// specified in the command line for various HDF5 functions.
    ibis::array_t<uint64_t> vals;
}; // class ibis::hyperslab

/// Class ibis::bord stores all its data in memory.  The function @c
/// ibis::table::select produces an ibis::bord object to store nontrivial
/// results.
///
/// @note Since all data records are stored in memory, the number of rows
/// that can be stored is limited.  Even when there is sufficient memory,
/// because the number of rows is internally stored as a 32-bit integer, it
/// can represent no more than 2 billion rows.
///
/// @note Bord is a Danish word for "table."
class FASTBIT_CXX_DLLSPEC ibis::bord : public ibis::table, public ibis::part {
public:
    bord(const char *tn, const char *td, uint64_t nr,
	 ibis::table::bufferArray &buf,
	 const ibis::table::typeArray &ct,
	 const ibis::table::stringArray &cn,
	 const ibis::table::stringArray *cdesc=0,
	 const std::vector<const ibis::dictionary*> *dct=0);
    bord(const char *tn, const char *td,
	 const ibis::selectClause &sc, const ibis::part &ref);
    bord(const char *tn, const char *td,
	 const ibis::selectClause &sc, const ibis::constPartList &ref);
    virtual ~bord() {clear();}

    virtual uint64_t nRows() const {return nEvents;}
    virtual uint32_t nColumns() const {return ibis::part::nColumns();}

    virtual ibis::table::stringArray columnNames() const;
    virtual ibis::table::typeArray columnTypes() const;

    virtual void describe(std::ostream&) const;
    virtual void dumpNames(std::ostream&, const char*) const;
    virtual int dump(std::ostream&, const char*) const;
    virtual int dump(std::ostream&, uint64_t, const char*) const;
    virtual int dumpJSON(std::ostream&, uint64_t) const;
    virtual int dump(std::ostream&, uint64_t, uint64_t, const char*) const;
    virtual int backup(const char* dir, const char* tname=0,
		       const char* tdesc=0) const;

    virtual int64_t getColumnAsBytes(const char*, char*,
				     uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsUBytes(const char*, unsigned char*,
				      uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsShorts(const char*, int16_t*,
				      uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsUShorts(const char*, uint16_t*,
				       uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsInts(const char*, int32_t*,
				    uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsUInts(const char*, uint32_t*,
				     uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsLongs(const char*, int64_t*,
				     uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsULongs(const char*, uint64_t*,
				      uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsFloats(const char*, float*,
				      uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsDoubles(const char*, double*,
				       uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsDoubles(const char*, std::vector<double>&,
				       uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsStrings(const char*, std::vector<std::string>&,
				       uint64_t =0, uint64_t =0) const;
    virtual int64_t getColumnAsOpaques(const char*, std::vector<ibis::opaque>&,
				       uint64_t =0, uint64_t =0) const;
    virtual double getColumnMin(const char*) const;
    virtual double getColumnMax(const char*) const;

    virtual long getHistogram(const char*, const char*,
			      double, double, double,
			      std::vector<uint32_t>&) const;
    virtual long getHistogram2D(const char*, const char*,
				double, double, double,
				const char*,
				double, double, double,
				std::vector<uint32_t>&) const;
    virtual long getHistogram3D(const char*, const char*,
				double, double, double,
				const char*,
				double, double, double,
				const char*,
				double, double, double,
				std::vector<uint32_t>&) const;

    virtual void estimate(const char* cond,
			  uint64_t &nmin, uint64_t &nmax) const;
    virtual void estimate(const ibis::qExpr* cond,
			  uint64_t &nmin, uint64_t &nmax) const;
    using table::select;
    virtual table* select(const char* sel, const char* cond) const;
    virtual table* groupby(const ibis::table::stringArray&) const;
    virtual table* groupby(const char* str) const;
    using table::orderby;
    virtual void   orderby(const ibis::table::stringArray&);
    virtual void   orderby(const ibis::table::stringArray&,
			   const std::vector<bool>&);
    virtual void reverseRows();

    using ibis::part::buildIndexes;
    virtual int buildIndex(const char*, const char*) {return -1;}
    virtual int buildIndexes(const ibis::table::stringArray&) {return -1;}
    virtual int buildIndexes(const char*) {return -1;}
    virtual int getPartitions(constPartList&) const;
    virtual void indexSpec(const char*, const char*) {return;}
    virtual const char* indexSpec(const char*) const {return 0;}

    int restoreCategoriesAsStrings(const ibis::part&);
    ibis::bord* evaluateTerms(const ibis::selectClause&,
			      const char*) const;

    int merge(const ibis::bord&, const ibis::selectClause&);
    ibis::table* xgroupby(const ibis::selectClause&) const;
    ibis::table* groupby(const ibis::selectClause&) const;
    static ibis::bord*
	groupbya(const ibis::bord&, const ibis::selectClause&);
    static ibis::bord*
	groupbyc(const ibis::bord&, const ibis::selectClause&);

    virtual long reorder();
    virtual long reorder(const ibis::table::stringArray&);
    virtual long reorder(const ibis::table::stringArray&,
			 const std::vector<bool>&);

    int append(const ibis::selectClause&, const ibis::part&,
	       const ibis::bitvector&);
    int append(const ibis::selectClause&, const ibis::part&,
	       const ibis::qContinuousRange&);
    int renameColumns(const ibis::selectClause&);
    int limit(uint32_t);

    template <typename T>
	long sortValues(array_t<T> &vals,
			array_t<uint32_t> &starts,
			array_t<uint32_t> &indout,
			const array_t<uint32_t> &indin,
			bool ascending) const;
    template <typename T>
	long reorderValues(array_t<T> &vals,
			   const array_t<uint32_t> &ind) const;
    long sortStrings(std::vector<std::string> &vals,
		     array_t<uint32_t> &starts,
		     array_t<uint32_t> &idxout,
		     const array_t<uint32_t> &idxin,
		     bool ascending) const;
    long reorderStrings(std::vector<std::string> &vals,
			const array_t<uint32_t> &ind) const;


    void copyColumn(const char*, ibis::TYPE_T&, void*&,
		    const ibis::dictionary*&) const;
    static void copyValue(ibis::TYPE_T type,
			  void* outbuf, size_t outpos,
			  const void* inbuf, size_t inpos);

    // Cursor class for row-wise data accesses.
    class cursor;
    /// Create a @c cursor object to perform row-wise data access.
    virtual ibis::table::cursor* createCursor() const;

    // forward declarations
    class column;
    bord(const std::vector<ibis::bord::column*> &cols, uint32_t nr=0);

protected:
    /// Default constructor.  Creates a empty unnamed data partition.
    bord() : ibis::part("in-core") {}
    void clear();
    int64_t computeHits(const char* cond) const;

    static int merger(std::vector<ibis::bord::column*>&,
		      std::vector<ibis::bord::column*>&,
		      const std::vector<ibis::bord::column*>&,
		      const std::vector<ibis::bord::column*>&,
		      const std::vector<ibis::selectClause::AGREGADO>&);

    static int merge0(std::vector<ibis::bord::column*>&,
		      const std::vector<ibis::bord::column*>&,
		      const std::vector<ibis::selectClause::AGREGADO>&);
    template <typename T> static int
	merge0T(ibis::array_t<T>&, const ibis::array_t<T>&,
		ibis::selectClause::AGREGADO);

    static int merge10(ibis::bord::column&,
		       std::vector<ibis::bord::column*>&,
		       const ibis::bord::column&,
		       const std::vector<ibis::bord::column*>&,
		       const std::vector<ibis::selectClause::AGREGADO>&);
    template <typename Tk> static int
	merge10T(ibis::array_t<Tk> &kout,
		 std::vector<ibis::bord::column*> &vout,
		 const ibis::array_t<Tk> &kin1,
		 const std::vector<ibis::bord::column*> &vin1,
		 const ibis::array_t<Tk> &kin2,
		 const std::vector<ibis::bord::column*> &vin2,
		 const std::vector<ibis::selectClause::AGREGADO> &agg);
    static int
	merge10S(std::vector<std::string> &kout,
		 std::vector<ibis::bord::column*> &vout,
		 const std::vector<std::string> &kin1,
		 const std::vector<ibis::bord::column*> &vin1,
		 const std::vector<std::string> &kin2,
		 const std::vector<ibis::bord::column*> &vin2,
		 const std::vector<ibis::selectClause::AGREGADO> &agg);

    static int merge11(ibis::bord::column&,
		       ibis::bord::column&,
		       const ibis::bord::column&,
		       const ibis::bord::column&,
		       ibis::selectClause::AGREGADO);
    template <typename Tk, typename Tv> static int
	merge11T(ibis::array_t<Tk> &kout,
		 ibis::array_t<Tv> &vout,
		 const ibis::array_t<Tk> &kin1,
		 const ibis::array_t<Tv> &vin1,
		 const ibis::array_t<Tk> &kin2,
		 const ibis::array_t<Tv> &vin2,
		 ibis::selectClause::AGREGADO agg);
    template <typename Tv> static int
	merge11S(std::vector<std::string> &kout,
		 ibis::array_t<Tv> &vout,
		 const std::vector<std::string> &kin1,
		 const ibis::array_t<Tv> &vin1,
		 const std::vector<std::string> &kin2,
		 const ibis::array_t<Tv> &vin2,
		 ibis::selectClause::AGREGADO agg);

    static int merge12(ibis::bord::column&,
		       ibis::bord::column&,
		       ibis::bord::column&,
		       const ibis::bord::column&,
		       const ibis::bord::column&,
		       const ibis::bord::column&,
		       ibis::selectClause::AGREGADO,
		       ibis::selectClause::AGREGADO);
    template <typename Tk> static int
	merge12T1(ibis::array_t<Tk> &kout,
		  const ibis::array_t<Tk> &kin1,
		  const ibis::array_t<Tk> &kin2,
		  ibis::bord::column&,
		  ibis::bord::column&,
		  const ibis::bord::column&,
		  const ibis::bord::column&,
		  ibis::selectClause::AGREGADO,
		  ibis::selectClause::AGREGADO);
    template <typename Tk, typename Tu, typename Tv> static int
	merge12T(ibis::array_t<Tk> &kout,
		 ibis::array_t<Tu> &uout,
		 ibis::array_t<Tv> &vout,
		 const ibis::array_t<Tk> &kin1,
		 const ibis::array_t<Tu> &uin1,
		 const ibis::array_t<Tv> &vin1,
		 const ibis::array_t<Tk> &kin2,
		 const ibis::array_t<Tu> &uin2,
		 const ibis::array_t<Tv> &vin2,
		 ibis::selectClause::AGREGADO au,
		 ibis::selectClause::AGREGADO av);
    static int
	merge12S1(std::vector<std::string> &kout,
		  const std::vector<std::string> &kin1,
		  const std::vector<std::string> &kin2,
		  ibis::bord::column&,
		  ibis::bord::column&,
		  const ibis::bord::column&,
		  const ibis::bord::column&,
		  ibis::selectClause::AGREGADO,
		  ibis::selectClause::AGREGADO);
    template <typename Tu, typename Tv> static int
	merge12S(std::vector<std::string> &kout,
		 ibis::array_t<Tu> &uout,
		 ibis::array_t<Tv> &vout,
		 const std::vector<std::string> &kin1,
		 const ibis::array_t<Tu> &uin1,
		 const ibis::array_t<Tv> &vin1,
		 const std::vector<std::string> &kin2,
		 const ibis::array_t<Tu> &uin2,
		 const ibis::array_t<Tv> &vin2,
		 ibis::selectClause::AGREGADO au,
		 ibis::selectClause::AGREGADO av);

    static int merge20(ibis::bord::column &k11,
		       ibis::bord::column &k21,
		       std::vector<ibis::bord::column*> &v1,
		       const ibis::bord::column &k12,
		       const ibis::bord::column &k22,
		       const std::vector<ibis::bord::column*> &v2,
		       const std::vector<ibis::selectClause::AGREGADO> &agg);
    template <typename Tk1> static int
	merge20T1(ibis::array_t<Tk1> &k1out,
		  const ibis::array_t<Tk1> &k1in1,
		  const ibis::array_t<Tk1> &k1in2,
		  ibis::bord::column &k21,
		  std::vector<ibis::bord::column*> &vin1,
		  const ibis::bord::column &k22,
		  const std::vector<ibis::bord::column*> &vin2,
		  const std::vector<ibis::selectClause::AGREGADO> &agg);
    template <typename Tk1, typename Tk2> static int
	merge20T2(ibis::array_t<Tk1> &k1out,
		  ibis::array_t<Tk2> &k2out,
		  std::vector<ibis::bord::column*> &vout,
		  const ibis::array_t<Tk1> &k1in1,
		  const ibis::array_t<Tk2> &k2in1,
		  const std::vector<ibis::bord::column*> &vin1,
		  const ibis::array_t<Tk1> &k1in2,
		  const ibis::array_t<Tk2> &k2in2,
		  const std::vector<ibis::bord::column*> &vin2,
		  const std::vector<ibis::selectClause::AGREGADO> &agg);
    static int
	merge20S0(std::vector<std::string> &k1out,
		  std::vector<std::string> &k2out,
		  std::vector<ibis::bord::column*> &vout,
		  const std::vector<std::string> &k1in1,
		  const std::vector<std::string> &k2in1,
		  const std::vector<ibis::bord::column*> &vin1,
		  const std::vector<std::string> &k1in2,
		  const std::vector<std::string> &k2in2,
		  const std::vector<ibis::bord::column*> &vin2,
		  const std::vector<ibis::selectClause::AGREGADO> &agg);
    static int
	merge20S1(std::vector<std::string> &k1out,
		  const std::vector<std::string> &k1in1,
		  const std::vector<std::string> &k1in2,
		  ibis::bord::column &k21,
		  std::vector<ibis::bord::column*> &vin1,
		  const ibis::bord::column &k22,
		  const std::vector<ibis::bord::column*> &vin2,
		  const std::vector<ibis::selectClause::AGREGADO> &agg);
    template <typename Tk2> static int
	merge20S2(std::vector<std::string> &k1out,
		  ibis::array_t<Tk2> &k2out,
		  std::vector<ibis::bord::column*> &vout,
		  const std::vector<std::string> &k1in1,
		  const ibis::array_t<Tk2> &k2in1,
		  const std::vector<ibis::bord::column*> &vin1,
		  const std::vector<std::string> &k1in2,
		  const ibis::array_t<Tk2> &k2in2,
		  const std::vector<ibis::bord::column*> &vin2,
		  const std::vector<ibis::selectClause::AGREGADO> &agg);
    template <typename Tk1> static int
	merge20S3(ibis::array_t<Tk1> &k1out,
		  std::vector<std::string> &k2out,
		  std::vector<ibis::bord::column*> &vout,
		  const ibis::array_t<Tk1> &k1in1,
		  const std::vector<std::string> &k2in1,
		  const std::vector<ibis::bord::column*> &vin1,
		  const ibis::array_t<Tk1> &k1in2,
		  const std::vector<std::string> &k2in2,
		  const std::vector<ibis::bord::column*> &vin2,
		  const std::vector<ibis::selectClause::AGREGADO> &agg);

    static int merge21(ibis::bord::column &k11,
		       ibis::bord::column &k21,
		       ibis::bord::column &v1,
		       const ibis::bord::column &k12,
		       const ibis::bord::column &k22,
		       const ibis::bord::column &v2,
		       ibis::selectClause::AGREGADO ag);
    template <typename Tk1> static int
	merge21T1(ibis::array_t<Tk1> &k1out,
		  const ibis::array_t<Tk1> &k1in1,
		  const ibis::array_t<Tk1> &k1in2,
		  ibis::bord::column &k21,
		  ibis::bord::column &v1,
		  const ibis::bord::column &k22,
		  const ibis::bord::column &v2,
		  ibis::selectClause::AGREGADO ag);
    template <typename Tk1, typename Tk2> static int
	merge21T2(ibis::array_t<Tk1> &k1out,
		  ibis::array_t<Tk2> &k2out,
		  const ibis::array_t<Tk1> &k1in1,
		  const ibis::array_t<Tk2> &k2in1,
		  const ibis::array_t<Tk1> &k1in2,
		  const ibis::array_t<Tk2> &k2in2,
		  ibis::bord::column &v1,
		  const ibis::bord::column &v2,
		  ibis::selectClause::AGREGADO ag);
    template <typename Tk1, typename Tk2, typename Tv> static int
	merge21T3(ibis::array_t<Tk1> &k1out,
		  ibis::array_t<Tk2> &k2out,
		  ibis::array_t<Tv>  &vout,
		  const ibis::array_t<Tk1> &k1in1,
		  const ibis::array_t<Tk2> &k2in1,
		  const ibis::array_t<Tv>  &vin1,
		  const ibis::array_t<Tk1> &k1in2,
		  const ibis::array_t<Tk2> &k2in2,
		  const ibis::array_t<Tv>  &vin2,
		  ibis::selectClause::AGREGADO av);
    static int
	merge21S1(std::vector<std::string> &k1out,
		  const std::vector<std::string> &k1in1,
		  const std::vector<std::string> &k1in2,
		  ibis::bord::column &k21,
		  ibis::bord::column &v1,
		  const ibis::bord::column &k22,
		  const ibis::bord::column &v2,
		  ibis::selectClause::AGREGADO ag);
    template <typename Tk2> static int
	merge21S2(std::vector<std::string> &k1out,
		  ibis::array_t<Tk2> &k2out,
		  const std::vector<std::string> &k1in1,
		  const ibis::array_t<Tk2> &k2in1,
		  const std::vector<std::string> &k1in2,
		  const ibis::array_t<Tk2> &k2in2,
		  ibis::bord::column &v1,
		  const ibis::bord::column &v2,
		  ibis::selectClause::AGREGADO ag);
    template <typename Tk2, typename Tv> static int
	merge21S3(std::vector<std::string> &k1out,
		  ibis::array_t<Tk2> &k2out,
		  ibis::array_t<Tv>  &vout,
		  const std::vector<std::string> &k1in1,
		  const ibis::array_t<Tk2> &k2in1,
		  const ibis::array_t<Tv>  &vin1,
		  const std::vector<std::string> &k1in2,
		  const ibis::array_t<Tk2> &k2in2,
		  const ibis::array_t<Tv>  &vin2,
		  ibis::selectClause::AGREGADO av);
    static int
	merge21S4(std::vector<std::string> &k1out,
		  std::vector<std::string> &k2out,
		  const std::vector<std::string> &k1in1,
		  const std::vector<std::string> &k2in1,
		  const std::vector<std::string> &k1in2,
		  const std::vector<std::string> &k2in2,
		  ibis::bord::column &v1,
		  const ibis::bord::column &v2,
		  ibis::selectClause::AGREGADO ag);
    template <typename Tv> static int
	merge21S5(std::vector<std::string> &k1out,
		  std::vector<std::string> &k2out,
		  ibis::array_t<Tv>  &vout,
		  const std::vector<std::string> &k1in1,
		  const std::vector<std::string> &k2in1,
		  const ibis::array_t<Tv>  &vin1,
		  const std::vector<std::string> &k1in2,
		  const std::vector<std::string> &k2in2,
		  const ibis::array_t<Tv>  &vin2,
		  ibis::selectClause::AGREGADO av);
    template <typename Tk1> static int
	merge21S6(ibis::array_t<Tk1> &k1out,
		  std::vector<std::string> &k2out,
		  const ibis::array_t<Tk1> &k1in1,
		  const std::vector<std::string> &k2in1,
		  const ibis::array_t<Tk1> &k1in2,
		  const std::vector<std::string> &k2in2,
		  ibis::bord::column &v1,
		  const ibis::bord::column &v2,
		  ibis::selectClause::AGREGADO ag);
    template <typename Tk1, typename Tv> static int
	merge21S7(ibis::array_t<Tk1> &k1out,
		  std::vector<std::string> &k2out,
		  ibis::array_t<Tv>  &vout,
		  const ibis::array_t<Tk1> &k1in1,
		  const std::vector<std::string> &k2in1,
		  const ibis::array_t<Tv>  &vin1,
		  const ibis::array_t<Tk1> &k1in2,
		  const std::vector<std::string> &k2in2,
		  const ibis::array_t<Tv>  &vin2,
		  ibis::selectClause::AGREGADO av);

private:
    // disallow copying.
    bord(const bord&);
    bord &operator=(const bord&);

    friend class cursor;
}; // ibis::bord

/// An in-memory version of ibis::column.  For integers and floating-point
/// values, the buffer (with type void*) points to an ibis::array_t<T>
/// where the type T is designated by the column type.  For a string-valued
/// column, the buffer (with type void*) is std::vector<std::string>*.
///
/// @note Since the in-memory data tables are typically created at run-time
/// through select operations, the data types associated with a column is
/// only known at run-time.  Casting to void* is a ugly option; the
/// developers welcome suggestions for a replacement.
class ibis::bord::column : public ibis::column {
public:
    column(const ibis::bord* tbl, ibis::TYPE_T t, const char* name,
	   void *buf=0, const char* desc="", double low=DBL_MAX,
	   double high=-DBL_MAX);
    column(ibis::TYPE_T t, const char *nm, void *st,
           uint64_t *dim, uint64_t nd);
    column(FastBitReadExtArray rd, void *ctx, uint64_t *dims, uint64_t nd,
           ibis::TYPE_T t, const char *name, const char *desc="",
           double lo=DBL_MAX, double hi=-DBL_MAX);
    column(const ibis::bord*, const ibis::column&, void *buf);
    column(const column &rhs);
    virtual ~column();

    virtual ibis::fileManager::storage* getRawData() const;
    virtual bool hasRawData() const;

    using ibis::column::evaluateRange;
    virtual long evaluateRange(const ibis::qContinuousRange &cmp,
			       const ibis::bitvector &mask,
			       ibis::bitvector &res) const;
    virtual long evaluateRange(const ibis::qDiscreteRange &cmp,
			       const ibis::bitvector &mask,
			       ibis::bitvector &res) const;
    virtual long stringSearch(const char*, ibis::bitvector&) const;
    virtual long stringSearch(const std::vector<std::string>&,
			      ibis::bitvector&) const;
    virtual long stringSearch(const char*) const;
    virtual long stringSearch(const std::vector<std::string>&) const;
    virtual long keywordSearch(const char*, ibis::bitvector&) const;
    virtual long keywordSearch(const char*) const;
    virtual long keywordSearch(const std::vector<std::string>&,
			       ibis::bitvector&) const;
    virtual long keywordSearch(const std::vector<std::string>&) const;
    virtual long patternSearch(const char*) const;
    virtual long patternSearch(const char*, ibis::bitvector &) const;

    virtual array_t<signed char>*   selectBytes(const ibis::bitvector&) const;
    virtual array_t<unsigned char>* selectUBytes(const ibis::bitvector&) const;
    virtual array_t<int16_t>*       selectShorts(const ibis::bitvector&) const;
    virtual array_t<uint16_t>*      selectUShorts(const ibis::bitvector&) const;
    virtual array_t<int32_t>*       selectInts(const ibis::bitvector&) const;
    virtual array_t<uint32_t>*      selectUInts(const ibis::bitvector&) const;
    virtual array_t<int64_t>*       selectLongs(const ibis::bitvector&) const;
    virtual array_t<uint64_t>*      selectULongs(const ibis::bitvector&) const;
    virtual array_t<float>*         selectFloats(const ibis::bitvector&) const;
    virtual array_t<double>*        selectDoubles(const ibis::bitvector&) const;
    virtual std::vector<std::string>*
	selectStrings(const bitvector &mask) const;
    virtual std::vector<ibis::opaque>*
	selectOpaques(const bitvector &mask) const;

    virtual long append(const char* dt, const char* df, const uint32_t nold,
			const uint32_t nnew, uint32_t nbuf, char* buf);
    virtual long append(const void* vals, const ibis::bitvector &msk);
    virtual long append(const ibis::column &scol, const ibis::bitvector &msk);
    virtual long append(const ibis::column &scol,
			const ibis::qContinuousRange &cnd);

    virtual void computeMinMax();
    virtual void computeMinMax(const char *);
    virtual void computeMinMax(const char *, double &, double &, bool &) const;

    void reverseRows();
    int  limit(uint32_t nr);

    virtual int  getString(uint32_t, std::string &) const;
    virtual int  getValuesArray(void*) const;

    void*& getArray()       {return buffer;}
    void*  getArray() const {return buffer;}
    int dump(std::ostream &out, uint32_t i) const;

    bool equal_to(const column&) const;
    inline bool equal_to(const column&, uint32_t, uint32_t) const;
    inline bool less_than(const column&, uint32_t, uint32_t) const;
    inline void append(const void*, uint32_t);
    inline void append(const void*, uint32_t, const void*, uint32_t,
		       ibis::selectClause::AGREGADO);
    void addCounts(uint32_t);

    int restoreCategoriesAsStrings(const ibis::category&);
    /// Append new data (in @c from) to a larger array (pointed to by
    /// @c to).
    template <typename T> static int 
	addIncoreData(array_t<T>* &to, uint32_t nold, const array_t<T> &from,
		      const T special);
    static int addStrings(std::vector<std::string>*&, uint32_t,
			  const std::vector<std::string>&);
    static int addBlobs(std::vector<ibis::opaque>*&, uint32_t,
			const std::vector<ibis::opaque>&);

    /// Return the dictionary associated with the column.  A dictionary is
    /// associated with the column originally stored as ibis::category, but
    /// has been converted to be an integer column of type ibis::UINT.
    virtual const ibis::dictionary* getDictionary() const {return dic;}
    /// Assign the dictionary to use.
    void setDictionary(const ibis::dictionary* d) {dic = d;}

    const ibis::array_t<uint64_t>& getMeshShape() const {return shape;}
    int setMeshShape(uint64_t*, uint64_t);

protected:
    /// The in-memory storage.  A pointer to an array<T> or
    /// std::vector<std::string> depending on the data type.
    /// @sa ibis::table::freeBuffer
    void *buffer;
    /// Reader for externally managed data.
    FastBitReadExtArray xreader;
    /// Context to be passed back to reader.
    void *xmeta;
    /// A dictionary.  It may be used with a column of type ibis::UINT or
    /// ibis::CATEGORY.  Normally, it is a nil pointer.
    const ibis::dictionary *dic;
    /// Shape of the mesh for the data.  If it is empty, the data is
    /// assumed to be 1-Dimensional.  If the shape array is provided, it is
    /// assumed that the data values are in the typical C order, where the
    /// 1st dimension is the slowest varying dimension and the last
    /// dimension is the fastest varying dimension.  It is equivalent to
    /// member variable shapeSize in ibis::part.
    ibis::array_t<uint64_t> shape;

    column &operator=(const column&); // no assignment
}; // ibis::bord::column

class FASTBIT_CXX_DLLSPEC ibis::bord::cursor : public ibis::table::cursor {
public:
    cursor(const ibis::bord &t);
    virtual ~cursor() {};

    virtual uint64_t nRows() const {return tab.nRows();}
    virtual uint32_t nColumns() const {return tab.nColumns();}
    virtual ibis::table::stringArray columnNames() const {
	return tab.columnNames();}
    virtual ibis::table::typeArray columnTypes() const {
	return tab.columnTypes();}
    virtual int fetch();
    virtual int fetch(uint64_t);
    virtual int fetch(ibis::table::row&);
    virtual int fetch(uint64_t, ibis::table::row&);
    virtual uint64_t getCurrentRowNumber() const {return curRow;}
    virtual int dump(std::ostream &out, const char* del) const;

    virtual int getColumnAsByte(const char*, char&) const;
    virtual int getColumnAsUByte(const char*, unsigned char&) const;
    virtual int getColumnAsShort(const char*, int16_t&) const;
    virtual int getColumnAsUShort(const char*, uint16_t&) const;
    virtual int getColumnAsInt(const char*, int32_t&) const;
    virtual int getColumnAsUInt(const char*, uint32_t&) const;
    virtual int getColumnAsLong(const char*, int64_t&) const;
    virtual int getColumnAsULong(const char*, uint64_t&) const;
    virtual int getColumnAsFloat(const char*, float&) const;
    virtual int getColumnAsDouble(const char*, double&) const;
    virtual int getColumnAsString(const char*, std::string&) const;
    virtual int getColumnAsOpaque(const char*, ibis::opaque&) const;

    virtual int getColumnAsByte(uint32_t, char&) const;
    virtual int getColumnAsUByte(uint32_t, unsigned char&) const;
    virtual int getColumnAsShort(uint32_t, int16_t&) const;
    virtual int getColumnAsUShort(uint32_t, uint16_t&) const;
    virtual int getColumnAsInt(uint32_t, int32_t&) const;
    virtual int getColumnAsUInt(uint32_t, uint32_t&) const;
    virtual int getColumnAsLong(uint32_t, int64_t&) const;
    virtual int getColumnAsULong(uint32_t, uint64_t&) const;
    virtual int getColumnAsFloat(uint32_t, float&) const;
    virtual int getColumnAsDouble(uint32_t, double&) const;
    virtual int getColumnAsString(uint32_t, std::string&) const;
    virtual int getColumnAsOpaque(uint32_t, ibis::opaque&) const;

protected:
    struct bufferElement {
	const char* cname;
	ibis::TYPE_T ctype;
	void* cval;
	const ibis::dictionary* dic;

	bufferElement()
	    : cname(0), ctype(ibis::UNKNOWN_TYPE), cval(0), dic(0) {}
    }; // bufferElement
    typedef std::map<const char*, uint32_t, ibis::lessi> bufferMap;
    std::vector<bufferElement> buffer;
    bufferMap bufmap;
    const ibis::bord &tab;
    int64_t curRow; // the current row number

    void fillRow(ibis::table::row &res) const;
    int dumpIJ(std::ostream&, uint32_t, uint32_t) const;

private:
    cursor();
    cursor(const cursor&);
    cursor &operator=(const cursor&);
}; // ibis::bord::cursor

/// Copy a single value from inbuf to outbuf.  The output buffer must have
/// the correct size on entry.  This function does *NOT* attempt to resize
/// the output buffer.
inline void
ibis::bord::copyValue(ibis::TYPE_T type, void* outbuf, size_t outpos,
		      const void* inbuf, size_t inpos) {
    switch (type) {
    default:
	LOGGER(ibis::gVerbose > 0)
	    << "Warning -- bord::copyValue can not copy a value of type "
	    << ibis::TYPESTRING[(int)type];
	break;
    case ibis::BYTE: {
	(*static_cast<array_t<signed char>*>(outbuf))[outpos]
	    = (*static_cast<const array_t<signed char>*>(inbuf))[inpos];
	break;}
    case ibis::UBYTE: {
	(*static_cast<array_t<unsigned char>*>(outbuf))[outpos]
	    = (*static_cast<const array_t<unsigned char>*>(inbuf))[inpos];
	break;}
    case ibis::SHORT: {
	(*static_cast<array_t<int16_t>*>(outbuf))[outpos]
	    = (*static_cast<const array_t<int16_t>*>(inbuf))[inpos];
	break;}
    case ibis::USHORT: {
	(*static_cast<array_t<uint16_t>*>(outbuf))[outpos]
	    = (*static_cast<const array_t<uint16_t>*>(inbuf))[inpos];
	break;}
    case ibis::INT: {
	(*static_cast<array_t<int32_t>*>(outbuf))[outpos]
	    = (*static_cast<const array_t<int32_t>*>(inbuf))[inpos];
	break;}
    case ibis::UINT: {
	(*static_cast<array_t<uint32_t>*>(outbuf))[outpos]
	    = (*static_cast<const array_t<uint32_t>*>(inbuf))[inpos];
	break;}
    case ibis::LONG: {
	(*static_cast<array_t<int64_t>*>(outbuf))[outpos]
	    = (*static_cast<const array_t<int64_t>*>(inbuf))[inpos];
	break;}
    case ibis::ULONG: {
	(*static_cast<array_t<uint64_t>*>(outbuf))[outpos]
	    = (*static_cast<const array_t<uint64_t>*>(inbuf))[inpos];
	break;}
    case ibis::FLOAT: {
	(*static_cast<array_t<float>*>(outbuf))[outpos]
	    = (*static_cast<const array_t<float>*>(inbuf))[inpos];
	break;}
    case ibis::DOUBLE: {
	(*static_cast<array_t<double>*>(outbuf))[outpos]
	    = (*static_cast<const array_t<double>*>(inbuf))[inpos];
	break;}
    case ibis::BLOB: {
	(*static_cast<std::vector<ibis::opaque>*>(outbuf))[outpos]
	    = (*static_cast<const std::vector<ibis::opaque>*>(inbuf))[inpos];
	break;}
    case ibis::TEXT:
    case ibis::CATEGORY: {
	(*static_cast<std::vector<std::string>*>(outbuf))[outpos]
	    = (*static_cast<const std::vector<std::string>*>(inbuf))[inpos];
	break;}
    }
} //ibis::bord::copyValue

/// Does the ith value of this column equal to the jth value of other?
inline bool
ibis::bord::column::equal_to(const ibis::bord::column &other,
			     uint32_t i, uint32_t j) const {
    if (m_type != other.m_type) return false;
    if (buffer == 0 || other.buffer == 0) return false;
    if (buffer == other.buffer  && i == j) return true;

    switch (m_type) {
    default:
	LOGGER(ibis::gVerbose > 0)
	    << "Warning -- bord::column[" << (thePart ? thePart->name() : "")
	    << '.' << m_name << "]::equal_to can not compare values of type "
	    << ibis::TYPESTRING[(int)m_type];
	return false;
    case ibis::BYTE: {
	const ibis::array_t<signed char> &v0 =
	    *static_cast<const ibis::array_t<signed char>*>(buffer);
	const ibis::array_t<signed char> &v1 =
	    *static_cast<const ibis::array_t<signed char>*>(other.buffer);
	return (v0[i] == v1[j]);}
    case ibis::UBYTE: {
	const ibis::array_t<unsigned char> &v0 =
	    *static_cast<const ibis::array_t<unsigned char>*>(buffer);
	const ibis::array_t<unsigned char> &v1 =
	    *static_cast<const ibis::array_t<unsigned char>*>(other.buffer);
	return (v0[i] == v1[j]);}
    case ibis::SHORT: {
	const ibis::array_t<int16_t> &v0 =
	    *static_cast<const ibis::array_t<int16_t>*>(buffer);
	const ibis::array_t<int16_t> &v1 =
	    *static_cast<const ibis::array_t<int16_t>*>(other.buffer);
	return (v0[i] == v1[j]);}
    case ibis::USHORT: {
	const ibis::array_t<uint16_t> &v0 =
	    *static_cast<const ibis::array_t<uint16_t>*>(buffer);
	const ibis::array_t<uint16_t> &v1 =
	    *static_cast<const ibis::array_t<uint16_t>*>(other.buffer);
	return (v0[i] == v1[j]);}
    case ibis::INT: {
	const ibis::array_t<int32_t> &v0 =
	    *static_cast<const ibis::array_t<int32_t>*>(buffer);
	const ibis::array_t<int32_t> &v1 =
	    *static_cast<const ibis::array_t<int32_t>*>(other.buffer);
	return (v0[i] == v1[j]);}
    case ibis::UINT: {
	const ibis::array_t<uint32_t> &v0 =
	    *static_cast<const ibis::array_t<uint32_t>*>(buffer);
	const ibis::array_t<uint32_t> &v1 =
	    *static_cast<const ibis::array_t<uint32_t>*>(other.buffer);
	return (v0[i] == v1[j]);}
    case ibis::LONG: {
	const ibis::array_t<int64_t> &v0 =
	    *static_cast<const ibis::array_t<int64_t>*>(buffer);
	const ibis::array_t<int64_t> &v1 =
	    *static_cast<const ibis::array_t<int64_t>*>(other.buffer);
	return (v0[i] == v1[j]);}
    case ibis::OID:
    case ibis::ULONG: {
	const ibis::array_t<uint64_t> &v0 =
	    *static_cast<const ibis::array_t<uint64_t>*>(buffer);
	const ibis::array_t<uint64_t> &v1 =
	    *static_cast<const ibis::array_t<uint64_t>*>(other.buffer);
	return (v0[i] == v1[j]);}
    case ibis::FLOAT: {
	const ibis::array_t<float> &v0 =
	    *static_cast<const ibis::array_t<float>*>(buffer);
	const ibis::array_t<float> &v1 =
	    *static_cast<const ibis::array_t<float>*>(other.buffer);
	return (v0[i] == v1[j]);}
    case ibis::DOUBLE: {
	const ibis::array_t<double> &v0 =
	    *static_cast<const ibis::array_t<double>*>(buffer);
	const ibis::array_t<double> &v1 =
	    *static_cast<const ibis::array_t<double>*>(other.buffer);
	return (v0[i] == v1[j]);}
    case ibis::BLOB: {
	const ibis::opaque &v0 =
	    (*static_cast<const std::vector<ibis::opaque>*>(buffer))[i];
	const ibis::opaque &v1 =
	    (*static_cast<const std::vector<ibis::opaque>*>(other.buffer))[j];
	bool match = (v0.size() == v1.size());
	for (size_t j = 0; match  && j < v0.size(); ++ j)
	    match = (v0.address()[j] == v1.address()[j]);
	return match;}
    case ibis::TEXT:
    case ibis::CATEGORY: {
	const std::string &v0 =
	    (*static_cast<const std::vector<std::string>*>(buffer))[i];
	const std::string &v1 =
	    (*static_cast<const std::vector<std::string>*>(other.buffer))[j];
	return (v0 == v1);}
    }
} // ibis::bord::column::equal_to

/// Is the ith value of this column less than the jth value of other?
inline bool
ibis::bord::column::less_than(const ibis::bord::column &other,
			      uint32_t i, uint32_t j) const {
    if (m_type != other.m_type) return false;
    if (buffer == 0 || other.buffer == 0) return false;
    if (buffer == other.buffer  && i == j) return true;

    switch (m_type) {
    default:
	LOGGER(ibis::gVerbose > 0)
	    << "Warning -- bord::column[" << (thePart ? thePart->name() : "")
	    << '.' << m_name << "]::less_than can not compare values of type "
	    << ibis::TYPESTRING[(int)m_type];
	return false;
    case ibis::BYTE: {
	const ibis::array_t<signed char> &v0 =
	    *static_cast<const ibis::array_t<signed char>*>(buffer);
	const ibis::array_t<signed char> &v1 =
	    *static_cast<const ibis::array_t<signed char>*>(other.buffer);
	return (v0[i] < v1[j]);}
    case ibis::UBYTE: {
	const ibis::array_t<unsigned char> &v0 =
	    *static_cast<const ibis::array_t<unsigned char>*>(buffer);
	const ibis::array_t<unsigned char> &v1 =
	    *static_cast<const ibis::array_t<unsigned char>*>(other.buffer);
	return (v0[i] < v1[j]);}
    case ibis::SHORT: {
	const ibis::array_t<int16_t> &v0 =
	    *static_cast<const ibis::array_t<int16_t>*>(buffer);
	const ibis::array_t<int16_t> &v1 =
	    *static_cast<const ibis::array_t<int16_t>*>(other.buffer);
	return (v0[i] < v1[j]);}
    case ibis::USHORT: {
	const ibis::array_t<uint16_t> &v0 =
	    *static_cast<const ibis::array_t<uint16_t>*>(buffer);
	const ibis::array_t<uint16_t> &v1 =
	    *static_cast<const ibis::array_t<uint16_t>*>(other.buffer);
	return (v0[i] < v1[j]);}
    case ibis::INT: {
	const ibis::array_t<int32_t> &v0 =
	    *static_cast<const ibis::array_t<int32_t>*>(buffer);
	const ibis::array_t<int32_t> &v1 =
	    *static_cast<const ibis::array_t<int32_t>*>(other.buffer);
	return (v0[i] < v1[j]);}
    case ibis::UINT: {
	const ibis::array_t<uint32_t> &v0 =
	    *static_cast<const ibis::array_t<uint32_t>*>(buffer);
	const ibis::array_t<uint32_t> &v1 =
	    *static_cast<const ibis::array_t<uint32_t>*>(other.buffer);
	return (v0[i] < v1[j]);}
    case ibis::LONG: {
	const ibis::array_t<int64_t> &v0 =
	    *static_cast<const ibis::array_t<int64_t>*>(buffer);
	const ibis::array_t<int64_t> &v1 =
	    *static_cast<const ibis::array_t<int64_t>*>(other.buffer);
	return (v0[i] < v1[j]);}
    case ibis::OID:
    case ibis::ULONG: {
	const ibis::array_t<uint64_t> &v0 =
	    *static_cast<const ibis::array_t<uint64_t>*>(buffer);
	const ibis::array_t<uint64_t> &v1 =
	    *static_cast<const ibis::array_t<uint64_t>*>(other.buffer);
	return (v0[i] < v1[j]);}
    case ibis::FLOAT: {
	const ibis::array_t<float> &v0 =
	    *static_cast<const ibis::array_t<float>*>(buffer);
	const ibis::array_t<float> &v1 =
	    *static_cast<const ibis::array_t<float>*>(other.buffer);
	return (v0[i] < v1[j]);}
    case ibis::DOUBLE: {
	const ibis::array_t<double> &v0 =
	    *static_cast<const ibis::array_t<double>*>(buffer);
	const ibis::array_t<double> &v1 =
	    *static_cast<const ibis::array_t<double>*>(other.buffer);
	return (v0[i] < v1[j]);}
    case ibis::BLOB: {
	const ibis::opaque &v0 =
	    (*static_cast<const std::vector<ibis::opaque>*>(buffer))[i];
	const ibis::opaque &v1 =
	    (*static_cast<const std::vector<ibis::opaque>*>(other.buffer))[j];
	const size_t minlen = (v0.size() <= v1.size() ? v0.size() : v1.size());
	int cmp = 0;
	for (size_t j = 0; cmp == 0  && j < v0.size(); ++ j)
	    cmp = ((int)(v0.address()[j]) - (int)(v1.address()[j]));
	if (cmp == 0  && v1.size() > minlen)
	    cmp = -1;
	return (cmp < 0);}
    case ibis::TEXT:
    case ibis::CATEGORY: {
	const std::string &v0 =
	    (*static_cast<const std::vector<std::string>*>(buffer))[i];
	const std::string &v1 =
	    (*static_cast<const std::vector<std::string>*>(other.buffer))[j];
	return (v0 < v1);}
    }
} // ibis::bord::column::less_than

/// Append a value.
///
/// @note The first argument c1 is expected to be an array_t object with
/// data type same as this column.
inline void
ibis::bord::column::append(const void* c1, uint32_t i1) {
    switch (m_type) {
    default:
	LOGGER(ibis::gVerbose > 0)
	    << "Warning -- bord::column[" << (thePart ? thePart->name() : "")
	    << '.' << m_name << "]::append can not handle data type "
	    << ibis::TYPESTRING[(int)m_type];
	break;
    case ibis::BYTE: {
	ibis::array_t<signed char> &v0 =
	    *(static_cast<array_t<signed char>*>(buffer));
	const ibis::array_t<signed char> &v1 =
	    *(static_cast<const array_t<signed char>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    case ibis::UBYTE: {
	ibis::array_t<unsigned char> &v0 =
	    *(static_cast<array_t<unsigned char>*>(buffer));
	const ibis::array_t<unsigned char> &v1 =
	    *(static_cast<const array_t<unsigned char>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    case ibis::SHORT: {
	ibis::array_t<int16_t> &v0 =
	    *(static_cast<array_t<int16_t>*>(buffer));
	const ibis::array_t<int16_t> &v1 =
	    *(static_cast<const array_t<int16_t>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    case ibis::USHORT: {
	ibis::array_t<uint16_t> &v0 =
	    *(static_cast<array_t<uint16_t>*>(buffer));
	const ibis::array_t<uint16_t> &v1 =
	    *(static_cast<const array_t<uint16_t>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    case ibis::INT: {
	ibis::array_t<int32_t> &v0 =
	    *(static_cast<array_t<int32_t>*>(buffer));
	const ibis::array_t<int32_t> &v1 =
	    *(static_cast<const array_t<int32_t>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    case ibis::UINT: {
	ibis::array_t<uint32_t> &v0 =
	    *(static_cast<array_t<uint32_t>*>(buffer));
	const ibis::array_t<uint32_t> &v1 =
	    *(static_cast<const array_t<uint32_t>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    case ibis::LONG: {
	ibis::array_t<int64_t> &v0 =
	    *(static_cast<array_t<int64_t>*>(buffer));
	const ibis::array_t<int64_t> &v1 =
	    *(static_cast<const array_t<int64_t>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    case ibis::ULONG: {
	ibis::array_t<uint64_t> &v0 =
	    *(static_cast<array_t<uint64_t>*>(buffer));
	const ibis::array_t<uint64_t> &v1 =
	    *(static_cast<const array_t<uint64_t>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    case ibis::FLOAT: {
	ibis::array_t<float> &v0 =
	    *(static_cast<array_t<float>*>(buffer));
	const ibis::array_t<float> &v1 =
	    *(static_cast<const array_t<float>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    case ibis::DOUBLE: {
	ibis::array_t<double> &v0 =
	    *(static_cast<array_t<double>*>(buffer));
	const ibis::array_t<double> &v1 =
	    *(static_cast<const array_t<double>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    case ibis::TEXT:
    case ibis::CATEGORY: {
	std::vector<std::string> &v0 =
	    *(static_cast<std::vector<std::string>*>(buffer));
	const std::vector<std::string> &v1 =
	    *(static_cast<const std::vector<std::string>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    case ibis::BLOB: {
	std::vector<ibis::opaque> &v0 =
	    *(static_cast<std::vector<ibis::opaque>*>(buffer));
	const std::vector<ibis::opaque> &v1 =
	    *(static_cast<const std::vector<ibis::opaque>*>(c1));
	v0.push_back(v1[i1]);
	break;}
    }
} // ibis::bord::column::append

/// Append the value genenerated from the the operation on the incoming
/// columns.
///
/// @note Both arguemnt c1 and c2 are expected to array_t objects with the
/// same data type as this column.
inline void
ibis::bord::column::append(const void* c1, uint32_t i1,
			   const void* c2, uint32_t i2,
			   ibis::selectClause::AGREGADO agg) {
    switch (m_type) {
    default:
	LOGGER(ibis::gVerbose > 0)
	    << "Warning -- bord::column[" << (thePart ? thePart->name() : "")
	    << '.' << m_name << "]::append can not handle data type "
	    << ibis::TYPESTRING[(int)m_type] << " with aggregations";
	return;
    case ibis::BYTE: {
	ibis::array_t<signed char> &v0 =
	    *(static_cast<array_t<signed char>*>(buffer));
	const ibis::array_t<signed char> &v1 =
	    *(static_cast<const array_t<signed char>*>(c1));
	const ibis::array_t<signed char> &v2 =
	    *(static_cast<const array_t<signed char>*>(c2));
	signed char tmp = 0;
	switch (agg) {
	default:
	    break;
	case ibis::selectClause::CNT:
	case ibis::selectClause::SUM:
	    tmp = v1[i1] + v2[i2];
	    break;
	case ibis::selectClause::MIN:
	    tmp = (v1[i1] <= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	case ibis::selectClause::MAX:
	    tmp = (v1[i1] >= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	}
	v0.push_back(tmp);
	break;}
    case ibis::UBYTE: {
	ibis::array_t<unsigned char> &v0 =
	    *(static_cast<array_t<unsigned char>*>(buffer));
	const ibis::array_t<unsigned char> &v1 =
	    *(static_cast<const array_t<unsigned char>*>(c1));
	const ibis::array_t<unsigned char> &v2 =
	    *(static_cast<const array_t<unsigned char>*>(c2));
	unsigned char tmp = 0;
	switch (agg) {
	default:
	    break;
	case ibis::selectClause::CNT:
	case ibis::selectClause::SUM:
	    tmp = v1[i1] + v2[i2];
	    break;
	case ibis::selectClause::MIN:
	    tmp = (v1[i1] <= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	case ibis::selectClause::MAX:
	    tmp = (v1[i1] >= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	}
	v0.push_back(tmp);
	break;}
    case ibis::SHORT: {
	ibis::array_t<int16_t> &v0 =
	    *(static_cast<array_t<int16_t>*>(buffer));
	const ibis::array_t<int16_t> &v1 =
	    *(static_cast<const array_t<int16_t>*>(c1));
	const ibis::array_t<int16_t> &v2 =
	    *(static_cast<const array_t<int16_t>*>(c2));
	int16_t tmp = 0;
	switch (agg) {
	default:
	    break;
	case ibis::selectClause::CNT:
	case ibis::selectClause::SUM:
	    tmp = v1[i1] + v2[i2];
	    break;
	case ibis::selectClause::MIN:
	    tmp = (v1[i1] <= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	case ibis::selectClause::MAX:
	    tmp = (v1[i1] >= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	}
	v0.push_back(tmp);
	break;}
    case ibis::USHORT: {
	ibis::array_t<uint16_t> &v0 =
	    *(static_cast<array_t<uint16_t>*>(buffer));
	const ibis::array_t<uint16_t> &v1 =
	    *(static_cast<const array_t<uint16_t>*>(c1));
	const ibis::array_t<uint16_t> &v2 =
	    *(static_cast<const array_t<uint16_t>*>(c2));
	uint16_t tmp = 0;
	switch (agg) {
	default:
	    break;
	case ibis::selectClause::CNT:
	case ibis::selectClause::SUM:
	    tmp = v1[i1] + v2[i2];
	    break;
	case ibis::selectClause::MIN:
	    tmp = (v1[i1] <= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	case ibis::selectClause::MAX:
	    tmp = (v1[i1] >= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	}
	v0.push_back(tmp);
	break;}
    case ibis::INT: {
	ibis::array_t<int32_t> &v0 =
	    *(static_cast<array_t<int32_t>*>(buffer));
	const ibis::array_t<int32_t> &v1 =
	    *(static_cast<const array_t<int32_t>*>(c1));
	const ibis::array_t<int32_t> &v2 =
	    *(static_cast<const array_t<int32_t>*>(c2));
	int32_t tmp = 0;
	switch (agg) {
	default:
	    break;
	case ibis::selectClause::CNT:
	case ibis::selectClause::SUM:
	    tmp = v1[i1] + v2[i2];
	    break;
	case ibis::selectClause::MIN:
	    tmp = (v1[i1] <= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	case ibis::selectClause::MAX:
	    tmp = (v1[i1] >= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	}
	v0.push_back(tmp);
	break;}
    case ibis::UINT: {
	ibis::array_t<uint32_t> &v0 =
	    *(static_cast<array_t<uint32_t>*>(buffer));
	const ibis::array_t<uint32_t> &v1 =
	    *(static_cast<const array_t<uint32_t>*>(c1));
	const ibis::array_t<uint32_t> &v2 =
	    *(static_cast<const array_t<uint32_t>*>(c2));
	uint32_t tmp = 0;
	switch (agg) {
	default:
	    break;
	case ibis::selectClause::CNT:
	case ibis::selectClause::SUM:
	    tmp = v1[i1] + v2[i2];
	    break;
	case ibis::selectClause::MIN:
	    tmp = (v1[i1] <= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	case ibis::selectClause::MAX:
	    tmp = (v1[i1] >= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	}
	v0.push_back(tmp);
	break;}
    case ibis::LONG: {
	ibis::array_t<int64_t> &v0 =
	    *(static_cast<array_t<int64_t>*>(buffer));
	const ibis::array_t<int64_t> &v1 =
	    *(static_cast<const array_t<int64_t>*>(c1));
	const ibis::array_t<int64_t> &v2 =
	    *(static_cast<const array_t<int64_t>*>(c2));
	int64_t tmp = 0;
	switch (agg) {
	default:
	    break;
	case ibis::selectClause::CNT:
	case ibis::selectClause::SUM:
	    tmp = v1[i1] + v2[i2];
	    break;
	case ibis::selectClause::MIN:
	    tmp = (v1[i1] <= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	case ibis::selectClause::MAX:
	    tmp = (v1[i1] >= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	}
	v0.push_back(tmp);
	break;}
    case ibis::ULONG: {
	ibis::array_t<uint64_t> &v0 =
	    *(static_cast<array_t<uint64_t>*>(buffer));
	const ibis::array_t<uint64_t> &v1 =
	    *(static_cast<const array_t<uint64_t>*>(c1));
	const ibis::array_t<uint64_t> &v2 =
	    *(static_cast<const array_t<uint64_t>*>(c2));
	uint64_t tmp = 0;
	switch (agg) {
	default:
	    break;
	case ibis::selectClause::CNT:
	case ibis::selectClause::SUM:
	    tmp = v1[i1] + v2[i2];
	    break;
	case ibis::selectClause::MIN:
	    tmp = (v1[i1] <= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	case ibis::selectClause::MAX:
	    tmp = (v1[i1] >= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	}
	v0.push_back(tmp);
	break;}
    case ibis::FLOAT: {
	ibis::array_t<float> &v0 =
	    *(static_cast<array_t<float>*>(buffer));
	const ibis::array_t<float> &v1 =
	    *(static_cast<const array_t<float>*>(c1));
	const ibis::array_t<float> &v2 =
	    *(static_cast<const array_t<float>*>(c2));
	float tmp = 0;
	switch (agg) {
	default:
	    break;
	case ibis::selectClause::CNT:
	case ibis::selectClause::SUM:
	    tmp = v1[i1] + v2[i2];
	    break;
	case ibis::selectClause::MIN:
	    tmp = (v1[i1] <= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	case ibis::selectClause::MAX:
	    tmp = (v1[i1] >= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	}
	v0.push_back(tmp);
	break;}
    case ibis::DOUBLE: {
	ibis::array_t<double> &v0 =
	    *(static_cast<array_t<double>*>(buffer));
	const ibis::array_t<double> &v1 =
	    *(static_cast<const array_t<double>*>(c1));
	const ibis::array_t<double> &v2 =
	    *(static_cast<const array_t<double>*>(c2));
	double tmp = 0;
	switch (agg) {
	default:
	    break;
	case ibis::selectClause::CNT:
	case ibis::selectClause::SUM:
	    tmp = v1[i1] + v2[i2];
	    break;
	case ibis::selectClause::MIN:
	    tmp = (v1[i1] <= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	case ibis::selectClause::MAX:
	    tmp = (v1[i1] >= v2[i2] ? v1[i1] : v2[i2]);
	    break;
	}
	v0.push_back(tmp);
	break;}
    }
} // ibis::bord::column::append

inline int ibis::bord::cursor::fetch() {
    ++ curRow;
    return (0 - (curRow >= (int64_t) tab.nRows()));
} // ibis::bord::cursor::fetch

inline int ibis::bord::cursor::fetch(uint64_t irow) {
    if (irow < tab.nRows()) {
	curRow = static_cast<int64_t>(irow);
	return 0;
    }
    else {
	return -1;
    }
} // ibis::bord::cursor::fetch

inline int ibis::bord::cursor::fetch(ibis::table::row &res) {
    ++ curRow;
    if ((uint64_t) curRow < tab.nRows()) {
	fillRow(res);
	return 0;
    }
    else {
	return -1;
    }
} // ibis::bord::cursor::fetch

inline int ibis::bord::cursor::fetch(uint64_t irow, ibis::table::row &res) {
    if (irow < tab.nRows()) {
	curRow = static_cast<int64_t>(irow);
	fillRow(res);
	return 0;
    }
    else {
	return -1;
    }
} // ibis::bord::cursor::fetch

inline int
ibis::bord::cursor::dumpIJ(std::ostream &out, uint32_t i,
			   uint32_t j) const {
    if (buffer[j].cval == 0) return -1;

    switch (buffer[j].ctype) {
    case ibis::BYTE: {
	const array_t<const signed char>* vals =
	    static_cast<const array_t<const signed char>*>(buffer[j].cval);
	out << (int) ((*vals)[i]);
	break;}
    case ibis::UBYTE: {
	const array_t<const unsigned char>* vals =
	    static_cast<const array_t<const unsigned char>*>(buffer[j].cval);
	out << (unsigned int) ((*vals)[i]);
	break;}
    case ibis::SHORT: {
	const array_t<const int16_t>* vals =
	    static_cast<const array_t<const int16_t>*>(buffer[j].cval);
	out << (*vals)[i];
	break;}
    case ibis::USHORT: {
	const array_t<const uint16_t>* vals =
	    static_cast<const array_t<const uint16_t>*>(buffer[j].cval);
	out << (*vals)[i];
	break;}
    case ibis::INT: {
	const array_t<const int32_t>* vals =
	    static_cast<const array_t<const int32_t>*>(buffer[j].cval);
	out << (*vals)[i];
	break;}
    case ibis::UINT: {
	const array_t<const uint32_t>* vals =
	    static_cast<const array_t<const uint32_t>*>(buffer[j].cval);
	if (buffer[j].dic == 0) {
	    out << (*vals)[i];
	}
	else if (buffer[j].dic->size() >= (*vals)[i]) {
	    out << buffer[j].dic->operator[]((*vals)[i]);
	}
	else {
	    out << (*vals)[i];
	}
	break;}
    case ibis::LONG: {
	const array_t<const int64_t>* vals =
	    static_cast<const array_t<const int64_t>*>(buffer[j].cval);
	out << (*vals)[i];
	break;}
    case ibis::ULONG: {
	const array_t<const uint64_t>* vals =
	    static_cast<const array_t<const uint64_t>*>(buffer[j].cval);
	out << (*vals)[i];
	break;}
    case ibis::FLOAT: {
	const array_t<const float>* vals =
	    static_cast<const array_t<const float>*>(buffer[j].cval);
	out << (*vals)[i];
	break;}
    case ibis::DOUBLE: {
	const array_t<const double>* vals =
	    static_cast<const array_t<const double>*>(buffer[j].cval);
	out << (*vals)[i];
	break;}
    case ibis::TEXT:
    case ibis::CATEGORY: {
	const std::vector<std::string>* vals =
	    static_cast<const std::vector<std::string>*>(buffer[j].cval);
	out << '"' << (*vals)[i] << '"';
	break;}
    default: {
	return -2;}
    }
    return 0;
} // ibis::bord::cursor::dumpIJ

inline int
ibis::bord::cursor::getColumnAsByte(const char* cn, char &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsByte((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsByte

inline int
ibis::bord::cursor::getColumnAsUByte(const char* cn,
				     unsigned char &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsUByte((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsUByte

inline int
ibis::bord::cursor::getColumnAsShort(const char* cn, int16_t &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsShort((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsShort

inline int
ibis::bord::cursor::getColumnAsUShort(const char* cn, uint16_t &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsUShort((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsUShort

inline int
ibis::bord::cursor::getColumnAsInt(const char* cn, int32_t &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsInt((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsInt

inline int
ibis::bord::cursor::getColumnAsUInt(const char* cn, uint32_t &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsUInt((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsUInt

inline int
ibis::bord::cursor::getColumnAsLong(const char* cn, int64_t &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsLong((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsLong

inline int
ibis::bord::cursor::getColumnAsULong(const char* cn, uint64_t &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsULong((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsULong

inline int
ibis::bord::cursor::getColumnAsFloat(const char* cn, float &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsFloat((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsFloat

inline int
ibis::bord::cursor::getColumnAsDouble(const char* cn, double &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsDouble((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsDouble

inline int
ibis::bord::cursor::getColumnAsString(const char* cn,
				      std::string &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsString((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsString

inline int
ibis::bord::cursor::getColumnAsOpaque(const char* cn,
				      ibis::opaque &val) const {
    if (curRow < 0 || curRow >= (int64_t) tab.nRows() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsOpaque((*it).second, val);
    else
	return -2;
} // ibis::bord::cursor::getColumnAsOpaque
#endif // IBIS_BORD_H
