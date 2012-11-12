/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <memory.h>
#include <string.h>
#include <ft/ybt.h>

#include "range_buffer.h"

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
    if (m_header.left_neg_inf) {
        return toku_dbt_negative_infinity();
    } else if (m_header.left_pos_inf) {
        return toku_dbt_positive_infinity();
    } else {
        return &m_left_key;
    }
}

const DBT *range_buffer::iterator::record::get_right_key(void) const {
    if (m_header.right_neg_inf) {
        return toku_dbt_negative_infinity();
    } else if (m_header.right_pos_inf) {
        return toku_dbt_positive_infinity();
    } else {
        return &m_right_key;
    }
}

size_t range_buffer::iterator::record::size(void) const {
    return sizeof(record_header) + m_header.left_key_size + m_header.right_key_size;
}

void range_buffer::iterator::record::deserialize(const char *buf) {
    size_t current = 0;

    // deserialize the header
    memcpy(&m_header, buf, sizeof(record_header));
    current += sizeof(record_header);

    // deserialize the left key if necessary
    if (!m_header.left_is_infinite()) {
        // point the left DBT's buffer into ours
        toku_fill_dbt(&m_left_key, buf + current, m_header.left_key_size);
        current += m_header.left_key_size;
    }

    // deserialize the right key if necessary
    if (!m_header.right_is_infinite()) {
        if (m_header.right_key_size == 0) {
            toku_copyref_dbt(&m_right_key, m_left_key);
        } else {
            toku_fill_dbt(&m_right_key, buf + current, m_header.right_key_size);
        }
    }
}

void range_buffer::iterator::create(const range_buffer *buffer) {
    m_buffer = buffer;
    m_current_offset = 0;
    m_current_size = 0;
}

bool range_buffer::iterator::current(record *rec) {
    if (m_current_offset < m_buffer->m_buf_current) {
        rec->deserialize(m_buffer->m_buf + m_current_offset);
        m_current_size = rec->size();
        return true;
    } else {
        return false;
    }
}

// move the iterator to the next record in the buffer
void range_buffer::iterator::next(void) {
    invariant(m_current_offset < m_buffer->m_buf_current);
    invariant(m_current_size > 0);

    // the next record is m_current_size bytes forward
    // now, we don't know how big the current is, set it to 0.
    m_current_offset += m_current_size;
    m_current_size = 0;
}

void range_buffer::create(void) {
    // allocate buffer space lazily instead of on creation. this way,
    // no malloc/free is done if the transaction ends up taking no locks.
    m_buf = nullptr;
    m_buf_size = 0;
    m_buf_current = 0;
}

void range_buffer::append(const DBT *left_key, const DBT *right_key) {
    // if the keys are equal, then only one copy is stored.
    if (toku_dbt_equals(left_key, right_key)) {
        append_point(left_key);
    } else {
        append_range(left_key, right_key);
    }
}

void range_buffer::destroy(void) {
    if (m_buf) {
        toku_free(m_buf);
    }
}

void range_buffer::append_range(const DBT *left_key, const DBT *right_key) {
    maybe_grow(sizeof(record_header) + left_key->size + right_key->size);

    record_header h;
    h.init(left_key, right_key);

    // serialize the header
    memcpy(m_buf + m_buf_current, &h, sizeof(record_header));
    m_buf_current += sizeof(record_header);

    // serialize the left key if necessary
    if (!h.left_is_infinite()) {
        memcpy(m_buf + m_buf_current, left_key->data, left_key->size);
        m_buf_current += left_key->size;
    }

    // serialize the right key if necessary
    if (!h.right_is_infinite()) {
        memcpy(m_buf + m_buf_current, right_key->data, right_key->size);
        m_buf_current += right_key->size;
    }
}

void range_buffer::append_point(const DBT *key) {
    maybe_grow(sizeof(record_header) + key->size);

    record_header h;
    h.init(key, nullptr);

    // serialize the header
    memcpy(m_buf + m_buf_current, &h, sizeof(record_header));
    m_buf_current += sizeof(record_header);

    // serialize the key if necessary
    if (!h.left_is_infinite()) {
        memcpy(m_buf + m_buf_current, key->data, key->size);
        m_buf_current += key->size;
    }
}

void range_buffer::maybe_grow(size_t size) {
    static const size_t aggressive_growth_threshold = 128 * 1024;
    if (m_buf_current + size > m_buf_size) {
        // aggressively grow the range buffer to the threshold,
        // but only additivately increase the size after that.
        if (m_buf_size < aggressive_growth_threshold) {
            m_buf_size = m_buf_size ? m_buf_size * 2 : get_initial_size(size);
        } else {
            m_buf_size += aggressive_growth_threshold;
        }
        XREALLOC(m_buf, m_buf_size);
    }
}

size_t range_buffer::get_initial_size(size_t n) const {
    size_t r = 4096;
    while (r < n) {
        r *= 2;
    }
    return r;
}

} /* namespace toku */
