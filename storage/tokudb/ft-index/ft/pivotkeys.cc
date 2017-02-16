/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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

#include <config.h>

#include <string>

#include "portability/memory.h"

#include "ft/node.h"
#include "ft/serialize/rbuf.h"
#include "ft/serialize/wbuf.h"

void ftnode_pivot_keys::create_empty() {
    _num_pivots = 0;
    _total_size = 0;
    _fixed_keys = nullptr;
    _fixed_keylen = 0;
    _fixed_keylen_aligned = 0;
    _dbt_keys = nullptr;
}

void ftnode_pivot_keys::create_from_dbts(const DBT *keys, int n) {
    create_empty();
    _num_pivots = n;

    // see if every key has the same length
    bool keys_same_size = true;
    for (int i = 1; i < _num_pivots; i++) {
        if (keys[i].size != keys[i - 1].size) {
            keys_same_size = false;
            break;
        }
    }

    if (keys_same_size && _num_pivots > 0) {
        // if so, store pivots in a tightly packed array of fixed length keys
        _fixed_keylen = keys[0].size;
        _fixed_keylen_aligned = _align4(_fixed_keylen);
        _total_size = _fixed_keylen_aligned * _num_pivots;
        XMALLOC_N_ALIGNED(64, _total_size, _fixed_keys);
        for (int i = 0; i < _num_pivots; i++) {
            invariant(keys[i].size == _fixed_keylen);
            memcpy(_fixed_key(i), keys[i].data, _fixed_keylen);
        }
    } else {
        // otherwise we'll just store the pivots in an array of dbts
        XMALLOC_N_ALIGNED(64, _num_pivots, _dbt_keys);
        for (int i = 0; i < _num_pivots; i++) {
            size_t size = keys[i].size;
            toku_memdup_dbt(&_dbt_keys[i], keys[i].data, size);
            _total_size += size;
        }
    }

    sanity_check();
}

void ftnode_pivot_keys::_create_from_fixed_keys(const char *fixedkeys, size_t fixed_keylen, int n) {
    create_empty();
    _num_pivots = n;
    _fixed_keylen = fixed_keylen;
    _fixed_keylen_aligned = _align4(fixed_keylen);
    _total_size = _fixed_keylen_aligned * _num_pivots;
    XMEMDUP_N(_fixed_keys, fixedkeys, _total_size);
}

// effect: create pivot keys as a clone of an existing set of pivotkeys
void ftnode_pivot_keys::create_from_pivot_keys(const ftnode_pivot_keys &pivotkeys) {
    if (pivotkeys._fixed_format()) {
        _create_from_fixed_keys(pivotkeys._fixed_keys, pivotkeys._fixed_keylen, pivotkeys._num_pivots);
    } else {
        create_from_dbts(pivotkeys._dbt_keys, pivotkeys._num_pivots);
    }

    sanity_check();
}

void ftnode_pivot_keys::destroy() {
    if (_dbt_keys != nullptr) {
        for (int i = 0; i < _num_pivots; i++) {
            toku_destroy_dbt(&_dbt_keys[i]);
        }
        toku_free(_dbt_keys);
        _dbt_keys = nullptr;
    }
    if (_fixed_keys != nullptr) {
        toku_free(_fixed_keys);
        _fixed_keys = nullptr;
    }
    _fixed_keylen = 0;
    _fixed_keylen_aligned = 0;
    _num_pivots = 0;
    _total_size = 0;
}

void ftnode_pivot_keys::_convert_to_fixed_format() {
    invariant(!_fixed_format());

    // convert to a tightly packed array of fixed length keys
    _fixed_keylen = _dbt_keys[0].size;
    _fixed_keylen_aligned = _align4(_fixed_keylen);
    _total_size = _fixed_keylen_aligned * _num_pivots;
    XMALLOC_N_ALIGNED(64, _total_size, _fixed_keys);
    for (int i = 0; i < _num_pivots; i++) {
        invariant(_dbt_keys[i].size == _fixed_keylen);
        memcpy(_fixed_key(i), _dbt_keys[i].data, _fixed_keylen);
    }

    // destroy the dbt array format
    for (int i = 0; i < _num_pivots; i++) {
        toku_destroy_dbt(&_dbt_keys[i]);
    }
    toku_free(_dbt_keys);
    _dbt_keys = nullptr;

    invariant(_fixed_format());
    sanity_check();
}

