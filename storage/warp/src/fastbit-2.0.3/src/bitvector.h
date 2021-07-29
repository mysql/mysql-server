// $Id$
//	Author: John Wu <John.Wu at ACM.org>
//              Lawrence Berkeley National Laboratory
//	Copyright (c) 2000-2016 the Regents of the University of California
#ifndef BITVECTOR_H
#define BITVECTOR_H
///@file
/// Definition of Word-Aligned Hybrid code.

#include "array_t.h"	// alternative to std::vector

#if defined(_MSC_VER) && defined(_WIN32)
//disable warnings on extern before template instantiation
#pragma warning (disable : 4231)
#endif
#if defined(_WIN32) && (defined(_MSC_VER) || defined(__MINGW32__)) && defined(CXX_USE_DLL) && !defined(DLL_EXPORT)
extern template class FASTBIT_CXX_DLLSPEC ibis::array_t<uint32_t>;
#endif


/**
  @brief A data structure to represent a sequence of bits.

Key features

 A bitvector object stores a sequence of bits and provides fast bitwise
 logical operations.  In addition, it supports operations to append new
 bits from the end, read bits at arbitrary location and set bits at
 arbitrary location.  It also supports an iterator, a const_iterator and an
 indexSet.

Encoding format <http://lbl.gov/~kwu/ps/LBNL-49626.html>
<http://portal.acm.org/citation.cfm?doid=1132863.1132864>

 Incoming bits are organized into words (bitvector::word_t).  A word is a
 literal word if its Most Significant Bit (MSB) is 0, it is a fill word if
 its MSB is 1.  A literal word stores literal bit values in the bit
 position following the MSB and a fill word stores a sequence of
 consecutive bits that are of the same value, i.e., a fill.  The second
 most significant bit of the fill word is the bit value, the remaining bits
 of the word is a unsigned integer that stores the length of the fill as
 number of equivalent literal words, i.e., how many literal words it will
 take if the fill is stored in literal words.

 Restrictions
<ul>
<li> The number of bits must be expressible by one single
    bitvector::word_t.  This ensures that a fill word can store a fill of
    any valid length without performing a bound check.  If
    bitvector::word_t is 32-bit long, the maximum number of bits that can
    be represented by a bitvector object is 4 billion.

<li> When adding a bit with bitvector::operator+=, the integer value passed
    in must be one of 0 or 1.  Since checking whether the import value is 0
    or not 0 causes pipeline bubble in CPU, we have opted for not performing
    the check.  An input value other than 0 or 1 will cause unrelated bits
    to be modified and producing an incorrect bitvector.
</ul>

@ingroup FastBitIBIS
*/
class FASTBIT_CXX_DLLSPEC ibis::bitvector {
public:
    typedef uint32_t word_t;///!< The basic unit of data storage.

    /// Destructor.
    ~bitvector() {clear();};
    // constructors of bitvector class
    bitvector();
    bitvector(const bitvector& bv);
    explicit bitvector(const char* file);
    explicit bitvector(const array_t<word_t>& arr);
    explicit bitvector(const array_t<word_t>& arr, const size_t begin,
                       const size_t end);
    explicit bitvector(word_t*, size_t);

    inline const bitvector& operator=(const bitvector& bv);
    inline bitvector& copy(const bitvector& bv);
    inline bitvector& swap(bitvector& bv);

    // use bv to replace part of the existing value, match the ith bit with
    // the first of bv, return reference to self
    //bitvector& copy(const word_t i, const bitvector& bv);
    void setBit(const word_t i, int val);
    int  getBit(const word_t i) const;
    inline void turnOnRawBit(const word_t i);
    void erase(word_t i, word_t j);

    void set(int val, word_t n);
    void clear();

    bitvector& operator+=(const bitvector& bv);
    inline bitvector& operator+=(int b);
    inline void appendByte(unsigned char);
    void appendWord(word_t w);
    inline void appendFill(int val, word_t n);

    int operator==(const bitvector& rhs) const;

    void flip();
    ///@brief Perform bitwise AND between this bitvector and @c rhs.
    void operator&=(const bitvector& rhs);
    ///@brief Perform bitwise AND between this bitvector and @c rhs, return
    /// the result as a new bitvector.
    bitvector* operator&(const bitvector&) const;
    ///@brief Perform bitwise OR.
    void operator|=(const bitvector& rhs);
    ///@brief Perform bitwise OR and return the result as a new bitvector.
    bitvector* operator|(const bitvector&) const;
    ///@brief Perform bitwise exclusive or (XOR).
    void operator^=(const bitvector& rhs);
    ///@brief Perform bitwise XOR and return the result as a new bitvector.
    bitvector* operator^(const bitvector&) const;
    ///@brief Perform bitwise subtraction (a & !b).
    void operator-=(const bitvector& rhs);
    ///@brief Perform bitwise subtraction and return the result as a new
    /// bitvector.
    bitvector* operator-(const bitvector&) const;
    bool operator<(const bitvector&) const;

    void subset(const bitvector& mask, bitvector& res) const;
    word_t count(const bitvector& mask) const;

    // I/O functions.
    void read(const char *fn);
    void write(const char *fn) const;
    void write(int fdes) const;
    void write(array_t<word_t>& arr) const;

    void compress();
    void decompress();
    word_t compressible() const;
    /// Does this bit vector use less space than the maximum? Return true
    /// if yes, otherwise false.
    bool isCompressed() const {return (m_vec.size()*MAXBITS < nbits);}

    inline word_t size() const throw();
    inline void sloppySize(word_t n) const;
    inline word_t cnt() const;
    inline word_t count() const {return cnt();}
    inline word_t sloppyCount() const;
    inline word_t numFillWords() const;
    /// Return the number of bytes used by the bitvector object in memory.
    uint32_t bytes() const throw() {
	return (m_vec.size()*sizeof(word_t) + sizeof(bitvector));
    };
    /// Compute the number of bytes in the serialized version of this
    /// bitvector object.  This would be the number of bytes this bitvector
    /// needs on disk or in an array_t<word_t>.
    uint32_t getSerialSize() const throw() {
	return (m_vec.size() + 1 + (active.nbits>0)) * sizeof(word_t);
    };
    /// Return the number of bits in a literal word.
    static word_t bitsPerLiteral() {return MAXBITS;}
    inline static double randomSize(word_t nb, word_t nc);
    inline static double markovSize(word_t nb, word_t nc, double f);
    static double clusteringFactor(word_t nb, word_t nc, word_t sz);

