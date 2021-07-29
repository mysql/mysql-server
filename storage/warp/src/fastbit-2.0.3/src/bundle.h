//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
//         Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_BUNDLE_H
#define IBIS_BUNDLE_H
///@file
/// Classes to store selected values.
///
/// The class ibis::bundle is use to represent a sorted version of the
/// selected columns of a query.  The selected columns can be of any type.
/// The string values are internally recorded as integers.  The bundles are
/// written to a directory containing other type of information about the
/// query.
///
/// The class ibis::query::result is a thin wrapper on top of ibis::bundle
/// to provide row-wise data accesses.
#include "query.h"	// ibis::query, ibis::selectClause
#include "colValues.h"	// ibis::colValues

namespace ibis {
    // class bundle defined in const.h,
    // the following are concrete classes used in the implementation
    class bundle0;
    class bundle1;
    //class bundle2s;
    //class bundle2t;
    class bundles;
    //class bundlet;
}

/// @ingroup FastBitIBIS
/// The public interface of bundles.
///
/// This is an incore implementation, that is, it stores all relevant values
/// in memory.  It is intended to be used only to sort the selected values
/// and immediately write out the content to files.
///
/// Given a select clause, its terms are logically re-ordered so that the
/// plain column names are placed before all the functions, such as,
/// "SELECT variable1, variable2, ..., aggregation1, aggregation2, ..."
/// where the relative order of the plain column names are preserved as
/// they appeared on input.  The rows/records of the query results are
/// ordered according to the values of the plain columns.
///
/// @note This implementation only works with select clauses containing
/// plain column names and aggregations on plain columns, no arithmetic
/// expressions, no aliases.
///
/// @note The sorting on string-valued columns does NOT produce
/// alphabetical ordering.  For categorical values, the sorting is done on
/// the integer version of the data, not on the string values.  Therefore,
/// the string will not appear in alphabetically order!  However,
/// aggregation functions should still produce correct results.  The text
/// values will appear in the order they are in the data files, i.e., the
/// same order as they are inputed.  Furthermore, all text values are
/// treated as different because the actual string values are not examined
/// during sorting.  DO NOT use text columns with aggregation functions!
///
/// @note When multiple components are selected, a generic version of the
/// sorting algorithm is used.  It may be faster to handle special versions
/// separately.  For example, if all the selected components are of the
/// same type, it is possible to use a more compact array structure for
/// comparisons.  It might be also useful to separate out the cases where
/// there are only two components.
///
class FASTBIT_CXX_DLLSPEC ibis::bundle {
public:
    static bundle* create(const ibis::query&, int =0);
    static bundle* create(const ibis::query&, const ibis::bitvector&, int =0);
    static bundle* create(const ibis::part&, const ibis::selectClause&, int =0);

    /// Return the RIDs related to the ith bundle.
    static const ibis::RIDSet* readRIDs(const char* dir, const uint32_t i);

    /// Return the number of bundles.
    virtual uint32_t size() const {
	return (starts != 0 && starts->size()>0 ? starts->size()-1 : 0);}
    /// Return the width of the bundles.
    virtual uint32_t width() const {return 0;}
    /// Print the bundle values to the specified output stream.
    virtual void print(std::ostream& out) const = 0;
    /// Print the bundle values along with the RIDs.
    virtual void printAll(std::ostream& out) const = 0;
    /// Print column names.
    virtual void printColumnNames(std::ostream& out) const = 0;
    /// Retrieve a single value.  Numerical values will be casted into
    /// the return type.
    /// @note Most compilers will emit numerous complains about the
    /// potential data loss due to type conversions.  User should employ
    /// the correct types to avoid actual loss of precision.
    /// @{
    virtual int32_t  getInt(uint32_t, uint32_t) const;
    virtual uint32_t getUInt(uint32_t, uint32_t) const;
    virtual int64_t  getLong(uint32_t, uint32_t) const;
    virtual uint64_t getULong(uint32_t, uint32_t) const;
    virtual float    getFloat(uint32_t, uint32_t) const;
    virtual double   getDouble(uint32_t, uint32_t) const;
    /// Retrieve a string value.  It converts all data types to its string
    /// representation through the string stream library.
    /// @note This is generic, but slow!
    virtual std::string getString(uint32_t, uint32_t) const;
    /// @}

