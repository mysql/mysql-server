// $Id$
// Author: John Wu <John.Wu at ACM.org> Lawrence Berkeley National Laboratory
// Copyright (c) 2000-2016 the Regents of the University of California
//
// The implementation of class bitvector64 as defined in bitvector64.h
// The major goal of this implementation is to avoid accessing anything
// smaller than a word (unsigned int)
//
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(disable:4786)   // some identifier longer than 256 characters
#endif
#include "bitvector64.h"
#include "bitvector.h"
#include <iomanip>      // setw

// constances defined in bitvector64
const unsigned ibis::bitvector64::MAXBITS = 63;
const unsigned ibis::bitvector64::SECONDBIT = 62;
const ibis::bitvector64::word_t ibis::bitvector64::ALLONES =
0x7FFFFFFFFFFFFFFFLL;
const ibis::bitvector64::word_t ibis::bitvector64::MAXCNT =
0x3FFFFFFFFFFFFFFFLL;
const ibis::bitvector64::word_t ibis::bitvector64::FILLBIT =
((ibis::bitvector64::word_t)1 << 62);
const ibis::bitvector64::word_t ibis::bitvector64::HEADER0 =
((ibis::bitvector64::word_t)2 << 62);
const ibis::bitvector64::word_t ibis::bitvector64::HEADER1 =
((ibis::bitvector64::word_t)3 << 62);

// construct a bitvector64 from an array
ibis::bitvector64::bitvector64(const array_t<ibis::bitvector64::word_t>& arr)
    : m_vec(arr) {
    if (m_vec.size() > 1) { // non-trivial size
        if (m_vec.back()) { // has active bits
            if (m_vec.back() < MAXBITS) {
                active.nbits = m_vec.back();
                m_vec.pop_back();
                active.val = m_vec.back();
            }
            else {
                ibis::util::logMessage
                    ("Error", "the serialized version of bitvector contains "
                     "an expected last word (%lu)",
                     static_cast<long unsigned>(m_vec.back()));
                throw "bitvector constructor failure -- the input "
                    "is not a serialized bitvector" IBIS_FILE_LINE;
            }
        }
        else {
            active.reset();
        }
        m_vec.pop_back();
#ifndef FASTBIT_LAZY_INIT
        nbits = do_cnt(); // count the number of bits
#else
        nbits = 0;
        nset = 0;
#endif
    }
    else { // can only be an empty bitvector
        clear();
    }
} // ctor from array_t

ibis::bitvector64::bitvector64(const char* file) : nbits(0), nset(0) {
    try {read(file);} catch(...) {/*return empty bitvector64*/}
} // ctor from file

// set a bitvector64 to contain n bits of val
void ibis::bitvector64::set(int val, word_t n) {
    clear(); // clear the current content
    word_t k = n / MAXBITS;

    if (k > 1) {
        append_counter(val, k);
    }
    else if (k == 1) {
        if (val != 0) active.val = ALLONES;
        else active.val = 0;
        append_active();
    }

    // put the left over bits into active
    active.nbits = n - k * MAXBITS;
    if (val != 0) {
        active.val = (1LL<<active.nbits) - 1;
        nset = k * MAXBITS;
    }
} // ibis::bitvector64::set

/// Append a WAH compressed word.  The general case, active word may not be
/// empty.
void ibis::bitvector64::appendWord(word_t w) {
    word_t nb1, nb2;
    int64_t cps = (w>>MAXBITS);
    nset = 0;
    if (active.nbits) { // active contains some uncompressed bits
        word_t w1;
        nb1 = active.nbits;
        nb2 = MAXBITS - active.nbits;
        active.val <<= nb2;
        if (cps != 0) { // incoming bits are comporessed
            int b2 = (w>=HEADER1);
            if (b2 != 0) {
                w1 = (1LL<<nb2)-1;
                active.val |= w1;
            }
            append_active();
            nb2 = (w & MAXCNT) - 1;
            if (nb2 > 1) {              // append a counter
                append_counter(b2, nb2);
            }
            else if (nb2 == 1) {
                if (b2 != 0) active.val = ALLONES;
                append_active();
            }
            active.nbits = nb1;
            active.val = ((1LL << nb1) - 1)*b2;
        }
        else { // incoming bits are not compressed
            w1 = (w>>nb1);
            active.val |= w1;
            append_active();
            w1 = (1LL<<nb1)-1;
            active.val = (w & w1);
            active.nbits = nb1;
        }
    } // end of the case where there are active bits
    else if (cps != 0) { // no active bit
        int b2 = (w>=HEADER1);
        nb2 = (w & MAXCNT);
        if (nb2 > 1)
            append_counter(b2, nb2);
        else if (nb2 == 1) {
            if (b2) active.val = ALLONES;
            append_active();
        }
    }
    else { // no active bits
        // new word is a raw bit pattern, simply add the word
        active.val= w;
        append_active();
    }
} //bitvector64& ibis::bitvector64::appendWord

// append another bitvector64 to the current
ibis::bitvector64& ibis::bitvector64::operator+=(const ibis::bitvector64& bv) {
    if (nset>0 && bv.nset>0)
        nset += bv.nset;
    else
        nset = 0;
    word_t expbits = size() + bv.size();

    // append the words in bv.m_vec
    for (array_t<word_t>::const_iterator i=bv.m_vec.begin();
         i!=bv.m_vec.end(); i++) appendWord(*i);

    // append active bits of bv
    if (active.nbits) { // need to combine the two active bit sets
        if (active.nbits + bv.active.nbits < MAXBITS) {
            // two active words can fit into one
            active.val <<= bv.active.nbits;
            active.val |= bv.active.val;
            active.nbits += bv.active.nbits;
        }
        else { // two sets can not be fit into one word
            word_t nb1, w1;
            nb1 = (active.nbits + bv.active.nbits) - MAXBITS;
            active.val <<= (MAXBITS - active.nbits);
            w1 = (bv.active.val >> nb1);
            active.val |= w1;
            append_active();
            w1 <<= nb1;
            active.nbits = nb1;
            active.val = w1 ^ bv.active.val;
        }
    }
    else { // simply copy the active_word from bv the *this
        active = bv.active;
    }

    if (expbits != size())
        ibis::util::logMessage("Warning", "operator+= expected %lu bits in the "
                               "resulting bitvector64, but got %lu instead",
                               static_cast<long unsigned>(expbits),
                               static_cast<long unsigned>(size()));
    return *this;
} // ibis::bitvector64::operator+=

// compress the current m_vec -- don't try to find out whether it can be done
// or not
void ibis::bitvector64::compress() {
    if (m_vec.size() < 2) // there is nothing to do
        return;

    array_t<word_t> tmp;
    // expect about 38.2% compression ratio
    tmp.reserve(static_cast<uint32_t>(m_vec.size() * 0.382));

    array_t<word_t>::const_iterator i0 = m_vec.begin();
    tmp.push_back(*i0);
    for (i0++; i0!=m_vec.end(); i0++) {
        if (*i0 > ALLONES) { // a counter
            if ((*i0 & HEADER1) == (tmp.back() & HEADER1))
                tmp.back() += (*i0 & MAXCNT);
            else
                tmp.push_back(*i0);
        }
        else if (*i0 == 0) {
            if (tmp.back() == 0)
                tmp.back() = (HEADER0 | 2);
            else if (tmp.back() > HEADER0 && tmp.back() < HEADER1)
                ++tmp.back();
            else
                tmp.push_back(0);
        }
        else if (*i0 == ALLONES) {
            if (tmp.back() == ALLONES)
                tmp.back() = (HEADER1 | 2);
            else if (tmp.back() > HEADER1)
                ++tmp.back();
            else
                tmp.push_back(ALLONES);
        }
        else {
            tmp.push_back(*i0);
        }
    }

    if (m_vec.size() != tmp.size())
        m_vec.swap(tmp);  // take on the new vector
} // ibis::bitvector64::compress

// decompress the current compressed bitvector64
void ibis::bitvector64::decompress() {
    if (nbits == 0 && m_vec.size() > 0)
        nbits = do_cnt();
    if (m_vec.size()*MAXBITS == nbits) // already uncompressed
        return;

    array_t<word_t> tmp;
    try {
        tmp.resize(nbits/MAXBITS);
        if (nbits != tmp.size()*MAXBITS) {
            ibis::util::logMessage
                ("Warning", "bitvector64 nbits=%lu is not an "
                 "integer multiple of %lu", static_cast<long unsigned>(nbits),
                 static_cast<long unsigned>(MAXBITS));
            return;
        }
    }
    catch (const std::exception& e) {
        ibis::util::logMessage
            ("Warning", "bitvector64::decompress() failed to "
             "allocate %lu words to store decompressed "
             "bitvector64 -- exception \"%s\"",
             static_cast<long unsigned>(nbits/MAXBITS), e.what());
        return;
    }

    word_t j, cnt;
    array_t<word_t>::iterator it = tmp.begin();
    array_t<word_t>::const_iterator i0 = m_vec.begin();
    for (; i0!=m_vec.end(); i0++) {
        if ((*i0) > ALLONES) {
            cnt = (*i0 & MAXCNT);
            if ((*i0)>=HEADER1) {
                for (j=0; j<cnt; j++, it++)
                    *it = ALLONES;
            }
            else {
                for (j=0; j<cnt; j++, it++)
                    *it = 0;
            }
        }
        else {
            *it = *i0;
            it++;
        }
    }

    if (m_vec.size() != tmp.size())
        m_vec.swap(tmp);  // take on the new vector
} // ibis::bitvector64::decompress

// decompress the current content to an array_t<word_t>
void ibis::bitvector64::decompress(array_t<ibis::bitvector64::word_t>& tmp)
    const {
    word_t cnt = (nbits==0 && m_vec.size() > 0 ? do_cnt() : nbits) / MAXBITS;
    tmp.resize(cnt);

    array_t<word_t>::iterator it = tmp.begin();
    array_t<word_t>::const_iterator i0 = m_vec.begin();
    for (; i0!=m_vec.end(); i0++) {
        if ((*i0) > ALLONES) {
            cnt = (*i0 & MAXCNT);
            if ((*i0)>=HEADER1) {
                for (uint32_t j=0; j<cnt; j++, it++)
                    *it = ALLONES;
            }
            else {
                for (uint32_t j=0; j<cnt; j++, it++)
                    *it = 0;
            }
        }
        else {
            *it = *i0;
            it++;
        }
    }
} // ibis::bitvector64::decompress

// decompress the current content to an array_t<word_t> and complement every
// bit
void
ibis::bitvector64::copy_comp(array_t<ibis::bitvector64::word_t>& tmp) const {
    word_t cnt = (nbits == 0 && m_vec.size() > 0 ? do_cnt() : nbits) / MAXBITS;
    tmp.resize(cnt);

    array_t<word_t>::iterator it = tmp.begin();
    array_t<word_t>::const_iterator i0 = m_vec.begin();
    for (; i0!=m_vec.end(); i0++) {
        if ((*i0) > ALLONES) {
            cnt = (*i0 & MAXCNT);
            if ((*i0)>=HEADER1) {
                for (uint32_t j=0; j<cnt; j++, it++)
                    *it = 0;
            }
            else {
                for (uint32_t j=0; j<cnt; j++, it++)
                    *it = ALLONES;
            }
        }
        else {
            *it = ALLONES ^ *i0;
            it++;
        }
    }
} // ibis::bitvector64::copy_comp

// determine the number of word that would be eliminated by compression
ibis::bitvector64::word_t ibis::bitvector64::compressible() const {
    word_t cnt = 0;
    for (word_t i = 0; i+1 < m_vec.size(); ++ i) {
        cnt += ((m_vec[i] == m_vec[i+1]) &&
                ((m_vec[i] == static_cast<word_t>(0)) ||
                 (m_vec[i] == ALLONES)));
    }
    return cnt;
} // ibis::bitvector64::compressible

// return the number of bits counted and modify the member variable nset to
// the correct value
ibis::bitvector64::word_t ibis::bitvector64::do_cnt() const {
    nset = 0;
    ibis::bitvector64::word_t nb = 0;
    array_t<word_t>::const_iterator i;

    for (i = m_vec.begin(); i < m_vec.end(); ++i) {
        if ((*i) < HEADER0) {
            nb += MAXBITS;
            nset += cnt_ones(*i);
        }
        else {
            word_t tmp = (*i & MAXCNT) * MAXBITS;
            nb += tmp;
            nset += tmp * ((*i) >= HEADER1);
        }
    }
    return nb;
} // ibis::bitvector64::do_cnt

/// replace the ind'th bit with val.  val is assumed to be either 0 or 1.  If
/// val is not 0 or 1, it could cause serious problems.
/// This function can be used to extend the length of the bit sequence.
/// When the given index (ind) is beyond the end of the current sequence, the
/// unspecified bits in the range of [size(), ind) are assumed to be 0.
///
/// @warning This function is very expensive.  In order to get to bit ind,
/// it has to go through all bits 0 through ind-1.  In addition, it might
/// have to make a copy of all the bits following bit ind.  Use it only if
/// you have to.
void ibis::bitvector64::setBit(const word_t ind, int val) {
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    LOGGER(ibis::gVerbose >= 0)
        << "bitvector64::setBit(" << ind << ", " << val << ") "
        << "-- " << nbits << " bit(s) in m_vec and " << active.nbits
        << " bit(s) in the active word";
#endif
    m_vec.nosharing(); // make sure the array is not shared
    if (ind >= size()) {
        word_t diff = ind - size() + 1;
        if (active.nbits) {
            if (ind+1 >= nbits+MAXBITS) {
                diff -= MAXBITS - active.nbits;
                active.val <<= (MAXBITS - active.nbits);
                if (diff == 0)
                    active.val += (val!=0);
                append_active();
            }
            else {
                active.nbits += diff;
                active.val <<= diff;
                active.val += (val!=0);
                diff = 0;
            }
        }
        if (diff) {
            word_t w = diff / MAXBITS;
            diff -= w * MAXBITS;
            if (diff) {
                if (w > 1) {
                    append_counter(0, w);
                }
                else if (w) {
                    append_active();
                }
                active.nbits = diff;
                active.val += (val!=0);
            }
            else if (val != 0) {
                if (w > 2) {
                    append_counter(0, w-1);
                }
                else if (w == 2) {
                    append_active();
                }
                active.val = 1;
                append_active();
            }
            else {
                if (w > 1) {
                    append_counter(0, w);
                }
                else if (w) {
                    append_active();
                }
            }
        }
        if (size() != ind+1)
            ibis::util::logMessage("Warning", "bitvector64::setBit(%lu, %d) "
                                   "changed bitvector64 size() to %lu, but "
                                   "%lu was expected",
                                   static_cast<long unsigned>(ind), val,
                                   static_cast<long unsigned>(ind+1),
                                   static_cast<long unsigned>(size()));
        if (nset)
            nset += (val!=0);
        return;
    }
    else if (ind >= nbits) { // modify an active bit
        word_t u = active.val;
        if (val != 0) {
            active.val |= (1LL << (active.nbits - (ind - nbits) - 1));
        }
        else {
            active.val &= ~(1LL << (active.nbits - (ind - nbits) - 1));
        }
        if (nset && (u != active.val))
            nset += (val?1:-1);
        return;
    }
    else if (m_vec.size()*MAXBITS == nbits) { // uncompressed
        word_t i = ind / MAXBITS;
        word_t u = m_vec[i];
        word_t w = (1LL << (SECONDBIT - (ind % MAXBITS)));
        if (val != 0)
            m_vec[i] |= w;
        else
            m_vec[i] &= ~w;
        if (nset && (m_vec[i] != u))
            nset += (val?1:-1);
        return;
    }

    // normal case, compressed bit vector -- 
    // the bit to be modified is in m_vec
    array_t<word_t>::iterator it = m_vec.begin();
    word_t compressed = 0, cnt = 0, ind1 = 0, ind0 = ind;
    word_t current = 0; // current bit value
    while ((ind0>0) && (it<m_vec.end())) {
        if (*it >= HEADER0) { // a fill
            cnt = ((*it) & MAXCNT) * MAXBITS;
            if (cnt > ind0) { // found the location
                current = (*it >= HEADER1);
                compressed = 1;
                ind1 = ind0;
                ind0 = 0;
            }
            else {
                ind0 -= cnt;
                ind1 = ind0;
                ++it;
            }
        }
        else { // a literal word
            cnt = MAXBITS;
            if (MAXBITS > ind0) { // found the location
                current = (1 & ((*it) >> (SECONDBIT - ind0)));
                compressed = 0;
                ind1 = ind0;
                ind0 = 0;
            }
            else {
                ind0 -= MAXBITS;
                ind1 = ind0;
                ++it;
            }
        }
    } // while (ind...
    if (ind1 == 0) { // set current and compressed
        if (*it >= HEADER0) {
            cnt = ((*it) & MAXCNT) * MAXBITS;
            current = (*it >= HEADER1);
            compressed = 1;
        }
        else {
            cnt = MAXBITS;
            current = (*it >> SECONDBIT);
            compressed = 0;
        }
    }

    if (ind0>0) { // has not found the right location yet
        ibis::util::logMessage("Warning", "bitvector64::setBit(%lu, %d) passed "
                               "the end (%lu) of bit sequence while searching "
                               "for position %lu",
                               static_cast<long unsigned>(ind), val,
                               static_cast<long unsigned>(size()),
                               static_cast<long unsigned>(ind));
        if (ind0 < active.nbits) { // in the active word
            ind1 = (1LL << (active.nbits - ind0 - 1));
            if (val != 0) {
                active.val |= ind1;
            }
            else {
                active.val &= ~ind1;
            }
        }
        else { // extends the current bit vector
            ind1 = ind0 - active.nbits - 1;
            appendWord(HEADER0 | (ind1/MAXBITS));
            for (ind1%=MAXBITS; ind1>0; --ind1) operator+=(0);
            operator+=(val);
        }
        if (nset) nset += val?1:-1;
        return;
    }

    // locate the bit to be changed, lots of work hidden here
    if (current == static_cast<word_t>(val)) return; // nothing to do

    // need to actually modify the bit
    if (compressed == 0) { // toggle a single bit of a literal word
        *it ^= (1LL << (SECONDBIT - ind1));
    }
    else if (ind1 < MAXBITS) {
        // bit to be modified is in the first word, two pieces
        -- (*it);
        if ((*it & MAXCNT) == 1)
            *it = (current)?ALLONES:0;
        word_t w = 1LL << (SECONDBIT-ind1);
        if (val == 0) w ^= ALLONES;
        it = m_vec.insert(it, w);
    }
    else if (cnt - ind1 <= MAXBITS) {
        // bit to be modified is in the last word, two pieces
        -- (*it);
        if ((*it & MAXCNT) == 1)
            *it = (current)?ALLONES:0;
        word_t w = 1LL << (cnt-ind1-1);
        if (val == 0) w ^= ALLONES;
        ++it;
        it = m_vec.insert(it, w);
    }
    else { // the counter breaks into three pieces
        word_t u[2], w;
        u[0] = ind1 / MAXBITS;
        w = (*it & MAXCNT) - u[0] - 1;
        u[1] = 1LL << (SECONDBIT-ind1+u[0]*MAXBITS);
        if (val==0) {
            u[0] = (u[0]>1)?(HEADER1|u[0]):(ALLONES);
            u[1] ^= ALLONES;
            w = (w>1)?(HEADER1|w):(ALLONES);
        }
        else {
            u[0] = (u[0]>1)?(HEADER0|u[0]):static_cast<word_t>(0);
            w = (w>1)?(HEADER0|w):static_cast<word_t>(0);
        }
        *it = w;
        m_vec.insert(it, u, u+2);
    }
    if (nset)
        nset += val?1:-1;
} // ibis::bitvector64::setBit

