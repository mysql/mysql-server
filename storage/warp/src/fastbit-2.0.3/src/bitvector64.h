// $Id$
//	Author: John Wu <John.Wu at ACM.org>
//              Lawrence Berkeley National Laboratory
//	Copyright (c) 2000-2016 the Regents of the University of California
#ifndef BITVECTOR64_H
#define BITVECTOR64_H
///@file
/// Definition of 64-bit version of the Word-Aligned Hybrid code.

#include "array_t.h"	// alternative to std::vector

#include <stdio.h>	// sprintf


/**
  @brief A data structure to represent a sequence of bits. The 64-bit
  version.

Key features

 A bitvector object stores a sequence of bits and provides fast bitwise
 logical operations.  In addition, it supports operations to append new
 bits from the end, read bits at arbitrary location and set bits at
 arbitrary location.  It also supports an iterator, a const_iterator and an
 indexSet.

Encoding format

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
    bitvector::word_t.  This ensure that a fill word can store a fill of
    any valid length without performing a bound check.  In this 64-bit
    version, the maximum number of bits that can
    be represented by a bitvector object is 16 quintillion (16x10^{18}).

<li> When adding a bit with bitvector::operator+=, the integer value passed
    in must be one of 0 or 1.  Since checking whether the import value is 0
    or not 0 causes pipeline bubble in CPU, we have opted for not performing
    the check.  An input value other than 0 or 1 will cause existing bits
    to be modified in unpredictable ways.
</ul>
*/
class ibis::bitvector64 {
public:
    typedef uint64_t word_t;///!< The basic unit of data storage is 64-bit.

    // constructors of bitvector64 class
    bitvector64() : nbits(0), nset(0), active(), m_vec() {};
    ~bitvector64() {clear();};
    bitvector64(const bitvector64& bv) : nbits(bv.nbits), nset(bv.nset),
	active(bv.active), m_vec(bv.m_vec) {};
    bitvector64(const array_t<word_t>& arr);
    bitvector64(const char* file); ///!< Read the content of the named file.
    inline bitvector64& operator=(const bitvector64& bv); ///!<@note Deep copy.
    inline bitvector64& copy(const bitvector64& bv);      ///!<@note Deep copy.
    inline bitvector64& swap(bitvector64& bv);
    // use bv to replace part of the existing value, match the ith bit with
    // the first of bv, return reference to self
    //bitvector64& copy(const word_t i, const bitvector64& bv);
    /// Replace a single bit at position @c i.
    ///@note @c val must be either 0 or 1.
    void setBit(const word_t i, int val);
    int  getBit(const word_t i) const;
    /// Remove the bits in the range of [i, j).
    void erase(word_t i, word_t j);

    /// Create a vector with @c n bits of value @c val (cf. memset()).
    ///@note @c val must be either 0 or 1.
    void set(int val, word_t n);
    /// Remove the existing content of a bitvector64.
    void clear() {nbits = 0; nset = 0; active.reset(); m_vec.clear();}

    bitvector64& operator+=(const bitvector64& bv); ///!< Append a bitvector64.
    inline bitvector64& operator+=(int b);	///!< Append a single bit.
    inline void appendByte(unsigned char);
    void appendWord(word_t w);			///!< Append a WAH word.
    /// Append @c n bits of @c val.
    inline void appendFill(int val, word_t n);

    /// Return 1 if two bit sequences have the same content, 0 otherwise
    int operator==(const bitvector64& rhs) const;

    /// Complement all bits of a bit sequence.
    void flip();
    ///@brief Perform bitwise AND between this bitvector64 and @c rhs.
    void operator&=(const bitvector64& rhs);
    ///@brief Perform bitwise AND, return the pointer to the result.
    bitvector64* operator&(const bitvector64&) const;
    ///@brief Perform bitwise OR.
    void operator|=(const bitvector64& rhs);
    ///@brief Perform bitwise OR, return the pointer to the result.
    bitvector64* operator|(const bitvector64&) const;
    ///@brief Perform bitwise exclusive or (XOR).
    void operator^=(const bitvector64& rhs);
    ///@brief Perform bitwise XOR, return the pointer to the result.
    bitvector64* operator^(const bitvector64&) const;
    ///@brief Perform bitwise subtraction (a & !b).
    void operator-=(const bitvector64& rhs);
    ///@brief Perform bitwise subtraction and return the pointer to the
    /// result.
    bitvector64* operator-(const bitvector64&) const;