    void adjustSize(word_t nv, word_t nt);
    void reserve(unsigned nb, unsigned nc, double cf=0.0);

    /// Is the bitvector empty?  For efficiency reasons, this funciton
    /// only works correctly on a properly compressed bitvector.
    bool empty() const {return all0s() && active.val == 0;}
    /// The print function.
    std::ostream& print(std::ostream &) const;

    /// Iterator that supports modification of individual bit.
    class iterator;
    inline iterator end();
    inline iterator begin();

    /// The read-only iterator.
    class const_iterator;
    inline const_iterator end() const;
    inline const_iterator begin() const;

    /// A data structure to provide the position of bits that are one.
    class indexSet;
    inline indexSet firstIndexSet() const;

    /// An iterator over the positions that are one.
    class pit;

    // give accesses to some friends
    friend class indexSet;
    friend class iterator;
    friend class const_iterator;

protected:
    inline bool all0s() const;
    inline bool all1s() const;

private:
    // static members, i.e., constants to be used internally
    static const unsigned MAXBITS;
    static const unsigned SECONDBIT;
    static const word_t FILLBIT;
    static const word_t HEADER0;
    static const word_t HEADER1;
    static const word_t ALLONES;
    static const word_t MAXCNT;

    /// @brief The struct active_word stores the last few bits that do not
    /// fill a whole word.
    ///
    /// It only stores raw bit sequences.  The bits are pushed
    /// from the right, i.e., the newest bit is stored in the LSB (right
    /// most) position.
    struct active_word {
	word_t val;	// the value
	word_t nbits;	// total number of bits

	active_word() : val(0), nbits(0) {};
	void reset() {val = 0; nbits = 0;};
	int is_full() const {return (nbits >= MAXBITS);};
        /// Append a single bit.  The argument must be either 0 or 1.
	void append(int b) {
	    val <<= 1; nbits ++; val += b;
	};
    }; // struct active_word

    /// @brief An internal struct used during logical operations to track
    /// the usage of fill words.
    struct run {
	int isFill;
	int fillBit;
	word_t nWords;
	array_t<word_t>::const_iterator it;
	run() : isFill(0), fillBit(0), nWords(0), it(0) {};
	void decode() { ///!< Decode the word pointed by @c it.
	    fillBit = (*it > HEADER1);
	    if (*it > ALLONES) {
		nWords = (*it & MAXCNT);
		isFill = 1;
	    }
	    else {
		nWords = 1;
		isFill = 0;
	    }
	};
	/// Reduce the run size by 1 word.  Advance the iterator @c it
	/// forward if necessary.
	void operator--() {
	    if (nWords > 1) {--nWords;}
	    else {++ it; nWords = 0;}
	};
	/// Reduce the run size by @len words.  Advance the iterator @c
	/// it forward as necessary.
	void operator-=(word_t len) {
	    while (len > 0) {
		if (nWords == 0) decode();
		if (isFill != 0) {
		    if (nWords > len) {nWords -= len; len = 0;}
		    else if (nWords == len) {nWords = 0; len = 0; ++ it;}
		    else {len -= nWords; ++ it; nWords = 0;}
		}
		else {-- len; ++ it; nWords = 0;}
	    }
	};
    };
    friend struct run;
    friend struct active_word;

    // member variables of bitvector class
    mutable word_t nbits;	///!< Number of bits in @c m_vec.
    mutable word_t nset;	///!< Number of bits that are 1 in @c m_vec.
    active_word active;		///!< The active word.
    array_t<word_t> m_vec;	///!< Store whole words.

    // private functions of bitvector class
    word_t count_c1(const bitvector& mask) const;
    word_t count_c2(const bitvector& mask) const;
    // The following three functions all performs or operation, _c2 and _c1
    // generate compressed solutions, but _c0, _d1, and _d2 generate
    // uncompressed solutions.
    // or_c2 assumes there are compressed word in both operands
    // or_c1 *this may contain compressed word, but not rhs
    // or_c0 assumes both operands are not compressed
    // or_d1 *this contains no compressed word and is overwritten with the
    //       result
    // or_d2 both *this and rhs are compressed, but res is not compressed
    void or_c2(const bitvector& rhs, bitvector& res) const;
    void or_c1(const bitvector& rhs, bitvector& res) const;
    void or_c0(const bitvector& rhs);
    void or_d1(const bitvector& rhs);
    void or_d2(const bitvector& rhs, bitvector& res) const;
    void and_c2(const bitvector& rhs, bitvector& res) const;
    void and_c1(const bitvector& rhs, bitvector& res) const;
    void and_c0(const bitvector& rhs);
    void and_d1(const bitvector& rhs);
    void and_d2(const bitvector& rhs, bitvector& res) const;
    void xor_c2(const bitvector& rhs, bitvector& res) const;
    void xor_c1(const bitvector& rhs, bitvector& res) const;
    void xor_c0(const bitvector& rhs);
    void xor_d1(const bitvector& rhs);
    void xor_d2(const bitvector& rhs, bitvector& res) const;
    void minus_c2(const bitvector& rhs, bitvector& res) const;
    void minus_c1(const bitvector& rhs, bitvector& res) const;
    void minus_c1x(const bitvector& rhs, bitvector& res) const;
    void minus_c0(const bitvector& rhs);
    void minus_d1(const bitvector& rhs);
    void minus_d2(const bitvector& rhs, bitvector& res) const;
    inline void copy_runs(run& it, word_t& nw); // copy nw words
    inline void copy_runsn(run& it, word_t& nw); // copy nw words and negate
    inline void copy_fill(array_t<word_t>::iterator& jt, run& it);
    inline void copy_runs(array_t<word_t>::iterator& jt, run& it,
			  word_t& nw);
    inline void copy_runsn(array_t<word_t>::iterator& jt, run& it,
			   word_t& nw);
    void decompress(array_t<word_t>& tmp) const;
    void copy_comp(array_t<word_t>& tmp) const;
    inline void append_active();
    inline void append_counter(int val, word_t cnt);
    inline word_t cnt_ones(word_t) const; // number of ones in a word
    inline word_t cnt_bits(word_t) const; // number of bits in a word
    word_t do_cnt() const throw ();
}; // class bitvector