/// Return the value of a bit.
///
/// @note If the incoming position is beyond the end of this bitmap, this
/// function returns 0.
///
/// @warning To access the ith bit, this function essentially has to
/// determine the values of bits 0 through i-1, therefore, it is highly
/// recommended that you don't use this function.  A compressed bitmap data
/// structure is simply not the right data structure to support random
/// accesses!
int ibis::bitvector64::getBit(const ibis::bitvector64::word_t ind) const {
    if (ind >= size()) {
        return 0;
    }
    else if (ind >= nbits) {
        return ((active.val >> (active.nbits - (ind - nbits) - 1)) & 1U);
    }
    else if (m_vec.size()*MAXBITS == nbits) { // uncompressed
        return ((m_vec[ind/MAXBITS] >> (SECONDBIT - (ind % MAXBITS))) & 1U);
    }
    else { // need to decompress the compressed words
        word_t jnd = ind;
        array_t<word_t>::const_iterator it = m_vec.begin();
        while (it < m_vec.end()) {
            if (*it > HEADER0) { // a fill
                const word_t cnt = ((*it) & MAXCNT) * MAXBITS;
                if (cnt > jnd) {
                    return (*it >= HEADER1);
                }
                jnd -= cnt;
            }
            else if (jnd < MAXBITS) {
                return ((*it >> (SECONDBIT - jnd)) & 1U);
            }
            else {
                jnd -= MAXBITS;
            }
            ++ it;
        }
    }
    return 0;
} // ibis::bitvector64::getBit

// erase the bit in the range of [i, j)
void ibis::bitvector64::erase(word_t i, word_t j) {
    if (i >= j) {
        return;
    }

    ibis::bitvector64 res;
    if (i > 0) { // copy the leading part to res
        const_iterator ip = const_cast<const ibis::bitvector64*>(this)->begin();
        ip += i;
        array_t<word_t>::const_iterator cit = m_vec.begin();
        while (cit < ip.it) {
            res.m_vec.push_back(*cit);
            ++ cit;
        }
        res.nbits = i - ip.ind;
        if (ip.compressed) {
            for (word_t ii=0; ii<ip.ind; ++ii) res += ip.fillbit;
        }
        else {
            res.active.val = (ip.literalvalue >> (MAXBITS-ip.ind));
            res.active.nbits = ip.ind;
        }
    }

    if (j < nbits) { // need to copy something from m_vec
        const_iterator iq = const_cast<const ibis::bitvector64*>(this)->begin();
        iq += j;
        // copy second half of iq.it
        if (iq.compressed) {
            for (word_t ii=iq.ind; ii<iq.nbits; ++ii) {
                res += iq.fillbit;
            }
        }
        else {
            for (int64_t ii=(iq.nbits-iq.ind-1); ii>=0; --ii) {
                res += (1 & (iq.literalvalue >> ii));
            }
        }
        // copy the remaining whole words
        ++ (iq.it);
        while (iq.it != m_vec.end()) {
            res.appendWord(*(iq.it));
            ++ (iq.it);
        }
        // copy the active word
        for (int64_t ii=(active.nbits-1); ii>=0; --ii) {
            res += (word_t) ((active.val >> ii) & 1);
        }
    }
    else if (j < nbits+active.nbits) { // only something from active word
        for (int64_t ii=(active.nbits-j+nbits-1); ii>=0; --ii) {
            res += (word_t) ((active.val >> ii) & 1);
        }
    }
    swap(res);
} // ibis::bitvector64::erase

