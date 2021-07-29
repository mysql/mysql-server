// File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2007-2016 the Regents of the University of California
#ifndef IBIS_TAB_H
#define IBIS_TAB_H
/**@file

   This file stores two trivial concrete classes of ibis::table: tabula and
   tabele.

   @remarks Here is an explanation of how these two words are related to
   "table".  The term "table" is derived from a merger of French table and
   Old English tabele, ultimately from the Latin word tabula, "a board,
   plank, flat piece".  In Late Latin, tabula took over the meaning
   previously reserved to mensa (preserved in Spanish mesa "table").  In
   Old English, the word replaced bord for this meaning. -- Wikipedia.

*/

#include "table.h"	// ibis::table
#include "array_t.h"	// ibis::array_t
#include <iostream>

namespace ibis {
    class tabula;
    class tabele;
} // namespace ibis

/// A trivial class for a table with no columns.  This type of table is
/// generated when the select clause is blank or not specified.  It is also
/// used to represent an empty table with zero rows.
class ibis::tabula : public ibis::table {
public:
    tabula(const char* na, const char* de, uint64_t nr) :
	table(na, de), nrows(nr) {};
    explicit tabula(uint64_t nr=0) : nrows(nr) {};
    virtual ~tabula() {};
    virtual uint64_t nRows() const {return nrows;}
    virtual uint32_t nColumns() const {return 0;}

    virtual stringArray columnNames() const;
    virtual typeArray columnTypes() const;

    virtual void describe(std::ostream&) const;
    virtual void dumpNames(std::ostream&, const char*) const {}
    virtual int dump(std::ostream&, const char*) const {return 0;}
    virtual int dump(std::ostream&, uint64_t, const char*) const {return 0;}
    virtual int dump(std::ostream&, uint64_t, uint64_t, const char*) const {
	return 0;}
    /// The backup function.  It does nothing.
    virtual int backup(const char*, const char* =0,
		       const char* =0) const {return 0;}