/// The const_iterator class.  It iterates on the individual bits.
class ibis::bitvector::const_iterator {
public:
    const_iterator() : compressed(0), ind(0), nbits(0), literalvalue(0),
		       fillbit(0), active(0) {}

    const_iterator(const const_iterator& r)
	: compressed(r.compressed), ind(r.ind), nbits(r.nbits),
	  literalvalue(r.literalvalue), fillbit(r.fillbit), active(r.active),
	  end(r.end), begin(r.begin), it(r.it) {};
    const_iterator& operator=(const const_iterator& r) {
	compressed = r.compressed; ind = r.ind; nbits = r.nbits;
	literalvalue = r.literalvalue; fillbit = r.fillbit; active = r.active;
	end = r.end; begin = r.begin; it = r.it;
	return *this;}

    inline bool operator*() const;
    inline int operator!=(const const_iterator& rhs) const throw ();
    inline int operator==(const const_iterator& rhs) const throw ();

    inline const_iterator& operator++();
    inline const_iterator& operator--();
    const_iterator& operator+=(int incr);

private:
    ibis::bitvector::word_t	compressed;
    ibis::bitvector::word_t	ind;
    ibis::bitvector::word_t	nbits;
    ibis::bitvector::word_t	literalvalue;
    int				fillbit;
    const active_word*		active;
    array_t<word_t>::const_iterator end;
    array_t<word_t>::const_iterator begin;
    array_t<word_t>::const_iterator it;

    void decodeWord();

    // give three functions of bitvector access to private variables
    friend void ibis::bitvector::erase(word_t i, word_t j);
    friend const_iterator ibis::bitvector::begin() const;
    friend const_iterator ibis::bitvector::end() const;
    friend class ibis::bitvector::iterator;
}; // class ibis::bitvector::const_iterator

/// The iterator that allows modification of bits.  It provides only one
/// additional function (operator=) than const_iterator to allow
/// modification of the bit pointed.
/// ********************IMPORTANT********************
/// operator= modifies the content of the bitvector it points to and it can
/// invalidate other iterators or const_iterators referring to the same
/// bitvector.
class ibis::bitvector::iterator {
public:
    iterator() : compressed(0), ind(0), nbits(0), literalvalue(0), fillbit(0),
		 bitv(0), active(0), vec(0) {}
    iterator(const iterator& r)
	: compressed(r.compressed), ind(r.ind), nbits(r.nbits),
	  literalvalue(r.literalvalue), fillbit(r.fillbit), bitv(r.bitv),
	  active(r.active), vec(r.vec), it(r.it) {};
    const iterator& operator=(const iterator& r) {
	compressed = r.compressed; ind = r.ind; nbits = r.nbits;
	literalvalue = r.literalvalue; fillbit = r.fillbit; bitv = r.bitv;
	active = r.active; vec = r.vec; it = r.it; return *this;}

    inline bool operator*() const; // still can not modify this
    inline int operator!=(const const_iterator& rhs) const throw ();
    inline int operator==(const const_iterator& rhs) const throw ();
    inline int operator!=(const iterator& rhs) const throw ();
    inline int operator==(const iterator& rhs) const throw ();

    inline iterator& operator++();
    inline iterator& operator--();
    iterator& operator+=(int incr);
    const iterator& operator=(int val);

private:
    ibis::bitvector::word_t	compressed;
    ibis::bitvector::word_t	ind;
    ibis::bitvector::word_t	nbits;
    ibis::bitvector::word_t	literalvalue;
    int				fillbit;
    ibis::bitvector*		bitv;
    active_word*		active;
    array_t<word_t>*		vec;
    array_t<word_t>::iterator	it;

    void decodeWord();

    friend iterator ibis::bitvector::begin();
    friend iterator ibis::bitvector::end();
}; // class ibis::bitvector::iterator