    /// Return the type used to store the values of the jth column of the
    /// bundle.
    virtual ibis::TYPE_T columnType(uint32_t) const {
	return ibis::UNKNOWN_TYPE;}
    /// Return the pointer to the underlying array used to store the jth
    /// column of the bundle.
    virtual void* columnArray(uint32_t) const {return 0;}
    /// Column pointer.
    virtual const ibis::column* columnPointer(uint32_t) const {return 0;}

    /// Re-order the bundles according to the new keys.
    virtual void reorder(const char *) = 0;
    /// Truncate the list of bundles.
    virtual long truncate(uint32_t keep) = 0;
    /// Truncate the list of bundles.
    virtual long truncate(uint32_t keep, uint32_t start) = 0;
    /// Truncate the list of bundle based on specified keys.
    virtual long truncate(const char *names, uint32_t keep) = 0;

    virtual ~bundle();
    /// Write the bundle to the directory for the query @c q.
    virtual void write(const ibis::query& q) const = 0;

    // some utility functions
    void sortRIDs(uint32_t i, uint32_t j);
    void swapRIDs(uint32_t i, uint32_t j) { // no error checking!
	if (rids) {
	    ibis::rid_t tmp = (*rids)[i];
	    (*rids)[i] = (*rids)[j];
	    (*rids)[j] = tmp;
	}
    };
    /// Compute the number of rows in bundle @c ind.
    uint32_t numRowsInBundle(uint32_t ind) const {
	if (starts != 0 && ind+1 < starts->size()) {
	    return (*starts)[ind+1]-(*starts)[ind];
	}
	else {
	    return 1U;
	}
    }
    /// Compute the number of rows in each group(bundle).  Return the
    /// number of bundles.
    uint32_t rowCounts(array_t<uint32_t>& cnt) const;

    /// Return the RIDs of the <code>ind</code>th bundle.
    const ibis::RIDSet* getRIDs(uint32_t ind) const {
	if (rids != 0 && starts != 0 && ind+1 < starts->size()) {
	    return new ibis::RIDSet(*rids, (*starts)[ind], (*starts)[ind+1]);
	}
	else {
	    return static_cast<ibis::RIDSet*>(0);
	}
    };
    /// Return the pointer to all RIDs.
    const ibis::RIDSet* getRIDs() const {return rids;}
    /// Return the pointer to array starts.
    const ibis::array_t<uint32_t>* getStarts() const {return starts;}

protected:
    const ibis::selectClause& comps;
    array_t<uint32_t>* starts; // starting positions of bundles (in rids)
    ibis::RIDSet* rids;
    const char* id;
    mutable bool infile; // is the current content in file?

    // Hides constructors from others.
    bundle(const ibis::selectClause& c)
	: comps(c), starts(0), rids(0), id(""), infile(false) {};
    // use ibis::query::getRIDs(const ibis::bitvector&) const to avoid the
    // read lock required by ibis::query::getRIDs() const.
    explicit bundle(const ibis::query &q)
	: comps(q.components()), starts(0),
	  rids(q.getRIDs(*(q.getHitVector()))), id(q.id()),
	  infile(false) {
	if (rids != 0 && static_cast<long>(rids->size()) != q.getNumHits()) {
	    delete rids;
	    rids = 0;
	}
    };
    bundle(const ibis::query &q, const ibis::bitvector& hits)
	: comps(q.components()), starts(0), rids(q.getRIDs(hits)),
	  id(q.id()), infile(false) {};
    bundle(const ibis::part &t, const ibis::selectClause &s)
	: comps(s), starts(new array_t<uint32_t>), rids(t.getRIDs()),
	id(t.name()), infile(false) {
	starts->resize(2);
	(*starts)[0] = 0;
	(*starts)[1] = t.nRows();
    }

private:
    bundle(); // no default constructor
    bundle(const bundle&); // no copy constructor
    bundle& operator=(const bundle&); // no assignment operator
}; // class ibis::bundle

/// The null bundle.  It contains only a list of RIDs.
class FASTBIT_CXX_DLLSPEC ibis::bundle0 : public ibis::bundle {
public:
    explicit bundle0(const ibis::query& q);
    bundle0(const ibis::query& q, const ibis::bitvector& hits);
    bundle0(const ibis::part& t, const ibis::selectClause &s);

    virtual void print(std::ostream& out) const;
    virtual void printAll(std::ostream& out) const;
    /// Print column names.  No column name to print in this case.
    virtual void printColumnNames(std::ostream&) const {};

