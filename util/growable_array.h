/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef UTIL_GROWABLE_ARRAY_H
#define UTIL_GROWABLE_ARRAY_H
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <memory.h>

//******************************************************************************
//
// Overview: A growable array is a little bit like std::vector except that
//  it doesn't have constructors (hence can be used in static constructs, since
//  the google style guide says no constructors), and it's a little simpler.
// Operations:
//   init and deinit (we don't have constructors and destructors).
//   fetch_unchecked to get values out.
//   store_unchecked to put values in.
//   push to add an element at the end
//   get_size to find out the size
//   get_memory_size to find out how much memory the data stucture is using.
//
//******************************************************************************

namespace toku {

template<typename T> class GrowableArray {
 public:
    void init (void)
    // Effect: Initialize the array to contain no elements.
    {
	m_array=NULL;
	m_size=0;
	m_size_limit=0;
    }

    void deinit (void)
    // Effect: Deinitialize the array (freeing any memory it uses, for example).
    {
	toku_free(m_array);
	m_array     =NULL;
	m_size      =0;
	m_size_limit=0;
    }

    T fetch_unchecked (size_t i) const
    // Effect: Fetch the ith element.  If i is out of range, the system asserts.
    {
	return m_array[i];
    }

    void store_unchecked (size_t i, T v)
    // Effect: Store v in the ith element.  If i is out of range, the system asserts.
    {
	paranoid_invariant(i<m_size);
	m_array[i]=v;
    }

    void push (T v)
    // Effect: Add v to the end of the array (increasing the size).  The amortized cost of this operation is constant.
    // Implementation hint:  Double the size of the array when it gets too big so that the amortized cost stays constant.
    {
	if (m_size>=m_size_limit) {
	    if (m_array==NULL) {
		m_size_limit=1;
	    } else {
		m_size_limit*=2;
	    }
	    XREALLOC_N(m_size_limit, m_array);
	}
	m_array[m_size++]=v;
    }

    size_t get_size (void) const
    // Effect: Return the number of elements in the array.
    {
	return m_size;
    }
    size_t memory_size(void) const
    // Effect: Return the size (in bytes) that the array occupies in memory.  This is really only an estimate.
    {
	return sizeof(*this)+sizeof(T)*m_size_limit;
    }

 private:
    T     *m_array;
    size_t m_size;
    size_t m_size_limit; // How much space is allocated in array.
};

}

#endif // UTIL_GROWABLE_ARRAY_H