/// @brief The indexSet stores positions of bits that are one.
///
/// It decodes one word of the bitvector at a time.  For a fill of ones,
/// the function @c isRange returns true, otherwise it returns false.  If
/// isRange returns true, the position of the first bit is pointed by the
/// pointer returned by function @c indices, and there are @c nIndices
/// consecutive ones.  If @c isRange returns false, there are @c nIndices
/// bits that are one and the positions of these bits are stored in the
/// array returned by function @c indices.
class FASTBIT_CXX_DLLSPEC ibis::bitvector::indexSet {
public:
    /// Default constructor.
    indexSet() : it(0), end(0), active(0), nind(0) {}
    /// Copy constructor.
    indexSet(const indexSet& rhs)
	: it(rhs.it), end(rhs.end), active(rhs.active), nind(rhs.nind) {
	ind[0] = rhs.ind[0];
	ind[1] = rhs.ind[1];
	ind[2] = rhs.ind[2];
	ind[3] = rhs.ind[3];
	ind[4] = rhs.ind[4];
	ind[5] = rhs.ind[5];
	ind[6] = rhs.ind[6];
	ind[7] = rhs.ind[7];
	ind[8] = rhs.ind[8];
	ind[9] = rhs.ind[9];
	ind[10] = rhs.ind[10];
	ind[11] = rhs.ind[11];
	ind[12] = rhs.ind[12];
	ind[13] = rhs.ind[13];
	ind[14] = rhs.ind[14];
	ind[15] = rhs.ind[15];
	ind[16] = rhs.ind[16];
	ind[17] = rhs.ind[17];
	ind[18] = rhs.ind[18];
	ind[19] = rhs.ind[19];
	ind[20] = rhs.ind[20];
	ind[21] = rhs.ind[21];
	ind[22] = rhs.ind[22];
	ind[23] = rhs.ind[23];
	ind[24] = rhs.ind[24];
	ind[25] = rhs.ind[25];
	ind[26] = rhs.ind[26];
	ind[27] = rhs.ind[27];
	ind[28] = rhs.ind[28];
	ind[29] = rhs.ind[29];
	ind[30] = rhs.ind[30];
	ind[31] = rhs.ind[31];
    }
    /// Assignment operator.
    indexSet& operator=(const indexSet& rhs) {
	it = rhs.it;
	end = rhs.end;
	active = rhs.active;
	nind = rhs.nind;
	ind[0] = rhs.ind[0];
	ind[1] = rhs.ind[1];
	ind[2] = rhs.ind[2];
	ind[3] = rhs.ind[3];
	ind[4] = rhs.ind[4];
	ind[5] = rhs.ind[5];
	ind[6] = rhs.ind[6];
	ind[7] = rhs.ind[7];
	ind[8] = rhs.ind[8];
	ind[9] = rhs.ind[9];
	ind[10] = rhs.ind[10];
	ind[11] = rhs.ind[11];
	ind[12] = rhs.ind[12];
	ind[13] = rhs.ind[13];
	ind[14] = rhs.ind[14];
	ind[15] = rhs.ind[15];
	ind[16] = rhs.ind[16];
	ind[17] = rhs.ind[17];
	ind[18] = rhs.ind[18];
	ind[19] = rhs.ind[19];
	ind[20] = rhs.ind[20];
	ind[21] = rhs.ind[21];
	ind[22] = rhs.ind[22];
	ind[23] = rhs.ind[23];
	ind[24] = rhs.ind[24];
	ind[25] = rhs.ind[25];
	ind[26] = rhs.ind[26];
	ind[27] = rhs.ind[27];
	ind[28] = rhs.ind[28];
	ind[29] = rhs.ind[29];
	ind[30] = rhs.ind[30];
	ind[31] = rhs.ind[31];
	return *this;
    }

    //int operator!=(const indexSet& rhs) const {return (it != rhs.it);};
    /// Is the index set a consecutive range?
    bool isRange() const {return (nind>=ibis::bitvector::MAXBITS);}
    /// Pointer to the indices.
    const word_t* indices() const {return ind;};
    /// Number of indices.
    word_t nIndices() const {return nind;}
    /// The value of the current compressed word.
    const word_t& currentWord() const {return *it;}
    indexSet& operator++();

    // allow bitvector::firstIndexSet() to access private member variables
    friend indexSet ibis::bitvector::firstIndexSet() const;

private:
    array_t<word_t>::const_iterator it;
    array_t<word_t>::const_iterator end;
    const active_word* active; // points back to the active word
    word_t nind; // number of indices
    word_t ind[32];
}; // class ibis::bitvector::indexSet

/// Iterate over the positive positions one at a time.  A positive position
/// is the position where a bit is 1.
///
/// This class iterates over all the positive positions.  Immediately after
/// initialization, the "current" bit is the first bit that is 1.
class ibis::bitvector::pit {
public:
    pit(): curr(0xFFFFFFFFU) {}
    pit(const ibis::bitvector &bv) : curr(0xFFFFFFFFU) {init(bv);}

    inline ibis::bitvector::word_t operator*() const;
    inline void next();
    inline void skip(unsigned);
    inline void init(const ibis::bitvector&);

private:
    ibis::bitvector::word_t curr;
    ibis::bitvector::indexSet iset;
}; // class ibis::bitvector::pit

/// Explicitly set the size of the bitvector.  This is intended to be used
/// by indexing functions to avoid counting the number of bits.  Caller is
/// responsible for ensuring the size assigned is actually correct.  It
/// does not affect the number of bits actually represented by this data
/// structure.  To change the number of bits represented by this data
/// structure use the function adjustSize instead.
inline void ibis::bitvector::sloppySize(word_t n) const {
    nbits = n-active.nbits;
#if defined(WAH_CHECK_SIZE)
    word_t nb = do_cnt();
    if (nb != nbits) {
	const_cast<ibis::bitvector*>(this)->adjustSize(0, nbits);
	LOGGER(ibis::gVerbose >= 0)
	    << "bitvector::sloppySize -- adjust the number of bits to "
	    << n;
    }
#endif
} // ibis::bitvector::sloppySize

/// Provide a sloppy count of the number of bits that are 1.  If it returns
/// 0, this bit vector has NO bits that are 1, otherwise, there might be
/// some bits that are 1.  However, the return value not equaling to 0 does
/// not necessarily mean there are actually no bit that is 1.  It simply
/// means that we can not determine whether all bits are 0 without
/// additional work.  This is a sloppy version of count, it only checks the
/// active word and the first one regular word and therefore should be very
/// cheap to run.  This function is more useful for situations where one
/// wants to detect an empty bit vector.
inline ibis::bitvector::word_t ibis::bitvector::sloppyCount() const {
    if (nset > 0) {
	return nset;
    }
    else if (active.nbits == 0 || active.val == 0) {
	if (m_vec.empty() ||
	    (m_vec.size() == 1 &&
	     (m_vec[0] == 0 ||
	      (m_vec[0]>=HEADER0 && m_vec[0]<HEADER1))))
	    return 0;
	else
	    return 2;
    }
    else {
	return 1;
    }
} // ibis::bitvector::sloppyCount

/// Are all bits in regular words 0?  It assumes the regular words have
/// been properly compressed and therefore only need to check one word.
inline bool ibis::bitvector::all0s() const {
    if (m_vec.empty()) {
	return true;
    }
    else if (m_vec.size() == 1) {
	return (m_vec[0] == 0 || (m_vec[0]>=HEADER0 && m_vec[0]<HEADER1));
    }
    else {
	return false;
    }
} // ibis::bitvector::all0s