    // I/O functions.
    /// Read a bit vector from the file.  Purge current contents before
    /// read.
    void read(const char *fn);
    /// Write the bit vector to a file.
    void write(const char *fn) const;
    void write(FILE* fptr) const;
    /// Write the bit vector to an array_t<word_t>.
    void write(array_t<word_t>& arr) const;

    void compress();	///!< Merge fills into fill words.
    void decompress();	///!< Turn all fill words into literal words.
    /// Return the number of word saved if the function compress is called.
    word_t compressible() const;
    /// Does this bit vector use less space than the maximum? Return true
    /// if yes, otherwise false.
    bool isCompressed() const {return (m_vec.size()*MAXBITS < nbits);}

    /// Return the number of bits that are one.
    word_t cnt() const {
	if (nset==0) do_cnt(); return (nset+cnt_ones(active.val));
    };

    /// Return the total number of bits in the bit sequence.
    word_t size() const throw() {return (nbits+active.nbits);};
    /// Return the number of fill words.
    inline word_t numFillWords() const;
    /// Return the number of bytes used by the bitvector object in memory.
    word_t bytes() const throw() {
	return (m_vec.size()*sizeof(word_t) + sizeof(bitvector64));
    };
    /// Compute the number of words in serialized version of the bitvector
    /// object.
    word_t getSerialSize() const throw() {
	return (m_vec.size() + 1 + (active.nbits>0));
    };
    /// Return the number of bits in a literal word.
    static unsigned bitsPerLiteral() {return MAXBITS;}
    /// Compute the expected number of bytes required to store a random
    /// sequence.   The random bit sequence is to have @c nb total bits
    /// and @c nc bits of one.
    inline static double randomSize(word_t nb, word_t nc);
    /// Compute the expected size (bytes) of a random sequence generated
    /// from a Markov process.  The bit sequence is to have @c nb total
    /// bits, @c nc bits of one, and @c f consecutive ones on the average.
    /// The argument @c f is known as the clustering factor.
    inline static double markovSize(word_t nb, word_t nc, double f);
    /// Estimate clustering factor based on the size.
    ///@sa markovSize.
    static double clusteringFactor(word_t nb, word_t nc, word_t nw);

