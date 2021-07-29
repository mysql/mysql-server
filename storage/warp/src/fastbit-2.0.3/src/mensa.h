// File: $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
#ifndef IBIS_MENSA_H
#define IBIS_MENSA_H
#include "table.h"	// ibis::table
#include "array_t.h"	// ibis::array_t

/**@file

A table with multiple data partitions on disk.
This class defines the data structure to encapsulate multiple on-disk data
partitions into a logical table.  The class translates the function defined
on ibis::part to the ibis::table interface.
 */
namespace ibis {
    class mensa;
    class liga;
} // namespace ibis

/// Class ibis::mensa contains multiple (horizontal) data partitions (@c
/// ibis::part) to form a logical data table.  The base data contained in
/// this table is logically immutable as reordering rows (through function
/// @c orderby) does not change the overall content of the table.  The
/// functions @c reverseRows and @c groupby are not implmented.
///
/// @note Mensa is a Latin word for "table."
class ibis::mensa : public ibis::table {
public:
    mensa() : nrows(0) {};
    explicit mensa(const char* dir);
    mensa(const char* dir1, const char* dir2);
    virtual ~mensa() {clear();}

    virtual uint64_t nRows() const {return nrows;}
    virtual uint32_t nColumns() const;

    virtual typeArray columnTypes() const;
    virtual stringArray columnNames() const;
    virtual int addPartition(const char*);
    virtual int dropPartition(const char*);

    virtual void describe(std::ostream&) const;
    virtual void dumpNames(std::ostream&, const char*) const;
    virtual int dump(std::ostream&, const char*) const;
    virtual int dump(std::ostream&, uint64_t, const char*) const;
    virtual int dump(std::ostream&, uint64_t, uint64_t, const char*) const;
    virtual int backup(const char* dir, const char* tname=0,
		       const char* tdesc=0) const;

