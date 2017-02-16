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

#include <string.h>

#include "portability/memory.h"

#include "locktree/range_buffer.h"
#include "util/dbt.h"

namespace toku {

    bool range_buffer::record_header::left_is_infinite(void) const {
        return left_neg_inf || left_pos_inf;
    }

    bool range_buffer::record_header::right_is_infinite(void) const {
        return right_neg_inf || right_pos_inf;
    }

    void range_buffer::record_header::init(const DBT *left_key, const DBT *right_key) {
        left_neg_inf = left_key == toku_dbt_negative_infinity();
        left_pos_inf = left_key == toku_dbt_positive_infinity();
        left_key_size = toku_dbt_is_infinite(left_key) ? 0 : left_key->size;
        if (right_key) {
            right_neg_inf = right_key == toku_dbt_negative_infinity();
            right_pos_inf = right_key == toku_dbt_positive_infinity();
            right_key_size = toku_dbt_is_infinite(right_key) ? 0 : right_key->size; 
        } else {
            right_neg_inf = left_neg_inf;
            right_pos_inf = left_pos_inf;
            right_key_size = 0;
        }
    }
        
    const DBT *range_buffer::iterator::record::get_left_key(void) const {
        if (_header.left_neg_inf) {
            return toku_dbt_negative_infinity();
        } else if (_header.left_pos_inf) {
            return toku_dbt_positive_infinity();
        } else {
            return &_left_key;
        }
    }

    const DBT *range_buffer::iterator::record::get_right_key(void) const {
        if (_header.right_neg_inf) {
            return toku_dbt_negative_infinity();
        } else if (_header.right_pos_inf) {
            return toku_dbt_positive_infinity();
        } else {
            return &_right_key;
        }
    }

    size_t range_buffer::iterator::record::size(void) const {
        return sizeof(record_header) + _header.left_key_size + _header.right_key_size;
    }

    void range_buffer::iterator::record::deserialize(const char *buf) {
        size_t current = 0;

        // deserialize the header
        memcpy(&_header, buf, sizeof(record_header));
        current += sizeof(record_header);

        // deserialize the left key if necessary
        if (!_header.left_is_infinite()) {
            // point the left DBT's buffer into ours
            toku_fill_dbt(&_left_key, buf + current, _header.left_key_size);
            current += _header.left_key_size;
        }

        // deserialize the right key if necessary
        if (!_header.right_is_infinite()) {
            if (_header.right_key_size == 0) {
                toku_copyref_dbt(&_right_key, _left_key);
            } else {
                toku_fill_dbt(&_right_key, buf + current, _header.right_key_size);
            }
        }
    }

    toku::range_buffer::iterator::iterator() :
        _ma_chunk_iterator(nullptr),
        _current_chunk_base(nullptr),
        _current_chunk_offset(0), _current_chunk_max(0),
        _current_rec_size(0) {
    }

    toku::range_buffer::iterator::iterator(const range_buffer *buffer) :
        _ma_chunk_iterator(&buffer->_arena),
        _current_chunk_base(nullptr),
        _current_chunk_offset(0), _current_chunk_max(0),
        _current_rec_size(0) {
        reset_current_chunk();
    }

    void range_buffer::iterator::reset_current_chunk() {
        _current_chunk_base = _ma_chunk_iterator.current(&_current_chunk_max);
        _current_chunk_offset = 0;
    }

    bool range_buffer::iterator::current(record *rec) {
        if (_current_chunk_offset < _current_chunk_max) {
            const char *buf = reinterpret_cast<const char *>(_current_chunk_base); 
            rec->deserialize(buf + _current_chunk_offset);
            _current_rec_size = rec->size();
            return true;
        } else {
            return false;
        }
    }

    // move the iterator to the next record in the buffer
    void range_buffer::iterator::next(void) {
        invariant(_current_chunk_offset < _current_chunk_max);
        invariant(_current_rec_size > 0);

        // the next record is _current_rec_size bytes forward
        _current_chunk_offset += _current_rec_size;
        // now, we don't know how big the current is, set it to 0.
        _current_rec_size = 0;

        if (_current_chunk_offset >= _current_chunk_max) {
            // current chunk is exhausted, try moving to the next one
            if (_ma_chunk_iterator.more()) {
                _ma_chunk_iterator.next();
                reset_current_chunk();
            }
        }
    }

    void range_buffer::create(void) {
        // allocate buffer space lazily instead of on creation. this way,
        // no malloc/free is done if the transaction ends up taking no locks.
        _arena.create(0);
        _num_ranges = 0;
    }

    void range_buffer::append(const DBT *left_key, const DBT *right_key) {
        // if the keys are equal, then only one copy is stored.
        if (toku_dbt_equals(left_key, right_key)) {
            invariant(left_key->size <= MAX_KEY_SIZE);
            append_point(left_key);
        } else {
            invariant(left_key->size <= MAX_KEY_SIZE);
            invariant(right_key->size <= MAX_KEY_SIZE);
            append_range(left_key, right_key);
        }
        _num_ranges++;
    }

    bool range_buffer::is_empty(void) const {
        return total_memory_size() == 0;
    }

    uint64_t range_buffer::total_memory_size(void) const {
        return _arena.total_size_in_use();
    }

    int range_buffer::get_num_ranges(void) const {
        return _num_ranges;
    }

    void range_buffer::destroy(void) {
        _arena.destroy();
    }

    void range_buffer::append_range(const DBT *left_key, const DBT *right_key) {
        size_t record_length = sizeof(record_header) + left_key->size + right_key->size;
        char *buf = reinterpret_cast<char *>(_arena.malloc_from_arena(record_length));

        record_header h;
        h.init(left_key, right_key);

        // serialize the header
        memcpy(buf, &h, sizeof(record_header));
        buf += sizeof(record_header);

        // serialize the left key if necessary
        if (!h.left_is_infinite()) {
            memcpy(buf, left_key->data, left_key->size);
            buf += left_key->size;
        }

        // serialize the right key if necessary
        if (!h.right_is_infinite()) {
            memcpy(buf, right_key->data, right_key->size);
        }
    }

    void range_buffer::append_point(const DBT *key) {
        size_t record_length = sizeof(record_header) + key->size;
        char *buf = reinterpret_cast<char *>(_arena.malloc_from_arena(record_length));

        record_header h;
        h.init(key, nullptr);

        // serialize the header
        memcpy(buf, &h, sizeof(record_header));
        buf += sizeof(record_header);

        // serialize the key if necessary
        if (!h.left_is_infinite()) {
            memcpy(buf, key->data, key->size);
        }
    }

} /* namespace toku */