void ftnode_pivot_keys::_convert_to_dbt_format() {
    invariant(_fixed_format());

    // convert to an aray of dbts
    REALLOC_N_ALIGNED(64, _num_pivots, _dbt_keys);
    for (int i = 0; i < _num_pivots; i++) {
        toku_memdup_dbt(&_dbt_keys[i], _fixed_key(i), _fixed_keylen);
    }
    // pivots sizes are not aligned up  dbt format
    _total_size = _num_pivots * _fixed_keylen;

    // destroy the fixed key format
    toku_free(_fixed_keys);
    _fixed_keys = nullptr;
    _fixed_keylen = 0;
    _fixed_keylen_aligned = 0;

    invariant(!_fixed_format());
    sanity_check();
}

void ftnode_pivot_keys::deserialize_from_rbuf(struct rbuf *rb, int n) {
    _num_pivots = n;
    _total_size = 0;
    _fixed_keys = nullptr;
    _fixed_keylen = 0;
    _dbt_keys = nullptr;

    XMALLOC_N_ALIGNED(64, _num_pivots, _dbt_keys);
    bool keys_same_size = true;
    for (int i = 0; i < _num_pivots; i++) {
        const void *pivotkeyptr;
        uint32_t size;
        rbuf_bytes(rb, &pivotkeyptr, &size);
        toku_memdup_dbt(&_dbt_keys[i], pivotkeyptr, size);
        _total_size += size;
        if (i > 0 && keys_same_size && _dbt_keys[i].size != _dbt_keys[i - 1].size) {
            // not all keys are the same size, we'll stick to the dbt array format
            keys_same_size = false;
        }
    }

    if (keys_same_size && _num_pivots > 0) {
        _convert_to_fixed_format();
    }

    sanity_check();
}

DBT ftnode_pivot_keys::get_pivot(int i) const {
    paranoid_invariant(i < _num_pivots);
    if (_fixed_format()) {
        paranoid_invariant(i * _fixed_keylen_aligned < _total_size);
        DBT dbt;
        toku_fill_dbt(&dbt, _fixed_key(i), _fixed_keylen);
        return dbt;
    } else {
        return _dbt_keys[i];
    }
}

DBT *ftnode_pivot_keys::fill_pivot(int i, DBT *dbt) const {
    paranoid_invariant(i < _num_pivots);
    if (_fixed_format()) {
        toku_fill_dbt(dbt, _fixed_key(i), _fixed_keylen);
    } else {
        toku_copyref_dbt(dbt, _dbt_keys[i]);
    }
    return dbt;
}

void ftnode_pivot_keys::_add_key_dbt(const DBT *key, int i) {
    toku_clone_dbt(&_dbt_keys[i], *key);
    _total_size += _dbt_keys[i].size;
}

void ftnode_pivot_keys::_destroy_key_dbt(int i) {
    invariant(_total_size >= _dbt_keys[i].size);
    _total_size -= _dbt_keys[i].size;
    toku_destroy_dbt(&_dbt_keys[i]);
}

void ftnode_pivot_keys::_insert_at_dbt(const DBT *key, int i) {
    // make space for a new pivot, slide existing keys to the right
    REALLOC_N_ALIGNED(64, _num_pivots + 1, _dbt_keys);
    memmove(&_dbt_keys[i + 1], &_dbt_keys[i], (_num_pivots - i) * sizeof(DBT));
    _add_key_dbt(key, i);
}