    virtual int64_t
    getColumnAsBytes(const char*, char*, uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsUBytes(const char*, unsigned char*,
		      uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsShorts(const char*, int16_t*, uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsUShorts(const char*, uint16_t*, uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsInts(const char*, int32_t*, uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsUInts(const char*, uint32_t*, uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsLongs(const char*, int64_t*, uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsULongs(const char*, uint64_t*, uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsFloats(const char*, float*, uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsDoubles(const char*, double*, uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsDoubles(const char*, std::vector<double>&,
		       uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsStrings(const char*, std::vector<std::string>&,
		       uint64_t =0, uint64_t =0) const;
    virtual int64_t
    getColumnAsOpaques(const char*, std::vector<ibis::opaque>&,
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
			  uint64_t& nmin, uint64_t& nmax) const;
    virtual void estimate(const ibis::qExpr* cond,
			  uint64_t& nmin, uint64_t& nmax) const;
    using table::select;
    virtual table* select(const char* sel, const char* cond) const;
    virtual table* select2(const char* sel, const char* cond,
			   const char* pts) const;

    virtual void orderby(const stringArray&, const std::vector<bool>&);
    virtual void orderby(const stringArray&);
    virtual void orderby(const char *str) {ibis::table::orderby(str);}
    /// Reversing the ordering of the rows on disk requires too much work
    /// but has no obvious benefit.
    virtual void reverseRows() {};
    /// Directly performing group-by on the base data (without selection)
    /// is not currently supported.
    virtual table* groupby(const stringArray&) const {return 0;}
    /// Directly performing group-by on the base data (without selection)
    /// is not currently supported.
    virtual table* groupby(const char *) const {return 0;}

    virtual int buildIndex(const char*, const char*);
    virtual int buildIndexes(const char*);
    virtual int buildIndexes(const ibis::table::stringArray&);
    virtual const char* indexSpec(const char*) const;
    virtual void indexSpec(const char*, const char*);
    virtual int getPartitions(ibis::constPartList &) const;
    virtual int mergeCategories(const ibis::table::stringArray&);

    // Cursor class for row-wise data accesses.
    class cursor;
    /// Create a @c cursor object to perform row-wise data access.
    virtual ibis::table::cursor* createCursor() const;

protected:
    /// List of data partitions.
    ibis::partList parts;
    /// A combined list of columns names.
    ibis::table::namesTypes naty;
    uint64_t nrows;

    /// Clear the existing content.
    void clear();
    /// Compute the number of hits.
    int64_t computeHits(const char* cond) const {
	return ibis::table::computeHits
	    (reinterpret_cast<const ibis::constPartList&>(parts),
	     cond);}

private:
    // disallow copying.
    mensa(const mensa&);
    mensa& operator=(const mensa&);

    friend class cursor;
}; // ibis::mensa

class ibis::mensa::cursor : public ibis::table::cursor {
public:
    cursor(const ibis::mensa& t);
    virtual ~cursor() {clearBuffers();};

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
    virtual int dump(std::ostream& out, const char* del) const;

    int dumpBlock(std::ostream& out, const char* del);
    int dumpSome(std::ostream& out, uint64_t nr, const char* del);

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
    /// A buffer element is a minimal data structure to store a column in
    /// memory.  It only holds a pointer to the column name, therefore the
    /// original column data structure must existing while this data
    /// structure is active.
    /// TODO: unify this with ibis::tafel::column.
    struct bufferElement {
	const char* cname; ///!< Column name.
	ibis::TYPE_T ctype; ///!< Column type.
	mutable void* cval; ///!< Pointer to raw data.

	bufferElement() : cname(0), ctype(ibis::UNKNOWN_TYPE), cval(0) {}
	~bufferElement();
    }; // bufferElement
    typedef std::map<const char*, uint32_t, ibis::lessi> bufferMap;
    std::vector<bufferElement> buffer;
    bufferMap bufmap;
    const ibis::mensa& tab;
    unsigned curPart;
    unsigned preferred_block_size;
    uint64_t pBegin; ///!< the first row number of the current partition
    uint64_t bBegin; ///!< the first row number of the current block
    uint64_t bEnd;   ///!< end of the current block
    int64_t  curRow; ///!< the current row number

    void clearBuffers();
    int  fillBuffers() const;
    int  fillBuffer(uint32_t) const;
    void fillRow(ibis::table::row& res) const;
    int  dumpIJ(std::ostream&, uint32_t, uint32_t) const;

private:
    cursor();
    cursor(const cursor&);
    cursor& operator=(const cursor&);
}; // ibis::mensa::cursor

/// A specialization of class mensa.  It holds a list of data partitions
/// but does not own them.  It does not create these partitions nor delete
/// them.  It inherits the public functions of mensa without making any
/// additions or modifications.
///
/// @note About the name: Liga is the Danish translation of the term
/// "league table."  Oh, well, guess we are running out short names for a
/// table.
class ibis::liga : public ibis::mensa {
public:
    liga(ibis::part&);
    liga(const ibis::partList&);
    ~liga();

    /// The list of partitions in this class can NOT be expanded or
    /// otherwise modified.
    virtual int addPartition(const char*) {return -1;}

private:
    liga();
    liga(const liga&);
    liga& operator=(const liga&);
}; // ibis::liga

inline int
ibis::mensa::cursor::getColumnAsByte(const char* cn, char& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsByte((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsByte

inline int
ibis::mensa::cursor::getColumnAsUByte(const char* cn,
				      unsigned char& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsUByte((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsUByte

inline int
ibis::mensa::cursor::getColumnAsShort(const char* cn, int16_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsShort((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsShort

inline int
ibis::mensa::cursor::getColumnAsUShort(const char* cn, uint16_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsUShort((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsUShort

inline int
ibis::mensa::cursor::getColumnAsInt(const char* cn, int32_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsInt((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsInt

inline int
ibis::mensa::cursor::getColumnAsUInt(const char* cn, uint32_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsUInt((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsUInt

inline int
ibis::mensa::cursor::getColumnAsLong(const char* cn, int64_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsLong((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsLong

inline int
ibis::mensa::cursor::getColumnAsULong(const char* cn, uint64_t& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsULong((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsULong

inline int
ibis::mensa::cursor::getColumnAsFloat(const char* cn, float& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsFloat((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsFloat

inline int
ibis::mensa::cursor::getColumnAsDouble(const char* cn, double& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsDouble((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsDouble

inline int
ibis::mensa::cursor::getColumnAsString(const char* cn,
				       std::string& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsString((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsString

inline int
ibis::mensa::cursor::getColumnAsOpaque(const char* cn,
				       ibis::opaque& val) const {
    if (curRow < 0 || curPart >= tab.parts.size() || cn == 0 || *cn == 0)
	return -1;
    bufferMap::const_iterator it = bufmap.find(cn);
    if (it != bufmap.end())
	return getColumnAsOpaque((*it).second, val);
    else
	return -2;
} // ibis::mensa::cursor::getColumnAsOpaque

inline int
ibis::mensa::dump(std::ostream& out, uint64_t nr, const char* del) const {
    if (parts.empty() || nr == 0) return 0;
    ibis::mensa::cursor cur(*this);
    int ierr = cur.dumpSome(out, nr, del);
    return ierr;
} // ibis::mensa::dump

inline int
ibis::mensa::dump(std::ostream& out, uint64_t off, uint64_t nr,
		  const char* del) const {
    if (parts.empty() || nr == 0 || off > nrows) return 0;
    ibis::mensa::cursor cur(*this);
    int ierr = cur.fetch(off);
    if (ierr < 0) return ierr;

    ierr = cur.dumpSome(out, nr, del);
    return ierr;
} // ibis::mensa::dump
#endif // IBIS_MENSA_H