    // can not do anything
    virtual void reorder(const char *) {};
    // only one bundle.
    virtual long truncate(uint32_t) {return 1;}
    virtual long truncate(uint32_t, uint32_t) {return 1;}
    virtual long truncate(const char *, uint32_t) {return 1;}

    virtual void write(const ibis::query& q) const {
	if (rids != 0 && infile == false) {
	    q.writeRIDs(rids);
	    infile = true;
	}
    }
}; // ibis::bundle0

/// The bundle with only one component.
class FASTBIT_CXX_DLLSPEC ibis::bundle1 : public ibis::bundle {
public:
    explicit bundle1(const ibis::query&, int =0);
    bundle1(const ibis::query&, const ibis::bitvector&, int =0);
    bundle1(const ibis::part&, const ibis::selectClause&, int =0);
    virtual ~bundle1();

    virtual uint32_t size() const {return (col ? col->size() : 0);}
    virtual uint32_t width() const {return 1;}

    virtual void write(const ibis::query&) const;
    virtual void print(std::ostream& out) const;
    virtual void printAll(std::ostream& out) const;
    virtual void printColumnNames(std::ostream& out) const;

    virtual int32_t     getInt(uint32_t, uint32_t) const;
    virtual uint32_t    getUInt(uint32_t, uint32_t) const;
    virtual int64_t     getLong(uint32_t, uint32_t) const;
    virtual uint64_t    getULong(uint32_t, uint32_t) const;
    virtual float       getFloat(uint32_t, uint32_t) const;
    virtual double      getDouble(uint32_t, uint32_t) const;
    virtual std::string getString(uint32_t, uint32_t) const;

    virtual const ibis::column* columnPointer(uint32_t j) const {
	return (j == 0 ? col->columnPointer() :
		static_cast<const ibis::column*>(0));}
    virtual ibis::TYPE_T columnType(uint32_t j) const {
	return (j == 0 ? col->getType() : ibis::UNKNOWN_TYPE);}
    virtual void* columnArray(uint32_t j) const {
	return (j == 0 ? col->getArray() : 0);}

    /// Reorder the values.  Since there is only one column in the bundle,
    /// the only valid name that can be passed in is the name of the sole
    /// column.  Because the bundle is already sorted according to this
    /// column, there is nothing to do expect to reverse the order of the
    /// values based on the second argument.
    virtual void reorder(const char *) {
	// if (direc.size() > 0 && direc[0] == false) {
	//     reverse();
	//     infile = false;
	// }
    }
    virtual long truncate(uint32_t);
    virtual long truncate(uint32_t, uint32_t);
    virtual long truncate(const char *, uint32_t keep);

private:
    ibis::colValues* col;
    ibis::selectClause::AGREGADO aggr;

    /// Sort all records.
    void sort(int);
    /// Reverse the order of records.
    void reverse();

    bundle1();
    bundle1(const bundle1&);
    bundle1& operator=(const bundle1&);
}; // ibis::bundle1

/// The bundle with multiple components.
class FASTBIT_CXX_DLLSPEC ibis::bundles : public ibis::bundle {
public:
    explicit bundles(const ibis::query&, int =0);
    bundles(const ibis::query&, const ibis::bitvector&, int =0);
    bundles(const ibis::part&, const ibis::selectClause&, int =0);
    virtual ~bundles() {clear();}

    virtual uint32_t size() const {
	return (cols.empty() ? 0 : cols.back()->size());}
    virtual uint32_t width() const {return cols.size();}

    virtual void print(std::ostream& out) const;
    virtual void printAll(std::ostream& out) const;
    virtual void printColumnNames(std::ostream& out) const;
    virtual void write(const ibis::query&) const;

    virtual int32_t     getInt(uint32_t, uint32_t) const;
    virtual uint32_t    getUInt(uint32_t, uint32_t) const;
    virtual int64_t     getLong(uint32_t, uint32_t) const;
    virtual uint64_t    getULong(uint32_t, uint32_t) const;
    virtual float       getFloat(uint32_t, uint32_t) const;
    virtual double      getDouble(uint32_t, uint32_t) const;
    virtual std::string getString(uint32_t, uint32_t) const;

    virtual const ibis::column* columnPointer(uint32_t j) const {
	return (j < cols.size() ? cols[j]->columnPointer() :
		static_cast<const ibis::column*>(0));}
    virtual ibis::TYPE_T columnType(uint32_t j) const {
	return (j < cols.size() ? cols[j]->getType() : ibis::UNKNOWN_TYPE);}
    virtual void* columnArray(uint32_t j) const {
	return (j < cols.size() ? cols[j]->getArray() : 0);}