/// Are all bits in regular words 1?  It assumes the regular words are
/// properly compressed and therefore only need to examine one word.
inline bool ibis::bitvector::all1s() const {
    if (m_vec.size() == 1) {
	return (m_vec[0] == ALLONES || (m_vec[0] > HEADER1));
    }
    else {
	return false;
    }
} // ibis::bitvector::all1s

/// Compute the number of bits represented by a word.
inline ibis::bitvector::word_t
ibis::bitvector::cnt_bits(ibis::bitvector::word_t val) const {
    return ((val>ALLONES) ? ((val&MAXCNT)*MAXBITS) : MAXBITS);
} // ibis::bitvector::cnt_bits

/// Compute the number of ones in a literal word.
inline ibis::bitvector::word_t
ibis::bitvector::cnt_ones(ibis::bitvector::word_t val) const {
    // number of 1 bits in a value between 0 and 255
    static const word_t table[256] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};
    return table[val&0xFFU] + table[(val>>8)&0xFFU] +
	table[(val>>16)&0xFFU] + table[(val>>24)&0xFFU];
} // ibis::bitvector::cnt_ones

/// Return the total number of bits in the bit sequence.
inline ibis::bitvector::word_t ibis::bitvector::size() const throw() {
    return ((nbits?nbits:(nbits=do_cnt()))+active.nbits);
} // ibis::bitvector::size

/// Return the number of bits that are one.
inline ibis::bitvector::word_t ibis::bitvector::cnt() const {
    if (nset==0 && !m_vec.empty())
	nbits = do_cnt();
    return (nset+cnt_ones(active.val));
} // ibis::bitvector::cnt

/// Return the number of fill words.
inline ibis::bitvector::word_t ibis::bitvector::numFillWords() const {
    word_t cnt=0;
    array_t<ibis::bitvector::word_t>::const_iterator it = m_vec.begin();
    while (it != m_vec.end()) {
	cnt += (*it >> ibis::bitvector::MAXBITS);
	it++;
    }
    return cnt;
} // inline word_t ibis::bitvector::numFillWords() const {

/// The assignment operator.  Use deep copy.  Wanted to use shallow copy
/// for efficiency considerations, but SHALLOW copy causes unexpected
/// problem in test program bitty.cpp.
inline const ibis::bitvector&
ibis::bitvector::operator=(const ibis::bitvector& bv) {
    nbits = bv.nbits; nset = bv.nset; active = bv.active;
    m_vec.deepCopy(bv.m_vec);
    return *this;
}

/// Make a copy.  Performs a deep copy.
inline ibis::bitvector& ibis::bitvector::copy(const ibis::bitvector& bv) {
    nbits = bv.nbits; nset = bv.nset; active = bv.active;
    m_vec.deepCopy(bv.m_vec);
    return *this;
}

inline ibis::bitvector& ibis::bitvector::swap(bitvector& bv) {
    word_t tmp;
    tmp = bv.nbits; bv.nbits = nbits; nbits = tmp;
    tmp = bv.nset; bv.nset = nset; nset = tmp;
    active_word atmp = bv.active;
    bv.active = active; active = atmp;
    m_vec.swap(bv.m_vec);
    return *this;
}

/// A private function called to make the active word part of the regular
/// words.  It assumes that nbits == MAXBITS and refers to MAXBITS instead
/// of @c nbits.
inline void ibis::bitvector::append_active() {
//      std::cout << "before: active.val = " << std::hex << active.val;
//      if (m_vec.size())
//  	std::cout << ", m_vec.back() = " << m_vec.back();
//      std::cout << std::dec << std::endl;
    if (m_vec.empty()) {
	m_vec.push_back(active.val);
    }
    else if (active.val == 0) {// incoming word is zero
	if (m_vec.back() == 0) {
	    m_vec.back() = (HEADER0 + 2);
	}
	else if (m_vec.back() >= HEADER0 && m_vec.back() < HEADER1) {
	    ++ m_vec.back();
	}
	else {
	    m_vec.push_back(active.val);
	}
    }
    else if (active.val == ALLONES) {// incoming word is allones
	if (m_vec.back() == ALLONES) {
	    m_vec.back() = (HEADER1 | 2);
	}
	else if (m_vec.back() >= HEADER1) {
	    ++ m_vec.back();
	}
	else {
	    m_vec.push_back(active.val);
	}
    }
    else { // incoming word contains a mixture of bits
	m_vec.push_back(active.val);
    }
    nbits += MAXBITS;
    active.reset();
    nset = 0;
//      std::cout << "after: m_vec.back() = " << std::hex << m_vec.back()
//  	      << std::dec << std::endl;
} // ibis::bitvector::append_active

/// A private function to append a single counter when the active word is
/// empty.  The value of @c cnt is assumed to be greater than 0.
inline void ibis::bitvector::append_counter(int val, word_t cnt) {
    word_t head = 2 + val;
    word_t w = (head << SECONDBIT) + cnt;
    nbits += cnt*MAXBITS;
    if (m_vec.empty()) {
        m_vec.push_back(w);
    }
    else if ((m_vec.back()>>SECONDBIT) == head) {
        m_vec.back() += cnt;
    }
    else if ((m_vec.back()==ALLONES) && head==3) {
        m_vec.back() = w + 1;
    }
    else if ((m_vec.back() == 0) && head==2) {
        m_vec.back() = w + 1;
    }
    else {
        m_vec.push_back(w);
    }
} // ibis::bitvector::append_counter

/// Append a single bit.  The incoming value must be 0 or 1.
inline ibis::bitvector& ibis::bitvector::operator+=(int b) {
    active.append(b);
    if (active.is_full())
	append_active();
    return *this;
} // ibis::bitvector::operator+=