void ftnode_pivot_keys::_insert_at_fixed(const DBT *key, int i) {
    REALLOC_N_ALIGNED(64, (_num_pivots + 1) * _fixed_keylen_aligned, _fixed_keys); 
    // TODO: This is not going to be valgrind-safe, because we do not initialize the space
    // between _fixed_keylen and _fixed_keylen_aligned (but we probably should)
    memmove(_fixed_key(i + 1), _fixed_key(i), (_num_pivots - i) * _fixed_keylen_aligned);
    memcpy(_fixed_key(i), key->data, _fixed_keylen);
    _total_size += _fixed_keylen_aligned;
}

void ftnode_pivot_keys::insert_at(const DBT *key, int i) {
    invariant(i <= _num_pivots); // it's ok to insert at the end, so we check <= n

    // if the new key doesn't have the same size, we can't be in fixed format
    if (_fixed_format() && key->size != _fixed_keylen) {
        _convert_to_dbt_format();
    }

    if (_fixed_format()) {
        _insert_at_fixed(key, i);
    } else {
        _insert_at_dbt(key, i);
    }
    _num_pivots++;

    invariant(total_size() > 0);
}

void ftnode_pivot_keys::_append_dbt(const ftnode_pivot_keys &pivotkeys) {
    REALLOC_N_ALIGNED(64, _num_pivots + pivotkeys._num_pivots, _dbt_keys);
    bool other_fixed = pivotkeys._fixed_format();
    for (int i = 0; i < pivotkeys._num_pivots; i++) {
        size_t size = other_fixed ? pivotkeys._fixed_keylen :
                                    pivotkeys._dbt_keys[i].size;
        toku_memdup_dbt(&_dbt_keys[_num_pivots + i],
                        other_fixed ? pivotkeys._fixed_key(i) :
                                      pivotkeys._dbt_keys[i].data,
                        size);
        _total_size += size;
    }
}

void ftnode_pivot_keys::_append_fixed(const ftnode_pivot_keys &pivotkeys) {
    if (pivotkeys._fixed_format() && pivotkeys._fixed_keylen == _fixed_keylen) {
        // other pivotkeys have the same fixed keylen 
        REALLOC_N_ALIGNED(64, (_num_pivots + pivotkeys._num_pivots) * _fixed_keylen_aligned, _fixed_keys);
        memcpy(_fixed_key(_num_pivots), pivotkeys._fixed_keys, pivotkeys._total_size);
        _total_size += pivotkeys._total_size;
    } else {
        // must convert to dbt format, other pivotkeys have different length'd keys
        _convert_to_dbt_format();
        _append_dbt(pivotkeys);
    }
}

void ftnode_pivot_keys::append(const ftnode_pivot_keys &pivotkeys) {
    if (_fixed_format()) {
        _append_fixed(pivotkeys);
    } else {
        _append_dbt(pivotkeys);
    }
    _num_pivots += pivotkeys._num_pivots;

    sanity_check();
}

void ftnode_pivot_keys::_replace_at_dbt(const DBT *key, int i) {
    _destroy_key_dbt(i);
    _add_key_dbt(key, i);
}

void ftnode_pivot_keys::_replace_at_fixed(const DBT *key, int i) {
    if (key->size == _fixed_keylen) {
        memcpy(_fixed_key(i), key->data, _fixed_keylen);
    } else {
        // must convert to dbt format, replacement key has different length
        _convert_to_dbt_format();
        _replace_at_dbt(key, i);
    }
}

void ftnode_pivot_keys::replace_at(const DBT *key, int i) {
    if (i < _num_pivots) {
        if (_fixed_format()) {
            _replace_at_fixed(key, i);
        } else {
            _replace_at_dbt(key, i);
        }
    } else {
        invariant(i == _num_pivots); // appending to the end is ok
        insert_at(key, i);
    }
    invariant(total_size() > 0);
}