    virtual void reorder(const char *);
    virtual long truncate(uint32_t keep);
    virtual long truncate(uint32_t keep, uint32_t start);
    virtual long truncate(const char *names, uint32_t keep);

private:
    colList cols;
    std::vector<ibis::selectClause::AGREGADO> aggr;

    void sort(int);
    void clear();
    void reverse();

    bundles();
    bundles(const bundles&);
    bundles& operator=(const bundles&);
}; // ibis::bundles

/// The class ibis::query::result allows user to retrieve query result one
/// row at a time.  It matches the semantics of an ODBC cursor; that is the
/// function @c next has to be called before the first set of results can
/// be used.
///
/// @note This implementation makes use of ibis::bundle for internal
/// storage, which means the results returned are sorted, see documentation
/// about ibis::bundle for more detail.
///
/// @note This implementation stores the results in memory.  Therefore, it
/// is not suitable for handling large result sets.
class FASTBIT_CXX_DLLSPEC ibis::query::result {
public:
    result(ibis::query& q);
    ~result();

    uint32_t width() const {return(bdl_!=0 ? bdl_->width() : 0U);}

    /// Move to the next row/record of results.
    bool next();
    /// Jump to the next bundle of results.
    bool nextBundle();
    /// Move the internal pointer back to the beginning.  Must call @c next
    /// to use the first set of results.
    void reset();

    /// Retrieve the value of the named column as a signed integer.
    /// @note The name must appeared in the select clause of the query used
    /// to construct the @c result object.
    int32_t getInt(const char *cname) const;
    /// Retrieve the value of the named column as an unsigned integer.
    uint32_t getUInt(const char *cname) const;
    /// Retrieve the value as a 64-bit integer.
    int64_t getLong(const char *cname) const;
    /// Retrieve the value as a 64-bit unsigned integer.
    uint64_t getULong(const char *cname) const;
    /// Retrieve the value of the named column as a single-precision
    /// floating-point number.
    float getFloat(const char *cname) const;
    /// Retrieve the value of the named column as a double-precision
    /// floating-point number.
    double getDouble(const char *cname) const;
    std::string getString(const char *cname) const;

    /// Retrieve the value of column @c selind in the select clause as a
    /// signed integer.
    /// @note Since this version avoids the name look up, it should be more
    /// efficient than the version taking the column name as argument.
    int32_t getInt(uint32_t selind) const {
	return bdl_->getInt(bid_-1, selind);
    }
    /// Retrieve the value of column @c selind in the select clause as an
    /// unsigned integer.
    uint32_t getUInt(uint32_t selind) const {
	return bdl_->getUInt(bid_-1, selind);
    }
    /// Retrieve the value as a 64-bit integer.
    int64_t getLong(uint32_t selind) const {
	return bdl_->getLong(bid_-1, selind);
    }
    /// Retrieve the value of column @c selind in the select clause as a
    /// 64-bit unsigned integer.
    uint64_t getULong(uint32_t selind) const {
	return bdl_->getULong(bid_-1, selind);
    }
    /// Retrieve the value of column @c selind in the select clause as a
    /// single-precision floating-point number.
    float getFloat(uint32_t selind) const {
	return bdl_->getFloat(bid_-1, selind);
    }
    /// Retrieve the value of column @c selind in the select clause as a
    /// double-precision floating-point number.
    double getDouble(uint32_t selind) const {
	return bdl_->getDouble(bid_-1, selind);
    }

    /// Retrieve the string value.
    /// @see ibis::bundle::getString for limitations.
    std::string getString(uint32_t selind) const {
	return bdl_->getString(bid_-1, selind);
    }
    inline uint32_t colPosition(const char *cname) const
    {return sel.find(cname);}

    void printColumnNames(std::ostream& out) const {
	if (bdl_ != 0) {
	    bdl_->printColumnNames(out);
	}
    }

private:
    ibis::query &que_;
    ibis::bundle *bdl_;
    const ibis::selectClause& sel;
    uint32_t bid_; // 0 for unused.
    uint32_t lib_; // results left in the bundle.

    result();
    result(const result&);
    result& operator=(const result&);
}; // ibis::query::result
#endif // IBIS_BUNDLE_H
