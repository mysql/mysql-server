/*
   Copyright (c) 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SectionIterators_H
#define SectionIterators_H


#include <ndb_global.h>

class NdbApiSignal;

/**
 * LinearSectionIterator
 *
 * This is an implementation of GenericSectionIterator
 * that iterates over one linear section of memory.
 * The iterator is used by the transporter at signal
 * send time to obtain all of the relevant words for the
 * signal section
 */
class LinearSectionIterator: public GenericSectionIterator
{
private :
  const Uint32* data;
  Uint32 len;
  bool read;
public :
  LinearSectionIterator(const Uint32* _data, Uint32 _len);
  ~LinearSectionIterator() override;

  void reset() override;
  const Uint32* getNextWords(Uint32& sz) override;
};


/**
 * SignalSectionIterator
 *
 * This is an implementation of GenericSectionIterator
 * that uses chained NdbApiSignal objects to store a
 * signal section.
 * The iterator is used by the transporter at signal
 * send time to obtain all of the relevant words for the
 * signal section
 */
class SignalSectionIterator: public GenericSectionIterator
{
private :
  NdbApiSignal* firstSignal;
  NdbApiSignal* currentSignal;
public :
  SignalSectionIterator(NdbApiSignal* signal);
  ~SignalSectionIterator() override;

  void reset() override;
  const Uint32* getNextWords(Uint32& sz) override;
};


/**
 * FragmentedSectionIterator
 * -------------------------
 * This class acts as an adapter to a GenericSectionIterator
 * instance, providing a sub-range iterator interface.
 * It is used when long sections of a signal are fragmented
 * across multiple actual signals - the user-supplied
 * GenericSectionIterator is then adapted into a
 * GenericSectionIterator that only returns a subset of
 * the contained words for each signal fragment.
 */
class FragmentedSectionIterator: public GenericSectionIterator
{
private :
  GenericSectionIterator* realIterator; /* Real underlying iterator */
  Uint32 realIterWords;                 /* Total size of underlying */
  Uint32 realCurrPos;                   /* Offset of start of last chunk
                                         * obtained from underlying */
  Uint32 rangeStart;                    /* Sub range start in underlying */
  Uint32 rangeLen;                      /* Sub range len in underlying */
  Uint32 rangeRemain;                   /* Remaining unconsumed words in subrange */
  const Uint32* lastReadPtr;            /* Ptr to last chunk obtained from
                                         * underlying */
  Uint32 lastReadTotal;                 /* Total read words in last chunk, starting from
                                         * lastReadPtr */
  Uint32 lastReadAvail;                 /* Remaining words (unconsumed) in last chunk
                                         * obtained from underlying */
public:
  /* Constructor
   * The instance is constructed with the sub-range set to be the
   * full range of the underlying iterator
   */
  FragmentedSectionIterator(GenericSectionPtr ptr);

private:
  /** 
   * checkInvariants
   * These class invariants must hold true at all stable states
   * of the iterator
   */
  bool checkInvariants();

  /**
   * moveToPos
   * This method is used when the iterator is reset(), to move
   * to the start of the current sub-range.
   * If the iterator is already in-position then this is efficient
   * Otherwise, it has to reset() the underling iterator and
   * advance it until the start position is reached.
   * Note that opposed to getNextWords(), moveToPos do not 'consume'
   * what is read from the underlying iterator.
   */
  void moveToPos(Uint32 pos);

public:
  /**
   * setRange
   * Set the sub-range of the iterator.  Must be within the
   * bounds of the underlying iterator
   * After the range is set, the iterator is reset() to the
   * start of the supplied subrange
   */
  bool setRange(Uint32 start, Uint32 len);

  /**
   * reset
   * (GenericSectionIterator)
   * Reset the iterator to the start of the current sub-range
   * Avoid calling as it could be expensive.
   */
  void reset() override;

  /**
   * getNextWords
   * (GenericSectionIterator)
   * Get ptr and size of next contiguous words in subrange
   */
  const Uint32* getNextWords(Uint32& sz) override;

};


#endif // SectionIterators_H