// toggle every bit of the bit sequence
void ibis::bitvector64::flip() {
    m_vec.nosharing(); // make sure *this is not shared!
    // toggle those words in m_vec
    if (nbits > 0) {
        for (array_t<word_t>::iterator i=m_vec.begin(); i!=m_vec.end(); i++) {
            if (*i > ALLONES)
                *i ^= FILLBIT;
            else
                *i ^= ALLONES;
        }
    }
    else {
        nbits = 0;
        for (array_t<word_t>::iterator i=m_vec.begin(); i!=m_vec.end(); i++) {
            if (*i > ALLONES) {
                *i ^= FILLBIT;
                nbits += MAXBITS * (*i & MAXCNT);
            }
            else {
                *i ^= ALLONES;
                nbits += MAXBITS;
            }
        }
    }

    if (nset)
        nset = (nbits - nset);

    if (active.nbits > 0) { // also need to toggle active_word
        active.val ^= ((1LL<<active.nbits) - 1);
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    word_t nb = do_cnt();
    if (nbits == 0)
        nbits = nb;
    if (nb != nbits)
        ibis::util::logMessage("Warning", "bitvector64::flip() expects "
                               "to have %lu bits but got %lu",
                               static_cast<long unsigned>(nbits),
                               static_cast<long unsigned>(nb));
#endif
} // ibis::bitvector64::flip

// compare two bitvector64s
int ibis::bitvector64::operator==(const ibis::bitvector64& rhs) const {
    if (nbits != rhs.nbits) return 0;
    if (m_vec.size() != rhs.m_vec.size()) return 0;
    if (active.val != rhs.active.val) return 0;
    for (word_t i=0; i<m_vec.size(); i++)
        if (m_vec[i] != rhs.m_vec[i]) return 0;

    return 1;
} // ibis::bitvector64::operator==

// bitwise and (&) operation
///@sa ibis::bitvector::operator&=
void ibis::bitvector64::operator&=(const ibis::bitvector64& rhs) {
    if ((nbits > 0 && rhs.nbits > 0 && nbits != rhs.nbits) ||
        active.nbits != rhs.active.nbits) {
        ibis::util::logMessage("Warning", "bitvector64::operator&= "
                               "can not operate on two bitvector64 of "
                               "different sizes (%lu != %lu)",
                               static_cast<long unsigned>(size()),
                               static_cast<long unsigned>(rhs.size()));
    }

    const bool ca = (m_vec.size()*MAXBITS == nbits && nbits > 0);
    const bool cb = (rhs.m_vec.size()*MAXBITS == rhs.nbits && rhs.nbits > 0);
    if (ca) {
        if (cb)
            and_c0(rhs);
        else
            and_d1(rhs);
    }
    else if (cb) {
        bitvector64 tmp;
        tmp.copy(rhs);
        swap(tmp);
        and_d1(tmp);
    }
    else if (all0s() || rhs.all1s()) { // content in *this is almost good
        active.val &= rhs.active.val;
    }
    else if (all1s() || rhs.all0s()) { // copy rhs
        nset = rhs.nset;
        m_vec.copy(rhs.m_vec);
        active.val &= rhs.active.val;
    }
    else if ((m_vec.size()+rhs.m_vec.size())*MAXBITS >= rhs.nbits) {
        // if the total size of the two operands are large, generate an
        // decompressed solution
        bitvector64 res;
        and_d2(rhs, res);
        swap(res);
    }
    else {
        bitvector64 res;
        and_c2(rhs, res);
        swap(res);
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    word_t nb = do_cnt();
    if (nbits == 0)
        nbits = nb;
    if (nb != rhs.nbits && rhs.nbits > 0)
        ibis::util::logMessage("Warning", "bitvector64::operator&=() "
                               "expects to have %lu bits but got %lu",
                               static_cast<long unsigned>(rhs.nbits),
                               static_cast<long unsigned>(nb));
#endif
} // ibis::bitvector64::operator&=

/// @sa ibis::bitvector::operator&
ibis::bitvector64* ibis::bitvector64::operator&(const ibis::bitvector64& rhs)
    const {
    if ((nbits > 0 && rhs.nbits > 0 && nbits != rhs.nbits) ||
        active.nbits != rhs.active.nbits) {
        ibis::util::logMessage
            ("Warning", "bitvector64::operator& can not "
             "operate on two bitvector64 of different sizes "
             "(%lu != %lu)", static_cast<long unsigned>(size()),
             static_cast<long unsigned>(rhs.size()));
    }

    ibis::bitvector64 *res = new ibis::bitvector64;
    const bool ca = (m_vec.size()*MAXBITS == nbits && nbits > 0);
    const bool cb = (rhs.m_vec.size()*MAXBITS == rhs.nbits && rhs.nbits > 0);
    if (ca && cb) {
        res->m_vec.resize(m_vec.size());
        array_t<word_t>::iterator i = res->m_vec.begin();
        array_t<word_t>::const_iterator j = m_vec.begin();
        array_t<word_t>::const_iterator k = rhs.m_vec.begin();
        while (i != res->m_vec.end()) {
            *i = *j & *k; ++i; ++j; ++k;
        }
        res->active.val = active.val & rhs.active.val;
        res->active.nbits = active.nbits;
        res->nbits = nbits;
    }
    else if (ca) {
        rhs.and_c1(*this, *res);
    }
    else if (cb) {
        and_c1(rhs, *res);
    }
    else if (all0s() || rhs.all1s()) { // copy *this
        res->copy(*this);
        res->active.val &= rhs.active.val;
    }
    else if (all1s() || rhs.all0s()) { // copy rhs
        res->copy(rhs);
        res->active.val &= active.val;
    }
    else if ((m_vec.size()+rhs.m_vec.size())*MAXBITS > nbits) {
        and_d2(rhs, *res);
    }
    else {
        and_c2(rhs, *res);
    }

#if DEBUG+0 > 0 || _DEBUG+0 > 0
    word_t nb = res->do_cnt();
    if (res->nbits == 0)
        res->nbits = nb;
    if (nb != rhs.nbits && rhs.nbits > 0)
        ibis::util::logMessage("Warning", "bitvector64::operator&() "
                               "expects to have %lu bits but got %lu",
                               static_cast<long unsigned>(rhs.nbits),
                               static_cast<long unsigned>(nb));
#endif
    return res;
} // ibis::bitvector64::operator&

// bitwise or (|) operation
/// @sa ibis::bitvector::operator|=
void ibis::bitvector64::operator|=(const ibis::bitvector64& rhs) {
    if ((nbits > 0 && rhs.nbits > 0 && nbits != rhs.nbits) ||
        active.nbits != rhs.active.nbits) {
        ibis::util::logMessage
            ("Warning", "bitvector64::operator|= can not "
             "operate on two bitvector64 of different sizes "
             "(%lu != %lu)", static_cast<long unsigned>(size()),
             static_cast<long unsigned>(rhs.size()));
    }

    const bool ca = (m_vec.size()*MAXBITS == nbits && nbits > 0);
    const bool cb = (rhs.m_vec.size()*MAXBITS == rhs.nbits && rhs.nbits > 0);
    if (ca) {
        if (cb)
            or_c0(rhs);
        else
            or_d1(rhs);
    }
    else if (cb) {
        bitvector64 tmp;
        tmp.copy(rhs);
        swap(tmp);
        or_d1(tmp);
    }
    else if (all1s() || rhs.all0s()) {
        active.val |= rhs.active.val;
    }
    else if (all0s() || rhs.all1s()) {
        nset = rhs.nset;
        m_vec.copy(rhs.m_vec);
        active.val |= rhs.active.val;
    }
    else if ((m_vec.size()+rhs.m_vec.size())*MAXBITS >= rhs.nbits) {
        // if the total size of the two operands are large, generate an
        // uncompressed result
        bitvector64 res;
        or_d2(rhs, res);
        swap(res);
    }
    else {
        bitvector64 res;
        or_c2(rhs, res);
        swap(res);
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    word_t nb = do_cnt();
    if (nbits == 0)
        nbits = nb;
    if (nb != rhs.nbits && rhs.nbits > 0) {
        ibis::util::logMessage("Warning", "bitvector64::operator|=() "
                               "expects to have %lu bits but got %lu",
                               static_cast<long unsigned>(rhs.nbits),
                               static_cast<long unsigned>(nb));
    }
#endif
} // ibis::bitvector64::operator|=

/// @sa ibis::bitvector::operator|
ibis::bitvector64* ibis::bitvector64::operator|(const ibis::bitvector64& rhs)
    const {
    if ((nbits > 0 && rhs.nbits > 0 && nbits != rhs.nbits) ||
        active.nbits != rhs.active.nbits) {
        ibis::util::logMessage
            ("Warning", "bitvector64::operator| can not "
             "operate on two bitvector64 of different sizes "
             "(%lu != %lu)", static_cast<long unsigned>(size()),
             static_cast<long unsigned>(rhs.size()));
    }

    ibis::bitvector64 *res = new ibis::bitvector64;
    const bool ca = (m_vec.size()*MAXBITS == nbits && nbits > 0);
    const bool cb = (rhs.m_vec.size()*MAXBITS == rhs.nbits && rhs.nbits > 0);
    if (ca && cb) {
        res->m_vec.resize(m_vec.size());
        array_t<word_t>::iterator i = res->m_vec.begin();
        array_t<word_t>::const_iterator j = m_vec.begin();
        array_t<word_t>::const_iterator k = rhs.m_vec.begin();
        while (i != res->m_vec.end()) {
            *i = *j | *k; ++i; ++j; ++k;
        }
        res->active.val = active.val | rhs.active.val;
        res->active.nbits = active.nbits;
        res->nbits = nbits;
    }
    else if (ca) {
        rhs.or_c1(*this, *res);
    }
    else if (cb) {
        or_c1(rhs, *res);
    }
    else if (all1s() || rhs.all0s()) { // copy *this
        res->copy(*this);
        res->active.val |= rhs.active.val;
    }
    else if (all0s() || rhs.all1s()) { // copy rhs
        res->copy(rhs);
        res->active.val |= active.val;
    }
    else if ((m_vec.size()+rhs.m_vec.size())*MAXBITS > nbits) {
        or_d2(rhs, *res);
    }
    else {
        or_c2(rhs, *res);
    }

#if DEBUG+0 > 0 || _DEBUG+0 > 0
    word_t nb = res->do_cnt();
    if (res->nbits == 0)
        res->nbits = nb;
    if (nb != rhs.nbits && rhs.nbits > 0)
        ibis::util::logMessage("Warning", "bitvector64::operator|() "
                               "expects to have %lu bits but got %lu",
                               static_cast<long unsigned>(rhs.nbits),
                               static_cast<long unsigned>(nb));
#endif
    return res;
} // ibis::bitvector64::operator|

// bitwise xor (^) operation
/// @sa ibis::bitvector::operator^=
void ibis::bitvector64::operator^=(const ibis::bitvector64& rhs) {
    if ((nbits > 0 && rhs.nbits > 0 && nbits != rhs.nbits) ||
        active.nbits != rhs.active.nbits) {
        ibis::util::logMessage
            ("Warning", "bitvector64::operator^= can not "
             "operate on two bitvector64 of different sizes "
             "(%lu != %lu)", static_cast<long unsigned>(size()),
             static_cast<long unsigned>(rhs.size()));
    }

    const bool ca = (m_vec.size()*MAXBITS == nbits && nbits > 0);
    const bool cb = (rhs.m_vec.size()*MAXBITS == rhs.nbits && rhs.nbits > 0);
    if (ca) {
        if (cb)
            xor_c0(rhs);
        else
            xor_d1(rhs);
    }
    else if (cb) {
        bitvector64 res;
        xor_c1(rhs, res);
        swap(res);
    }
    else if ((m_vec.size()+rhs.m_vec.size())*MAXBITS >= rhs.nbits) {
        // if the total size of the two operands are large, generate an
        // uncompressed result
        bitvector64 res;
        xor_d2(rhs, res);
        swap(res);
    }
    else {
        bitvector64 res;
        xor_c2(rhs, res);
        swap(res);
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    word_t nb = do_cnt();
    if (nbits == 0)
        nbits = nb;
    if (nb != rhs.nbits && rhs.nbits > 0)
        ibis::util::logMessage("Warning", "bitvector64::operator^=() "
                               "expects to have %lu bits but got %lu",
                               static_cast<long unsigned>(rhs.nbits),
                               static_cast<long unsigned>(nb));
#endif
} // ibis::bitvector64::operator^=

/// @sa ibis::bitvector::operator^=
ibis::bitvector64* ibis::bitvector64::operator^(const ibis::bitvector64& rhs)
    const {
    if ((nbits > 0 && rhs.nbits > 0 && nbits != rhs.nbits) ||
        active.nbits != rhs.active.nbits) {
        ibis::util::logMessage
            ("Warning", "bitvector64::operator^ can not "
             "operate on two bitvector64 of different sizes "
             "(%lu != %lu)", static_cast<long unsigned>(size()),
             static_cast<long unsigned>(rhs.size()));
    }

    ibis::bitvector64 *res = new ibis::bitvector64;
    const bool ca = (m_vec.size()*MAXBITS == nbits && nbits > 0);
    const bool cb = (rhs.m_vec.size()*MAXBITS == rhs.nbits && rhs.nbits > 0);
    if (ca && cb) {
        res->m_vec.resize(m_vec.size());
        array_t<word_t>::iterator i = res->m_vec.begin();
        array_t<word_t>::const_iterator j = m_vec.begin();
        array_t<word_t>::const_iterator k = rhs.m_vec.begin();
        while (i != res->m_vec.end()) {
            *i = *j ^ *k; ++i; ++j; ++k;
        }
        res->active.val = active.val ^ rhs.active.val;
        res->active.nbits = active.nbits;
        res->nbits = nbits;
    }
    else if (ca) {
        rhs.xor_c1(*this, *res);
    }
    else if (cb) {
        xor_c1(rhs, *res);
    }
    else if ((m_vec.size()+rhs.m_vec.size())*MAXBITS > nbits) {
        xor_d2(rhs, *res);
    }
    else {
        xor_c2(rhs, *res);
    }

#if DEBUG+0 > 0 || _DEBUG+0 > 0
    word_t nb = res->do_cnt();
    if (res->nbits == 0)
        res->nbits = nb;
    if (nb != rhs.nbits && rhs.nbits > 0)
        ibis::util::logMessage("Warning", "bitvector64::operator^() "
                               "expects to have %lu bits but got %lu",
                               static_cast<long unsigned>(rhs.nbits),
                               static_cast<long unsigned>(nb));
#endif
    return res;
} // ibis::bitvector64::operator^

// bitwise minus (-) operation
/// @sa ibis::bitvector::operator-=
void ibis::bitvector64::operator-=(const ibis::bitvector64& rhs) {
    if ((nbits > 0 && rhs.nbits > 0 && nbits != rhs.nbits) ||
        active.nbits != rhs.active.nbits) {
        ibis::util::logMessage
            ("Warning", "bitvector64::operator-= can not "
             "operate on two bitvector64 of different sizes "
             "(%lu != %lu)", static_cast<long unsigned>(size()),
             static_cast<long unsigned>(rhs.size()));
    }

    const bool ca = (m_vec.size()*MAXBITS == nbits && nbits > 0);
    const bool cb = (rhs.m_vec.size()*MAXBITS == rhs.nbits && rhs.nbits > 0);
    if (ca) {
        if (cb)
            minus_c0(rhs);
        else
            minus_d1(rhs);
    }
    else if (cb) {
        bitvector64 res;
        minus_c1(rhs, res);
        swap(res);
    }
    else if (all0s() || rhs.all0s()) { // keep *this
        active.val &= ~(rhs.active.val);
    }
    else if (rhs.all1s()) { // zero out m_vec
        nset = 0;
        nbits = 0;
        m_vec.nosharing();
        m_vec.clear();
        active.val &= ~(rhs.active.val);
        append_counter(0, rhs.m_vec[0]&ALLONES);
    }
    else if (all1s()) { // ~rhs
        word_t tmp = active.val;
        copy(rhs);
        flip();
        active.val &= tmp;
    }
    else if ((m_vec.size()+rhs.m_vec.size())*MAXBITS >= rhs.nbits) {
        // if the total size of the two operands are large, generate an
        // uncompressed result
        bitvector64 res;
        minus_d2(rhs, res);
        swap(res);
    }
    else {
        bitvector64 res;
        minus_c2(rhs, res);
        swap(res);
    }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
    word_t nb = do_cnt();
    if (nbits == 0)
        nbits = nb;
    if (nb != rhs.nbits && rhs.nbits > 0)
        ibis::util::logMessage("Warning", "bitvector64::operator-=() "
                               "expects to have %lu bits but got %lu",
                               static_cast<long unsigned>(rhs.nbits),
                               static_cast<long unsigned>(nb));
#endif
} // ibis::bitvector64::operator-=

/// @sa ibis::bitvector::operator-
ibis::bitvector64* ibis::bitvector64::operator-(const ibis::bitvector64& rhs)
    const {
    if ((nbits > 0 && rhs.nbits > 0 && nbits != rhs.nbits) ||
        active.nbits != rhs.active.nbits) {
        ibis::util::logMessage
            ("Warning", "bitvector64::operator- can not "
             "operate on two bitvector64 of different sizes "
             "(%lu != %lu)", static_cast<long unsigned>(size()),
             static_cast<long unsigned>(rhs.size()));
    }

    ibis::bitvector64 *res = new ibis::bitvector64;
    const bool ca = (m_vec.size()*MAXBITS == nbits && nbits > 0);
    const bool cb = (rhs.m_vec.size()*MAXBITS == rhs.nbits && rhs.nbits > 0);
    if (ca && cb) {
        res->m_vec.resize(m_vec.size());
        array_t<word_t>::iterator i = res->m_vec.begin();
        array_t<word_t>::const_iterator j = m_vec.begin();
        array_t<word_t>::const_iterator k = rhs.m_vec.begin();
        while (i != res->m_vec.end()) {
            *i = *j & ~(*k); ++i; ++j; ++k;
        }
        res->active.val = active.val & ~(rhs.active.val);
        res->active.nbits = active.nbits;
        res->nbits = nbits;
    }
    else if (ca) {
        minus_c1x(rhs, *res);
    }
    else if (cb) {
        minus_c1(rhs, *res);
    }
    else if (all0s() || rhs.all0s()) { // copy *this
        res->copy(*this);
        res->active.val &= ~(rhs.active.val);
    }
    else if (rhs.all1s()) { // zero out m_vec
        res->append_counter(0, rhs.m_vec[0] & ALLONES);
        res->active.nbits = active.nbits;
        res->active.val = active.val & ~(rhs.active.val);
    }
    else if (all1s()) { // ~rhs
        res->copy(rhs);
        res->flip();
        res->active.val &= active.val;
    }
    else if ((m_vec.size()+rhs.m_vec.size())*MAXBITS > nbits) {
        minus_d2(rhs, *res);
    }
    else {
        minus_c2(rhs, *res);
    }

#if DEBUG+0 > 0 || _DEBUG+0 > 0
    word_t nb = res->do_cnt();
    if (res->nbits == 0)
        res->nbits = nb;
    if (nb != rhs.nbits && rhs.nbits > 0)
        ibis::util::logMessage("Warning", "bitvector64::operator-() "
                               "expects to have %lu bits but got %lu",
                               static_cast<long unsigned>(rhs.nbits),
                               static_cast<long unsigned>(nb));
#endif
    return res;
} // ibis::bitvector64::operator-

// print each word in bitvector64 to a line
std::ostream& ibis::bitvector64::print(std::ostream& o) const {
    o << "\nThis bitvector64 stores " << nbits << " bits of a " << size()
      << "-bit (" << cnt() << " set) sequence in a " << m_vec.size()
      << "-word array and ";
    if (active.nbits==0)
        o << "zero bit in the active word" << std::endl;
    else if (active.nbits==1)
        o << "one bit in the active word" << std::endl;
    else
        o << active.nbits << " bits in the active word"
          << std::endl;
    if (size() == 0)
        return o;

    // print header
    o << "\t\t\t\t"
        "0    0    1    1    2    2    3    3    4    4    5    5    6"
        "\n\t\t\t\t"
        "012345678901234567890123456789012345678901234567890123456789012"
        "\n\t\t\t\t"
        "---------------------------------------------------------------"
      << std::endl;

    // print all words in m_vec
    array_t<word_t>::const_iterator i;
    word_t j, k;
    for (i=m_vec.begin(), k=0; i!=m_vec.end(); ++i,++k) {
        o << k << "\t" << std::hex << std::setw(16) << std::setfill('0')
          << (*i) << std::dec << "\t";
        if (*i > ALLONES) {
            o << (*i & MAXCNT)*MAXBITS << "*" << ((*i)>=HEADER1);
        }
        else {
            for (j=0; j<MAXBITS; j++)
                o << ((*i>>(SECONDBIT-j))&1);
        }
        o << std::endl;
    }

    if (active.nbits>0) {    // print the active_word
        o << "\t" << std::hex << std::setw(16) << std::setfill('0')
          << (active.val << (MAXBITS-active.nbits))
          << std::dec << "\t";
        for (j=0; j<active.nbits; j++)
            o << (1 & (active.val>>(active.nbits-j-1)));
        //for (; j<MAXBITS; j++) o << ' ';
    }
    o << std::endl;
    if (ibis::gVerbose > 16)
        m_vec.printStatus(o);

    return o;
} // ibis::bitvector64::print

std::ostream& operator<<(std::ostream& o, const ibis::bitvector64& b) {
    return b.print(o);
}

// read vector from file (purge current contents first)
// minimal amount of integrity checking
void ibis::bitvector64::read(const char * fn) {
    // let the file manager handle IO to avoid extra copying
    int ierr = ibis::fileManager::instance().getFile(fn, m_vec);
    if (ierr != 0) {
        if (ibis::gVerbose > 5)
            ibis::util::logMessage("bitvector64", "read(%s) is "
                                   "unable to open the named file", fn);
        return;
    }

    if (m_vec.size() > 1) { // read a file correctly
        //      nbits = m_vec.back(); m_vec.pop_back();
        //      nset = m_vec.back(); m_vec.pop_back();
        if (m_vec.back() > 0) { // has active bits
            active.nbits = m_vec.back();
            m_vec.pop_back();
            active.val = m_vec.back();
        }
        else {
            active.reset();
        }
        m_vec.pop_back();
    }

    nbits = do_cnt();
    // some integrity check here
    if (nbits % MAXBITS != 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bitvector64::nbits(" << nbits
            << ") is expected to be multiples of "
            << MAXBITS << ", but it is not.";
        ierr ++;
    }
    if (nset > nbits+active.nbits) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bitvector64::nset (" << nset
            << ") is expected to be not greater than "
            << nbits+active.nbits << ", but it is.";
        ierr ++;
    }
    if (active.nbits >= MAXBITS) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- bitvector64::active::nbits ("
            << active.nbits << ") is expected to be less than "
            << MAXBITS << ", but it is not.";
        ierr ++;
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    if (size() > 0) {
        ibis::util::logger lg(4);
        lg() << "bitvector64::read(" << fn << ") --\n";
        (void)print(lg());
    }
    else {
        LOGGER(ibis::gVerbose >= 0) << "empty file";
    }
#endif
    if (ierr) {
        ibis::util::logMessage("Error", "bitvector64::read(%s) found "
                               "%d error%s in four integrity checks.",
                               fn, ierr, (ierr>1?"s":""));
        throw "bitvector64::read failed integrity check" IBIS_FILE_LINE;
    }
} // ibis::bitvector64::read

// // write bitvector64 to file (contents of vector is not changed)
void ibis::bitvector64::write(const char * fn) const {
    FILE *out = fopen(fn, "wb");
    if (out == 0) {
        ibis::util::logMessage
            ("Error", "bitvector64::write() Failed to "
             "open \"%s\" to write the bit vector ... %s",
             fn, (errno ? strerror(errno) : "no free stdio stream"));
        throw "bitvector64::write failed to open file" IBIS_FILE_LINE;
    }

#if defined(WAH_CHECK_SIZE)
    word_t nb = do_cnt();
    if (nb != nbits) {
        ibis::util::logger lg(4);
        print(lg());
        lg()
            << "Error ibis::bitvector64::write() the value returned by "
            << "do_cnt() (" << nb << ") is different from nbits ("
            << nbits << ")\n" << "Reset nbits to " << nb;
    }
#endif
    word_t n = m_vec.size();
    word_t j = fwrite((const void*)m_vec.begin(), sizeof(word_t),
                      n, out);
    if (j != n) {
        ibis::util::logMessage("Error", "bitvector64::write() only "
                               "wrote %lu out of %lu words to %s",
                               static_cast<long unsigned>(j),
                               static_cast<long unsigned>(n), fn);
        fclose(out);
        throw "bitvector64::write failed to write all bytes" IBIS_FILE_LINE;
    }
    if (active.nbits > 0) {
        fwrite((const void*)&(active.val), sizeof(word_t), 1, out);
    }
    fwrite((const void*)&(active.nbits), sizeof(word_t), 1, out);

    //     if (0 == fwrite((const void*)&nset, sizeof(word_t), 1, out)) {
    //  ibis::util::logMessage("Error", "bitvector64::write() fail to "
    //                         "write nset to %s", fn);
    //  fclose(out);
    //  throw "bitvector64::write failed to write the size" IBIS_FILE_LINE;
    //     }

    //     if (0 == fwrite((const void*)&nbits, sizeof(word_t), 1, out)) {
    //  ibis::util::logMessage("Error", "bitvector64::write() fail to "
    //                         "write nbits to %s", fn);
    //  fclose(out);
    //  throw "bitvector64::write failed to write the cnt" IBIS_FILE_LINE;
    //     }

    fclose(out);
} // ibis::bitvector64::write

void ibis::bitvector64::write(FILE* out) const {
    if (out == 0) return;

#if defined(WAH_CHECK_SIZE)
    word_t nb = do_cnt();
    if (nb != nbits) {
        LOGGER(ibis::gVerbose >= 0)
            << "Error ibis::bitvector64::write() the value returned by "
            << "do_cnt() (" << nb << ") is different from nbits ("
            << nbits << ")\n" << "Reset nbits to " << nb;
    }
#endif
    word_t n = m_vec.size();
    word_t j = fwrite((const void*)m_vec.begin(), sizeof(word_t),
                      n, out);
    if (j != n) {
        ibis::util::logMessage("Error", "bitvector64::write() only "
                               "wrote %lu out of %lu words",
                               static_cast<long unsigned>(j),
                               static_cast<long unsigned>(n));
        throw "bitvector64::write failed to write all bytes" IBIS_FILE_LINE;
    }
    if (active.nbits > 0) {
        fwrite((const void*)&(active.val), sizeof(word_t), 1, out);
    }
    fwrite((const void*)&(active.nbits), sizeof(word_t), 1, out);
    //     fwrite((const void*)&nset, sizeof(word_t), 1, out);
    //     fwrite((const void*)&nbits, sizeof(word_t), 1, out);
} // ibis::bitvector64::write

void ibis::bitvector64::write(array_t<ibis::bitvector64::word_t>& arr) const {
    arr.reserve(m_vec.size()+1+(active.nbits>0));
    arr.resize(m_vec.size());
    (void) memcpy(arr.begin(), m_vec.begin(), sizeof(word_t)*m_vec.size());
    if (active.nbits > 0) {
        arr.push_back(active.val);
    }
    arr.push_back(active.nbits);
} // ibis::bitvector64::write

// bitwise and (&) operation -- both operands may contain compressed words
void ibis::bitvector64::and_c2(const ibis::bitvector64& rhs,
                               ibis::bitvector64& res) const {
    res.clear();
    if (m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = m_vec.begin();
        if (*it > HEADER1) {
            res.m_vec.deepCopy(rhs.m_vec);
            res.nbits = rhs.nbits;
            res.nset = rhs.nset;
        }
        else if (*it > ALLONES) {
            res.m_vec.deepCopy(m_vec);
            res.nbits = nbits;
            res.nset = 0;
        }
        else {
            res.m_vec.push_back(*it & *(rhs.m_vec.begin()));
            res.nbits = nbits;
        }
    }
    else if (rhs.m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = rhs.m_vec.begin();
        if (*it > HEADER1) {
            res.m_vec.deepCopy(m_vec);
            res.nbits = nbits;
            res.nset = nset;
        }
        else if (*it > ALLONES) {
            res.m_vec.deepCopy(rhs.m_vec);
            res.nbits = rhs.nbits;
            res.nset = 0;
        }
        else {
            res.m_vec.push_back(*it & *(m_vec.begin()));
            res.nbits = nbits;
        }
    }
    else if (m_vec.size() > 1) {
        run x, y;
        x.it = m_vec.begin();
        y.it = rhs.m_vec.begin();
        while (x.it < m_vec.end()) { // go through all words in m_vec
            if (x.nWords == 0)
                x.decode();
            if (y.nWords == 0)
                y.decode();
            if (x.nWords == 0 || y.nWords == 0) {
                while (x.nWords == 0 && x.it < m_vec.end()) {
                    ++ x.it;
                    x.decode();
                }
                while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                    ++ y.it;
                    y.decode();
                }
                if (x.nWords == 0 || y.nWords == 0) {
                    while (x.nWords == 0 && x.it < m_vec.end()) {
                        ++ x.it;
                        x.decode();
                    }
                    while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                        ++ y.it;
                        y.decode();
                    }
                    LOGGER(ibis::gVerbose >= 0 &&
                           (x.nWords == 0 || y.nWords == 0))
                        << "ERROR bitvector64::and_c2 -- serious problem ...";
                }
            }
            if (x.isFill) {         // x points to a fill
                // if both x and y point to fills, use the long one
                if (y.isFill && y.nWords >= x.nWords) {
                    if (y.fillBit == 0) {
                        res.append_counter(0, y.nWords);
                        x -= y.nWords;
                        y.nWords = 0;
                        ++ y.it;
                    }
                    else {
                        res.copy_runs(x, y.nWords);
                        y.it += (y.nWords == 0);
                    }
                }
                else if (x.fillBit == 0) { // generate a 0-fill as the result
                    res.append_counter(0, x.nWords);
                    y -= x.nWords;
                    x.nWords = 0;
                    ++ x.it;
                }
                else { // copy the content of y
                    res.copy_runs(y, x.nWords);
                    x.it += (x.nWords == 0);
                }
            }
            else if (y.isFill) {            // i1 is compressed
                if (y.fillBit == 0) { // generate a 0-fill as the result
                    res.append_counter(0, y.nWords);
                    x -= y.nWords;
                    y.nWords = 0;
                    ++ y.it;
                }
                else { // copy the content of x
                    res.copy_runs(x, y.nWords);
                    y.it += (y.nWords == 0);
                }
            }
            else { // both words are not compressed
                res.active.val = *(x.it) & *(y.it);
                res.append_active();
                x.nWords = 0;
                y.nWords = 0;
                ++ x.it;
                ++ y.it;
            }
        } // while (x.it < m_vec.end())

        if (x.it != m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::and_c2 "
                                   "expects to exhaust i0 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(m_vec.end() - x.it));
            throw "and_c2 iternal error" IBIS_FILE_LINE;
        }

        if (y.it != rhs.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::and_c2 "
                                   "expects to exhaust i1 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(rhs.m_vec.end() - y.it));
            throw "and_c2 iternal error" IBIS_FILE_LINE;
        }
    }

    // the last thing -- work with the two active_words
    if (active.nbits) {
        res.active.val = active.val & rhs.active.val;
        res.active.nbits = active.nbits;
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    LOGGER(ibis::gVerbose >= 0) << "result of AND " << res;
#endif
} // ibis::bitvector64::and_c2

// and operation where rhs is not compressed (not one word is a counter)
void ibis::bitvector64::and_c1(const ibis::bitvector64& rhs,
                               ibis::bitvector64& res) const {
    res.clear();
    if (m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = m_vec.begin();
        if (*it > HEADER1) {
            res.m_vec.deepCopy(rhs.m_vec);
            res.nbits = rhs.nbits;
            res.nset = rhs.nset;
        }
        else if (*it > ALLONES) {
            res.m_vec.deepCopy(m_vec);
            res.nbits = nbits;
            res.nset = 0;
        }
        else {
            res.m_vec.push_back(*it & *(rhs.m_vec.begin()));
            res.nbits = nbits;
        }
    }
    else if (m_vec.size() > 1) {
        array_t<word_t>::const_iterator i0 = m_vec.begin();
        array_t<word_t>::const_iterator i1 = rhs.m_vec.begin(), i2;
        word_t s0;
        res.m_vec.reserve(rhs.m_vec.size());
        while (i0 != m_vec.end()) {     // go through all words in m_vec
            if (*i0 > ALLONES) { // i0 is compressed
                s0 = ((*i0) & MAXCNT);
                if ((*i0)<HEADER1) { // the result is all zero
                    if (s0 > 1)
                        res.append_counter(0, s0);
                    else
                        res.append_active();
                    i1 += s0;
                }
                else { // the result is *i1
                    i2 = i1 + s0;
                    for (; i1<i2; i1++)
                        res.m_vec.push_back(*i1);
                    res.nbits += s0 * MAXBITS;
                }
            }
            else { // both words are not compressed
                res.active.val = *i0 & *i1;
                res.append_active();
                i1++;
            }
            i0++;
        } // while (i0 != m_vec.end())

        if (i1 != rhs.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::and_c1 "
                                   "expects to exhaust i1 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(rhs.m_vec.end() - i1));
            throw "and_c1 iternal error" IBIS_FILE_LINE;
        }
    }

    // the last thing -- work with the two active_words
    res.active.val = active.val & rhs.active.val;
    res.active.nbits = active.nbits;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    LOGGER(ibis::gVerbose >= 0) << "result of AND " << res;
#endif
} // ibis::bitvector64::and_c1

// and operation on two compressed bitvector64s, generates uncompressed result
void ibis::bitvector64::and_d2(const ibis::bitvector64& rhs,
                               ibis::bitvector64& res) const {
    // establish a uncompressed bitvector64 with the right size
    res.nbits = ((nbits==0 && m_vec.size()>0) ?
                 (rhs.nbits == 0 && rhs.m_vec.size()>0) ? 
                 (m_vec.size() <= rhs.m_vec.size() ? do_cnt() : rhs.do_cnt())
                 : rhs.nbits : nbits);
    res.m_vec.resize(res.nbits/MAXBITS);

    // fill res with the right numbers
    if (m_vec.size() == 1) { // only one word in *this
        array_t<word_t>::const_iterator it = m_vec.begin();
        if (*it > HEADER1) { // copy rhs
            rhs.decompress(res.m_vec);
            res.nset = rhs.nset;
        }
        else if (*it > ALLONES) { // all zeros
            decompress(res.m_vec);
            res.nset = 0;
        }
        else {
            res.m_vec[0] = (*it & *(rhs.m_vec.begin()));
            res.nset = cnt_ones(res.m_vec[0]);
        }
    }
    else if (rhs.m_vec.size() == 1) { // only one word in rhs
        array_t<word_t>::const_iterator it = rhs.m_vec.begin();
        if (*it > HEADER1) { // copy *this
            decompress(res.m_vec);
            res.nset = nset;
        }
        else if (*it > ALLONES) { // all zero
            rhs.decompress(res.m_vec);
            res.nset = 0;
        }
        else {
            res.m_vec[0] = (*it & *(m_vec.begin()));
            res.nset = cnt_ones(res.m_vec[0]);
        }
    }
    else if (m_vec.size() > 1) { // more than one word in *this
        run x, y;
        res.nset = 0;
        x.it = m_vec.begin();
        y.it = rhs.m_vec.begin();
        array_t<word_t>::iterator ir = res.m_vec.begin();
        while (x.it < m_vec.end()) {    // go through all words in m_vec
            if (x.nWords == 0)
                x.decode();
            if (y.nWords == 0)
                y.decode();
            if (x.nWords == 0 || y.nWords == 0) {
                while (x.nWords == 0 && x.it < m_vec.end()) {
                    ++ x.it;
                    x.decode();
                }
                while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                    ++ y.it;
                    y.decode();
                }
                if (x.nWords == 0 || y.nWords == 0) {
                    while (x.nWords == 0 && x.it < m_vec.end()) {
                        ++ x.it;
                        x.decode();
                    }
                    while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                        ++ y.it;
                        y.decode();
                    }
                    LOGGER(ibis::gVerbose >= 0 &&
                           (x.nWords == 0 || y.nWords == 0))
                        << "ERROR bitvector64::and_d2 -- serious problem ...";
                }
            }
            if (x.isFill) {
                if (y.isFill && y.nWords >= x.nWords) {
                    if (y.fillBit == 0) {
                        x -= y.nWords;
                        res.copy_fill(ir, y);
                    }
                    else {
                        res.copy_runs(ir, x, y.nWords);
                        y.it += (y.nWords == 0);
                    }
                }
                else if (x.fillBit == 0) {
                    y -= x.nWords;
                    res.copy_fill(ir, x);
                }
                else {
                    res.copy_runs(ir, y, x.nWords);
                    x.it += (x.nWords == 0);
                }
            }
            else if (y.isFill) {
                if (y.fillBit == 0) {
                    x -= y.nWords;
                    res.copy_fill(ir, y);
                }
                else {
                    res.copy_runs(ir, x, y.nWords);
                    y.it += (y.nWords == 0);
                }
            }
            else { // both words are not compressed
                *ir = *x.it & *y.it;
                x.nWords = 0;
                y.nWords = 0;
                ++ x.it;
                ++ y.it;
                ++ ir;
            }
        } // while (x.it < m_vec.end())

        if (x.it != m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::and_d2 "
                                   "expects to exhaust i0 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(m_vec.end() - x.it));
            throw "and_d2 internal error" IBIS_FILE_LINE;
        }

        if (y.it != rhs.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::and_d2 "
                                   "expects to exhaust i1 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(rhs.m_vec.end() - y.it));
            throw "and_d2 internal error" IBIS_FILE_LINE;
        }

        if (ir != res.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::and_d2 "
                                   "expects to exhaust ir but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(res.m_vec.end() - ir));
            throw "and_d2 internal error" IBIS_FILE_LINE;
        }
    }

    // work with the two active_words
    res.active.val = active.val & rhs.active.val;
    res.active.nbits = active.nbits;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "operand 1 of AND" << *this << std::endl;
    lg() << "operand 2 of AND" << rhs << std::endl;
    lg() << "result of AND " << res << std::endl;
#endif
} // ibis::bitvector64::and_d2

// assuming *this is uncompressed but rhs is not, this function performs the
// AND operation and overwrites *this with the result
void ibis::bitvector64::and_d1(const ibis::bitvector64& rhs) {
    m_vec.nosharing();
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "operand 1 of AND" << *this << std::endl;
        lg() << "operand 2 of AND" << rhs << std::endl;
    }
#endif
    if (rhs.m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = rhs.m_vec.begin();
        if (*it < HEADER1) { // nothing to do for *it >= HEADER1
            if (*it > ALLONES) { // all zero
                memset(m_vec.begin(), 0, sizeof(word_t)*m_vec.size());
                nset = 0;
            }
            else { // one literal word
                m_vec[0] = (*it & *(rhs.m_vec.begin()));
                nset = cnt_ones(m_vec[0]);
            }
        }
    }
    else if (rhs.m_vec.size() > 1) {
        array_t<word_t>::iterator i0 = m_vec.begin();
        array_t<word_t>::const_iterator i1 = rhs.m_vec.begin();
        word_t s0;
        nset = 0;
        while (i1 != rhs.m_vec.end()) { // go through all words in m_vec
            if (*i1 > ALLONES) { // i1 is compressed
                s0 = ((*i1) & MAXCNT);
                if ((*i1) < HEADER1) { // set literal words to zero
                    memset(i0, 0, sizeof(word_t)*s0);
                }
                i0 += s0;
            }
            else { // both words are not compressed
                *i0 &= *i1;
                ++ i0;
            }
            ++ i1;
        } // while (i1 != rhs.m_vec.end())

        if (i0 != m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::and_d1 "
                                   "expects to exhaust i0 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(m_vec.end() - i0));
            throw "and_d1 internal error" IBIS_FILE_LINE;
        }
    }

    // the last thing -- work with the two active_words
    active.val &= rhs.active.val;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    LOGGER(ibis::gVerbose >= 0) << "result of AND" << *this;
#endif
} // ibis::bitvector64::and_d1

// both operands of the 'and' operation are not compressed
void ibis::bitvector64::and_c0(const ibis::bitvector64& rhs) {
    nset = 0;
    m_vec.nosharing();
    array_t<word_t>::iterator i0 = m_vec.begin();
    array_t<word_t>::const_iterator i1 = rhs.m_vec.begin();
    while (i0 != m_vec.end()) { // go through all words in m_vec
        *i0 &= *i1;
        i0++; i1++;
    } // while (i0 != m_vec.end())

    // the last thing -- work with the two active_words
    active.val &= rhs.active.val;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    LOGGER(ibis::gVerbose >= 0) << "result of AND " << *this;
#endif
} // ibis::bitvector64::and_c0

// or operation on two compressed bitvector64s
void ibis::bitvector64::or_c2(const ibis::bitvector64& rhs,
                              ibis::bitvector64& res) const {
    res.clear();
    if (m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = m_vec.begin();
        if (*it > HEADER1) {
            res.m_vec.deepCopy(m_vec);
            res.nbits = nbits;
            res.nset = nbits;
        }
        else if (*it > ALLONES) {
            res.m_vec.deepCopy(rhs.m_vec);
            res.nbits = rhs.nbits;
            res.nset = rhs.nset;
        }
        else {
            res.m_vec.push_back(*it | *(rhs.m_vec.begin()));
            res.nbits = nbits;
        }
    }
    else if (rhs.m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = rhs.m_vec.begin();
        if (*it > HEADER1) {
            res.m_vec.deepCopy(rhs.m_vec);
            res.nbits = rhs.nbits;
            res.nset = rhs.nbits;
        }
        else if (*it > ALLONES) {
            res.m_vec.deepCopy(m_vec);
            res.nbits = nbits;
            res.nset = nset;
        }
        else {
            res.m_vec.push_back(*it | *(m_vec.begin()));
            res.nbits = nbits;
        }
    }
    else if (m_vec.size() > 1) {
        run x, y;
        x.it = m_vec.begin();
        y.it = rhs.m_vec.begin();
        while (x.it < m_vec.end()) {    // go through all words in m_vec
            if (x.nWords == 0)
                x.decode();
            if (y.nWords == 0)
                y.decode();
            if (x.nWords == 0 || y.nWords == 0) {
                while (x.nWords == 0 && x.it < m_vec.end()) {
                    ++ x.it;
                    x.decode();
                }
                while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                    ++ y.it;
                    y.decode();
                }
                if (x.nWords == 0 || y.nWords == 0) {
                    while (x.nWords == 0 && x.it < m_vec.end()) {
                        ++ x.it;
                        x.decode();
                    }
                    while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                        ++ y.it;
                        y.decode();
                    }
                    LOGGER(ibis::gVerbose >= 0 &&
                           (x.nWords == 0 || y.nWords == 0))
                        << "ERROR bitvector64::or_c2 -- serious problem ...";
                }
            }
            if (x.isFill) { // x points to a fill
                // if both x and y point to fills, use the longer one
                if (y.isFill && y.nWords >= x.nWords) {
                    if (y.fillBit) {
                        res.append_counter(y.fillBit, y.nWords);
                        x -= y.nWords;
                        y.nWords = 0;
                        ++ y.it;
                    }
                    else {
                        res.copy_runs(x, y.nWords);
                        y.it += (y.nWords == 0);
                    }
                }
                else if (x.fillBit) { // the result is all ones
                    res.append_counter(x.fillBit, x.nWords);
                    y -= x.nWords; // advance the pointer in y
                    x.nWords = 0;
                    ++ x.it;
                }
                else { // copy the content of y
                    res.copy_runs(y, x.nWords);
                    x.it += (x.nWords == 0);
                }
            }
            else if (y.isFill) { // y points to a fill
                if (y.fillBit) {
                    res.append_counter(y.fillBit, y.nWords);
                    x -= y.nWords;
                    y.nWords = 0;
                    ++ y.it;
                }
                else {
                    res.copy_runs(x, y.nWords);
                    y.it += (y.nWords == 0);
                }
            }
            else { // both words are not compressed
                res.active.val = *x.it | *y.it;
                res.append_active();
                x.nWords = 0;
                y.nWords = 0;
                ++ x.it;
                ++ y.it;
            }
        } // while (x.it < m_vec.end())

        if (x.it != m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::or_c2 "
                                   "expects to exhaust i0 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(m_vec.end() - x.it));
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            {
                ibis::util::logger lg(4);
                lg() << "*this: " << *this << std::endl;
                lg() << "rhs: " << rhs << std::endl;
                lg() << "res: " << res << std::endl;
            }
#else
            throw "or_c2 internal error" IBIS_FILE_LINE;
#endif
        }

        if (y.it != rhs.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::or_c2 "
                                   "expects to exhaust i1 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(rhs.m_vec.end() - y.it));
#if DEBUG+0 > 1 || _DEBUG+0 > 1
            {
                ibis::util::logger lg(4);
                lg() << "*this: " << *this << std::endl;
                lg() << "rhs: " << rhs << std::endl;
                lg() << "res: " << res << std::endl;
            }
#else
            throw "or_c2 internal error" IBIS_FILE_LINE;
#endif
        }
    }

    // work with the two active_words
    res.active.val = active.val | rhs.active.val;
    res.active.nbits = active.nbits;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "operand 1 of OR" << *this << std::endl;
    lg() << "operand 2 of OR" << rhs << std::endl;
    lg() << "result of OR " << res << std::endl;
#endif
} // ibis::bitvector64::or_c2

