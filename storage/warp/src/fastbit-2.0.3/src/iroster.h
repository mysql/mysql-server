//File: $Id$
// Author: John Wu <John.Wu at ACM.org>
// Copyright (c) 2000-2016 the Regents of the University of California
#ifndef IBIS_ROSTER_H
#define IBIS_ROSTER_H
#include "array_t.h"
#include "util.h"
///@file
/// Defines a pseudo-index.  Used in some performance comparisons.

/// A roster is a list of values in ascending order plus their original
/// positions.  It can use an external sort if the data and indices can
/// not fit into memory.  The indices will be written to a file with
/// extension .ind and the sorted values in a file with extension .srt.  If
/// the indices can not be loaded into memory as a whole, the .ind file
/// will be opened for future read operations.
///
/// @ingroup FastBitIBIS
class ibis::roster {
public:
    ~roster() {clear();};
    roster(const ibis::column* c, const char* dir = 0);
    roster(const ibis::column* c, ibis::fileManager::storage* st,
	   uint32_t offset = 8);

    const char* name() const {return "roster list";}
    const ibis::column* getColumn() const {return col;}
    uint32_t size() const;

    int read(const char* idxfile);
    int read(ibis::fileManager::storage* st);

    /// Output a minimal information about the roster list.
    void print(std::ostream& out) const;
    /// Write two files, .ind for indices and .srt to the sorted values.
    int write(const char* dt) const;
    /// Write the sorted version of the attribute values to a .srt file.
    int writeSorted(const char* dt) const;

    const array_t<uint32_t>& array() const {return ind;}
    inline uint32_t operator[](uint32_t i) const;

    /// Locate the values and set their positions in the bitvector.
    ///
    /// Return a negative value for error, zero or a positive value for in
    /// case of success.
    /// 
    /// @note The input values are assumed to be sorted in ascending order.
    template <typename T>
    int locate(const ibis::array_t<T>& vals,
	       ibis::bitvector& positions) const;
    /// Locate the values and set their positions in the bitvector.
    /// Return the positions as a list of 32-bit integers.
    template <typename T>
    int locate(const ibis::array_t<T>& vals,
	       std::vector<uint32_t>& positions) const;

    /// Locate the values and set their positions in the bitvector.
    /// Return the positions of the matching entries as a bitvector.
    /// Return a negative value for error, zero or a positive value for
    /// success.
    /// The input values are assumed to be sorted in ascending order.
    template <typename T>
    int locate(const std::vector<T>& vals,
	       ibis::bitvector& positions) const;
    /// Locate the values and set their positions in the bitvector.
    /// Return the positions as a list of 32-bit integers.
    template <typename T>
    int locate(const std::vector<T>& vals,
	       std::vector<uint32_t>& positions) const;

    /// A two-way merge algorithm.  Uses std::less<T> for comparisons.
    /// Assumes the sorted segment size is @c segment elements of type T.
    template <class T>
    static long mergeBlock2(const char *dsrc, const char *dout,
			    const uint32_t segment, array_t<T>& buf1,
			    array_t<T>& buf2, array_t<T>& buf3);

//     /// A templated multi-way merge sort algorithm for a data file with
//     /// values of the same type.  Return the number of passes if
//     /// successful, other return a negative number to indicate error.
//     template <class Type, uint32_t mway=64, uint32_t block=8192>
//     static int diskSort(const char *datafile, const char *scratchfile);

protected:
//     /// This function performs the initial sort of blocks.  Entries in each
//     /// (@c mway * @c block)-segment is sorted and written back to the same
//     /// data file.
//     template <class Type, uint32_t mway, uint32_t block>
//     static int diskSortInit(const char *datafile);
//     /// Merge blocks.  The variable @c segment contains the number of
//     /// consecutive entries (a segement) that are already sorted.  To
//     /// consecutive such segments will be merged.
//     template <class Type, uint32_t mway, uint32_t block>
//     static int diskSortMerge(const char *from, const char *to,
// 			     uint32_t segment);

    uint32_t locate(const double& val) const;

    template <typename T> int
    icSearch(const std::vector<T>& vals, std::vector<uint32_t>& pos) const;
    template <typename T> int
    oocSearch(const std::vector<T>& vals, std::vector<uint32_t>& pos) const;
    template <typename inT, typename myT> int
    locate2(const std::vector<inT>&, std::vector<uint32_t>&) const;
    template <typename T> int
    icSearch(const ibis::array_t<T>& vals, std::vector<uint32_t>& pos) const;
    template <typename T> int
    oocSearch(const ibis::array_t<T>& vals, std::vector<uint32_t>& pos) const;
    template <typename inT, typename myT> int
    locate2(const ibis::array_t<inT>&, std::vector<uint32_t>&) const;

private:
    // private member variables
    const ibis::column* col;    ///!< Each roster is for one column.
    array_t<uint32_t> ind;	///!< @c [ind[i]] is the ith smallest value.
    mutable int inddes;		///!< The descriptor for the @c .ind file.

    // private member functions
    void clear() {ind.clear(); if (inddes>=0) UnixClose(inddes);};
    int write(FILE* fptr) const;

    /// The in-core sorting function to build the roster list.
    void icSort(const char* f = 0);
    /// The out-of-core sorting function to build the roster list.
    void oocSort(const char* f = 0);
    template <class T>
    long oocSortBlocks(const char *src, const char *dest,
		       const char *ind, const uint32_t mblock,
		       array_t<T>& dbuf1, array_t<T>& dbuf2,
		       array_t<uint32_t>& ibuf) const;
    template <class T>
    long oocMergeBlocks(const char *dsrc, const char *dout,
			const char *isrc, const char *iout,
			const uint32_t mblock, const uint32_t stride,
			array_t<T>& dbuf1, array_t<T>& dbuf2,
			array_t<uint32_t>& ibuf1,
			array_t<uint32_t>& ibuf2) const;

    roster(); // not implemented
    roster(const roster&); // not implemented
    const roster& operator=(const roster&); // not implemented
}; // ibis::roster

namespace ibis {
    template <> int
    roster::locate(const std::vector<double>&, ibis::bitvector&) const;
    template <> int
    roster::locate(const ibis::array_t<double>&, ibis::bitvector&) const;
}

/// Return the row number of the ith smallest value.
inline uint32_t ibis::roster::operator[](uint32_t i) const {
    uint32_t tmp = UINT_MAX;
    if (i < ind.size()) {
	tmp = ind[i];
    }
    else if (inddes >= 0) {
	if (static_cast<off_t>(i*sizeof(uint32_t)) !=
	    UnixSeek(inddes, i*sizeof(uint32_t), SEEK_SET))
	    return tmp;
	if (sizeof(uint32_t) != UnixRead(inddes, &tmp, sizeof(uint32_t)))
	    return UINT_MAX;
    }
    else {
	LOGGER(ibis::gVerbose > 0)
	    << "Warning -- roster(ind[" << ind.size() << "], inddes="
	    << inddes << ")::operator[]: index i (" << i
	    << ") is out of range";
    }
    return tmp;
} // ibis::roster::operator[]
#endif // IBIS_ROSTER_H