/// Append all 8 bits of the incoming bytes as literal bits.
void ibis::bitvector::appendByte(unsigned char c) {
    if (active.nbits >= MAXBITS)
        append_active();

    if (active.nbits+8 < MAXBITS) {
        active.val <<= 8;
        active.nbits += 8;
        active.val += c;
    }
    else if (active.nbits+8 > MAXBITS) {
        unsigned na = MAXBITS - active.nbits;
        unsigned hi = (c >> (8 - na));
        active.val <<= na;
        active.val += hi;
        append_active();
        active.nbits = 8 - na;
        active.val = ((hi << active.nbits) ^ c);
    }
    else {
        active.val <<= 8;
        active.val += c;
        append_active();
    }
} // ibis::bitvector::appendByte

/// Append @c n bits of @c val.  The value @c n may be arbitrary integer as
/// long as the resulting size is still representable by a
/// ibis::bitvector::word_t, however, the value @c val must be either 0 or
/// 1.
inline void ibis::bitvector::appendFill(int val, word_t n) {
    if (n == 0) return;
    if (active.nbits > 0) {
	word_t tmp = (MAXBITS - active.nbits);
	if (tmp > n) tmp = n;
	active.nbits += tmp;
	active.val <<= tmp;
	n -= tmp;
	if (val != 0)
	    active.val |= (1U<<tmp) - 1;
	if (active.nbits >= MAXBITS)
	    append_active();
    }
    if (n >= MAXBITS) {
	word_t cnt = n / MAXBITS;
	if (cnt > 1)
	    append_counter(val, cnt);
	else if (val != 0) {
	    active.val = ALLONES;
	    append_active();
	}
	else {
	    active.val = 0;
	    append_active();
	}
	n -= cnt * MAXBITS;
    }
    if (n > 0) {
	active.nbits = n;
	active.val = val*((1U<<n)-1);
    }
} // ibis::bitvector::appendFill

/// Copy a group of consecutive runs.  It appends nw words starting from
/// 'it' to the current bit vector, assuming active is empty.  Both it and
/// nw are modified in this function.  On returning from this function, it
/// points to the next unused word and nw stores the value of remaining
/// words to copy.
inline void ibis::bitvector::copy_runs(run& it, word_t& nw) {
    // deal with the first word -- attach it to the last word in m_vec
    if (it.isFill == 0) {
	active.val = *(it.it);
	append_active();
	-- nw;
    }
    else if (it.nWords > 1) {
        append_counter(it.fillBit, it.nWords);
        nw -= it.nWords;
    }
    else if (it.nWords == 1) {
        active.val = (it.fillBit != 0 ? ALLONES : 0);
        append_active();
        -- nw;
    }
    ++ it.it;
    nset = 0;
    it.nWords = 0;
    nbits += MAXBITS * nw;

    while (nw > 0) { // copy the words
	it.decode();
	if (nw >= it.nWords) {
	    m_vec.push_back(*(it.it));
	    nw -= it.nWords;
	    it.nWords = 0;
	    ++ it.it;
	}
	else {
	    break;
	}
    }
    nbits -= MAXBITS * nw;
} // ibis::bitvector::copy_runs

/// Copy the complements of a set of consecutive runs.  It assumes
/// active word to be empty.
inline void ibis::bitvector::copy_runsn(run& it, word_t& nw) {
    // deal with the first word -- need to attach it to the last word in m_vec
    if (it.isFill == 0) {
	active.val = ALLONES ^ *(it.it);
	append_active();
	-- nw;
    }
    else if (it.nWords > 1) {
        append_counter(!it.fillBit, it.nWords);
        nw -= it.nWords;
    }
    else if (it.nWords == 1) {
        active.val = (it.fillBit != 0 ? 0 : ALLONES);
        append_active();
        -- nw;
    }
    ++ it.it; // advance to the next word
    nset = 0;
    it.nWords = 0;
    nbits += MAXBITS * nw;

    while (nw > 0) { // copy the words
	it.decode();
	if (nw >= it.nWords) {
	    m_vec.push_back((it.isFill?FILLBIT:ALLONES) ^ *(it.it));
	    nw -= it.nWords;
	    it.nWords = 0;
	    ++ it.it;
	}
	else {
	    break;
	}
    }
    nbits -= MAXBITS * nw;
} // ibis::bitvector::copy_runsn

/// Copy the fill in "run it" as literal words.
/// This implementation relies on the fact that the iterator jt is actually
/// a bare pointer.
inline void ibis::bitvector::copy_fill
(array_t<ibis::bitvector::word_t>::iterator& jt, run& it) {
    if (it.fillBit == 0) {
	while (it.nWords > 3) {
	    *jt = 0;
	    jt[1] = 0;
	    jt[2] = 0;
	    jt[3] = 0;
	    jt += 4;
	    it.nWords -= 4;
	}
	if (it.nWords == 1) {
	    *jt = 0; ++jt;
	}
	else if (it.nWords == 2) {
	    *jt = 0; jt[1] = 0; jt += 2;
	}
	else if (it.nWords == 3) {
	    *jt = 0; jt[1] = 0; jt[2] = 0; jt += 3;
	}
    }
    else {
	while (it.nWords > 3) {
	    *jt = ALLONES;
	    jt[1] = ALLONES;
	    jt[2] = ALLONES;
	    jt[3] = ALLONES;
	    jt += 4;
	    it.nWords -= 4;
	}
	if (it.nWords == 1) {
	    *jt = ALLONES; ++jt;
	}
	else if (it.nWords == 2) {
	    *jt = ALLONES; jt[1] = ALLONES; jt += 2;
	}
	else if (it.nWords == 3) {
	    *jt = ALLONES; jt[1] = ALLONES; jt[2] = ALLONES; jt += 3;
	}
    }
    it.nWords = 0;
    ++ it.it;
} // ibis::bitvector::copy_fill