    virtual int64_t
    getColumnAsBytes(const char*, char*, uint64_t =0,
		     uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsUBytes(const char*, unsigned char*, uint64_t =0,
		      uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsShorts(const char*, int16_t*, uint64_t =0,
		      uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsUShorts(const char*, uint16_t*, uint64_t =0,
		       uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsInts(const char*, int32_t*, uint64_t =0,
		    uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsUInts(const char*, uint32_t*, uint64_t =0,
		     uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsLongs(const char*, int64_t*, uint64_t =0,
		     uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsULongs(const char*, uint64_t*, uint64_t =0,
		      uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsFloats(const char*, float*, uint64_t =0,
		      uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsDoubles(const char*, double*, uint64_t =0,
		       uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsDoubles(const char*, std::vector<double>&, uint64_t =0,
		       uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsStrings(const char*, std::vector<std::string>&, uint64_t =0,
		       uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsOpaques(const char*, std::vector<ibis::opaque>&, uint64_t =0,
		       uint64_t =0) const {return -1;}
    virtual double getColumnMin(const char*) const {
	return DBL_MAX;}
    virtual double getColumnMax(const char*) const {
	return -DBL_MAX;}

    virtual long getHistogram(const char*, const char*,
			      double, double, double,
			      std::vector<uint32_t>&) const {return -1;}
    virtual long getHistogram2D(const char*, const char*,
				double, double, double,
				const char*,
				double, double, double,
				std::vector<uint32_t>&) const {return -1;}
    virtual long getHistogram3D(const char*, const char*,
				double, double, double,
				const char*,
				double, double, double,
				const char*,
				double, double, double,
				std::vector<uint32_t>&) const {return -1;}

    virtual void estimate(const char* cond,
			  uint64_t& nmin, uint64_t& nmax) const;
    virtual void estimate(const ibis::qExpr* cond,
			  uint64_t& nmin, uint64_t& nmax) const;
    using ibis::table::select;
    virtual table* select(const char*, const char*) const {return 0;}

    virtual table* groupby(const stringArray&) const {return 0;}
    virtual table* groupby(const char*) const {return 0;}
    virtual void orderby(const stringArray&, const std::vector<bool>&) {};
    virtual void orderby(const stringArray&) {};
    virtual void orderby(const char*) {};
    virtual void reverseRows() {};

    virtual int buildIndex(const char*, const char*) {return -1;}
    virtual int buildIndexes(const char*) {return -1;}
    virtual int buildIndexes(const ibis::table::stringArray&) {return -1;}
    virtual const char* indexSpec(const char*) const {return 0;}
    virtual void indexSpec(const char*, const char*) {return;}

    // Cursor class for row-wise data accesses.
    class cursor;
    /// Create a @c cursor object to perform row-wise data access.
    virtual ibis::table::cursor* createCursor() const;

private:
    /// The number of rows.
    uint64_t nrows;

    tabula(const tabula&);
    tabula& operator=(const tabula&);
}; // ibis::tabula

/// A trivial class for a table with exactly one row, one column and one
/// integer value.  This type of table is generated when the select clause
/// is "count(*)".  This class could be replaced with @ref ibis::tabula,
/// however, treating the output of "count(*)" as a one-row-and-one-column
/// table is closer to the ODBC/JDBC convention.
class ibis::tabele : public ibis::table {
public:
    /// Constructor.  If the name of the sole column is not given, it is
    /// assumed to be "nrows".
    tabele(const char* na, const char* de, uint64_t nr, const char* nm=0) :
	table(na, de), nrows(nr), col(nm && *nm ? nm : "nrows") {};
    /// Constructor.  A table name will be generated automatically based on
    /// the current time.  If the name of the sole column is not specified,
    /// it is assumed to be "nrows".
    explicit tabele(uint64_t nr=0, const char* nm=0) :
	nrows(nr), col(nm && *nm ? nm : "nrows") {};
    virtual ~tabele() {};
    virtual uint64_t nRows() const {return 1U;}
    virtual uint32_t nColumns() const {return 1U;}

    virtual stringArray columnNames() const;
    virtual typeArray columnTypes() const;

    virtual void describe(std::ostream&) const;
    virtual void dumpNames(std::ostream&, const char*) const;
    virtual int dump(std::ostream&, const char*) const;
    virtual int dump(std::ostream& out, uint64_t nr, const char* sep) const {
	if (nr > 0) return dump(out, sep); else return 0;}
    virtual int dump(std::ostream& out, uint64_t off, uint64_t nr,
		     const char* sep) const {
	if (off==0 && nr > 0) return dump(out, sep); else return 0;}
    /// The backup operation.  It does nothing.
    virtual int backup(const char*, const char* =0,
		       const char* =0) const {return 0;}

    virtual int64_t getColumnAsBytes(const char*, char*, uint64_t =0,
				     uint64_t =0) const {return -1;}
    virtual int64_t getColumnAsUBytes(const char*,
				      unsigned char*, uint64_t =0,
				      uint64_t =0) const {return -1;}
    virtual int64_t getColumnAsShorts(const char*,
				      int16_t*, uint64_t =0,
				      uint64_t =0) const {return -1;}
    virtual int64_t getColumnAsUShorts(const char*,
				       uint16_t*, uint64_t =0,
				       uint64_t =0) const {return -1;}
    virtual int64_t getColumnAsInts(const char*, int32_t*, uint64_t =0,
				    uint64_t =0) const {return -1;}
    virtual int64_t getColumnAsUInts(const char* cn, uint32_t* vals,
				     uint64_t begin =0, uint64_t =0) const {
	if (stricmp(col.c_str(), cn) == 0) {
	    if (begin == 0) {
		*vals = static_cast<uint32_t>(nrows);
		return 1;
	    }
	    else {
		return 0;
	    }
	}
	else {
	    return -1;
	}
    }
    virtual int64_t getColumnAsLongs(const char* cn, int64_t* vals,
				     uint64_t begin =0, uint64_t =0) const {
	if (stricmp(col.c_str(), cn) == 0) {
	    if (begin == 0) {
		*vals = nrows;
		return 1;
	    }
	    else {
		return 0;
	    }
	}
	else {
	    return -1;
	}
    }
    virtual int64_t getColumnAsULongs(const char* cn, uint64_t* vals,
				      uint64_t begin =0, uint64_t =0) const {
	if (stricmp(col.c_str(), cn) == 0) {
	    if (begin == 0) {
		*vals = nrows;
		return 1;
	    }
	    else {
		return 0;
	    }
	}
	else {
	    return -1;
	}
    }
    virtual int64_t getColumnAsFloats(const char*, float*, uint64_t =0,
				      uint64_t =0) const {return -1;}
    virtual int64_t getColumnAsDoubles(const char*, double*, uint64_t =0,
				     uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsDoubles(const char*, std::vector<double>&, uint64_t =0,
		       uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsStrings(const char*, std::vector<std::string>&, uint64_t =0,
		       uint64_t =0) const {return -1;}
    virtual int64_t
    getColumnAsOpaques(const char*, std::vector<ibis::opaque>&, uint64_t =0,
		       uint64_t =0) const {return -1;}
    virtual double getColumnMin(const char* cn) const {
	if (stricmp(cn, col.c_str()) == 0)
	    return nrows;
	else
	    return DBL_MAX;
    }
    virtual double getColumnMax(const char* cn) const {
	if (stricmp(cn, col.c_str()) == 0)
	    return nrows;
	else
	    return -DBL_MAX;
    }

    virtual long getHistogram(const char*, const char*,
			      double, double, double,
			      std::vector<uint32_t>&) const {return -1;}
    virtual long getHistogram2D(const char*, const char*,
				double, double, double,
				const char*,
				double, double, double,
				std::vector<uint32_t>&) const {return -1;}
    virtual long getHistogram3D(const char*, const char*,
				double, double, double,
				const char*,
				double, double, double,
				const char*,
				double, double, double,
				std::vector<uint32_t>&) const {return -1;}

    virtual void estimate(const char* cond,
			  uint64_t& nmin, uint64_t& nmax) const;
    virtual void estimate(const ibis::qExpr* cond,
			  uint64_t& nmin, uint64_t& nmax) const;
    using ibis::table::select;
    virtual table* select(const char*, const char*) const {return 0;}

    virtual table* groupby(const stringArray&) const {return 0;}
    virtual table* groupby(const char*) const {return 0;}
    virtual void orderby(const stringArray&, const std::vector<bool>&) {};
    virtual void orderby(const stringArray&) {};
    virtual void orderby(const char*) {};
    virtual void reverseRows() {};

    virtual int buildIndex(const char*, const char*) {return -1;}
    virtual int buildIndexes(const char*) {return -1;}
    virtual int buildIndexes(const ibis::table::stringArray&) {return -1;}
    virtual const char* indexSpec(const char*) const {return 0;}
    virtual void indexSpec(const char*, const char*) {return;}

    // Cursor class for row-wise data accesses.
    class cursor;
    /// Create a @c cursor object to perform row-wise data access.
    virtual ibis::table::cursor* createCursor() const;

    const char* colName() const {return col.c_str();}

private:
    // the number of rows selected
    uint64_t nrows;
    // the name of the column
    std::string col;

    tabele(const tabele&);
    tabele& operator=(const tabele&);

    friend class cursor;
}; // ibis::tabele

// Inline functions
inline ibis::table::stringArray ibis::tabula::columnNames() const {
    ibis::table::stringArray tmp;
    return tmp;
}

inline ibis::table::typeArray ibis::tabula::columnTypes() const {
    ibis::table::typeArray tmp;
    return tmp;
}

inline void ibis::tabula::describe(std::ostream& out) const {
    out << "Table " << name_ << " (" << desc_ << ") contains "
	<< nrows << " row" << (nrows > 1 ? "s" : "")
	<< (nrows>0?" but":" and") << " no column" << std::endl;
}

inline void
ibis::tabula::estimate(const char*, uint64_t& nmin, uint64_t& nmax) const {
    nmin = 0;
    nmax = nrows;
}

inline void
ibis::tabula::estimate(const ibis::qExpr*,
		       uint64_t& nmin, uint64_t& nmax) const {
    nmin = 0;
    nmax = nrows;
}

class ibis::tabula::cursor : public ibis::table::cursor {
public:
    cursor(const ibis::tabula& t) : tab(t) {}
    virtual ~cursor() {};

    virtual uint64_t nRows() const {return tab.nRows();}
    virtual uint32_t nColumns() const {return tab.nColumns();}
    virtual ibis::table::stringArray columnNames() const {
	return tab.columnNames();}
    virtual ibis::table::typeArray columnTypes() const {
	return tab.columnTypes();}
    virtual int fetch() {return -1;}
    virtual int fetch(uint64_t) {return -1;}
    virtual int fetch(ibis::table::row&) {return -1;}
    virtual int fetch(uint64_t, ibis::table::row&) {return -1;}
    virtual uint64_t getCurrentRowNumber() const {return tab.nRows();}
    virtual int dump(std::ostream&, const char* del) const {return 0;}

    virtual int getColumnAsByte(const char*, char&) const {return -1;}
    virtual int getColumnAsUByte(const char*, unsigned char&) const
    {return -1;}
    virtual int getColumnAsShort(const char*, int16_t&) const {return -1;}
    virtual int getColumnAsUShort(const char*, uint16_t&) const {return -1;}
    virtual int getColumnAsInt(const char*, int32_t&) const {return -1;}
    virtual int getColumnAsUInt(const char*, uint32_t&) const {return -1;}
    virtual int getColumnAsLong(const char*, int64_t&) const {return -1;}
    virtual int getColumnAsULong(const char*, uint64_t&) const {return -1;}
    virtual int getColumnAsFloat(const char*, float&) const {return -1;}
    virtual int getColumnAsDouble(const char*, double&) const {return -1;}
    virtual int getColumnAsString(const char*, std::string&) const {return -1;}
    virtual int getColumnAsOpaque(const char*, ibis::opaque&) const {return -1;}

    virtual int getColumnAsByte(uint32_t, char&) const {return -1;}
    virtual int getColumnAsUByte(uint32_t, unsigned char&) const {return -1;}
    virtual int getColumnAsShort(uint32_t, int16_t&) const {return -1;}
    virtual int getColumnAsUShort(uint32_t, uint16_t&) const {return -1;}
    virtual int getColumnAsInt(uint32_t, int32_t&) const {return -1;}
    virtual int getColumnAsUInt(uint32_t, uint32_t&) const {return -1;}
    virtual int getColumnAsLong(uint32_t, int64_t&) const {return -1;}
    virtual int getColumnAsULong(uint32_t, uint64_t&) const {return -1;}
    virtual int getColumnAsFloat(uint32_t, float&) const {return -1;}
    virtual int getColumnAsDouble(uint32_t, double&) const {return -1;}
    virtual int getColumnAsString(uint32_t, std::string&) const {return -1;}
    virtual int getColumnAsOpaque(uint32_t, ibis::opaque&) const {return -1;}

private:
    const ibis::tabula& tab;

    cursor();
    cursor(const cursor&);
    cursor& operator=(const cursor&);
}; // ibis::tabula::cursor

inline ibis::table::cursor* ibis::tabula::createCursor() const {
    return new ibis::tabula::cursor(*this);
} // ibis::tabula::createCursor

inline ibis::table::stringArray ibis::tabele::columnNames() const {
    ibis::table::stringArray tmp(1);
    tmp[0] = col.c_str();
    return tmp;
}

inline ibis::table::typeArray ibis::tabele::columnTypes() const {
    ibis::table::typeArray tmp(1);
    tmp[0] = ibis::ULONG;
    return tmp;
}

inline void ibis::tabele::describe(std::ostream& out) const {
    out << "Table " << name_ << " (" << desc_
	<< ") contains 1 column and 1 row\n"
	<< col << "\t" << ibis::TYPESTRING[(int)ibis::ULONG] << "\n"
	<< std::endl;
}

inline void ibis::tabele::dumpNames(std::ostream& out, const char*) const {
    out << col << std::endl;
}

inline int ibis::tabele::dump(std::ostream& out, const char *) const {
    out << nrows << std::endl;
    return 0;
}

inline void
ibis::tabele::estimate(const char*, uint64_t& nmin, uint64_t& nmax) const {
    nmin = 0;
    nmax = 1;
}

inline void
ibis::tabele::estimate(const ibis::qExpr*,
		       uint64_t& nmin, uint64_t& nmax) const {
    nmin = 0;
    nmax = 1;
}

class ibis::tabele::cursor : public ibis::table::cursor {
public:
    cursor(const ibis::tabele& t) : tab(t), current(-1) {}
    virtual ~cursor() {};

    virtual uint64_t nRows() const {return tab.nRows();}
    virtual uint32_t nColumns() const {return tab.nColumns();}
    virtual ibis::table::stringArray columnNames() const {
	return tab.columnNames();}
    virtual ibis::table::typeArray columnTypes() const {
	return tab.columnTypes();}
    virtual int fetch() {
	++ current;
	return (static_cast<uint64_t>(current) < tab.nRows() ? 0 : -1);}
    virtual int fetch(uint64_t irow) {
	if (irow < tab.nRows()) {
	    current = irow;
	    return 0;
	}
	else {
	    return -1;
	}}
    virtual int fetch(ibis::table::row& res) {
	++ current;
	res.clear();
	if (current == 0) {
	    res.ulongsnames.push_back(tab.col);
	    res.ulongsvalues.push_back(tab.nrows);
	    return 0;
	}
	else {
	    return -1;
	}
    }
    virtual int fetch(uint64_t irow, ibis::table::row& res) {
	res.clear();
	if (irow < 1U) {
	    current = 0U;
	    res.ulongsnames.push_back(tab.col);
	    res.ulongsvalues.push_back(tab.nrows);
	    return 0;
	}
	else {
	    return -1;
	}
    }
    virtual uint64_t getCurrentRowNumber() const {return current;}
    virtual int dump(std::ostream& out, const char* del) const {
	if (current == 0) {
	    out << tab.nrows << "\n";
	    return 0;
	}
	else {
	    return -1;
	}
    }

    virtual int getColumnAsByte(const char*, char&) const {return -1;}
    virtual int getColumnAsUByte(const char*, unsigned char&) const {
	return -1;}
    virtual int getColumnAsShort(const char*, int16_t&) const {return -1;}
    virtual int getColumnAsUShort(const char*, uint16_t&) const {return -1;}
    virtual int getColumnAsInt(const char*, int32_t&) const {return -1;}
    virtual int getColumnAsUInt(const char* cn, uint32_t&) const {
	return -1;}
    virtual int getColumnAsLong(const char* cn, int64_t& val) const {
	if (current == 0 && stricmp(tab.colName(), cn) == 0) {
	    val = tab.nrows;
	    return 1;}
	else {
	    return -1;
	}
    }
    virtual int getColumnAsULong(const char* cn, uint64_t& val) const {
	if (current == 0 && stricmp(tab.colName(), cn) == 0) {
	    val = tab.nrows;
	    return 1;}
	else {
	    return -1;
	}
    }
    virtual int getColumnAsFloat(const char*, float&) const {return -1;}
    virtual int getColumnAsDouble(const char*, double&) const {return -1;}
    virtual int getColumnAsString(const char*, std::string&) const {return -1;}
    virtual int getColumnAsOpaque(const char*, ibis::opaque&) const {return -1;}

    virtual int getColumnAsByte(uint32_t, char&) const {return -1;}
    virtual int getColumnAsUByte(uint32_t, unsigned char&) const {return -1;}
    virtual int getColumnAsShort(uint32_t, int16_t&) const {return -1;}
    virtual int getColumnAsUShort(uint32_t, uint16_t&) const {return -1;}
    virtual int getColumnAsInt(uint32_t, int32_t&) const {return -1;}
    virtual int getColumnAsUInt(uint32_t cn, uint32_t&) const {
	return -1;}
    virtual int getColumnAsLong(uint32_t cn, int64_t& val) const {
	if (current == 0 && cn == 0) {
	    val = tab.nrows;
	    return 1;}
	else {
	    return -1;
	}}
    virtual int getColumnAsULong(uint32_t cn, uint64_t& val) const {
	if (current == 0 && cn == 0) {
	    val = tab.nrows;
	    return 1;}
	else {
	    return -1;
	}}
    virtual int getColumnAsFloat(uint32_t, float&) const {return -1;}
    virtual int getColumnAsDouble(uint32_t, double&) const {return -1;}
    virtual int getColumnAsString(uint32_t, std::string&) const {return -1;}
    virtual int getColumnAsOpaque(uint32_t, ibis::opaque&) const {return -1;}

private:
    const ibis::tabele& tab;
    int64_t current;

    cursor();
    cursor(const cursor&);
    cursor& operator=(const cursor&);
}; // ibis::tabele::cursor

inline ibis::table::cursor* ibis::tabele::createCursor() const {
    return new ibis::tabele::cursor(*this);
} // ibis::tabele::createCursor
#endif // IBIS_TAB_H