void ftnode_pivot_keys::_delete_at_fixed(int i) {
    memmove(_fixed_key(i), _fixed_key(i + 1), (_num_pivots - 1 - i) * _fixed_keylen_aligned);
    _total_size -= _fixed_keylen_aligned;
}

void ftnode_pivot_keys::_delete_at_dbt(int i) {
    // slide over existing keys, then shrink down to size
    _destroy_key_dbt(i);
    memmove(&_dbt_keys[i], &_dbt_keys[i + 1], (_num_pivots - 1 - i) * sizeof(DBT));
    REALLOC_N_ALIGNED(64, _num_pivots - 1, _dbt_keys);
}

void ftnode_pivot_keys::delete_at(int i) {
    invariant(i < _num_pivots);

    if (_fixed_format()) {
        _delete_at_fixed(i);
    } else {
        _delete_at_dbt(i);
    }

    _num_pivots--;
}

void ftnode_pivot_keys::_split_at_fixed(int i, ftnode_pivot_keys *other) {
    // recreate the other set of pivots from index >= i
    other->_create_from_fixed_keys(_fixed_key(i), _fixed_keylen, _num_pivots - i);

    // shrink down to size
    _total_size = i * _fixed_keylen_aligned;
    REALLOC_N_ALIGNED(64, _total_size, _fixed_keys);
}

void ftnode_pivot_keys::_split_at_dbt(int i, ftnode_pivot_keys *other) {
    // recreate the other set of pivots from index >= i
    other->create_from_dbts(&_dbt_keys[i], _num_pivots - i);

    // destroy everything greater, shrink down to size
    for (int k = i; k < _num_pivots; k++) {
        _destroy_key_dbt(k);
    }
    REALLOC_N_ALIGNED(64, i, _dbt_keys);
}

void ftnode_pivot_keys::split_at(int i, ftnode_pivot_keys *other) {
    if (i < _num_pivots) {
        if (_fixed_format()) {
            _split_at_fixed(i, other);
        } else {
            _split_at_dbt(i, other);
        }
        _num_pivots = i;
    }

    sanity_check();
}

void ftnode_pivot_keys::serialize_to_wbuf(struct wbuf *wb) const {
    bool fixed = _fixed_format();
    size_t written = 0;
    for (int i = 0; i < _num_pivots; i++) {
        size_t size = fixed ? _fixed_keylen : _dbt_keys[i].size;
        invariant(size);
        wbuf_nocrc_bytes(wb, fixed ? _fixed_key(i) : _dbt_keys[i].data, size);
        written += size;
    }
    invariant(written == serialized_size());
}

int ftnode_pivot_keys::num_pivots() const {
    // if we have fixed size keys, the number of pivots should be consistent
    paranoid_invariant(_fixed_keys == nullptr || (_total_size == _fixed_keylen_aligned * _num_pivots));
    return _num_pivots;
}

size_t ftnode_pivot_keys::total_size() const {
    // if we have fixed size keys, the total size should be consistent
    paranoid_invariant(_fixed_keys == nullptr || (_total_size == _fixed_keylen_aligned * _num_pivots));
    return _total_size;
}

size_t ftnode_pivot_keys::serialized_size() const {
    // we only return the size that will be used when serialized, so we calculate based
    // on the fixed keylen and not the aligned keylen.
    return _fixed_format() ? _num_pivots * _fixed_keylen : _total_size;
}

void ftnode_pivot_keys::sanity_check() const {
    if (_fixed_format()) {
        invariant(_dbt_keys == nullptr);
        invariant(_fixed_keylen_aligned == _align4(_fixed_keylen));
        invariant(_num_pivots * _fixed_keylen <= _total_size);
        invariant(_num_pivots * _fixed_keylen_aligned == _total_size);
    } else {
        invariant(_num_pivots == 0 || _dbt_keys != nullptr);
        size_t size = 0;
        for (int i = 0; i < _num_pivots; i++) {
            size += _dbt_keys[i].size;
        }
        invariant(size == _total_size);
    }
}