/// Copy the next nw words (nw * MAXBITS bits) starting with run it
/// to an array_t as uncompressed words.  If the run has more words than nw,
/// return the left over words to give it a chance for the longer run to be
/// counted first.
inline void ibis::bitvector::copy_runs
(array_t<ibis::bitvector::word_t>::iterator& jt, run& it, word_t& nw) {
    while (nw >= it.nWords && nw > 0) {
	if (it.isFill != 0) { // copy a fill
	    const array_t<word_t>::iterator iend = jt + it.nWords;
	    if (it.fillBit == 0) {
		while (jt < iend) {
		    *jt = 0;
		    ++ jt;
		}
	    }
	    else {
		while (jt < iend) {
		    *jt = ALLONES;
		    ++ jt;
		}
	    }
	    nw -= it.nWords;
	}
	else { // copy a single word
	    *jt = *(it.it);
	    ++ jt;
	    -- nw;
	}
	++ it.it; // advance to the next word
	if (nw > 0) {
	    it.decode();
	}
	else {
	    it.nWords = 0;
	    return;
	}
    }
} // ibis::bitvector::copy_runs

/// Copy the complements of the next nw words (nw * MAXBITS bits)
/// starting with "run it" as uncompressed words.
inline void ibis::bitvector::copy_runsn
(array_t<ibis::bitvector::word_t>::iterator& jt, run& it, word_t& nw) {
    while (nw >= it.nWords) {
	if (it.isFill != 0) { // a fill
	    const array_t<word_t>::iterator iend = jt + it.nWords;
	    if (it.fillBit == 0) {
		while (jt < iend) {
		    *jt = ALLONES;
		    ++ jt;
		}
	    }
	    else {
		while (jt < iend) {
		    *jt = 0;
		    ++ jt;
		}
	    }
	    nw -= it.nWords;
	}
	else { // a literal word
	    *jt = *(it.it) ^ ALLONES;
	    ++ jt;
	    -- nw;
	}
	++ it.it; // advance to the next word
	if (nw > 0) {
	    it.decode();
	}
	else {
	    it.nWords = 0;
	    return;
	}
    }
} // ibis::bitvector::copy_runsn

inline ibis::bitvector::iterator ibis::bitvector::begin() {
    iterator it;
    it.it     = m_vec.begin();
    it.vec    = &m_vec;
    it.bitv   = this;
    it.active = &active;
    it.decodeWord();
    return it;
} // ibis::bitvector::begin

inline ibis::bitvector::iterator ibis::bitvector::end() {
    iterator it;
    it.ind		= 0;
    it.compressed	= 0;
    it.nbits		= 0;
    it.literalvalue	= 0;
    it.fillbit		= 0;
    it.it     = m_vec.end() + 1;
    it.vec    = &m_vec;
    it.bitv   = this;
    it.active = &active;
    return it;
} // ibis::bitvector::end

inline ibis::bitvector::const_iterator ibis::bitvector::begin() const {
    const_iterator it;
    it.it     = m_vec.begin();
    it.begin  = m_vec.begin();
    it.end    = m_vec.end();
    it.active = &active;
    it.decodeWord();
    return it;
} // ibis::bitvector::begin

/// Dereference the current bit.  No error checking.
inline bool ibis::bitvector::iterator::operator*() const {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (vec==0 || it<vec->begin() ||  it>vec->end())
	throw "bitvector::const_iterator not initialized correctly.";
#endif
    if (compressed != 0)
	return (fillbit != 0);
    else
	return (1 & (literalvalue >> (bitvector::SECONDBIT - ind)));
} // ibis::bitvector::iterator::operator*

/// Comparing two iterators.  It only compare the content of the iterator
/// to m_vec.
inline int ibis::bitvector::iterator::operator!=
(const ibis::bitvector::const_iterator& rhs) const throw () {
    return (it != rhs.it);
}

inline int ibis::bitvector::iterator::operator==
(const ibis::bitvector::const_iterator& rhs) const throw () {
    return (it == rhs.it);
}

inline int ibis::bitvector::iterator::operator!=
(const ibis::bitvector::iterator& rhs) const throw () {
    return (it != rhs.it);
}

inline int ibis::bitvector::iterator::operator==
(const ibis::bitvector::iterator& rhs) const throw () {
    return (it == rhs.it);
}

/// Increment the interator.  Move on to the next bit.
inline ibis::bitvector::iterator& ibis::bitvector::iterator::operator++() {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (vec==0 || it<vec->begin() || it>vec->end())
	throw "bitvector::iterator not formed correctly.";
#endif
    if (ind+1<nbits) {++ind;}
    else {++ it; decodeWord();}
    return *this;
}

/// Decrement the interator.  Move back by one bit.
inline ibis::bitvector::iterator& ibis::bitvector::iterator::operator--() {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (vec==0 || it<vec->begin() || it>vec->end()+1)
	throw "bitvector::iterator not formed correctly.";
#endif
    if (ind) -- ind;
    else {
	if (it <= vec->end()) -- it;
	else if (active->nbits) it = vec->end();
	else it = vec->end() - 1;
	decodeWord();
	if (nbits) ind = nbits - 1;
    }
    return *this;
}

inline ibis::bitvector::const_iterator ibis::bitvector::end() const {
    const_iterator it;
    it.ind		= 0;
    it.compressed	= 0;
    it.nbits		= 0;
    it.literalvalue	= 0;
    it.fillbit		= 0;
    it.it    = m_vec.end() + 1;
    it.begin = m_vec.begin();
    it.end   = m_vec.end();
    it.active = &active;
    return it;
} // ibis::bitvector::end

/// Dereference the current bit value.  No error checking.
inline bool ibis::bitvector::const_iterator::operator*() const {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (it==0 || end==0 || it>end || nbits<=ind)
	throw "bitvector::const_iterator not initialized correctly.";
#endif
    if (compressed != 0)
	return (fillbit != 0);
    else
	return (1 & (literalvalue >> (bitvector::SECONDBIT - ind)));
}

/// Comparing two iterators.  It actually only check the value of the
/// iterator pointing to m_vec.
inline int ibis::bitvector::const_iterator::operator!=
(const ibis::bitvector::const_iterator& rhs) const throw (){
    return (it != rhs.it);
}
inline int ibis::bitvector::const_iterator::operator==
(const ibis::bitvector::const_iterator& rhs) const throw () {
    return (it == rhs.it);
}

