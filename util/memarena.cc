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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <algorithm>
#include <string.h>
#include <memory.h>

#include <util/memarena.h>

void memarena::create(size_t initial_size) {
    _current_chunk = arena_chunk();
    _other_chunks = nullptr;
    _size_of_other_chunks = 0;
    _footprint_of_other_chunks = 0;
    _n_other_chunks = 0;

    _current_chunk.size = initial_size;
    if (_current_chunk.size > 0) {
        XMALLOC_N(_current_chunk.size, _current_chunk.buf);
    }
}

void memarena::destroy(void) {
    if (_current_chunk.buf) {
        toku_free(_current_chunk.buf);
    }
    for (int i = 0; i < _n_other_chunks; i++) {
        toku_free(_other_chunks[i].buf);
    }
    if (_other_chunks) {
        toku_free(_other_chunks);
    }
    _current_chunk = arena_chunk();
    _other_chunks = nullptr;
    _n_other_chunks = 0;
}

static size_t round_to_page(size_t size) {
    const size_t page_size = 4096;
    const size_t r = page_size + ((size - 1) & ~(page_size - 1));
    assert((r & (page_size - 1)) == 0); // make sure it's aligned
    assert(r >= size);              // make sure it's not too small
    assert(r < size + page_size);     // make sure we didn't grow by more than a page.
    return r;
}

static const size_t MEMARENA_MAX_CHUNK_SIZE = 64 * 1024 * 1024;

void *memarena::malloc_from_arena(size_t size) {
    if (_current_chunk.buf == nullptr || _current_chunk.size < _current_chunk.used + size) {
        // The existing block isn't big enough.
        // Add the block to the vector of blocks.
        if (_current_chunk.buf) {
            invariant(_current_chunk.size > 0);
            int old_n = _n_other_chunks;
            XREALLOC_N(old_n + 1, _other_chunks);
            _other_chunks[old_n] = _current_chunk;
            _n_other_chunks = old_n + 1;
            _size_of_other_chunks += _current_chunk.size;
            _footprint_of_other_chunks += toku_memory_footprint(_current_chunk.buf, _current_chunk.used);
        }

        // Make a new one. Grow the buffer size exponentially until we hit
        // the max chunk size, but make it at least `size' bytes so the
        // current allocation always fit.
        size_t new_size = std::min(MEMARENA_MAX_CHUNK_SIZE, 2 * _current_chunk.size);
        if (new_size < size) {
            new_size = size;
        }
        new_size = round_to_page(new_size); // at least size, but round to the next page size
        XMALLOC_N(new_size, _current_chunk.buf);
        _current_chunk.used = 0;
        _current_chunk.size = new_size;
    }
    invariant(_current_chunk.buf != nullptr);

    // allocate in the existing block.
    char *p = _current_chunk.buf + _current_chunk.used;
    _current_chunk.used += size;
    return p;
}

void memarena::move_memory(memarena *dest) {
    // Move memory to dest
    XREALLOC_N(dest->_n_other_chunks + _n_other_chunks + 1, dest->_other_chunks);
    dest->_size_of_other_chunks += _size_of_other_chunks + _current_chunk.size;
    dest->_footprint_of_other_chunks += _footprint_of_other_chunks + toku_memory_footprint(_current_chunk.buf, _current_chunk.used);
    for (int i = 0; i < _n_other_chunks; i++) {
        dest->_other_chunks[dest->_n_other_chunks++] = _other_chunks[i];
    }
    dest->_other_chunks[dest->_n_other_chunks++] = _current_chunk;

    // Clear out this memarena's memory
    toku_free(_other_chunks);
    _current_chunk = arena_chunk();
    _other_chunks = nullptr;
    _size_of_other_chunks = 0;
    _footprint_of_other_chunks = 0;
    _n_other_chunks = 0;
}

size_t memarena::total_memory_size(void) const {
    return sizeof(*this) +
           total_size_in_use() +
           _n_other_chunks * sizeof(*_other_chunks);
}

size_t memarena::total_size_in_use(void) const {
    return _size_of_other_chunks + _current_chunk.used;
}

size_t memarena::total_footprint(void) const {
    return sizeof(*this) +
           _footprint_of_other_chunks +
           toku_memory_footprint(_current_chunk.buf, _current_chunk.used) +
           _n_other_chunks * sizeof(*_other_chunks);
}

////////////////////////////////////////////////////////////////////////////////

const void *memarena::chunk_iterator::current(size_t *used) const {
    if (_chunk_idx < 0) {
        *used = _ma->_current_chunk.used;
        return _ma->_current_chunk.buf;
    } else if (_chunk_idx < _ma->_n_other_chunks) {
        *used = _ma->_other_chunks[_chunk_idx].used;
        return _ma->_other_chunks[_chunk_idx].buf;
    }
    *used = 0;
    return nullptr;
}

void memarena::chunk_iterator::next() {
    _chunk_idx++;
}

bool memarena::chunk_iterator::more() const {
    if (_chunk_idx < 0) {
        return _ma->_current_chunk.buf != nullptr;
    }
    return _chunk_idx < _ma->_n_other_chunks;
}