// or operation where rhs is not compressed (not one word is a counter)
void ibis::bitvector64::or_c1(const ibis::bitvector64& rhs,
                              ibis::bitvector64& res) const {
    res.clear();
    if (m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = m_vec.begin();
        if (*it > HEADER1) {
            res.m_vec.deepCopy(m_vec);
            res.nbits = nbits;
            res.nset = nbits;
        }
        else if (*it > ALLONES) {
            res.m_vec.deepCopy(rhs.m_vec);
            res.nbits = rhs.nbits;
            res.nset = rhs.nset;
        }
        else {
            res.m_vec.push_back(*it | *(rhs.m_vec.begin()));
            res.nbits = nbits;
        }
    }
    else if (m_vec.size() > 1) {
        array_t<word_t>::const_iterator i0 = m_vec.begin();
        array_t<word_t>::const_iterator i1 = rhs.m_vec.begin(), i2;
        word_t s0;
        res.m_vec.reserve(rhs.m_vec.size());
        while (i0 != m_vec.end()) {     // go through all words in m_vec
            if (*i0 > ALLONES) { // i0 is compressed
                s0 = ((*i0) & MAXCNT);
                if ((*i0)>=HEADER1) { // the result is all ones
                    if (s0 > 1)
                        res.append_counter(1, s0);
                    else {
                        res.active.val = ALLONES;
                        res.append_active();
                    }
                    i1 += s0;
                }
                else { // the result is *i1
                    i2 = i1 + s0;
                    for (; i1<i2; i1++)
                        res.m_vec.push_back(*i1);
                    res.nbits += s0 * MAXBITS;
                }
            }
            else { // both words are not compressed
                res.active.val = *i0 | *i1;
                res.append_active();
                i1++;
            }
            i0++;
        } // while (i0 != m_vec.end())

        if (i1 != rhs.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::or_c1 "
                                   "expects to exhaust i1 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(rhs.m_vec.end() - i1));
            throw "or_c1 internal error" IBIS_FILE_LINE;
        }
    }

    // the last thing -- work with the two active_words
    res.active.val = active.val | rhs.active.val;
    res.active.nbits = active.nbits;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "operand 1 of OR" << *this << std::endl;
    lg() << "operand 2 of OR" << rhs << std::endl;
    lg() << "result or OR " << res << std::endl;
#endif
} // ibis::bitvector64::or_c1