/// Increment the iterator.  Move on to the next bit.
inline ibis::bitvector::const_iterator&
ibis::bitvector::const_iterator::operator++() {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (it==0 || end==0 || it>end)
	throw "bitvector::const_iterator not formed correctly.";
#endif
    if (ind+1<nbits) {++ind;}
    else {++ it; decodeWord();}
    return *this;
}

/// Decrement the iterator.  Move back one bit.
inline ibis::bitvector::const_iterator&
ibis::bitvector::const_iterator::operator--() {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (it==0 || end==0 || it>end)
	throw "bitvector::const_iterator not formed correctly.";
#endif
    if (ind) -- ind;
    else {
	if (it <= end) -- it;
	else if (active->nbits) it = end;
	else it = end - 1;
	decodeWord();
	if (nbits) ind = nbits - 1;
    }
    return *this;
}

/// Operator to retrieve the position of the current bit.
///
/// It returns the current position or 0xFFFFFFFFU when there is no bit to
/// report or the object is not initialized.
inline ibis::bitvector::word_t ibis::bitvector::pit::operator*() const {
    if (curr < iset.nIndices()) {
        if (iset.isRange()) {
            return (*iset.indices())+curr;
        }
        else {
            return iset.indices()[curr];
        }
    }
    else {
        return 0xFFFFFFFFU;
    }
} // ibis::bitvector::pit::operator*

/// Initialize the data structure.  The
void ibis::bitvector::pit::init(const ibis::bitvector &bv) {
    iset = bv.firstIndexSet();
    curr = 0;
} // ibis::bitvector::pit::init

/// Skip over next k positive positions.  It is equivalent to call the
/// function next k times.
void ibis::bitvector::pit::skip(unsigned k) {
    while (k > 0) {
        if (k+curr < iset.nIndices()) {
            // stop in the middle of a run
            curr += k;
            k = 0;
        }
        else if (iset.nIndices() > 0) {
            // skip over this run
            k = iset.nIndices() - curr;
            curr = 0;
            ++ iset;
        }
        else {
            // reached the end of the bitvector
            k = 0;
        }
    }
} // ibis::bitvector::pit::next

/// Move on to the next bit that is 1.
void ibis::bitvector::pit::next() {
    ++ curr;
    if (curr >= iset.nIndices()) {
        ++ iset;
        curr = 0;
    }
} // ibis::bitvector::pit::next

inline ibis::bitvector::indexSet ibis::bitvector::firstIndexSet() const {
    indexSet is;
    if (m_vec.end() > m_vec.begin()) {
	is.it = m_vec.begin() - 1;
	is.end = m_vec.end();
    }
    else {
	is.it = 0;
	is.end = 0;
    }
    is.active = &active;
    is.ind[0] = static_cast<word_t>(-1);
    is.nind = 0;
    ++is;
    return is;
} // ibis::bitvector::firstIndexSet;

/// Compute the expected number of bytes required to store a random
/// sequence.   The random bit sequence is to have @c nb total bits
/// and @c nc bits of one.
inline double ibis::bitvector::randomSize(word_t nb, word_t nc) {
    double sz = 0.0;
    if (nb > 0 && nb >= nc && nb > MAXBITS) {
	const double den = static_cast<double>(nc) /
	    static_cast<double>(nb);
	const word_t nw = nb / MAXBITS;
	sz = 3.0 + nw - nw * (pow(1.0-den, static_cast<int>(2*MAXBITS))
			      + pow(den, static_cast<int>(2*MAXBITS)));
    }
    return sz*sizeof(word_t);
} // ibis::bitvector::randomSize

/// Compute the expected size (number of bytes) of a random sequence
/// generated from a Markov process.  The bit sequence is to have @c nb
/// total bits, @c nc bits of one, and @c f consecutive ones on the
/// average.  The argument @c f is known as the clustering factor.
inline double ibis::bitvector::markovSize(word_t nb, word_t nc, double f) {
    double sz = 0.0;
    if (nb > 0 && nb >= nc && nb > MAXBITS) {
	const double den = static_cast<double>(nc) /
	    static_cast<double>(nb);
	const word_t nw = nb / MAXBITS;
	if ((den <= 0.5 && f > 1.00001) || (den > 0.5 && (1.0-den)*f > den))
	    sz = ((1.0-den) * pow(1.0-den/((1.0-den)*f),
				  static_cast<int>(2*MAXBITS-3)) +
		  den * pow(1.0-1.0/f, static_cast<int>(2*MAXBITS-3)));
	else
	    sz = (pow(1.0-den, static_cast<int>(2*MAXBITS)) +
		  pow(den, static_cast<int>(2*MAXBITS)));
	sz = 3.0 + nw * (1.0 - sz);
    }
    return sz*sizeof(word_t);
} // ibis::bitvector::markovSize

/// Turn on a single bit in a uncompressed bitvector.
///
/// @warning Use only if you are sure that the bitvector object represents
/// a uncompressed bitmap!
inline void ibis::bitvector::turnOnRawBit(const word_t ind) {
    if (ind < nbits) { // in regular words
	m_vec[ind / MAXBITS] |= (1 << (SECONDBIT - (ind % MAXBITS)));
	nset = 0;  // don't track nset
    }
    else { // assume to be in the active word
	active.val |= (1 << (active.nbits - (ind - nbits) - 1));
#if defined(DEBUG)
	if (ind >= nbits + active.nbits ||
	    active.val >= (1U << active.nbits)) {
	    LOGGER(ibis::gVerbose >= 0)
		<< "bitvector::turnOnRawBit(" << ind
		<< ") found a bad active word";
	}
#endif
    }
} // ibis::bitvector::turnOnRawBit

std::ostream& operator<<(std::ostream&, const ibis::bitvector&);
#endif // __BITVECTOR_H