    /// Adjust the size of the bit sequence.  If current size is less than
    /// @c nv, append enough 1 bits so that it has @c nv bits.  If the
    /// resulting total number of bits is less than @c nt, append 0 bits
    /// so that there are @c nt total bits.  The final result always
    /// contains @c nt bits.
    void adjustSize(word_t nv, word_t nt);
    std::ostream& print(std::ostream &) const; ///!< The print function

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
	void append(int b) { // append a single bit
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
		if (isFill) {
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

    // member variables of bitvector64 class
    word_t nbits;	///!< Number of bits in @c m_vec.
    mutable word_t nset;///!< Number of bits that are 1 in @c m_vec.
    active_word active;	///!< The active word.
    array_t<word_t> m_vec;	///!< Store whole words.

    // private functions of bitvector64 class
    // The following three functions all performs or operation, _c2 and _c1
    // generate compressed solutions, but _c0, _d1, and _d2 generate
    // uncompressed solutions.
    // or_c2 assumes there are compressed word in both operands
    // or_c1 *this may contain compressed word, but not rhs
    // or_c0 assumes both operands are not compressed
    // or_d1 *this contains no compressed word and is overwritten with the
    //       result
    // or_d2 both *this and rhs are compressed, but res will not be compressed
    void or_c2(const bitvector64& rhs, bitvector64& res) const;
    void or_c1(const bitvector64& rhs, bitvector64& res) const;
    void or_c0(const bitvector64& rhs);
    void or_d1(const bitvector64& rhs);
    void or_d2(const bitvector64& rhs, bitvector64& res) const;
    void and_c2(const bitvector64& rhs, bitvector64& res) const;
    void and_c1(const bitvector64& rhs, bitvector64& res) const;
    void and_c0(const bitvector64& rhs);
    void and_d1(const bitvector64& rhs);
    void and_d2(const bitvector64& rhs, bitvector64& res) const;
    void xor_c2(const bitvector64& rhs, bitvector64& res) const;
    void xor_c1(const bitvector64& rhs, bitvector64& res) const;
    void xor_c0(const bitvector64& rhs);
    void xor_d1(const bitvector64& rhs);
    void xor_d2(const bitvector64& rhs, bitvector64& res) const;
    void minus_c2(const bitvector64& rhs, bitvector64& res) const;
    void minus_c1(const bitvector64& rhs, bitvector64& res) const;
    void minus_c1x(const bitvector64& rhs, bitvector64& res) const;
    void minus_c0(const bitvector64& rhs);
    void minus_d1(const bitvector64& rhs);
    void minus_d2(const bitvector64& rhs, bitvector64& res) const;
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
    inline unsigned cnt_ones(word_t) const; // number of 1s in a literal word
    inline word_t cnt_bits(word_t) const; // number of bits in a word
    word_t do_cnt() const; // count the number of bits and number of ones
}; // end class bitvector64

/// @brief The indexSet stores positions of bits that are one.
///
/// It decodes one word of the bitvector64 as a time.  For a fill of ones,
/// the function @c isRange returns true, otherwise it returns false.  If
/// isRange returns true, the position of the first bit is pointed by the
/// pointer returned by function @c indices, and there are @c nIndices
/// consecutive ones.  If @c isRange returns false, there are @c nIndices
/// bits that are one and the positions of these bits are stored in the
/// array returned by function @c indices.
class ibis::bitvector64::indexSet {
public:
    // let the compiler define most of the canonical functions

    // allow bitvector64::firstIndexSet() to access private variables of this
    // class
    friend indexSet ibis::bitvector64::firstIndexSet() const;

    //int operator!=(const indexSet& rhs) const {return (it != rhs.it);};
    bool isRange() const {return (nind>=ibis::bitvector64::MAXBITS);}
    const word_t* indices() const {return ind;};
    word_t nIndices() const {return nind;}
    indexSet& operator++();

private:
    array_t<word_t>::const_iterator it;
    array_t<word_t>::const_iterator end;
    const active_word* active; // points back to the active word
    word_t nind; // number of indices
    word_t ind[64];
}; // class ibis::bitvector64::indexSet

/// The const_iterator class.  It iterates on the individual bits.
class ibis::bitvector64::const_iterator {
public:
    inline bool operator*() const;
    inline int operator!=(const const_iterator& rhs) const throw ();
    inline int operator==(const const_iterator& rhs) const throw ();

    inline const_iterator& operator++();
    inline const_iterator& operator--();
    const_iterator& operator+=(int64_t incr);

private:
    ibis::bitvector64::word_t	compressed;
    ibis::bitvector64::word_t	ind;
    ibis::bitvector64::word_t	nbits;
    ibis::bitvector64::word_t	literalvalue;
    int				fillbit;
    const active_word*		active;
    array_t<word_t>::const_iterator end;
    array_t<word_t>::const_iterator begin;
    array_t<word_t>::const_iterator it;

    void decodeWord();

    // give three functions of bitvector64 access to private variables
    friend void ibis::bitvector64::erase(word_t i, word_t j);
    friend const_iterator ibis::bitvector64::begin() const;
    friend const_iterator ibis::bitvector64::end() const;
    friend class ibis::bitvector64::iterator;
}; // end class ibis::bitvector64::const_iterator

/// The iterator that allows modification of bits.  It provides only one
/// additional function (operator=) than const_iterator to allow
/// modification of the bit pointed.
/// ********************IMPORTANT********************
/// operator= modifies the content of the bitvector64 it points to and it can
/// invalidate other iterator or const_iterator referring to the same
/// bitvector64.
class ibis::bitvector64::iterator {
public:
    inline bool operator*() const; // still can not modify this
    inline int operator!=(const const_iterator& rhs) const throw ();
    inline int operator==(const const_iterator& rhs) const throw ();
    inline int operator!=(const iterator& rhs) const throw ();
    inline int operator==(const iterator& rhs) const throw ();

    inline iterator& operator++();
    inline iterator& operator--();
    iterator& operator+=(int64_t incr);
    iterator& operator=(int val);

private:
    ibis::bitvector64::word_t	compressed;
    ibis::bitvector64::word_t	ind;
    ibis::bitvector64::word_t	nbits;
    ibis::bitvector64::word_t	literalvalue;
    int				fillbit;
    ibis::bitvector64*		bitv;
    active_word*		active;
    array_t<word_t>*		vec;
    array_t<word_t>::iterator	it;

    void decodeWord();

    friend iterator ibis::bitvector64::begin();
    friend iterator ibis::bitvector64::end();
}; // end class ibis::bitvector64::iterator

/// Are all bits in regular words 0?
inline bool ibis::bitvector64::all0s() const {
    if (m_vec.empty()) {
	return true;
    }
    else if (m_vec.size() == 1) {
	return (m_vec[0] == 0 || (m_vec[0] >= HEADER0 && m_vec[0] < HEADER1));
    }
    else {
	return false;
    }
} // ibis::bitvector64::all0s

/// Are all bits in regular words 1?
inline bool ibis::bitvector64::all1s() const {
    if (m_vec.size() == 1) {
	return (m_vec[0] == ALLONES || (m_vec[0] > HEADER1));
    }
    else {
	return false;
    }
} // ibis::bitvector64::all1s()

/// Compute the number of bits represented by a word.
inline ibis::bitvector64::word_t
ibis::bitvector64::cnt_bits(ibis::bitvector64::word_t val) const {
    return  ((val>ALLONES) ? ((val&MAXCNT)*MAXBITS) : MAXBITS);
}

/// Compute the number of ones in a literal word.
inline unsigned 
ibis::bitvector64::cnt_ones(ibis::bitvector64::word_t val) const {
    // number of 1 bits in a value between 0 and 255
    static const unsigned table[256] = {
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
    //assert(8 == sizeof(word_t));
    return table[val&0xFF] + table[(val>>8)&0xFF] +
	table[(val>>16)&0xFF] + table[(val>>24)&0xFF] +
	table[(val>>32)&0xFF] + table[(val>>40)&0xFF] +
	table[(val>>48)&0xFF] + table[val>>56];
} // inline unsigned ibis::bitvector64::cnt_ones(word_t) const

inline ibis::bitvector64::word_t
ibis::bitvector64::numFillWords() const {
    word_t cnt=0;
    array_t<word_t>::const_iterator it = m_vec.begin();
    while (it != m_vec.end()) {
	cnt += (*it >> ibis::bitvector64::MAXBITS);
	it++;
    }
    return cnt;
} // ibis::bitvector64::numFillWords

// The assignment operator.  Wanted to use shallow copy for efficiency
// consideration, but SHALLOW copy causes unexpected problem in test
// program bitty.cpp change to deep copy
inline ibis::bitvector64&
ibis::bitvector64::operator=(const ibis::bitvector64& bv) {
    nbits = bv.nbits; nset = bv.nset; active = bv.active;
    m_vec.deepCopy(bv.m_vec);
    return *this;
}

// deep copy
inline ibis::bitvector64& 
ibis::bitvector64::copy(const ibis::bitvector64& bv) {
    nbits = bv.nbits; nset = bv.nset; active = bv.active;
    m_vec.deepCopy(bv.m_vec);
    return *this;
}

inline ibis::bitvector64&
ibis::bitvector64::swap(bitvector64& bv) {
    word_t tmp;
    tmp = bv.nbits; bv.nbits = nbits; nbits = tmp;
    tmp = bv.nset; bv.nset = nset; nset = tmp;
    active_word atmp = bv.active;
    bv.active = active; active = atmp;
    m_vec.swap(bv.m_vec);
    return *this;
}

// append_active assumes that nbits == MAXBITS and refers to MAXBITS instead
// of nbits
inline void ibis::bitvector64::append_active() {
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
	    ++m_vec.back();
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
} // void ibis::bitvector64::append_active()

/// Append a counter.  A private function to append a single counter when
/// the active word is empty cnt is greater than 0.
inline void ibis::bitvector64::append_counter(int val, word_t cnt) {
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
} // ibis::bitvector64::append_counter

/// Append a single bit
inline ibis::bitvector64& ibis::bitvector64::operator+=(int b) {
    active.append(b);
    if (active.is_full()) append_active();
    return *this;
} // ibis::bitvector64& ibis::bitvector64::operator+=(int b)

/// Append all 8 bits of the incoming bytes as literal bits.
void ibis::bitvector64::appendByte(unsigned char c) {
    if (active.nbits >= MAXBITS)
        append_active();

    if (active.nbits+8 < MAXBITS) {
        active.val <<= 8;
        active.nbits += 8;
        active.val += c;
    }
    else if (active.nbits+8 > MAXBITS) {
        unsigned na = MAXBITS - nbits;
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
} // ibis::bitvector64::appendByte

/// Append n bits of val.  The value of n may be arbitrary integer, but the
/// value of val must be either 0 or 1.
inline void ibis::bitvector64::appendFill
(int val, ibis::bitvector64::word_t n) {
    if (active.nbits > 0) {
	word_t tmp = (MAXBITS - active.nbits);
	if (tmp > n) tmp = n;
	active.nbits += tmp;
	active.val <<= tmp;
	n -= tmp;
	if (val)
	    active.val |= (static_cast<word_t>(1)<<tmp) - 1;
	if (active.nbits >= MAXBITS) append_active();
    }
    if (n >= MAXBITS) {
	word_t cnt = n / MAXBITS;
	if (cnt > 1)
	    append_counter(val, cnt);
	else if (val) {
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
	active.val = val*((static_cast<word_t>(1)<<n)-1);
    }
} // ibis::bitvector64::appendFill

// append nw words starting from 'it' to the current bit vector -- assume
// active is empty
inline void ibis::bitvector64::copy_runs(run& it, word_t& nw) {
    // deal with the first word -- need to attach it to the last word in m_vec
    if (it.isFill) {
        if (it.nWords > 1) {
            append_counter(it.fillBit, it.nWords);
            nw -= it.nWords;
        }
        else if (it.nWords == 1) {
            active.val = (it.fillBit != 0 ? ALLONES : 0);
            append_active();
            -- nw;
        }
    }
    else {
	active.val = *(it.it);
	append_active();
	-- nw;
    }
    ++ it.it; // advance to the next word
    //if (nw == 0) {it.nWords = 0; return;}
    it.decode();
    nset = 0;
    nbits += MAXBITS * nw;
    while (nw >= it.nWords && nw > 0) { // only need to copy the word
	m_vec.push_back(*(it.it));
	nw -= it.nWords;
	++ it.it; // advance to the next word
	it.decode();
    }
    nbits -= MAXBITS * nw;
} // ibis::bitvector64::copy_runs

// append nw words starting from it to the current bit vector -- assume
// active is empty
inline void ibis::bitvector64::copy_runsn(run& it, word_t& nw) {
    // deal with the first word -- need to attach it to the last word in m_vec
    if (it.isFill) {
        if (it.nWords > 1) {
            append_counter(!it.fillBit, it.nWords);
            nw -= it.nWords;
        }
        else if (it.nWords == 1) {
            active.val = (it.fillBit != 0 ? 0 : ALLONES);
            append_active();
            -- nw;
        }
    }
    else {
	active.val = ALLONES ^ *(it.it);
	append_active();
	-- nw;
    }
    ++ it.it; // advance to the next word
    //if (nw == 0) {it.nWords = 0; return;}
    it.decode();
    nset = 0;
    nbits += MAXBITS * nw;
    while (nw >= it.nWords && nw > 0) { // only need to copy the word
	m_vec.push_back((it.isFill?FILLBIT:ALLONES) ^ *(it.it));
	nw -= it.nWords;
	++ it.it; // advance to the next word
	it.decode();
    }
    nbits -= MAXBITS * nw;
} // ibis::bitvector64::copy_runsn

// copy the fill in "run it" as literal words
inline void ibis::bitvector64::copy_fill
(array_t<ibis::bitvector64::word_t>::iterator& jt, run& it) {
    const array_t<word_t>::iterator iend = jt + it.nWords;
    if (it.fillBit == 0) {
	switch (it.nWords) {
	case 0: break;
	case 1:
	    *jt = 0; ++jt; break;
	case 2:
	    *jt = 0; ++jt; *jt = 0; ++jt; break;
	case 3:
	    *jt = 0; ++jt; *jt = 0; ++jt; *jt = 0; ++jt; break;
	default:
	    while (jt < iend) {
		*jt = 0;
		++ jt;
	    }
	    break;
	}
    }
    else {
	switch (it.nWords) {
	case 0: break;
	case 1:
	    *jt = ALLONES; ++jt; break;
	case 2:
	    *jt = ALLONES; ++jt; *jt = ALLONES; ++jt; break;
	case 3:
	    *jt = ALLONES; ++jt; *jt = ALLONES; ++jt; *jt = ALLONES; ++jt;
	    break;
	default:
	    while (jt < iend) {
		*jt = ALLONES;
		++ jt;
	    }
	    break;
	}
    }
    it.nWords = 0;
    ++ it.it;
} // ibis::bitvector64::copy_fill

// copy the next nw words (nw X MAXBITS bits) starting with run it
// to an array_t as uncompressed words
inline void ibis::bitvector64::copy_runs
(array_t<ibis::bitvector64::word_t>::iterator& jt, run& it, word_t& nw) {
    while (nw >= it.nWords && nw > 0) { // only need to copy the word
	if (it.isFill) {
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
	else {
	    *jt = *(it.it);
	    ++ jt;
	    -- nw;
	}
	++ it.it; // advance to the next word
	it.decode();
    }
} // ibis::bitvector64::copy_runs

// copy the complements of the next nw words (nw X MAXBITS bits)
// starting with "run it" as uncompressed words
inline void ibis::bitvector64::copy_runsn
(array_t<ibis::bitvector64::word_t>::iterator& jt, run& it, word_t& nw) {
    while (nw >= it.nWords && nw > 0) { // only need to copy the word
	if (it.isFill) {
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
	else {
	    *jt = *(it.it) ^ ALLONES;
	    ++ jt;
	    -- nw;
	}
	++ it.it; // advance to the next word
	it.decode();
    }
} // ibis::bitvector64::copy_runsn

inline ibis::bitvector64::iterator ibis::bitvector64::begin() {
    iterator it;
    it.it     = m_vec.begin();
    it.vec    = &m_vec;
    it.bitv   = this;
    it.active = &active;
    it.decodeWord();
    return it;
} // ibis::bitvector64::begin()

inline ibis::bitvector64::iterator ibis::bitvector64::end() {
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
} // ibis::bitvector64::end()

inline ibis::bitvector64::const_iterator ibis::bitvector64::begin() const {
    const_iterator it;
    it.it     = m_vec.begin();
    it.begin  = m_vec.begin();
    it.end    = m_vec.end();
    it.active = &active;
    it.decodeWord();
    return it;
} // ibis::bitvector64::begin()

// dereference -- no error checking
inline bool ibis::bitvector64::iterator::operator*() const {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (vec==0 || it<vec->begin() ||  it>vec->end())
	throw "bitvector64::const_iterator not initialized correctly.";
#endif
    if (compressed != 0)
	return (fillbit != 0);
    else
	return ((word_t)1 & (literalvalue >> (bitvector64::SECONDBIT - ind)));
}

// comparison only based on the iterator
inline int ibis::bitvector64::iterator::operator!=
(const ibis::bitvector64::const_iterator& rhs) const throw () {
    return (it != rhs.it);
}
inline int ibis::bitvector64::iterator::operator==
(const ibis::bitvector64::const_iterator& rhs) const throw () {
    return (it == rhs.it);
}
inline int ibis::bitvector64::iterator::operator!=
(const ibis::bitvector64::iterator& rhs) const throw () {
    return (it != rhs.it);
}
inline int ibis::bitvector64::iterator::operator==
(const ibis::bitvector64::iterator& rhs) const throw () {
    return (it == rhs.it);
}

// increment by one
inline ibis::bitvector64::iterator& ibis::bitvector64::iterator::operator++() {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (vec==0 || it<vec->begin() || it>vec->end())
	throw "bitvector64::iterator not formed correctly.";
#endif
    if (ind+1<nbits) {++ind;}
    else {++ it; decodeWord();}
    return *this;
}

// decrement by one
inline ibis::bitvector64::iterator& ibis::bitvector64::iterator::operator--() {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (vec==0 || it<vec->begin() || it>vec->end()+1)
	throw "bitvector64::iterator not formed correctly.";
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

inline ibis::bitvector64::const_iterator ibis::bitvector64::end() const {
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
} // ibis::bitvector64::end()

// dereference -- no error checking
inline bool ibis::bitvector64::const_iterator::operator*() const {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (it==0 || end==0 || it>end || nbits<=ind)
	throw "bitvector64::const_iterator not initialized correctly.";
#endif
    if (compressed != 0)
	return (fillbit != 0);
    else
	return ((word_t)1 & (literalvalue >> (bitvector64::SECONDBIT - ind)));
}

// comparison only based on the iterator
inline int ibis::bitvector64::const_iterator::operator!=
(const ibis::bitvector64::const_iterator& rhs) const throw (){
    return (it != rhs.it);
}
inline int ibis::bitvector64::const_iterator::operator==
(const ibis::bitvector64::const_iterator& rhs) const throw () {
    return (it == rhs.it);
}

// increment by one
inline ibis::bitvector64::const_iterator&
ibis::bitvector64::const_iterator::operator++() {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (it==0 || end==0 || it>end)
	throw "bitvector64::const_iterator not formed correctly.";
#endif
    if (ind+1<nbits) {++ind;}
    else {++ it; decodeWord();}
    return *this;
}

// decrement by one
inline ibis::bitvector64::const_iterator&
ibis::bitvector64::const_iterator::operator--() {
#if defined(DEBUG) && DEBUG + 0 > 1
    if (it==0 || end==0 || it>end)
	throw "bitvector64::const_iterator not formed correctly.";
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

inline ibis::bitvector64::indexSet ibis::bitvector64::firstIndexSet() const {
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
} // end ibis::bitvector64::firstIndexSet;

inline double ibis::bitvector64::randomSize(word_t nb, word_t nc) {
    double sz = 0.0;
    if (nb > 0 && nb >= nc) {
	const double den = static_cast<double>(nc) /
	    static_cast<double>(nb);
	const word_t nw = (nb > SECONDBIT ? nb / SECONDBIT - 1 : 0);
	sz = 3.0 + nw - nw *
	    (pow(1.0-den, static_cast<int>(2*SECONDBIT)) +
	     pow(den, static_cast<int>(2*SECONDBIT)));
    }
    return sz*sizeof(word_t);
} // end ibis::bitvector64::randomSize

inline double
ibis::bitvector64::markovSize(word_t nb, word_t nc, double f) {
    double sz = 0.0;
    if (nb > 0 && nb >= nc) {
	const double den = static_cast<double>(nc) /
	    static_cast<double>(nb);
	const word_t nw = (nb > SECONDBIT ? nb / SECONDBIT - 1 : 0);
	if ((den <= 0.5 && f > 1.0) || (den > 0.5 && (1.0-den)*f > den))
	    sz = ((1.0-den) * pow(1.0-den/((1.0-den)*f),
				  static_cast<int>(2*MAXBITS-3)) +
		  den * pow(1.0-1.0/f, static_cast<int>(2*MAXBITS-3)));
	else
	    sz = (pow(1.0-den, static_cast<int>(2*SECONDBIT)) +
		  pow(den, static_cast<int>(2*SECONDBIT)));
	sz = 3.0 + nw * (1.0 - sz);
    }
    return sz*sizeof(word_t);
} // end ibis::bitvector64::markovSize

std::ostream& operator<<(std::ostream&, const ibis::bitvector64&);
#endif // __BITVECTOR64_H