// or operation on two compressed bitvector64s, generates uncompressed result
void ibis::bitvector64::or_d2(const ibis::bitvector64& rhs,
                              ibis::bitvector64& res) const {
    // establish a uncompressed bitvector64 with the right size
    res.nbits = ((nbits==0 && m_vec.size()>0) ?
                 (rhs.nbits == 0 && rhs.m_vec.size()>0) ? 
                 (m_vec.size() <= rhs.m_vec.size() ? do_cnt() : rhs.do_cnt())
                 : rhs.nbits : nbits);
    res.m_vec.resize(res.nbits/MAXBITS);

    // fill res with the right numbers
    if (m_vec.size() == 1) { // only one word in *this
        array_t<word_t>::const_iterator it = m_vec.begin();
        if (*it > HEADER1) { // all ones
            decompress(res.m_vec);
            res.nset = nbits;
        }
        else if (*it > ALLONES) { // copy rhs
            rhs.decompress(res.m_vec);
            res.nset = rhs.nset;
        }
        else {
            res.m_vec[0] = (*it | *(rhs.m_vec.begin()));
            res.nset = cnt_ones(res.m_vec[0]);
        }
    }
    else if (rhs.m_vec.size() == 1) { // only one word in rhs
        array_t<word_t>::const_iterator it = rhs.m_vec.begin();
        if (*it > HEADER1) { // all ones
            rhs.decompress(res.m_vec);
            res.nset = rhs.nbits;
        }
        else if (*it > ALLONES) { // copy *this
            decompress(res.m_vec);
            res.nset = nset;
        }
        else {
            res.m_vec[0] = (*it | *(m_vec.begin()));
            res.nset = cnt_ones(res.m_vec[0]);
        }
    }
    else if (m_vec.size() > 1) { // more than one word in *this
        run x, y;
        res.nset = 0;
        x.it = m_vec.begin();
        y.it = rhs.m_vec.begin();
        array_t<word_t>::iterator ir = res.m_vec.begin();
        while (x.it < m_vec.end()) {    // go through all words in m_vec
            if (x.nWords == 0)
                x.decode();
            if (y.nWords == 0)
                y.decode();
            if (x.nWords == 0 || y.nWords == 0) {
                while (x.nWords == 0 && x.it < m_vec.end()) {
                    ++ x.it;
                    x.decode();
                }
                while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                    ++ y.it;
                    y.decode();
                }
                if (x.nWords == 0 || y.nWords == 0) {
                    while (x.nWords == 0 && x.it < m_vec.end()) {
                        ++ x.it;
                        x.decode();
                    }
                    while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                        ++ y.it;
                        y.decode();
                    }
                    LOGGER(ibis::gVerbose >= 0 &&
                           (x.nWords == 0 || y.nWords == 0))
                        << "ERROR bitvector64::or_d2 -- serious problem ...";
                }
            }
            if (x.isFill) {
                if (y.isFill && y.nWords >= x.nWords) {
                    if (y.fillBit == 0) {
                        res.copy_runs(ir, x, y.nWords);
                        y.it += (y.nWords == 0);
                    }
                    else {
                        x -= y.nWords;
                        res.copy_fill(ir, y);
                    }
                }
                else if (x.fillBit == 0) {
                    res.copy_runs(ir, y, x.nWords);
                    x.it += (x.nWords == 0);
                }
                else {
                    y -= x.nWords;
                    res.copy_fill(ir, x);
                }
            }
            else if (y.isFill) {
                if (y.fillBit == 0) {
                    res.copy_runs(ir, x, y.nWords);
                    y.it += (y.nWords == 0);
                }
                else {
                    x -= y.nWords;
                    res.copy_fill(ir, y);
                }
            }
            else { // both words are not compressed
                *ir = *x.it | *y.it;
                x.nWords = 0;
                y.nWords = 0;
                ++ x.it;
                ++ y.it;
                ++ ir;
            }
        } // while (x.it < m_vec.end())

        if (x.it != m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::or_d2 "
                                   "expects to exhaust i0 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(m_vec.end() - x.it));
            throw "or_d2 internal error" IBIS_FILE_LINE;
        }

        if (y.it != rhs.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::or_d2 "
                                   "expects to exhaust i1 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(rhs.m_vec.end() - y.it));
            throw "or_d2 internal error" IBIS_FILE_LINE;
        }

        if (ir != res.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::or_d2 "
                                   "expects to exhaust ir but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(res.m_vec.end() - ir));
            throw "or_d2 internal error" IBIS_FILE_LINE;
        }
    }

    // work with the two active_words
    res.active.val = active.val | rhs.active.val;
    res.active.nbits = active.nbits;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "operand 1 of OR" << *this << std::endl;
    lg() << "operand 2 of OR" << rhs << std::endl;
    lg() << "result of OR " << res << std::endl;
#endif
} // ibis::bitvector64::or_d2

// assuming *this is uncompressed but rhs is not, this function performs
// the OR operation and overwrites *this with the result
void ibis::bitvector64::or_d1(const ibis::bitvector64& rhs) {
    m_vec.nosharing();
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "operand 1 of OR" << *this << std::endl;
        lg() << "operand 2 of OR" << rhs << std::endl;
    }
#endif
    if (rhs.m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = rhs.m_vec.begin();
        if (*it > HEADER1) { // all ones
            rhs.decompress(m_vec);
            nset = nbits;
        }
        else if (*it <= ALLONES) { // one literal word
            m_vec[0] = (*it | *(rhs.m_vec.begin()));
            nset = cnt_ones(m_vec[0]);
        }
    }
    else if (rhs.m_vec.size() > 1) {
        array_t<word_t>::iterator i0 = m_vec.begin();
        array_t<word_t>::const_iterator i1 = rhs.m_vec.begin();
        word_t s0;
        nset = 0;
        while (i1 != rhs.m_vec.end()) { // go through all words in m_vec
            if (*i1 > ALLONES) { // i1 is compressed
                s0 = ((*i1) & MAXCNT);
                if ((*i1) >= HEADER1) { // the result is all ones
                    array_t<word_t>::const_iterator stp = i0 + s0;
                    while (i0 < stp) {
                        *i0 = ALLONES;
                        ++ i0;
                    }
                }
                else { // the result is *i0
                    i0 += s0;
                }
            }
            else { // both words are not compressed
                *i0 |= *i1;
                ++ i0;
            }
            ++ i1;
        } // while (i1 != rhs.m_vec.end())

        if (i0 != m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::or_d1 "
                                   "expects to exhaust i0 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(m_vec.end() - i0));
            throw "or_d1 internal error" IBIS_FILE_LINE;
        }
    }

    // the last thing -- work with the two active_words
    active.val |= rhs.active.val;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "result of OR" << *this << std::endl;
    }
#endif
} // ibis::bitvector64::or_d1

// both operands of the 'or' operation are not compressed
void ibis::bitvector64::or_c0(const ibis::bitvector64& rhs) {
    nset = 0;
    m_vec.nosharing();
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "operand 1 of OR" << *this << std::endl;
        lg() << "operand 2 of OR" << rhs << std::endl;
    }
#endif
    array_t<word_t>::iterator i0 = m_vec.begin();
    array_t<word_t>::const_iterator i1 = rhs.m_vec.begin();
    while (i0 != m_vec.end()) { // go through all words in m_vec
        *i0 |= *i1;
        i0++; i1++;
    } // while (i0 != m_vec.end())

    // the last thing -- work with the two active_words
    active.val |= rhs.active.val;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    word_t nb = do_cnt();
    if (nbits == 0)
        nbits = nb;
    if (nb != rhs.nbits && rhs.nbits > 0) {
        ibis::util::logMessage("Warning", "bitvector64::or_c0=() "
                               "expects to have %lu bits but got %lu",
                               static_cast<long unsigned>(rhs.nbits),
                               static_cast<long unsigned>(nb));
    }
    {
        ibis::util::logger lg(4);
        lg() << "result of OR " << *this;
    }
    //      if (m_vec.size()*(MAXBITS<<1) >= nbits)
    //          decompress();
#endif
} // ibis::bitvector64::or_c0

// bitwise xor (^) operation -- both operands may contain compressed words
void ibis::bitvector64::xor_c2(const ibis::bitvector64& rhs,
                               ibis::bitvector64& res) const {
    run x, y;
    res.clear();
    x.it = m_vec.begin();
    y.it = rhs.m_vec.begin();
    while (x.it < m_vec.end()) {        // go through all words in m_vec
        if (x.nWords == 0)
            x.decode();
        if (y.nWords == 0)
            y.decode();
        if (x.nWords == 0 || y.nWords == 0) {
            while (x.nWords == 0 && x.it < m_vec.end()) {
                ++ x.it;
                x.decode();
            }
            while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                ++ y.it;
                y.decode();
            }
            if (x.nWords == 0 || y.nWords == 0) {
                while (x.nWords == 0 && x.it < m_vec.end()) {
                    ++ x.it;
                    x.decode();
                }
                while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                    ++ y.it;
                    y.decode();
                }
                if (x.nWords == 0 || y.nWords == 0) {
                    while (x.nWords == 0 && x.it < m_vec.end()) {
                        ++ x.it;
                        x.decode();
                    }
                    while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                        ++ y.it;
                        y.decode();
                    }
                    LOGGER(ibis::gVerbose >= 0 &&
                           (x.nWords == 0 || y.nWords == 0))
                        << "ERROR bitvector64::xor_c2 -- serious problem ...";
                }
            }
        }
        if (x.isFill) { // x points to a fill
            // if both x and y point to a fill, use the longer fill
            if (y.isFill && y.nWords >= x.nWords) {
                if (y.fillBit == 0)
                    res.copy_runs(x, y.nWords);
                else
                    res.copy_runsn(x, y.nWords);
                y.it += (y.nWords == 0);
            }
            else if (x.fillBit == 0) {
                res.copy_runs(y, x.nWords);
                x.it += (x.nWords == 0);
            }
            else {
                res.copy_runsn(y, x.nWords);
                x.it += (x.nWords == 0);
            }
        }
        else if (y.isFill) {    // y points to a fill
            if (y.fillBit == 0)
                res.copy_runs(x, y.nWords);
            else
                res.copy_runsn(x, y.nWords);
            y.it += (y.nWords == 0);
        }
        else { // both words are not compressed
            res.active.val = *x.it ^ *y.it;
            res.append_active();
            x.nWords = 0;
            y.nWords = 0;
            ++ x.it;
            ++ y.it;
        }
    } // while (x.it < m_vec.end())

    if (x.it != m_vec.end()) {
        ibis::util::logMessage("Error", "bitvector64::xor_c2 expects "
                               "to exhaust i0 but there are %ld word(s) left",
                               static_cast<long>(m_vec.end() - x.it));
        throw "xor_c2 interal error" IBIS_FILE_LINE;
    }

    if (y.it != rhs.m_vec.end()) {
        ibis::util::logMessage("Error", "bitvector64::xor_c2 expects "
                               "to exhaust i1 but there are %ld word(s) left",
                               static_cast<long>(rhs.m_vec.end() - y.it));
        throw "xor_c2 internal error" IBIS_FILE_LINE;
    }

    // the last thing -- work with the two active_words
    res.active.val = active.val ^ rhs.active.val;
    res.active.nbits = active.nbits;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "result of XOR " << res;
