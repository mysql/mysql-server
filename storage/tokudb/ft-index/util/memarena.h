/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/*
 * A memarena is used to efficiently store a collection of objects that never move
 * The pattern is allocate more and more stuff and free all of the items at once.
 * The underlying memory will store 1 or more objects per chunk. Each chunk is 
 * contiguously laid out in memory but chunks are not necessarily contiguous with
 * each other.
 */
class memarena {
public:
    memarena() :
        _current_chunk(arena_chunk()),
        _other_chunks(nullptr),
        _n_other_chunks(0),
        _size_of_other_chunks(0),
        _footprint_of_other_chunks(0) {
    }

    // Effect: Create a memarena with the specified initial size
    void create(size_t initial_size);

    void destroy(void);

    // Effect: Allocate some memory.  The returned value remains valid until the memarena is cleared or closed.
    //  In case of ENOMEM, aborts.
    void *malloc_from_arena(size_t size);

    // Effect: Move all the memory from this memarena into DEST. 
    //         When SOURCE is closed the memory won't be freed. 
    //         When DEST is closed, the memory will be freed, unless DEST moves its memory to another memarena...
    void move_memory(memarena *dest);

    // Effect: Calculate the amount of memory used by a memory arena.
    size_t total_memory_size(void) const;

    // Effect: Calculate the used space of the memory arena (ie: excludes unused space)
    size_t total_size_in_use(void) const;

    // Effect: Calculate the amount of memory used, according to toku_memory_footprint(),
    //         which is a more expensive but more accurate count of memory used.
    size_t total_footprint(void) const;

    // iterator over the underlying chunks that store objects in the memarena.
    // a chunk is represented by a pointer to const memory and a usable byte count.
    class chunk_iterator {
    public:
        chunk_iterator(const memarena *ma) :
            _ma(ma), _chunk_idx(-1) {
        }

        // returns: base pointer to the current chunk
        //          *used set to the number of usable bytes
        //          if more() is false, returns nullptr and *used = 0
        const void *current(size_t *used) const;

        // requires: more() is true
        void next();

        bool more() const;

    private:
        // -1 represents the 'initial' chunk in a memarena, ie: ma->_current_chunk
        // >= 0 represents the i'th chunk in the ma->_other_chunks array
        const memarena *_ma;
        int _chunk_idx;
    };

private:
    struct arena_chunk {
        arena_chunk() : buf(nullptr), used(0), size(0) { }
        char *buf;
        size_t used;
        size_t size;
    };

    struct arena_chunk _current_chunk;
    struct arena_chunk *_other_chunks;
    int _n_other_chunks;
    size_t _size_of_other_chunks; // the buf_size of all the other chunks.
    size_t _footprint_of_other_chunks; // the footprint of all the other chunks.

    friend class memarena_unit_test;
};