#endif
} // bitvector64& ibis::bitvector64::xor_c2

// xor operation where rhs is not compressed (not one word is a counter)
void ibis::bitvector64::xor_c1(const ibis::bitvector64& rhs,
                               ibis::bitvector64& res) const {
    array_t<word_t>::const_iterator i0 = m_vec.begin();
    array_t<word_t>::const_iterator i1 = rhs.m_vec.begin(), i2;
    word_t s0;
    res.clear();
    res.m_vec.reserve(rhs.m_vec.size());
    while (i0 != m_vec.end()) { // go through all words in m_vec
        if (*i0 > ALLONES) { // i0 is compressed
            s0 = ((*i0) & MAXCNT);
            i2 = i1 + s0;
            res.nbits += s0*MAXBITS;
            if ((*i0)>=HEADER1) { // the result is the compliment of i1
                for (; i1!=i2; i1++)
                    res.m_vec.push_back((*i1) ^ ALLONES);
            }
            else { // the result is *i1
                for (; i1!=i2; i1++)
                    res.m_vec.push_back(*i1);
            }
        }
        else { // both words are not compressed
            res.active.val = *i0 ^ *i1;
            res.append_active();
            i1++;
        }
        i0++;
    } // while (i0 != m_vec.end())

    if (i1 != rhs.m_vec.end()) {
        ibis::util::logMessage("Error", "bitvector64::xor_c1 expects "
                               "to exhaust i1 but there are %ld word(s) left",
                               static_cast<long>(rhs.m_vec.end() - i1));
        throw "xor_c1 iternal error" IBIS_FILE_LINE;
    }

    // the last thing -- work with the two active_words
    res.active.val = active.val ^ rhs.active.val;
    res.active.nbits = active.nbits;

#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "result XOR " << res;
#endif
} // ibis::bitvector64::xor_c1

// xor operation on two compressed bitvector64s, generates uncompressed result
void ibis::bitvector64::xor_d2(const ibis::bitvector64& rhs,
                               ibis::bitvector64& res) const {
    // establish a uncompressed bitvector64 with the right size
    res.nbits = ((nbits==0 && m_vec.size()>0) ?
                 (rhs.nbits == 0 && rhs.m_vec.size()>0) ? 
                 (m_vec.size() <= rhs.m_vec.size() ? do_cnt() : rhs.do_cnt())
                 : rhs.nbits : nbits);
    res.m_vec.resize(res.nbits/MAXBITS);

    // fill res with the right numbers
    if (m_vec.size() == 1) { // only one word in *this
        array_t<word_t>::const_iterator it = m_vec.begin();
        if (*it > HEADER1) { // complement of rhs
            rhs.copy_comp(res.m_vec);
            if (rhs.nset > 0)
                res.nset = nbits - rhs.nset;
        }
        else if (*it > ALLONES) { // copy rhs
            rhs.decompress(res.m_vec);
            res.nset = rhs.nset;
        }
        else {
            res.m_vec[0] = (*it ^ *(rhs.m_vec.begin()));
            res.nset = cnt_ones(res.m_vec[0]);
        }
    }
    else if (rhs.m_vec.size() == 1) { // only one word in rhs
        array_t<word_t>::const_iterator it = rhs.m_vec.begin();
        if (*it > HEADER1) { // complement of *this
            copy_comp(res.m_vec);
            if (nset > 0) res.nset = nbits - nset;
        }
        else if (*it > ALLONES) { // copy *this
            decompress(res.m_vec);
            res.nset = nset;
        }
        else {
            res.m_vec[0] = (*it ^ *(m_vec.begin()));
            res.nset = cnt_ones(res.m_vec[0]);
        }
    }
    else if (m_vec.size() > 1) { // more than one word in *this
        run x, y;
        res.nset = 0;
        x.it = m_vec.begin();
        y.it = rhs.m_vec.begin();
        array_t<word_t>::iterator ir = res.m_vec.begin();
        while (x.it < m_vec.end()) {    // go through all words in m_vec
            if (x.nWords == 0)
                x.decode();
            if (y.nWords == 0)
                y.decode();
            if (x.nWords == 0 || y.nWords == 0) {
                while (x.nWords == 0 && x.it < m_vec.end()) {
                    ++ x.it;
                    x.decode();
                }
                while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                    ++ y.it;
                    y.decode();
                }
                if (x.nWords == 0 || y.nWords == 0) {
                    while (x.nWords == 0 && x.it < m_vec.end()) {
                        ++ x.it;
                        x.decode();
                    }
                    while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                        ++ y.it;
                        y.decode();
                    }
                    LOGGER(ibis::gVerbose >= 0 &&
                           (x.nWords == 0 || y.nWords == 0))
                        << "ERROR bitvector64::xor_d2 -- serious problem ...";
                }
            }
            if (x.isFill) {
                if (y.isFill && y.nWords >= x.nWords) {
                    if (y.fillBit == 0) {
                        res.copy_runs(ir, x, y.nWords);
                    }
                    else {
                        res.copy_runsn(ir, x, y.nWords);
                    }
                    y.it += (y.nWords == 0);
                }
                else {
                    if (x.fillBit == 0) {
                        res.copy_runs(ir, y, x.nWords);
                    }
                    else {
                        res.copy_runsn(ir, y, x.nWords);
                    }
                    x.it += (x.nWords == 0);
                }
            }
            else if (y.isFill) {
                if (y.fillBit == 0) {
                    res.copy_runs(ir, x, y.nWords);
                }
                else {
                    res.copy_runsn(ir, x, y.nWords);
                }
                y.it += (y.nWords == 0);
            }
            else { // both words are not compressed
                *ir = *x.it ^ *y.it;
                x.nWords = 0;
                y.nWords = 0;
                ++ x.it;
                ++ y.it;
                ++ ir;
            }
        } // while (x.it < m_vec.end())

        if (x.it != m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::xor_d2 "
                                   "expects to exhaust i0 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(m_vec.end() - x.it));
            throw "xor_d2 internal error" IBIS_FILE_LINE;
        }

        if (y.it != rhs.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::xor_d2 "
                                   "expects to exhaust i1 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(rhs.m_vec.end() - y.it));
            throw "xor_d2 internal error" IBIS_FILE_LINE;
        }

        if (ir != res.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::xor_d2 "
                                   "expects to exhaust ir but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(res.m_vec.end() - ir));
            throw "xor_d2 internal error" IBIS_FILE_LINE;
        }
    }

    // work with the two active_words
    res.active.val = active.val ^ rhs.active.val;
    res.active.nbits = active.nbits;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "operand 1 of XOR" << *this << std::endl;
    lg() << "operand 2 of XOR" << rhs << std::endl;
    lg() << "result of XOR " << res << std::endl;
#endif
} // ibis::bitvector64::xor_d2

// assuming *this is uncompressed but rhs is compressed, this function
// performs the XOR operation and overwrites *this with the result
void ibis::bitvector64::xor_d1(const ibis::bitvector64& rhs) {
    m_vec.nosharing();
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "operand 1 of XOR" << *this << std::endl;
        lg() << "operand 2 of XOR" << rhs;
    }
#endif
    if (rhs.m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = rhs.m_vec.begin();
        if (*it > HEADER1) { // complement every bit
            for (array_t<word_t>::iterator i=m_vec.begin(); i!=m_vec.end();
                 ++i) {
                if (*i > ALLONES)
                    *i ^= FILLBIT;
                else
                    *i ^= ALLONES;
            }
            if (nset > 0)
                nset = nbits - nset;
        }
        else if (*it <= ALLONES) {
            m_vec[0] = (*it ^ *(rhs.m_vec.begin()));
            nset = cnt_ones(m_vec[0]);
        }
    }
    else if (rhs.m_vec.size() > 1) {
        nset = 0;
        word_t s0;
        array_t<word_t>::iterator i0 = m_vec.begin();
        array_t<word_t>::const_iterator i1 = rhs.m_vec.begin();
        while (i1 != rhs.m_vec.end()) { // go through all words in m_vec
            if (*i1 > ALLONES) { // i1 is compressed
                s0 = ((*i1) & MAXCNT);
                if ((*i1) >= HEADER1) { // the result is the complement of i0
                    array_t<word_t>::const_iterator stp = i0 + s0;
                    while (i0 < stp) {
                        *i0 ^= ALLONES;
                        ++ i0;
                    }
                }
                else { // the result is *i0
                    i0 += s0;
                }
            }
            else { // both words are not compressed
                *i0 ^= *i1;
                ++ i0;
            }
            ++ i1;
        } // while (i1 != rhs.m_vec.end())

        if (i0 != m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::xor_d1 "
                                   "expects to exhaust i0 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(m_vec.end() - i0));
            throw "xor_d1 internal error" IBIS_FILE_LINE;
        }
    }

    // the last thing -- work with the two active_words
    active.val ^= rhs.active.val;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "result of XOR" << *this;
    }
#endif
} // ibis::bitvector64::xor_d1

// both operands of the 'xor' operation are not compressed
void ibis::bitvector64::xor_c0(const ibis::bitvector64& rhs) {
    nset = 0;
    m_vec.nosharing();
    array_t<word_t>::iterator i0 = m_vec.begin();
    array_t<word_t>::const_iterator i1 = rhs.m_vec.begin();
    while (i0 != m_vec.end()) { // go through all words in m_vec
        *i0 ^= *i1;
        i0++; i1++;
    } // while (i0 != m_vec.end())

    // the last thing -- work with the two active_words
    active.val ^= rhs.active.val;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "result XOR " << *this;
#endif
} // ibis::bitvector64::xor_c0

// bitwise minus (&!) operation -- both operands may contain compressed words
void ibis::bitvector64::minus_c2(const ibis::bitvector64& rhs,
                                 ibis::bitvector64& res) const {
    res.clear();
    if (m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = m_vec.begin();
        if (*it > HEADER1) {
            res.m_vec.resize(rhs.m_vec.size());
            for (word_t i=0; i<rhs.m_vec.size(); ++i) {
                if (rhs.m_vec[i] > ALLONES)
                    res.m_vec[i] = (rhs.m_vec[i] ^ FILLBIT);
                else
                    res.m_vec[i] = (rhs.m_vec[i] ^ ALLONES);
            }
            res.nbits = rhs.nbits;
            res.nset = rhs.nbits - rhs.nset;
        }
        else if (*it > ALLONES) {
            res.m_vec.deepCopy(m_vec);
            res.nbits = nbits;
            res.nset = 0;
        }
        else {
            res.m_vec.push_back(*it & ~(*(rhs.m_vec.begin())));
            res.nbits = nbits;
        }
    }
    else if (rhs.m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = rhs.m_vec.begin();
        if (*it > HEADER1) {
            res.appendFill(0, rhs.nbits);
        }
        else if (*it > ALLONES) {
            res.m_vec.deepCopy(m_vec);
            res.nbits = nbits;
            res.nset = nset;
        }
        else {
            res.m_vec.push_back(*it & *(m_vec.begin()));
            res.nbits = nbits;
        }
    }
    else if (m_vec.size() > 1) {
        run x, y;
        x.it = m_vec.begin();
        y.it = rhs.m_vec.begin();
        while (x.it < m_vec.end()) {    // go through all words in m_vec
            if (x.nWords == 0)
                x.decode();
            if (y.nWords == 0)
                y.decode();
            if (x.nWords == 0 || y.nWords == 0) {
                while (x.nWords == 0 && x.it < m_vec.end()) {
                    ++ x.it;
                    x.decode();
                }
                while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                    ++ y.it;
                    y.decode();
                }
                if (x.nWords == 0 || y.nWords == 0) {
                    while (x.nWords == 0 && x.it < m_vec.end()) {
                        ++ x.it;
                        x.decode();
                    }
                    while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                        ++ y.it;
                        y.decode();
                    }
                    LOGGER(ibis::gVerbose >= 0 &&
                           (x.nWords == 0 || y.nWords == 0))
                        << "ERROR bitvector64::minus_c2 -- serious problem ...";
                }
            }
            if (x.isFill) {
                if (y.isFill && y.nWords >= x.nWords) {
                    if (y.fillBit == 0) {
                        res.copy_runs(x, y.nWords);
                        y.it += (y.nWords == 0);
                    }
                    else {
                        res.append_counter(0, y.nWords);
                        x -= y.nWords;
                        y.nWords = 0;
                        ++ y.it;
                    }
                }
                else if (x.fillBit == 0) {
                    res.append_counter(0, x.nWords);
                    y -= x.nWords;
                    x.nWords = 0;
                    ++ x.it;
                }
                else {
                    res.copy_runsn(y, x.nWords);
                    x.it += (x.nWords == 0);
                }
            }
            else if (y.isFill) {            // y is compressed but not x
                if (y.fillBit == 0) {
                    res.copy_runs(x, y.nWords);
                    y.it += (y.nWords == 0);
                }
                else {
                    res.append_counter(0, y.nWords);
                    x -= y.nWords;
                    y.nWords = 0;
                    ++ y.it;
                }
            }
            else { // both words are not compressed
                res.active.val = *x.it & ~(*y.it);
                res.append_active();
                x.nWords = 0;
                y.nWords = 0;
                ++ x.it;
                ++ y.it;
            }
        } // while (x.it < m_vec.end())

        if (x.it != m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::minus_c2 "
                                   "expects to exhaust i0 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(m_vec.end() - x.it));
            throw "minus_c2 internal error" IBIS_FILE_LINE;
        }

        if (y.it != rhs.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::minus_c2 "
                                   "expects to exhaust i1 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(rhs.m_vec.end() - y.it));
            throw "minus_c2 internal error" IBIS_FILE_LINE;
        }
    }

    // the last thing -- work with the two active_words
    res.active.val = active.val & ~(rhs.active.val);
    res.active.nbits = active.nbits;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "result MINUS " << res;
#endif
} // ibis::bitvector64::minus_c2

// minus operation where rhs is not compressed (not one word is a counter)
void ibis::bitvector64::minus_c1(const ibis::bitvector64& rhs,
                                 ibis::bitvector64& res) const {
    res.clear();
    if (m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = m_vec.begin();
        if (*it > HEADER1) {
            res.m_vec.resize(rhs.m_vec.size());
            for (word_t i=0; i<rhs.m_vec.size(); ++i)
                res.m_vec[i] = (rhs.m_vec[i] ^ ALLONES);
            res.nbits = rhs.nbits;
            res.nset = rhs.nbits - rhs.nset;
        }
        else if (*it > ALLONES) {
            res.m_vec.deepCopy(m_vec);
            res.nbits = nbits;
            res.nset = 0;
        }
        else {
            res.m_vec.push_back(*it & ~(*(rhs.m_vec.begin())));
            res.nbits = nbits;
        }
    }
    else if (m_vec.size() > 1) {
        array_t<word_t>::const_iterator i0 = m_vec.begin();
        array_t<word_t>::const_iterator i1 = rhs.m_vec.begin(), i2;
        word_t s0;
        res.clear();
        res.m_vec.reserve(rhs.m_vec.size());
        while (i0 != m_vec.end()) {     // go through all words in m_vec
            if (*i0 > ALLONES) { // i0 is compressed
                s0 = ((*i0) & MAXCNT);
                i2 = i1 + s0;
                if ((*i0)>=HEADER1) { // the result is the compliment of i1
                    for (; i1 < i2; ++i1)
                        res.m_vec.push_back((*i1) ^ ALLONES);
                    res.nbits += s0*MAXBITS;
                }
                else { // the result is a fill of zero bits
                    i1 = i2;
                    if (s0 > 1)
                        res.append_counter(0, s0);
                    else
                        res.append_active();
                }
            }
            else { // both words are not compressed
                res.active.val = *i0 & ~(*i1);
                res.append_active();
                i1++;
            }
            i0++;
        } // while (i0 != m_vec.end())

        if (i1 != rhs.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::minus_c1 "
                                   "expects to exhaust i1 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(rhs.m_vec.end() - i1));
            throw "minus_c1 internal error" IBIS_FILE_LINE;
        }
    }

    // the last thing -- work with the two active_words
    res.active.val = active.val & ~(rhs.active.val);
    res.active.nbits = active.nbits;

#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "result MINUS " << res;
#endif
} // ibis::bitvector64::minus_c1

// minus operation where *this is not compressed (not one word is a counter)
void ibis::bitvector64::minus_c1x(const ibis::bitvector64& rhs,
                                  ibis::bitvector64& res) const {
    array_t<word_t>::const_iterator i0 = m_vec.begin();
    array_t<word_t>::const_iterator i1 = rhs.m_vec.begin(), i2;
    word_t s0;
    res.clear();
    res.m_vec.reserve(rhs.m_vec.size());
    while (i1 != rhs.m_vec.end()) {     // go through all words in m_vec
        if (*i1 > ALLONES) { // i1 is compressed
            s0 = ((*i1) & MAXCNT);
            i2 = i0 + s0;
            if ((*i1) >= HEADER1) {     // the result is a fill of zero bits
                i0 = i2;
                if (s0 > 1)
                    res.append_counter(0, s0);
                else
                    res.append_active();
            }
            else {      // the result is i0
                for (; i0 < i2; ++i0)
                    res.m_vec.push_back(*i0);
                res.nbits += s0*MAXBITS;
            }
        }
        else {  // both words are not compressed
            res.active.val = *i0 & ~(*i1);
            res.append_active();
            i0++;
        }
        i1++;
    } // while (i1 != rhs.m_vec.end())

    if (i0 != m_vec.end()) {
        ibis::util::logMessage("Error", "bitvector64::minus_c1x expects "
                               "to exhaust i0 but there are %ld word(s) left",
                               static_cast<long>(m_vec.end() - i0));
        throw "minus_c1x internal error" IBIS_FILE_LINE;
    }

    // the last thing -- work with the two active_words
    res.active.val = active.val & ~(rhs.active.val);
    res.active.nbits = active.nbits;

#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "result MINUS " << res;
#endif
} // ibis::bitvector64::minus_c1x

// minus operation on two compressed bitvector64s, it differs from minus_c2 in
// that this one generates a uncompressed result
void ibis::bitvector64::minus_d2(const ibis::bitvector64& rhs,
                                 ibis::bitvector64& res) const {
    // establish a uncompressed bitvector64 with the right size
    res.nbits = ((nbits==0 && m_vec.size()>0) ?
                 (rhs.nbits == 0 && rhs.m_vec.size()>0) ? 
                 (m_vec.size() <= rhs.m_vec.size() ? do_cnt() : rhs.do_cnt())
                 : rhs.nbits : nbits);
    res.m_vec.resize(res.nbits/MAXBITS);

    // fill res with the right numbers
    if (m_vec.size() == 1) { // only one word in *this
        array_t<word_t>::const_iterator it = m_vec.begin();
        if (*it > HEADER1) { // complement of rhs
            rhs.copy_comp(res.m_vec);
            if (rhs.nset > 0) res.nset = nbits - rhs.nset;
        }
        else if (*it > ALLONES) { // all zero
            decompress(res.m_vec);
            res.nset = 0;
        }
        else {
            res.m_vec[0] = (*it & ~*(rhs.m_vec.begin()));
            res.nset = cnt_ones(res.m_vec[0]);
        }
    }
    else if (rhs.m_vec.size() == 1) { // only one word in rhs
        array_t<word_t>::const_iterator it = rhs.m_vec.begin();
        if (*it > HEADER1) { // all zero
            (void) memset(res.m_vec.begin(), 0,
                          res.m_vec.size()*sizeof(word_t));
            res.nset = 0;
        }
        else if (*it > ALLONES) { // copy *this
            res.m_vec.deepCopy(m_vec);
            res.nset = nset;
        }
        else {
            res.m_vec[0] = (*it & ~*(m_vec.begin()));
            res.nset = cnt_ones(res.m_vec[0]);
        }
    }
    else if (m_vec.size() > 1) { // more than one word in *this
        run x, y;
        res.nset = 0;
        x.it = m_vec.begin();
        y.it = rhs.m_vec.begin();
        array_t<word_t>::iterator ir = res.m_vec.begin();
        while (x.it < m_vec.end()) {    // go through all words in m_vec
            if (x.nWords == 0)
                x.decode();
            if (y.nWords == 0)
                y.decode();
            if (x.nWords == 0 || y.nWords == 0) {
                while (x.nWords == 0 && x.it < m_vec.end()) {
                    ++ x.it;
                    x.decode();
                }
                while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                    ++ y.it;
                    y.decode();
                }
                if (x.nWords == 0 || y.nWords == 0) {
                    while (x.nWords == 0 && x.it < m_vec.end()) {
                        ++ x.it;
                        x.decode();
                    }
                    while (y.nWords == 0 && y.it < rhs.m_vec.end()) {
                        ++ y.it;
                        y.decode();
                    }
                    LOGGER(ibis::gVerbose >= 0 &&
                           (x.nWords == 0 || y.nWords == 0))
                        << "ERROR bitvector64::minus_d2 -- serious problem ...";
                }
            }
            if (x.isFill) {
                if (y.isFill && y.nWords >= x.nWords) {
                    if (y.fillBit == 0) {
                        res.copy_runs(ir, x, y.nWords);
                        y.it += (y.nWords == 0);
                    }
                    else {
                        x -= y.nWords;
                        y.fillBit = 0;
                        res.copy_fill(ir, y);
                    }
                }
                else if (x.fillBit == 0) {
                    y -= x.nWords;
                    res.copy_fill(ir, x);
                }
                else {
                    res.copy_runsn(ir, y, x.nWords);
                    x.it += (x.nWords == 0);
                }
            }
            else if (y.isFill) {
                if (y.fillBit == 0) {
                    res.copy_runs(ir, x, y.nWords);
                    y.it += (y.nWords == 0);
                }
                else {
                    x -= y.nWords;
                    y.fillBit = 0;
                    res.copy_fill(ir, y);
                }
            }
            else { // both words are not compressed
                *ir = *x.it & ~*y.it;
                x.nWords = 0;
                y.nWords = 0;
                ++ x.it;
                ++ y.it;
                ++ ir;
            }
        } // while (x.it < m_vec.end())

        if (x.it != m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::minus_d2 "
                                   "expects to exhaust i0 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(m_vec.end() - x.it));
            throw "minus_d2 internal error" IBIS_FILE_LINE;
        }

        if (y.it != rhs.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::minus_d2 "
                                   "expects to exhaust i1 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(rhs.m_vec.end() - y.it));
            throw "minus_d2 internal error" IBIS_FILE_LINE;
        }

        if (ir != res.m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::minus_d2 "
                                   "expects to exhaust ir but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(res.m_vec.end() - ir));
            throw "minus_d2 internal error" IBIS_FILE_LINE;
        }
    }

    // work with the two active_words
    res.active.val = active.val & ~rhs.active.val;
    res.active.nbits = active.nbits;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "operand 1 of MINUS" << *this << std::endl;
    lg() << "operand 2 of MINUS" << rhs << std::endl;
    lg() << "result of MINUS " << res;
#endif
} // ibis::bitvector64::minus_d2

// assuming *this is uncompressed but rhs is not, this function performs the
// MINUS operation and overwrites *this with the result
void ibis::bitvector64::minus_d1(const ibis::bitvector64& rhs) {
    m_vec.nosharing();
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "operand 1 of MINUS" << *this << std::endl;
        lg() << "operand 2 of MINUS" << rhs;
    }
#endif
    if (rhs.m_vec.size() == 1) {
        array_t<word_t>::const_iterator it = rhs.m_vec.begin();
        if (*it > HEADER1) { // 0-fill
            (void) memset(m_vec.begin(), 0,
                          m_vec.size()*sizeof(word_t));
            nset = 0;
        }
        else if (*it <= ALLONES) {
            m_vec[0] = (*it & ~*(rhs.m_vec.begin()));
            nset = cnt_ones(m_vec[0]);
        }
    }
    else if (rhs.m_vec.size() > 1) {
        nset = 0;
        word_t s0;
        array_t<word_t>::iterator i0 = m_vec.begin();
        array_t<word_t>::const_iterator i1 = rhs.m_vec.begin();
        while (i1 != rhs.m_vec.end()) { // go through all words in m_vec
            if (*i1 > ALLONES) { // i1 is compressed
                s0 = ((*i1) & MAXCNT);
                if ((*i1) >= HEADER1) { // the result is 0
                    memset(i0, 0, sizeof(word_t)*s0);
                }
                i0 += s0;
            }
            else { // both words are not compressed
                *i0 &= ~*i1;
                ++ i0;
            }
            ++ i1;
        } // while (i1 != rhs.m_vec.end())

        if (i0 != m_vec.end()) {
            ibis::util::logMessage("Error", "bitvector64::minus_d1 "
                                   "expects to exhaust i0 but there are %ld "
                                   "word(s) left",
                                   static_cast<long>(m_vec.end() - i0));
            throw "minus_d1 internal error" IBIS_FILE_LINE;
        }
    }

    // the last thing -- work with the two active_words
    active.val &= ~rhs.active.val;
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "result of MINUS" << *this;
    }
#endif
} // ibis::bitvector64::minus_d1

// both operands of the 'minus' operation are not compressed
void ibis::bitvector64::minus_c0(const ibis::bitvector64& rhs) {
    nset = 0;
    m_vec.nosharing();
    array_t<word_t>::iterator i0 = m_vec.begin();
    array_t<word_t>::const_iterator i1 = rhs.m_vec.begin();
    while (i0 != m_vec.end()) { // go through all words in m_vec
        *i0 &= ~(*i1);
        i0++; i1++;
    } // while (i0 != m_vec.end())

    // the last thing -- work with the two active_words
    active.val &= ~(rhs.active.val);
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    ibis::util::logger lg(4);
    lg() << "result MINUS " << *this;
#endif
} // ibis::bitvector64::minus_c0

// assignment operator for ibis::bitvector64::iterator
// ********************IMPORTANT********************
// operator= modifies the content of the bitvector64 it points to and it can
// invalidate other iterator or const_iterator referring to the same
// bitvector64.
ibis::bitvector64::iterator& ibis::bitvector64::iterator::operator=(int val) {
    if (it > vec->end()) { // end
        ibis::util::logMessage("Warning", "attempting to assign value to an "
                               "invalid bitvector64::iterator");
        return *this;
    }
    if (static_cast<bool>(val) == operator*()) {
        return *this; // no change
    }
    if (it == vec->end()) { // modify the active bit
        if (val != 0) {
            active->val |= (1LL << (active->nbits - ind - 1));
        }
        else {
            active->val &= ~(1LL << (active->nbits - ind - 1));
        }
        return *this;
    }

    /////////////////////////////////////
    // the bit to be modified is in m_vec
    //
    if (compressed == 0) { // toggle a single bit of a literal word
        *it ^= (1LL << (ibis::bitvector64::SECONDBIT - ind));
    }
    else if (ind < ibis::bitvector64::MAXBITS) {
        // bit to be modified is in the first word, two pieces
        -- (*it);
        if ((*it & ibis::bitvector64::MAXCNT) == 1)
            *it = (val)?0:ibis::bitvector64::ALLONES;
        word_t w = (1LL << (ibis::bitvector64::SECONDBIT-ind));
        if (val == 0) w ^= ibis::bitvector64::ALLONES;
        it = vec->insert(it, w);
    }
    else if (nbits - ind <= ibis::bitvector64::MAXBITS) {
        // bit to be modified is in the last word, two pieces
        -- (*it);
        if ((*it & ibis::bitvector64::MAXCNT) == 1)
            *it = (val)?0:ibis::bitvector64::ALLONES;
        word_t w = 1LL << (nbits-ind-1);
        if (val == 0) w ^= ibis::bitvector64::ALLONES;
        ++ it;
        it = vec->insert(it, w);
    }
    else { // the counter breaks into three pieces
        word_t u[2], w;
        u[0] = ind / MAXBITS;
        w = (*it & MAXCNT) - u[0] - 1;
        u[1] = 1LL << (SECONDBIT - ind + u[0]*MAXBITS);
        if (val==0) {
            u[0] = (u[0]>1)?(HEADER1|u[0]):(ALLONES);
            u[1] ^= ibis::bitvector64::ALLONES;
            w = (w>1)?(HEADER1|w):(ALLONES);
        }
        else {
            u[0] = (*u>1)?(HEADER0|u[0]):static_cast<word_t>(0);
            w = (w>1)?(HEADER0|w):static_cast<word_t>(0);
        }
        *it = w;
        // NOTE: std::vector::iterator can not be subtracted like this
        w = it - vec->begin();
        vec->insert(it, u, u+2);
        ++ w;
        it = vec->begin() + w;//std::vector::iterator can't be added like this
    }

    // restore the current iterator and the referred bitvector64 to the correct
    // states
    ind = ind % ibis::bitvector64::MAXBITS;
    nbits = ibis::bitvector64::MAXBITS;
    literalvalue = *it;
    compressed = 0;
    if (bitv->nset) bitv->nset += val?1:-1;
    return *this;
} // ibis::bitvector64::iterator::operator=

// advance position with a longer stride
ibis::bitvector64::iterator&
ibis::bitvector64::iterator::operator+=(int64_t incr) {
    if (incr < 0) { // advance backword
        if (ind >= static_cast<word_t>(-incr)) {
            ind += incr;
        }
        else { // need to move on the previous word
            int64_t incr0 = incr + ind;
            while (incr0 < 0 && it > vec->begin()) {
                --it;
                decodeWord();
                if (nbits >= static_cast<word_t>(-incr0)) {
                    ind = nbits + incr0;
                    incr0 = 0;
                }
                else {
                    incr0 += nbits;
                }
            }
            if (incr0 < 0) {
                ibis::util::logger lg(0);
                lg() << "Warning -- "
                    "bitvector64::iterator::operator+=("
                          << incr << ") passes the beginning of the "
                          << "bit sequence";
            }
        }
    }
    else if (incr > 0) { // advance forward
        if (ind+incr < nbits) {
            ind += incr;
        }
        else { // need to move on to the next word
            int64_t incr1 = incr + ind - nbits;
            while (incr1 >= 0 && it < vec->end()) {
                ++it;
                decodeWord();
                if (nbits > (word_t)incr1) {
                    ind   = incr1;
                    incr1 = INT_MIN;
                }
                else {
                    incr1 -= nbits;
                }
            }
            if (incr1 > 0) {
                ibis::util::logger lg(0);
                lg() << "Warning -- "
                    "bitvector64::iterator::operator+=("
                          << incr << ") passes the end of the "
                          << "bit sequence";
            }
        }
    }

    return *this;
} // ibis::bitvector64::iterator::operator+=

// decode the word pointed by it;
// when it is out of designated range, it returns end()
void ibis::bitvector64::iterator::decodeWord() {
    if (it < vec->end() && it >= vec->begin()) {
        // deal with the normal case first
        if (*it > ibis::bitvector64::HEADER1) {
            fillbit = 1;
            compressed = 1;
            nbits = ((*it) & MAXCNT) * MAXBITS;
        }
        else if (*it > ibis::bitvector64::HEADER0) {
            fillbit = 0;
            compressed = 1;
            nbits = ((*it) & MAXCNT) * MAXBITS;
        }
        else {
            compressed = 0;
            nbits = MAXBITS;
            literalvalue = *it;
        }
    }
    else if (it == vec->end()) { // return active word
        compressed = 0;
        nbits = active->nbits;
        literalvalue = (active->val << (MAXBITS - nbits));
        it += (nbits == 0);
    }
    else { // it is out of valid range -- treat it as the end
        it = vec->end() + 1;
        compressed = 0;
        nbits = 0;
        literalvalue = 0;
        fillbit = 0;
    }
    ind = 0;
} // ibis::bitvector64::iterator::decodeWord

// advance position with a longer stride
ibis::bitvector64::const_iterator&
ibis::bitvector64::const_iterator::operator+=(int64_t incr) {
    if (incr < 0) { // advance backword
        if (ind >= static_cast<word_t>(-incr)) {
            ind += incr;
        }
        else { // need to move on the previous word
            int64_t incr0 = incr + ind;
            while (incr0 < 0 && it > begin) {
                --it;
                decodeWord();
                if (nbits >= static_cast<word_t>(-incr0)) {
                    ind = nbits + incr0;
                    incr0 = 0;
                }
                else {
                    incr0 += nbits;
                }
            }
            if (incr0 < 0) {
                ibis::util::logger lg;
                lg() << "Warning -- bitvector64::const_iterator::"
                          << "operator+=(" << incr
                          << ") passes the beginning of the bit sequence";
            }
        }
    }
    else if (incr > 0) { // advance forward
        if (ind+incr < nbits) {
            ind += incr;
        }
        else { // need to move on to the next word
            int64_t incr1 = incr + ind - nbits;
            while (incr1 >= 0 && it < end) {
                ++it;
                decodeWord();
                if (nbits > (word_t)incr1) {
                    ind   = incr1;
                    incr1 = INT_MIN;
                }
                else {
                    incr1 -= nbits;
                }
            }
            LOGGER(incr1 > 0)
                << "Warning -- bitvector64::const_iterator::"
                << "operator+=(" << incr
                << ") passes the end of the bit sequence";
        }
    }

    return *this;
} // ibis::bitvector64::const_iterator::operator+=

// decode the word pointed by it;
// when it is out of designated range, it returns end()
void ibis::bitvector64::const_iterator::decodeWord() {
    if (it < end && it >= begin) { // deal with the normal case first
        if (*it > ibis::bitvector64::HEADER1) {
            fillbit = 1;
            compressed = 1;
            nbits = ((*it) & MAXCNT) * MAXBITS;
        }
        else if (*it > ibis::bitvector64::HEADER0) {
            fillbit = 0;
            compressed = 1;
            nbits = ((*it) & MAXCNT) * MAXBITS;
        }
        else {
            compressed = 0;
            nbits = MAXBITS;
            literalvalue = *it;
        }
    }
    else if (it == end) { // return active word
        compressed = 0;
        nbits = active->nbits;
        literalvalue = (active->val << (MAXBITS - nbits));
        it += (nbits == 0);
    }
    else { // it is out of valid range -- treat it as the end
        it = end + 1;
        compressed = 0; nbits = 0; literalvalue = 0; fillbit = 0;
    }
    ind = 0;
} // ibis::bitvector64::const_iterator::decodeWord

// advance to the next code word that is not zero
ibis::bitvector64::indexSet& ibis::bitvector64::indexSet::operator++() {
    if (it > end) { // already at the end
        nind = 0;
        return *this;
    }
    //     if (it != 0 && it == end) { // reaching the end
    //  ++ it;
    //  nind = 0;
    //  return *this;
    //     }

    // the index of the next position
    word_t index0 = ((ind[0]+(nind>MAXBITS?nind:MAXBITS)) / MAXBITS)
        * MAXBITS;

    // skip to the next code word containing any ones
    ++it; // examining the next word
    nind = 0;
    while (it < end) {
        if (*it >= HEADER1) { // 1-fill
            nind = ((*it) & MAXCNT) * MAXBITS;
            ind[1] = index0 + nind;
            ind[0] = index0;
            return *this;
        }
        else if (*it >= HEADER0) { // 0-fill
            index0 += ((*it) & MAXCNT) * MAXBITS;
            ++it;
        }
        else if (*it > 0) { // non-zero literal word
            if (*it < ALLONES) {
                word_t i, j = (*it<<1);
                for (i=0; j>0; ++i, j <<= 1) {
                    if (j > ALLONES) {
                        ind[nind] = index0 + i;
                        ++ nind;
                    }
                }
            }
            else {
                nind = MAXBITS;
                ind[0] = index0;
                ind[1] = index0 + nind;
                // for (int i=0; i<MAXBITS; ++i) ind[i] = index0+i;
            }
            return *this;
        }
        else { // zero word
            index0 += MAXBITS;
            ++it;
        }
    } // while (it < end)

    // deal with the active word
    if (active->nbits > 0 && active->val > 0) {
        // a non-zero active word
        word_t i, j = (active->val << (MAXBITS + 1 - active->nbits));
        for (i=0; j>0; ++i, j <<= 1) {
            if (j > ALLONES) {
                ind[nind] = index0 + i;
                ++ nind;
            }
        }
    }
    it = end + 1;
    return *this;
} // ibis::bitvector64::indexSet::operator++

// Adjust the size of bit sequence. if current size is less than nv,
// append enough 1 bits so that it has nv bits.  If the resulting total
// number of bits is less than nt, append 0 bits to that there are nt
// total bits.  The final result always contains nt bits.
void ibis::bitvector64::adjustSize(word_t nv, word_t nt) {
    if (nbits < m_vec.size() * MAXBITS)
        nbits = do_cnt();
    if (size() == nt) return;
    m_vec.nosharing();

    if (nv > nt)
        nv = nt;
    if (size() < nv)
        appendFill(1, nv - size());
    if (size() < nt) {
        appendFill(0, nt - size());
    }
    else if (size() > nt) {
        erase(nt, size());
    }
} // ibis::bitvector64::adjustSize

// estimate the clustering factor based on the size information of a bitmap
double ibis::bitvector64::clusteringFactor(word_t nb, word_t nc, word_t sz) {
    double f = 1.0;
    if (nb > 0 && nc > 0 && nb >= nc) {
        const int tw3 = MAXBITS + MAXBITS - 3;
        const double den = static_cast<double>(nc) /
            static_cast<double>(nb);
        const word_t nw = (nb > SECONDBIT ? nb / SECONDBIT - 1 : 0);
        const double f0 = (den > 0.5 ? den / (1 - den) : 1.0); // lower bound
        const double sz1 = 3.0 + nw - sz / sizeof(word_t);
        double ds = 0.0;
        f = f0;
#if DEBUG+0 > 0 || _DEBUG+0 > 0
        LOGGER(ibis::gVerbose >= 0)
            << "bitvector64:clusteringFactor(" << nb << ", " << nc
            << ", " << sz << "): sz=" << sz/sizeof(word_t) << ", den = "
            << den << ", nw = " << nw;
#endif
        do {
            // This is a simple combination of the Newton's method and the
            // secant method for finding a root.  It use the Newton's method
            // to find the second point and then use the two point to
            // perform an extrapolation (secant method).
            double f2, ds2;
            ds = sz1 - nw *
                ((1.0-den) * pow(1.0-den/((1.0-den)*f), tw3) +
                 den * pow(1.0-1.0/f, tw3));
            double deri = (tw3 * nw * den / (f*f)) *
                (pow(1.0-den/((1.0-den)*f), tw3-1) +
                 pow(1.0-1.0/f, tw3-1));
            if (deri != 0) {
                ds2 = ds / deri;
                if (f + ds2 > f0) { // a valid value
                    f2 = f + ds2;
                }
                else { // overshoting
                    f2 = sqrt(f0 * f);
                }
            }
            else { // zero derivative, try something smaller
                f2 = sqrt(f0 * f);
            }
            ds2 = sz1 - nw *
                ((1.0-den) * pow(1.0-den/((1.0-den)*f2), tw3) +
                 den * pow(1.0-1.0/f2, tw3));
#if DEBUG+0 > 0 || _DEBUG+0 > 0
            LOGGER(ibis::gVerbose >= 0)
                << "bitvector64:clusteringFactor(" << nb
                << ", " << nc << ", " << sz << "): computed size="
                << (ds + sz/sizeof(word_t)) << ", ds = "
                << ds << ", deri = " << deri << ", f = "
                << f << ", ds2 = " << ds2 << ", f2 = " << f2;
#endif
#if defined(QUARDRATIC_INTERPOLATION_FOR_CLUSTERING_FACTOR)
            // quardratic extrapolation does not work well, likely because
            // (1) have not found a strategy to decide which one of the two
            // solutions to use
            // (2) it relies on the first point more than the second point,
            // because it uses the function value and the derivative of the
            // first point but only use the function value of the second
            // point.  This biases the extrapolation too much to the first
            // point and reduces its effectiveness.
            if (f != f2) { // quardratic extrapolation
                double tmp, a, b, c;
                c = (f - f2);
                a = - deri - (ds - ds2) / c;
                if (a != 0) {
                    a /= c;
                    b = - deri - 2.0 * a * f;
                    c = ds - (a*f + b) * f;
                    if (b*b > 4.0*a*c) { // quardratic formula has solutions
                        c = 0.5 * sqrt(b*b - 4.0*a*c) / a;
                        b = -0.5 * b / a;
                        a = b - c;
                        b += c;
                        if (a > b) {
                            c = a;
                            a = b;
                            b = c;
                        }
#if DEBUG+0 > 0 || _DEBUG+0 > 0
                        LOGGER(ibis::gVerbose >= 0)
                            << "a = " << a << ", b = " << b
                            << ", linear extrapolation = "
                            << f - (f - f2) * ds / (ds - ds2);
#endif
                        //if (a < f0) tmp = b;
                        //else tmp = a;
                        tmp = b;
                    }
                    else { // use linear interpolation
                        tmp = f - (f - f2) * ds / (ds - ds2);
                    }
                    if (tmp > f0) f2 = tmp;
                    else f2 = sqrt(f0 * f2);
                }
            }
#else
            if (ds != ds2) { // use linear interpolation
                double tmp = f - (f - f2) * ds / (ds - ds2);
                if (tmp > f0) f2 = tmp;
                else f2 = sqrt(f0 * f2);
            }
            else {
                f2 = 0.5 *(f + f2);
            }
#endif
            ds = f2 - f;
            f = f2;
        } while (fabs(ds) > 1e-4*f);
    }
    return f;
} // ibis::bitvector64::clusteringFactor

/// This implementation only uses public functions of ibis::bitvector and
/// ibis::bitvector64.  This should make it possible to use both WAH and
/// BBC compress bitvector classes.  It is not likely that we can gain much
/// performance by directly using member variables of ibit::bitvector
/// because the unit of compressed data from ibit::vector do not fit neatly
/// into ibis::bitvector64::word_t.
///
/// Return a const reference of @c c.  If the input @c does not have
/// the correct size, it will be replaced by the outer product.
const ibis::bitvector64&
ibis::util::outerProduct(const ibis::bitvector& a,
                         const ibis::bitvector& b,
                         ibis::bitvector64& c) {
    if (b.cnt() == 0) // do nothing, empty a will be detected later
        return c;
    ibis::horometer timer;
    if (ibis::gVerbose > 2)
        timer.start();

    const unsigned nb = b.size();
    ibis::bitvector64 tmp;
    for (ibis::bitvector::indexSet aix = a.firstIndexSet();
         aix.nIndices() > 0; ++ aix) {
        const ibis::bitvector::word_t *ind1 = aix.indices();
        if (aix.isRange()) {
            for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                const ibis::bitvector64::word_t start =
                    static_cast<ibis::bitvector64::word_t>(i) * nb;
                for (ibis::bitvector::indexSet bix = b.firstIndexSet();
                     bix.nIndices() > 0; ++ bix) {
                    const ibis::bitvector::word_t *ind2 = bix.indices();
                    if (bix.isRange()) {
                        tmp.adjustSize(0, start + *ind2);
                        tmp.appendFill(1, bix.nIndices());
                    }
                    else {
                        for (uint32_t j = 0; j < bix.nIndices(); ++ j) {
                            tmp.setBit(start + ind2[j], 1);
                        }
                    }
                }
            }
        }
        else {
            for (uint32_t i = 0; i < aix.nIndices(); ++ i) {
                const ibis::bitvector64::word_t start =
                    static_cast<ibis::bitvector64::word_t>(ind1[i]) * nb;
                for (ibis::bitvector::indexSet bix = b.firstIndexSet();
                     bix.nIndices() > 0; ++ bix) {
                    const ibis::bitvector::word_t *ind2 = bix.indices();
                    if (bix.isRange()) {
                        tmp.adjustSize(0, start + *ind2);
                        tmp.appendFill(1, bix.nIndices());
                    }
                    else {
                        for (uint32_t j = 0; j < bix.nIndices(); ++ j) {
                            tmp.setBit(start + ind2[j], 1);
                        }
                    }
                }
            }
        }
    }

    // make sure it has the right size
    uint64_t oldcnt = 0;
    tmp.adjustSize(0, static_cast<ibis::bitvector64::word_t>(nb)*a.size());
    if (c.size() == tmp.size()) {
        oldcnt = c.cnt();
        c |= tmp;
    }
    else {
        c.swap(tmp);
    }
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg(4);
        uint64_t expected = a.cnt();
        expected *= b.cnt();
        uint64_t diff = c.cnt() - oldcnt;
        lg() << "util::outerProduct: adding the outer product "
            "between two bitvectors with "
                    << a.cnt() << " out of " << a.size() << " set bits and "
                    << b.cnt() << " out of " << b.size()
                    << " set bits produced " << c.cnt()
                    << " set bits in a bitvector of " << c.bytes()
                    << " bytes\n\t";
        if (diff == expected)
            lg() << "All additional entries (" << expected
                        << ") are new";
        else
            lg() << "Expected " << expected
                      << (expected > 1 ? " new entries" : " new entry")
                      << " but got " << diff;
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "DEBUG -- util::outerProduct\na" << a
                  << "\nb" << b << "\nresult" << c;
    }
#endif
    return c;
} // ibis::util::outerProduct

/// The result @c contains only the strict upper triangular portion of the
/// full outer product.
const ibis::bitvector64&
ibis::util::outerProductUpper(const ibis::bitvector& a,
                              const ibis::bitvector& b,
                              ibis::bitvector64& c) {
    if (b.cnt() == 0) // do nothing, empty a will be detected later
        return c;
    ibis::horometer timer;
    if (ibis::gVerbose > 2)
        timer.start();

    const unsigned nb = b.size();
    ibis::bitvector64 tmp;
    for (ibis::bitvector::indexSet aix = a.firstIndexSet();
         aix.nIndices() > 0; ++ aix) {
        const ibis::bitvector::word_t *ind1 = aix.indices();
        if (aix.isRange()) {
            for (unsigned i = *ind1; i < ind1[1]; ++ i) {
                const ibis::bitvector64::word_t start =
                    static_cast<ibis::bitvector64::word_t>(i) * nb;
                for (ibis::bitvector::indexSet bix = b.firstIndexSet();
                     bix.nIndices() > 0; ++ bix) {
                    const ibis::bitvector::word_t *ind2 = bix.indices();
                    if (bix.isRange()) {
                        const unsigned i1 =
                            (*ind2 > i ? *ind2 : i+1);
                        if (ind2[1] > i1) {
                            tmp.adjustSize(0, start + i1);
                            tmp.appendFill(1, ind2[1] - i1);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j < bix.nIndices(); ++ j) {
                            if (ind2[j] > i)
                                tmp.setBit(start + ind2[j], 1);
                        }
                    }
                }
            }
        }
        else {
            for (uint32_t i = 0; i < aix.nIndices(); ++ i) {
                const ibis::bitvector64::word_t start =
                    static_cast<ibis::bitvector64::word_t>(ind1[i]) * nb;
                for (ibis::bitvector::indexSet bix = b.firstIndexSet();
                     bix.nIndices() > 0; ++ bix) {
                    const ibis::bitvector::word_t *ind2 = bix.indices();
                    if (bix.isRange()) {
                        const unsigned i1 =
                            (*ind2 > ind1[i] ? *ind2 : ind1[i]+1);
                        if (ind2[1] > i1) {
                            tmp.adjustSize(0, start + i1);
                            tmp.appendFill(1, ind2[1] - i1);
                        }
                    }
                    else {
                        for (uint32_t j = 0; j < bix.nIndices(); ++ j) {
                            if (ind2[j] > ind1[i])
                                tmp.setBit(start + ind2[j], 1);
                        }
                    }
                }
            }
        }
    }

    // make sure it has the right size
    uint64_t oldcnt = 0;
    tmp.adjustSize(0, static_cast<ibis::bitvector64::word_t>(nb)*a.size());
    if (c.size() == tmp.size()) {
        oldcnt = c.cnt();
        c |= tmp;
    }
    else {
        c.swap(tmp);
    }
    if (ibis::gVerbose > 4) {
        ibis::util::logger lg(4);
        uint64_t expected = a.cnt();
        expected *= b.cnt();
        uint64_t diff = c.cnt();
        diff -= oldcnt;
        lg() << "util::outerProductUpper: adding the outer "
            "product between two bitvectors with "
                  << a.cnt() << " out of " << a.size() << " set bits and "
                  << b.cnt() << " out of " << b.size()
                  << " set bits produced " << c.cnt()
                  << " set bits in a bitvector of " << c.bytes()
                  << " bytes\n\t";
        if (diff == expected)
            lg() << "All additional entries (" << expected
                        << ") are new";
        else
            lg() << "Expected " << expected
                      << (expected > 1 ? " new entries" : " new entry")
                      << " but got " << diff;
    }
#if DEBUG+0 > 1 || _DEBUG+0 > 1
    {
        ibis::util::logger lg(4);
        lg() << "DEBUG -- util::outerProductUpper\na" << a
                    << "\nb" << b << "\nresult" << c;
    }
#endif
    return c;
} // ibis::util::outerProductUpper
